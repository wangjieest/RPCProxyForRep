在UE4中,只有祖先Owner为PlayerController的Actor或者Component才支持RPC,有很多场景要求其他replicated对象也具有RPC功能.

一般情况下, 我们有两个处理方式

1 利用下行广播：可以简单使用NetMulitcast

2 通过具有RPC能力的Actor或者Component， 转发RPC。

上述方式或多或少有一些问题.

NetMultiCast只有下行逻辑，老版本的NetMulticast甚至不考虑相关性，全局广播(自己改引擎的话加一行代码即可cover)。

通过其他RPC代理会导致跳转复杂，写的时候还知道逻辑，写完了就有些看不懂，写的逻辑非常绕，代码不够简洁内聚。还会增加需要配置的心智负担，毕竟需要多个模块合作。

这里实现一种通用的代理RPC方式，使得Replicated对象可以直接对自身进行RPC调用，代码也足够简洁。

巧妙的地方在于利用了一个前提, 即RPC都是有Connection归属的, 而Connection都是PlayerController相关的.

通过在PlayerController或者其上的Component实现两个固定的RPC函数。

```
UFUNCTION(Server, Reliable, WithValidation)
void RPC_Reuqest(UObject* Object, const FName& FuncName, const TArray<uint8>& Buffer);

UFUNCTION(Client, Reliable)
void RPC_Notify(UObject* Object, const FName& FuncName, const TArray<uint8>& Buffer);
```

利用Buffer承载序列化参数,调用对象Object上的FuncName函数.

外层再利用宏和元编程校验参数正确性.

```
RepCallNotify(Player, ReplicatedClassObject, &ReplicatedClass::UFUNCTIONs, params...);
```

调用RPC代码方式基本和原有方式一致.

内层将参数序列化进Buffer, 调用上述真正的RPC函数.

在远端,反序列化出参数信息,然后调用对应的函数.进行处理即可.

Request的情况需要注意下, 如何通过校验上行内容的安全性. 保证DS的稳定性.
