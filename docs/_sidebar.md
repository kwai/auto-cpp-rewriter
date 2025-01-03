- [简介](README.md)
- [问题描述](problem/README.md)
  - [背景](problem/background.md)
  - [原始数据格式与特征提取](problem/origin_format.md)
  - [迁移 kv 特征](problem/kv_feature/README.md)
    - [数据转换](problem/kv_feature/format_conversion.md)
    - [特征改写](problem/kv_feature/feature_rewrite.md)
    - [改写规则](problem/kv_feature/rewrite_rule/README.md)
- [挑战](challenge/README.md)
- [解决方案](solution/README.md)
  - [思路](solution/README.md)
    - [示例](solution/README.md?id=示例)
  - [整体架构](solution/overall_architecture.md)
  - [子模块](solution/sub_modules/README.md)
    - [proto_parser](solution/sub_modules/proto_parser.md)
    - [matcher](solution/sub_modules/matcher.md)
    - [ast](solution/sub_modules/ast.md)
    - [visitor](solution/sub_modules/visitor.md)
    - [env](solution/sub_modules/env.md)
    - [info](solution/sub_modules/info.md)
    - [expr_parser](solution/sub_modules/expr_parser.md)
    - [rewrite](solution/sub_modules/rewrite.md)
- [编译](compile/README.md)
- [测试](test/README.md)
