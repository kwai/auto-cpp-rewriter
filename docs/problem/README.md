# 问题描述

本项目解决的问题可以总结为一句话: 如何将读取 `protobuf` 数据的 `c++` 特征抽取类自动改写为读取 `kv` 数据的 `c++` 特征抽取类 ?

实际问题中涉及很多的细节，如数据格式的定义与映射、读取数据的逻辑、计算逻辑等。以下将分两部分详细介绍下。

- [原始数据格式与特征提取](problem/origin_format.md)
- [迁移 kv 特征](problem/kv_feature/README.md)