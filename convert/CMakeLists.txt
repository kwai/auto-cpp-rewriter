set(LLVM_LINK_COMPONENTS support)
add_compile_options(-Werror=return-type)

set(CMAKE_CXX_STANDARD 17)

find_package(gflags)
find_package (glog 0.6.0 REQUIRED)
find_package (absl REQUIRED)

include(FetchContent)

FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent
  GIT_TAG v3.10.2)

FetchContent_GetProperties(json)
if(NOT json_POPULATED)
  FetchContent_Populate(json)
  add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

add_clang_executable(convert
  Env.cpp
  Tool.cpp
  Config.cpp
  Convert.cpp
  Deleter.cpp
  ExprInfo.cpp
  ExprParser.cpp
  ExprParserDetail.cpp
  ExprParserBSField.cpp
  ConvertAction.cpp
  LogicParser.cpp
  info/Info.cpp
  info/IfInfo.cpp
  info/LoopInfo.cpp
  info/InfoBase.cpp
  info/NewVarDef.cpp
  info/MethodInfo.cpp
  info/ActionMethodInfo.cpp
  info/NewActionParam.cpp
  info/PrefixPair.cpp
  info/CommonInfo.cpp
  info/CommonInfoCore.cpp
  info/CommonInfoLeaf.cpp
  info/CommonInfoFixed.cpp
  info/CommonInfoFixedList.cpp
  info/CommonInfoBodyText.cpp
  info/CommonInfoDetail.cpp
  info/CommonInfoPrepare.cpp
  info/CommonInfoNormal.cpp
  info/CommonInfoMultiIntList.cpp
  info/CommonInfoMiddleNode.cpp
  info/VarDeclInfo.cpp
  info/FeatureInfo.cpp
  info/BinaryOpInfo.cpp
  info/ConstructorInfo.cpp
  info/MiddleNodeInfo.cpp
  info/SeqListInfo.cpp
  info/ActionDetailInfo.cpp
  info/ActionDetailFixedInfo.cpp
  info/BSFieldInfo.cpp
  rule/RuleBase.cpp
  rule/PreRule.cpp
  rule/GeneralRule.cpp
  rule/MiddleNodeRule.cpp
  rule/CommonInfoRule.cpp
  rule/ActionDetailRule.cpp
  rule/DoubleListRule.cpp
  rule/ProtoListRule.cpp
  rule/SeqListRule.cpp
  rule/AddFeatureMethodRule.cpp
  rule/HashFnRule.cpp
  rule/QueryTokenRule.cpp
  rule/StrRule.cpp
  rule/BSFieldOrderRule.cpp
  rule/proto_list/ProtoListExprInfo.cpp
  handler/StrictRewriter.cpp
  handler/OverviewHandler.cpp
  handler/AdlogFieldHandler.cpp
  handler/FieldDeclHandler.cpp
  handler/LogicHandler.cpp
  handler/BSFieldHandler.cpp
  expr_parser/ExprParserQueryToken.cpp
  visitor/CtorVisitor.cpp
  visitor/BSCtorVisitor.cpp
  visitor/FieldDeclVisitor.cpp
  visitor/ExtractMethodVisitor.cpp
  visitor/BSExtractMethodVisitor.cpp
  visitor/MiddleNodeJson.cpp
  matcher_callback/FeatureDeclCallback.cpp
  matcher_callback/InferFilterCallback.cpp
  matcher_callback/TypeAliasCallback.cpp
  matcher_callback/BSFeatureDeclCallback.cpp
  matcher_callback/BSTypeAliasCallback.cpp
  )

target_link_libraries(convert
  PUBLIC
  clangTooling
  clangBasic
  clangASTMatchers
  clangFrontend
  clangSerialization
  clangTooling
  glog
  gflags
  nlohmann_json::nlohmann_json
  absl::strings
  absl::optional
)
