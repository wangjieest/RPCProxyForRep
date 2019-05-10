// Fill out your copyright notice in the Description page of Project Settings.

#include "RPCProxy4Rep.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "PropertyPortFlags.h"
#include "ObjectMacros.h"
#include "MemoryReader.h"

//////////////////////////////////////////////////////////////////////////
URPCProxy4Rep::URPCProxy4Rep()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicated(true);

	// 	if (HasAnyFlags(RF_ClassDefaultObject))
	// 	{
	//		// Auto Bind To PlayerController
	// 		UAttachedComponentRegistry::RegisterAutoSpawnComponent(APlayerController::StaticClass(),
	// 															   URPCProxy4Rep::StaticClass());
	// 	}
}

void URPCProxy4Rep::__Internal_Call(UObject* InUserObject, FName InFunctionName, class AActor* PC,
									const TArray<uint8>& Buffer)
{
	UFunction* Function = InUserObject ? InUserObject->FindFunction(InFunctionName) : nullptr;
	bool bExist = Function && Function->HasAnyFunctionFlags(FUNC_Native);
	ensure(bExist || GetNetMode() == NM_Client);
	if (!bExist)
		return;

	uint8* Parms = (uint8*)FMemory_Alloca(Function->ParmsSize);
	FMemory::Memzero(Parms, Function->ParmsSize);
	int32 NumParamsEvaluated = 0;
	for (TFieldIterator<UProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It, ++NumParamsEvaluated)
	{
		UProperty* LocalProp = *It;
#if WITH_EDITOR
		if (!LocalProp || It->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			int32 Count = NumParamsEvaluated;
			for (TFieldIterator<UProperty> It2(Function); Count > 0 && It2->HasAnyPropertyFlags(CPF_Parm);
				 ++It2, --Count)
			{ It2->DestroyValue_InContainer(Parms); }
			return;
		}
#endif
		if (!LocalProp->HasAnyPropertyFlags(CPF_ZeroConstructor))
		{
			LocalProp->InitializeValue_InContainer(Parms);
		}
	}

	bool bSucc = true;
	do
	{
		// First Param Object?
		TFieldIterator<UProperty> It(Function);
		if (PC)
		{
			for (; It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				UObjectPropertyBase* Op = dynamic_cast<UObjectPropertyBase*>(*It);
				if (ensure(Op))
				{
					Op->SetObjectPropertyValue(Op->ContainerPtrToValuePtr<uint8>(Parms), PC);
					++It;
				}
				else
				{
					bSucc = false;
				}
				break;
			}
			if (!bSucc)
				break;
		}

		FMemoryReader Reader(Buffer);
		for (; It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				bSucc = false;
				break;
			}

			UProperty* PropertyParam = *It;

			PropertyParam->SerializeItem(Reader, PropertyParam->ContainerPtrToValuePtr<uint8>(Parms));
			if (!ensure(!Reader.GetError()))
			{
				bSucc = false;
				break;
			}
		}
	} while (0);

	if (bSucc)
		InUserObject->ProcessEvent(Function, Parms);

	for (TFieldIterator<UProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{ It->DestroyValue_InContainer(Parms); }
}

void URPCProxy4Rep::RPC_Reuqest_Implementation(UObject* Object, const FName& FuncName, const TArray<uint8>& Buffer)
{
	__Internal_Call(Object, FuncName, GetOwner(), Buffer);
}

bool URPCProxy4Rep::RPC_Reuqest_Validate(UObject* Object, const FName& FuncName, const TArray<uint8>& Buffer)
{
	return ensure(IsValid(Object) && Object->FindFunction(FuncName));
}

void URPCProxy4Rep::RPC_Notify_Implementation(UObject* Object, const FName& FuncName, const TArray<uint8>& Buffer)
{
	__Internal_Call(Object, FuncName, nullptr, Buffer);
}

void URPCProxy4Rep::__Internal_CallRemote(class APlayerController* PC, UObject* InUserObject,
										  const FName& InFunctionName, const TArray<uint8>& Buffer)
{
	if (auto World = GEngine->GetWorldFromContextObject(InUserObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		bool bClient = World->GetNetMode() != NM_DedicatedServer;
		if (!PC && bClient)
			PC = World->GetFirstPlayerController();
		if (ensure(PC))
		{
			if (auto Comp = PC->FindComponentByClass<URPCProxy4Rep>())
			{
				if (bClient)
					Comp->RPC_Reuqest(InUserObject, InFunctionName, Buffer);
				else
					Comp->RPC_Notify(InUserObject, InFunctionName, Buffer);
			}
		}
	}
}
