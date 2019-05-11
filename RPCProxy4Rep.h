#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CoreNet.h"
#include <type_traits>
#include "RPCProxy4Rep.generated.h"

class APlayerController;
UCLASS(NotBlueprintType, NotBlueprintable)
class UE4SINGLETONS_API URPCProxy4Rep : public UActorComponent
{
	GENERATED_BODY()
	URPCProxy4Rep();

protected:
	UFUNCTION(Server, Reliable, WithValidation)
	void RPC_Reuqest(UObject* Object, const FName& FuncName, const TArray<uint8>& Buffer);
	UFUNCTION(Client, Reliable)
	void RPC_Notify(UObject* Object, const FName& FuncName, const TArray<uint8>& Buffer);
	void __Internal_Call(UObject* InUserObject, FName InFunctionName, const TArray<uint8>& Buffer);

	static void __Internal_CallRemote(APlayerController* PC, UObject* InUserObject, const FName& InFunctionName,
									  const TArray<uint8>& Buffer);

	static class UPackageMap* GetPackageMap(APlayerController* PC);
	static int64 MaxBitCount;

	template<typename... Args>
	void MemoryWrite(FArchive& Ar, Args... InArgs)
	{
		int Temp[] = {0, (void(Ar << InArgs), 0)...};
		(void)(Temp);
	}

public:
	template<typename>
	friend struct RepRPCHelper;
};

template<typename F>
struct RepRPCHelper;

template<typename UserClass, typename... Args>
struct RepRPCHelper<void (UserClass::*)(Args...)>
{
	static auto CallRemote(APlayerController* PC, UserClass* InUserObject, const FName& InFunctionName, Args... InArgs)
	{
		check(InUserObject && PC);
		FNetBitWriter Writer{URPCProxy4Rep::GetPackageMap(PC), 8};
		URPCProxy4Rep::MemoryWrite(Writer, ((std::remove_cv_t<Args>&)InArgs)...);
		if (ensure(!Writer.IsError() && Writer.GetNumBits() <= UPlayerRPCProxy::MaxBitCount))
			URPCProxy4Rep::__Internal_CallRemote(PC, InUserObject, InFunctionName, *Writer.GetBuffer());
	}
};

#define _RepCallRemote(PC, Obj, FuncName, ...) \
	RepRPCHelper<decltype(FuncName)>::CallRemote(PC, Obj, STATIC_FUNCTION_FNAME(TEXT(#FuncName)), __VA_ARGS__)

#if __MSV_VER__
#define _RepCallExpand(x) x
#define RepCallRequest(...) _RepCallExpand(_RepCallRemote(__VA_ARGS__))
#define RepCallNotify(...) _RepCallExpand(_RepCallRemote(__VA_ARGS__))
#else
#define RepCallRequest _RepCallRemote
#define RepCallNotify _RepCallRemote
#endif
