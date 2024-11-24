#pragma once

#include <absl/types/optional.h>

#include <map>

#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/AST/AST.h"
#include "clang/AST/StmtCXX.h"
#include "clang/AST/ASTConsumer.h"

#include "./IfInfo.h"
#include "./SwitchCaseInfo.h"
#include "./DeclInfo.h"
#include "./LoopInfo.h"
#include "./CommonInfo.h"
#include "./CommonInfoDetail.h"
#include "./CommonInfoNormal.h"
#include "./CommonInfoFixed.h"
#include "./CommonInfoFixedList.h"
#include "./CommonInfoMultiMap.h"
#include "./CommonInfoMultiIntList.h"
#include "./CommonInfoPrepare.h"
#include "./CommonInfoMiddleNode.h"
#include "./ConstructorInfo.h"
#include "./ActionDetailInfo.h"
#include "./ActionDetailFixedInfo.h"
#include "./MiddleNodeInfo.h"
#include "./FeatureInfo.h"
#include "./VarDeclInfo.h"
#include "./BinaryOpInfo.h"
#include "./SeqListInfo.h"
#include "./ProtoListInfo.h"
#include "./AssignInfo.h"
#include "./BSFieldInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

template<typename T> struct InfoTraits {};

template<> struct InfoTraits<IfInfo> { static const IfInfo v; };
template<> struct InfoTraits<DeclInfo> { static const DeclInfo v; };
template<> struct InfoTraits<VarDeclInfo> { static const VarDeclInfo v; };
template<> struct InfoTraits<BinaryOpInfo> { static const BinaryOpInfo v; };
template<> struct InfoTraits<LoopInfo> { static const LoopInfo v; };
template<> struct InfoTraits<SwitchCaseInfo> { static const SwitchCaseInfo v; };
template<> struct InfoTraits<AssignInfo> { static const AssignInfo v; };

template<> struct InfoTraits<ActionDetailInfo> { static const ActionDetailInfo v; };
template<> struct InfoTraits<ActionDetailFixedInfo> { static const ActionDetailFixedInfo v; };
template<> struct InfoTraits<CommonInfoPrepare> { static const CommonInfoPrepare v; };
template<> struct InfoTraits<CommonInfoNormal> { static const CommonInfoNormal v; };
template<> struct InfoTraits<CommonInfoMultiMap> { static const CommonInfoMultiMap v; };
template<> struct InfoTraits<CommonInfoMultiIntList> { static const CommonInfoMultiIntList v; };
template<> struct InfoTraits<CommonInfoFixed> { static const CommonInfoFixed v; };
template<> struct InfoTraits<CommonInfoFixedList> { static const CommonInfoFixedList v; };
template<> struct InfoTraits<MiddleNodeInfo> { static const MiddleNodeInfo v; };
template<> struct InfoTraits<SeqListInfo> { static const SeqListInfo v; };
template<> struct InfoTraits<ProtoListInfo> { static const ProtoListInfo v; };
template<> struct InfoTraits<BSFieldInfo> { static const BSFieldInfo v; };

template<> struct InfoTraits<ConstructorInfo> { static const ConstructorInfo v; };
template<> struct InfoTraits<FeatureInfo> { static const FeatureInfo v; };

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
