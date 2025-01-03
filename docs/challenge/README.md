# 挑战

## 人工改写工作量非常大

如前文所提到的，存量 extractor 类有 `3000+`，还有 `1500+` 的 `item_filter` 和 `label_extractor`，数量非常大。
并且改写规则也非常多，涉及到很多 `c++` 的细节，算法同学需要首先花大量时间理解改写规则，然后才能进行改写。如果改写过程中
出现问题，还需要排查逻辑是否一致，非常耗费精力。

## 能否实现自动改写

是否有办法实现自动改写? 如果需要自动改写，我们首先需要能够用程序来处理 `adlog` 特征类，即需要能够解析 `c++` 代码，然后
分析代码中哪些部分是数据相关的逻辑，再根据规则进行重写。

从这个角度分析，理论上只要能够解析 `c++` 代码，就可以进行自动改写。

是否有办法解析 `c++` 代码 ？答案是肯定的。因为我们的特征类需要编译成功，就需要编译器首先解析 `c++` 代码，然后再转换成机器码。

因此，理论上这个程序是可以实现的。

## 如何解析 `c++` 代码

关键问题是如何解析 `c++` 代码。手动实现不太现实，需要借助一些工具。调研了下 `c++ static analysis`, `c++ 代码解析` 等相关工作，
发现几乎所有的结果都指向了同一个项目，那就是 [llvm](https://llvm.org/)。

`llvm` 是一个编译器基础项目，`c++` 编译器 `clang` 就是基于 `llvm` 开发的。`llvm` 提供了 `c++` 代码的词法分析、语法分析等
一系列编译器方面的功能。我们可以利用 `llvm` 提供的 `clang` 库来解析 `c++` 代码。

## 如何根据解析的结果区分数据相关逻辑以及计算相关逻辑

有了 `llvm` 解析的结果后，我们还需要区分哪些代码是数据相关逻辑，哪些代码是计算相关逻辑。如前文所提，这些逻辑经常是耦合在一起的。
如何进行区分也是一个重要的问题。

## 如何将 `c++` `proto` 字段映射到 `kv` 格式

`adlog` 数据的字段按路径展开可以映射到 `kv` 格式的 `key`，这些映射规则如何在代码解析中进行使用？因为特征类所使用的字段非常多，
我们不可能提前枚举所有字段，因此必须在运行时根据字段路径的字符串来找到对应的 `kv` 格式对应的字段和类型，如 `adlog.user_info.id`
对应到 `uint64_t` 类型的 `kv` 数据。

## 模板参数

特征类中还有很多模板类，参数通常是 `CommonInfoAttr` 枚举或者行为类型对应的 `int` 等。这些模板参数在改写时如何处理？

## 如何根据规则进行自动改写

如何根据规则进行改写也有很多挑战:
- 前文总结了主要的几十种改写规则，如何知道每行代码应该根据哪种规则进行改写？
- 确定了改写规则后，如何应用这些改写规则？
- 同一行代码是否会被应用到多种规则？
- 这些规则是否会冲突 ?
- 如何保证改写后的代码和 `adlog` 特征类逻辑对齐？
- 改写后的代码还需要能够编译通过，如何保证改写后的代码是合法的 `c++` 代码？