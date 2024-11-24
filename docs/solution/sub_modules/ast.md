# ast

我们再仔细看一下思路中的解析示例: teams/ad/ad_algorithm/feature/fast/impl/extract_ad_delay_label.h

`adlog` 特征类代码如下:

```cpp
class ExtractAdDelayLabel : public FastFeature {
 public:
    ExtractAdDelayLabel() : FastFeature(FeatureType::DENSE_ITEM) {}

  virtual void Extract(const AdLog& adlog, size_t pos,
                       std::vector<ExtractResult>* result) {
    if (adlog.item_size() <= pos) {
      return;
    }
    auto& item = adlog.item(pos);
    if ((item.type() == AD_DSP || item.type() == NATIVE_AD) && item.has_label_info()) {
      for (auto & attr : item.label_info().label_info_attr()) {
        if (attr.name_value() ==
            ::auto_cpp_rewriter::LabelInfoCommonAttr_Name_BACKEND_LABEL_MATCH_CALIBRATION_TAG) {
          AddFeature(0, attr.bool_value(), result);
        }
      }
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtractAdDelayLabel);
};
```

`llvm` 解析后的 `ast` 完整结构如下:

```cpp
CXXRecordDecl 0x7f909280fcc8 </home/liuzhishan/ast/teams/ad/ad_algorithm/feature/fast/impl/extract_ad_delay_label.h:9:1, line:31:1> line:9:7 referenced class ExtractAdDelayLabel definition
|-DefinitionData polymorphic has_user_declared_ctor can_const_default_init
| |-DefaultConstructor exists non_trivial user_provided
| |-CopyConstructor non_trivial user_declared has_const_param needs_overload_resolution implicit_has_const_param
| |-MoveConstructor needs_overload_resolution
| |-CopyAssignment non_trivial has_const_param user_declared needs_overload_resolution implicit_has_const_param
| |-MoveAssignment needs_overload_resolution
| `-Destructor simple non_trivial needs_overload_resolution
|-public 'FastFeature':'ks::ad_algorithm::FastFeature'
|-CXXRecordDecl 0x7f909280fe28 <col:1, col:7> col:7 implicit referenced class ExtractAdDelayLabel
|-AccessSpecDecl 0x7f909280feb8 <line:10:2, col:8> col:2 public
|-CXXConstructorDecl 0x7f909280ff48 <line:11:5, col:67> col:5 used ExtractAdDelayLabel 'void ()' implicit-inline
| |-CXXCtorInitializer 'FastFeature':'ks::ad_algorithm::FastFeature'
| | `-CXXConstructExpr 0x7f9092810b60 <col:29, col:64> 'FastFeature':'ks::ad_algorithm::FastFeature' 'void (FeatureType, size_t)'
| |   |-DeclRefExpr 0x7f9092810ae0 <col:41, col:54> 'ks::ad_algorithm::FeatureType' EnumConstant 0x1f693b00 'DENSE_ITEM' 'ks::ad_algorithm::FeatureType'
| |   | `-NestedNameSpecifier TypeSpec 'ks::ad_algorithm::FeatureType'
| |   `-CXXDefaultArgExpr 0x7f9092810b40 <<invalid sloc>> 'size_t':'unsigned long'
| `-CompoundStmt 0x7f9092810bc0 <col:66, col:67>
|-CXXMethodDecl 0x7f90928103c0 <line:13:3, line:27:3> line:13:16 used Extract 'void (const AdLog &, size_t, std::vector<ExtractResult> *)' virtual implicit-inline
| |-Overrides: [ 0x7f909534e860 FastFeature::Extract 'void (const AdLog &, size_t, std::vector<ExtractResult> *)' ]
| |-ParmVarDecl 0x7f9092810010 <col:24, col:37> col:37 used adlog 'const AdLog &'
| |-ParmVarDecl 0x7f9092810088 <col:44, col:51> col:51 used pos 'size_t':'unsigned long'
| |-ParmVarDecl 0x7f90928102d8 <line:14:24, col:52> col:52 used result 'std::vector<ExtractResult> *'
| `-CompoundStmt 0x7f9092815d20 <col:60, line:27:3>
|   |-IfStmt 0x7f9092810cf8 <line:15:5, line:17:5>
|   | |-BinaryOperator 0x7f9092810cb0 <line:15:9, col:30> 'bool' '<='
|   | | |-ImplicitCastExpr 0x7f9092810c98 <col:9, col:25> 'size_t':'unsigned long' <IntegralCast>
|   | | | `-CXXMemberCallExpr 0x7f9092810c40 <col:9, col:25> 'int'
|   | | |   `-MemberExpr 0x7f9092810bf0 <col:9, col:15> '<bound member function type>' .item_size 0x6095b90
|   | | |     `-ImplicitCastExpr 0x7f9092810c20 <col:9> 'const ks::ad_algorithm::AdLogInterface' lvalue <UncheckedDerivedToBase (AdLogInterface)>
|   | | |       `-DeclRefExpr 0x7f9092810bd0 <col:9> 'const AdLog':'const ks::ad_algorithm::AdLog' lvalue ParmVar 0x7f9092810010 'adlog' 'const AdLog &'
|   | | `-ImplicitCastExpr 0x7f9092810c80 <col:30> 'size_t':'unsigned long' <LValueToRValue>
|   | |   `-DeclRefExpr 0x7f9092810c60 <col:30> 'size_t':'unsigned long' lvalue ParmVar 0x7f9092810088 'pos' 'size_t':'unsigned long'
|   | `-CompoundStmt 0x7f9092810ce0 <col:35, line:17:5>
|   |   `-ReturnStmt 0x7f9092810cd0 <line:16:7>
|   |-DeclStmt 0x7f9092811008 <line:18:5, col:33>
|   | `-VarDecl 0x7f9092810d38 <col:5, col:32> col:11 used item 'const ItemAdaptorBase &' cinit
|   |   `-CXXMemberCallExpr 0x7f9092810e10 <col:18, col:32> 'const ItemAdaptorBase':'const ks::ad_algorithm::ItemAdaptorBase' lvalue
|   |     |-MemberExpr 0x7f9092810dc0 <col:18, col:24> '<bound member function type>' .item 0xafc3968
|   |     | `-DeclRefExpr 0x7f9092810da0 <col:18> 'const AdLog':'const ks::ad_algorithm::AdLog' lvalue ParmVar 0x7f9092810010 'adlog' 'const AdLog &'
|   |     `-ImplicitCastExpr 0x7f9092810e50 <col:29> 'int' <IntegralCast>
|   |       `-ImplicitCastExpr 0x7f9092810e38 <col:29> 'size_t':'unsigned long' <LValueToRValue>
|   |         `-DeclRefExpr 0x7f9092810df0 <col:29> 'size_t':'unsigned long' lvalue ParmVar 0x7f9092810088 'pos' 'size_t':'unsigned long'
|   `-IfStmt 0x7f9092815d00 <line:19:5, line:26:5>
|     |-BinaryOperator 0x7f90928113f0 <line:19:9, col:84> 'bool' '&&'
|     | |-ParenExpr 0x7f9092811360 <col:9, col:59> 'bool'
|     | | `-BinaryOperator 0x7f9092811340 <col:10, col:50> 'bool' '||'
|     | |   |-BinaryOperator 0x7f90928111a8 <col:10, col:25> 'bool' '=='
|     | |   | |-ImplicitCastExpr 0x7f9092811178 <col:10, col:20> 'int' <IntegralCast>
|     | |   | | `-CXXMemberCallExpr 0x7f9092811070 <col:10, col:20> 'auto_cpp_rewriter::ItemType'
|     | |   | |   `-MemberExpr 0x7f9092811040 <col:10, col:15> '<bound member function type>' .type 0x6911c90
|     | |   | |     `-DeclRefExpr 0x7f9092811020 <col:10> 'const ItemAdaptorBase':'const ks::ad_algorithm::ItemAdaptorBase' lvalue Var 0x7f9092810d38 'item' 'const ItemAdaptorBase &'
|     | |   | `-ImplicitCastExpr 0x7f9092811190 <col:25> 'int' <IntegralCast>
|     | |   |   `-DeclRefExpr 0x7f90928110c0 <col:25> 'auto_cpp_rewriter::ItemType' EnumConstant 0x7f90930c7ad0 'AD_DSP' 'auto_cpp_rewriter::ItemType' (UsingShadow 0x7f909280f530 'AD_DSP')
|     | |   `-BinaryOperator 0x7f9092811320 <col:35, col:50> 'bool' '=='
|     | |     |-ImplicitCastExpr 0x7f90928112f0 <col:35, col:45> 'int' <IntegralCast>
|     | |     | `-CXXMemberCallExpr 0x7f9092811218 <col:35, col:45> 'auto_cpp_rewriter::ItemType'
|     | |     |   `-MemberExpr 0x7f90928111e8 <col:35, col:40> '<bound member function type>' .type 0x6911c90
|     | |     |     `-DeclRefExpr 0x7f90928111c8 <col:35> 'const ItemAdaptorBase':'const ks::ad_algorithm::ItemAdaptorBase' lvalue Var 0x7f9092810d38 'item' 'const ItemAdaptorBase &'
|     | |     `-ImplicitCastExpr 0x7f9092811308 <col:50> 'int' <IntegralCast>
|     | |       `-DeclRefExpr 0x7f9092811238 <col:50> 'auto_cpp_rewriter::ItemType' EnumConstant 0x7f90930c8450 'NATIVE_AD' 'auto_cpp_rewriter::ItemType' (UsingShadow 0x7f909280f700 'NATIVE_AD')
|     | `-CXXMemberCallExpr 0x7f90928113d0 <col:64, col:84> 'bool'
|     |   `-MemberExpr 0x7f90928113a0 <col:64, col:69> '<bound member function type>' .has_label_info 0x6912388
|     |     `-DeclRefExpr 0x7f9092811380 <col:64> 'const ItemAdaptorBase':'const ks::ad_algorithm::ItemAdaptorBase' lvalue Var 0x7f9092810d38 'item' 'const ItemAdaptorBase &'
|     `-CompoundStmt 0x7f9092815ce8 <col:87, line:26:5>
|       `-CXXForRangeStmt 0x7f9092815878 <line:20:7, line:25:7>
|         |-<<<NULL>>>
|         |-DeclStmt 0x7f9092811828 <line:20:26>
|         | `-VarDecl 0x7f9092811620 <col:26, col:60> col:26 implicit used __range3 'const ::google::protobuf::RepeatedPtrField< ::auto_cpp_rewriter::LabelInfoCommonAttr> &' cinit
|         |   `-CXXMemberCallExpr 0x7f9092811510 <col:26, col:60> 'const ::google::protobuf::RepeatedPtrField< ::auto_cpp_rewriter::LabelInfoCommonAttr>':'const google::protobuf::RepeatedPtrField<auto_cpp_rewriter::LabelInfoCommonAttr>' lvalue
|         |     `-MemberExpr 0x7f90928114e0 <col:26, col:44> '<bound member function type>' .label_info_attr 0x7f9092d36bf8
|         |       `-CXXMemberCallExpr 0x7f9092811460 <col:26, col:42> 'const auto_cpp_rewriter::LabelInfo' lvalue
|         |         `-MemberExpr 0x7f9092811430 <col:26, col:31> '<bound member function type>' .label_info 0x6912598
|         |           `-DeclRefExpr 0x7f9092811410 <col:26> 'const ItemAdaptorBase':'const ks::ad_algorithm::ItemAdaptorBase' lvalue Var 0x7f9092810d38 'item' 'const ItemAdaptorBase &'
|         |-DeclStmt 0x7f9092815170 <col:24>
|         | `-VarDecl 0x7f9092811898 <col:24> col:24 implicit used __begin3 'const_iterator':'google::protobuf::internal::RepeatedPtrIterator<const auto_cpp_rewriter::LabelInfoCommonAttr>' cinit
|         |   `-CXXMemberCallExpr 0x7f9092811a10 <col:24> 'const_iterator':'google::protobuf::internal::RepeatedPtrIterator<const auto_cpp_rewriter::LabelInfoCommonAttr>'
|         |     `-MemberExpr 0x7f90928119e0 <col:24> '<bound member function type>' .begin 0x7f909316b670
|         |       `-DeclRefExpr 0x7f9092811840 <col:24> 'const ::google::protobuf::RepeatedPtrField< ::auto_cpp_rewriter::LabelInfoCommonAttr>':'const google::protobuf::RepeatedPtrField<auto_cpp_rewriter::LabelInfoCommonAttr>' lvalue Var 0x7f9092811620 '__range3' 'const ::google::protobuf::RepeatedPtrField< ::auto_cpp_rewriter::LabelInfoCommonAttr> &'
|         |-DeclStmt 0x7f9092815188 <col:24>
|         | `-VarDecl 0x7f9092811918 <col:24> col:24 implicit used __end3 'const_iterator':'google::protobuf::internal::RepeatedPtrIterator<const auto_cpp_rewriter::LabelInfoCommonAttr>' cinit
|         |   `-CXXMemberCallExpr 0x7f9092815090 <col:24> 'const_iterator':'google::protobuf::internal::RepeatedPtrIterator<const auto_cpp_rewriter::LabelInfoCommonAttr>'
|         |     `-MemberExpr 0x7f9092815060 <col:24> '<bound member function type>' .end 0x7f909316b908
|         |       `-DeclRefExpr 0x7f9092811860 <col:24> 'const ::google::protobuf::RepeatedPtrField< ::auto_cpp_rewriter::LabelInfoCommonAttr>':'const google::protobuf::RepeatedPtrField<auto_cpp_rewriter::LabelInfoCommonAttr>' lvalue Var 0x7f9092811620 '__range3' 'const ::google::protobuf::RepeatedPtrField< ::auto_cpp_rewriter::LabelInfoCommonAttr> &'
|         |-CXXOperatorCallExpr 0x7f9092815338 <col:24> 'bool' '!='
|         | |-ImplicitCastExpr 0x7f9092815320 <col:24> 'bool (*)(const iterator &) const' <FunctionToPointerDecay>
|         | | `-DeclRefExpr 0x7f90928152a0 <col:24> 'bool (const iterator &) const' lvalue CXXMethod 0x7f90928135e8 'operator!=' 'bool (const iterator &) const'
|         | |-ImplicitCastExpr 0x7f9092815270 <col:24> 'const google::protobuf::internal::RepeatedPtrIterator<const auto_cpp_rewriter::LabelInfoCommonAttr>' lvalue <NoOp>
|         | | `-DeclRefExpr 0x7f90928151a0 <col:24> 'const_iterator':'google::protobuf::internal::RepeatedPtrIterator<const auto_cpp_rewriter::LabelInfoCommonAttr>' lvalue Var 0x7f9092811898 '__begin3' 'const_iterator':'google::protobuf::internal::RepeatedPtrIterator<const auto_cpp_rewriter::LabelInfoCommonAttr>'
|         | `-ImplicitCastExpr 0x7f9092815288 <col:24> 'const iterator':'const google::protobuf::internal::RepeatedPtrIterator<const auto_cpp_rewriter::LabelInfoCommonAttr>' lvalue <NoOp>
|         |   `-DeclRefExpr 0x7f90928151c0 <col:24> 'const_iterator':'google::protobuf::internal::RepeatedPtrIterator<const auto_cpp_rewriter::LabelInfoCommonAttr>' lvalue Var 0x7f9092811918 '__end3' 'const_iterator':'google::protobuf::internal::RepeatedPtrIterator<const auto_cpp_rewriter::LabelInfoCommonAttr>'
|         |-CXXOperatorCallExpr 0x7f9092815538 <col:24> 'iterator':'google::protobuf::internal::RepeatedPtrIterator<const auto_cpp_rewriter::LabelInfoCommonAttr>' lvalue '++'
|         | |-ImplicitCastExpr 0x7f9092815520 <col:24> 'iterator &(*)()' <FunctionToPointerDecay>
|         | | `-DeclRefExpr 0x7f9092815498 <col:24> 'iterator &()' lvalue CXXMethod 0x7f9092812e28 'operator++' 'iterator &()'
|         | `-DeclRefExpr 0x7f9092815478 <col:24> 'const_iterator':'google::protobuf::internal::RepeatedPtrIterator<const auto_cpp_rewriter::LabelInfoCommonAttr>' lvalue Var 0x7f9092811898 '__begin3' 'const_iterator':'google::protobuf::internal::RepeatedPtrIterator<const auto_cpp_rewriter::LabelInfoCommonAttr>'
|         |-DeclStmt 0x7f90928115e8 <col:12, col:61>
|         | `-VarDecl 0x7f9092811580 <col:12, col:24> col:19 used attr 'const auto_cpp_rewriter::LabelInfoCommonAttr &' cinit
|         |   `-CXXOperatorCallExpr 0x7f90928156a8 <col:24> 'const auto_cpp_rewriter::LabelInfoCommonAttr' lvalue '*'
|         |     |-ImplicitCastExpr 0x7f9092815690 <col:24> 'reference (*)() const' <FunctionToPointerDecay>
|         |     | `-DeclRefExpr 0x7f9092815608 <col:24> 'reference () const' lvalue CXXMethod 0x7f9092812a80 'operator*' 'reference () const'
|         |     `-ImplicitCastExpr 0x7f90928155f0 <col:24> 'const google::protobuf::internal::RepeatedPtrIterator<const auto_cpp_rewriter::LabelInfoCommonAttr>' lvalue <NoOp>
|         |       `-DeclRefExpr 0x7f90928155d0 <col:24> 'const_iterator':'google::protobuf::internal::RepeatedPtrIterator<const auto_cpp_rewriter::LabelInfoCommonAttr>' lvalue Var 0x7f9092811898 '__begin3' 'const_iterator':'google::protobuf::internal::RepeatedPtrIterator<const auto_cpp_rewriter::LabelInfoCommonAttr>'
|         `-CompoundStmt 0x7f9092815cd0 <col:63, line:25:7>
|           `-IfStmt 0x7f9092815cb0 <line:21:9, line:24:9>
|             |-BinaryOperator 0x7f9092815a98 <line:21:13, line:22:29> 'bool' '=='
|             | |-CXXMemberCallExpr 0x7f9092815928 <line:21:13, col:29> 'int64_t':'long'
|             | | `-MemberExpr 0x7f90928158f8 <col:13, col:18> '<bound member function type>' .name_value 0x15afc2c0
|             | |   `-DeclRefExpr 0x7f90928158d8 <col:13> 'const auto_cpp_rewriter::LabelInfoCommonAttr' lvalue Var 0x7f9092811580 'attr' 'const auto_cpp_rewriter::LabelInfoCommonAttr &'
|             | `-ImplicitCastExpr 0x7f9092815a80 <line:22:13, col:29> 'int64_t':'long' <IntegralCast>
|             `-CompoundStmt 0x7f9092815c98 <col:91, line:24:9>
|               `-CXXMemberCallExpr 0x7f9092815c18 <line:23:11, col:50> 'void'
|                 |-MemberExpr 0x7f9092815b10 <col:11> '<bound member function type>' ->AddFeature 0x1f69a1e0
|                 | `-ImplicitCastExpr 0x7f9092815bf0 <col:11> 'ks::ad_algorithm::FastFeatureInterface *' <UncheckedDerivedToBase (FastFeature -> FastFeatureInterface)>
|                 |   `-CXXThisExpr 0x7f9092815b00 <col:11> 'ks::ad_algorithm::ExtractAdDelayLabel *' implicit this
|                 |-ImplicitCastExpr 0x7f9092815c50 <col:22> 'uint64_t':'unsigned long' <IntegralCast>
|                 | `-IntegerLiteral 0x7f9092815b40 <col:22> 'int' 0
|                 |-ImplicitCastExpr 0x7f9092815c68 <col:25, col:41> 'float' <IntegralToFloating>
|                 | `-CXXMemberCallExpr 0x7f9092815bb0 <col:25, col:41> 'bool'
|                 |   `-MemberExpr 0x7f9092815b80 <col:25, col:30> '<bound member function type>' .bool_value 0x15affd68
|                 |     `-DeclRefExpr 0x7f9092815b60 <col:25> 'const auto_cpp_rewriter::LabelInfoCommonAttr' lvalue Var 0x7f9092811580 'attr' 'const auto_cpp_rewriter::LabelInfoCommonAttr &'
|                 `-ImplicitCastExpr 0x7f9092815c80 <col:44> 'std::vector<ExtractResult> *' <LValueToRValue>
|                   `-DeclRefExpr 0x7f9092815bd0 <col:44> 'std::vector<ExtractResult> *' lvalue ParmVar 0x7f90928102d8 'result' 'std::vector<ExtractResult> *'
|-AccessSpecDecl 0x7f90928104b0 <line:29:2, col:9> col:2 private
|-AccessSpecDecl 0x7f90928104d8 <./base/common/basic_types.h:70:3, col:10> col:3 private
|-CXXConstructorDecl 0x7f90928106b0 </home/liuzhishan/ast/teams/ad/ad_algorithm/feature/fast/impl/extract_ad_delay_label.h:30:28, ./base/common/basic_types.h:71:27> /home/liuzhishan/ast/teams/ad/ad_algorithm/feature/fast/impl/extract_ad_delay_label.h:30:28 ExtractAdDelayLabel 'void (const ExtractAdDelayLabel &)'
| `-ParmVarDecl 0x7f90928105a8 <./base/common/basic_types.h:71:12, col:26> col:27 'const ExtractAdDelayLabel &'
|-CXXMethodDecl 0x7f9092810820 <line:72:3, col:33> col:8 operator= 'void (const ExtractAdDelayLabel &)'
| `-ParmVarDecl 0x7f9092810790 <col:18, col:32> col:33 'const ExtractAdDelayLabel &'
`-CXXDestructorDecl 0x7f90928108e8 </home/liuzhishan/ast/teams/ad/ad_algorithm/feature/fast/impl/extract_ad_delay_label.h:9:7> col:7 implicit used ~ExtractAdDelayLabel 'void () noexcept' inline default
  |-Overrides: [ 0x7f909534cb58 FastFeature::~FastFeature 'void () noexcept' ]
  `-CompoundStmt 0x7f909281a720 <col:7>
```

可以看出，`ast` 中包含了所有的信息，包含变量定义、函数定义、类定义、宏定义、模板定义等。

虽然 `ast` 结构很复杂，但也是按照代码本身递归的结构进行组织的, 因此，结合 `llvm` 提供的 `ast` 接口，以及节点
上下文的信息，我们可以很方便的遍历代码中的信息。进而我们可以知道每一行代码是否是取数据相关的逻辑，并且结合代码中的
信息，我们可以知道应该属于哪个改写规则。
