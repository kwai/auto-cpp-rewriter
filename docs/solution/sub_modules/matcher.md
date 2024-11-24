# matcher

`ASTMatcher` 相关文档可参考: [ASTMatcher](https://clang.llvm.org/docs/LibASTMatchersReference.html)

`matcher` 模块使用 `llvm` 提供的 `ASTMatcher` 功能，通过宏定义需要匹配的代码。

如下所示:

```cpp
  // 目前只能匹配到 typeAliasDecl(), 可能会有更好的匹配。
  auto TypeAliasMatcher = decl(typeAliasDecl(),
                                   namedDecl(matchesName("Extract.*"))).bind("TypeAlias");
  type_alias_finder_.addMatcher(TypeAliasMatcher, &type_alias_callback_);

  // TK_IgnoreUnlessSpelledInSource 用来忽略模板
  auto FeatureDeclMatcher = traverse(TK_IgnoreUnlessSpelledInSource,
                                     cxxRecordDecl(isDerivedFrom(hasName("FastFeature")),
                                                   unless(isExpandedFromMacro("DISALLOW_COPY_AND_ASSIGN")),
                                                   unless(isExpandedFromMacro("REGISTER_EXTRACTOR")))).bind("FeatureDecl");

  match_finder_.addMatcher(FeatureDeclMatcher, &feature_decl_callback_);
```

`TypeAliasMatcher` 表示匹配模板类 `alias`, 且名字以 `Extract` 开头。

`FeatureDeclMatcher` 表示匹配父类是 `FastFeature` 的类定义，并且忽略来自 `DISALLOW_COPY_AND_ASSIGN` 和 `REGISTER_EXTRACTOR` 宏展开的结果。

详细实现可参考: `convert/ConvertAction.cpp`。