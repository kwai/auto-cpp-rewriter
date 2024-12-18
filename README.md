# auto-cpp-rewriter

本项目是基于 `llvm` 的 `c++` 代码自动转换工具, 用于解决广告模型特征框架迁移中 `c++` 抽取类代码的改写问题。

以下简单介绍一下背景, 详细背景介绍可参考 [问题描述](docs/problem/README.md)。

商业化模型链路虽然新的特征使用的是 `c++` 特征类的方式，即用 `c++` 实现读取 `protobuf` 数据并进行特征抽取，
但是这种方式存在序列化反序列化开销大、链路难以治理、数据逻辑与计算逻辑耦合严重等问题, 严重影响特征迭代效率。

为了解决以上这些问题，我们需要对整个特征链路进行彻底的改造。从业界的经验来看，迁移 `kv` 特征是一个比较好的思路，
即将数据迁移成 `kv` 格式，使用 `flatbuffers` 等读取性能更好的存储格式，并进行特征抽取。由于中间涉及环节非常多，
为了保证线上效果，我们需要实现特征处理逻辑的平迁, 即两种数据格式对应的数据与特征处理逻辑要完全保持一致, 这是整
个特征迁移项目的基础。但由于历史上已经积累了大量的 `c++` 特征抽取类，如果手动改写，工作量非常大，且排查逻辑一致
也会耗费大量精力。

因此，我基于 `llvm` 实现了自动改写 `c++` 特征抽取类的工具，自动将抽取类转换为 `kv` 特征抽取类，且保证
逻辑完全一致。

详细文档见: https://kwai.github.io/auto-cpp-rewriter