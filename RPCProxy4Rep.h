#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MemoryWriter.h"
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
	void __Internal_Call(UObject* InUserObject, FName InFunctionName, class AActor* PC, const TArray<uint8>& Buffer);

	static void __Internal_CallRemote(class APlayerController* PC, UObject* InUserObject, const FName& InFunctionName,
									  const TArray<uint8>& Buffer);

public:
	template<typename>
	friend struct RepRPCHelper;
};

namespace NRPCProxy4Rep
{
template<typename... Args>
struct TArgsList
{
};
template<typename... Args>
struct TArgsTraits
{
	template<typename T, typename... S>
	static auto GetFirstType(TArgsList<T, S...>) -> T;
	static auto GetFirstType(TArgsList<>) -> void;

	using FirstType = decltype(GetFirstType(TArgsList<Args...>()));
	using IsPC = std::is_same<APlayerController*, std::decay_t<FirstType>>;
};
template<typename Skip, typename... Args>
void MemoryWrite(std::true_type, TArray<uint8>& Buffer, Skip s, Args... InArgs)
{
	FMemoryWriter Writer{Buffer};
	int Temp[] = {0, (void(Writer << InArgs), 0)...};
	(void)(Temp);
}
template<typename... Args>
void MemoryWrite(std::false_type, TArray<uint8>& Buffer, Args... InArgs)
{
	FMemoryWriter Writer{Buffer};
	int Temp[] = {0, (void(Writer << InArgs), 0)...};
	(void)(Temp);
}
}  // namespace NRPCProxy4Rep

template<typename F>
struct RepRPCHelper;

template<typename UserClass, typename... Args>
struct RepRPCHelper<void (UserClass::*)(Args...)>
{
	static auto CallRemote(APlayerController* PC, UserClass* InUserObject, const FName& InFunctionName, Args... InArgs)
	{
		check(InUserObject);
		TArray<uint8> Buffer;
		NRPCProxy4Rep::MemoryWrite(typename NRPCProxy4Rep::TArgsTraits<Args...>::IsPC(), Buffer,
								   ((std::remove_cv_t<Args>&)InArgs)...);
		return URPCProxy4Rep::__Internal_CallRemote(PC, InUserObject, InFunctionName, Buffer);
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
