# auto-cpp-rewriter

## 背景

本项目是基于 `llvm` 的 `c++` 代码自动转换工具, 用于解决广告模型特征框架迁移中 `c++` 抽取类代码的改写问题。

以下简单介绍一下背景, 详细背景介绍可参考 [问题描述](problem/README.md)。

商业化模型链路虽然新的特征使用的是 `c++` 特征类的方式，即用 `c++` 实现读取 `protobuf` 数据并进行特征抽取，
但是这种方式存在序列化反序列化开销大、链路难以治理、数据逻辑与计算逻辑耦合严重等问题, 严重影响特征迭代效率。

为了解决以上这些问题，我们需要对整个特征链路进行彻底的改造。从业界的经验来看，迁移 `kv` 特征是一个比较好的思路，
即将数据迁移成 `kv` 格式，使用 `flatbuffers` 等读取性能更好的存储格式，并进行特征抽取。由于中间涉及环节非常多，
为了保证线上效果，我们需要实现特征处理逻辑的平迁, 即两种数据格式对应的数据与特征处理逻辑要完全保持一致, 这是整
个特征迁移项目的基础。但由于历史上已经积累了大量的 `c++` 特征抽取类，如果手动改写，工作量非常大，且排查逻辑一致
也会耗费大量精力。

因此，我基于 `llvm` 实现了自动改写 `c++` 特征抽取类的工具，自动将抽取类转换为 `kv` 特征抽取类，且保证
逻辑完全一致。

## 目录

本文档将按如下部分介绍此项目(待完善)：

- [问题描述](problem/README.md)
  - [背景](problem/README.md)
  - [原始数据格式与特征提取](problem/original_format.md)
  - [迁移 kv 特征](problem/kv_feature/README.md)
    - [数据转换](problem/kv_feature/format_conversion.md)
    - [特征改写](problem/kv_feature/feature_rewrite.md)
    - [改写规则](problem/kv_feature/rewrite_rule/README.md)
- [挑战](challenge/README.md)
- [解决方案](solution/README.md)
  - [思路](solution/README.md)
  - [整体架构](solution/overall_architecture.md)
  - [子模块](solution/sub_modules.md)
    - [proto_parser](solution/sub_modules/proto_parser.md)
    - [matcher](solution/sub_modules/matcher.md)
    - [visitor](solution/sub_modules/visitor.md)
    - [env](solution/sub_modules/env.md)
    - [info](solution/sub_modules/info.md)
    - [expr_parser](solution/sub_modules/expr_parser.md)
    - [rewrite](solution/sub_modules/rewrite.md)
- [编译](compile/README.md)
- [测试](test/README.md)
