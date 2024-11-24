#include <absl/types/optional.h>
#include <sys/stat.h>
#include <regex>
#include <cctype>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>

#include "clang/AST/ExprCXX.h"

#include "Env.h"
#include "Tool.h"
#include "info/CommonInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

clang::SourceRange find_source_range(clang::Stmt* stmt) {
  if (stmt == nullptr) {
    LOG(INFO) << "stmt is nullptr";
    return clang::SourceRange();
  }

  if (clang::ReturnStmt* returnStmt = dyn_cast<clang::ReturnStmt>(stmt)) {
    clang::Expr* retValue = returnStmt->getRetValue();
    if (retValue != nullptr) {
      return clang::SourceRange(returnStmt->getBeginLoc(), retValue->getEndLoc());
    } else {
      return clang::SourceRange(returnStmt->getBeginLoc(), returnStmt->getBeginLoc().getLocWithOffset(6));
    }
  }

  return clang::SourceRange(stmt->getBeginLoc(), stmt->getEndLoc());
}

std::string stmt_to_string(clang::Stmt* stmt, unsigned suppressTagKeyword) {
  if (stmt == nullptr) {
    return "";
  }

  if (clang::CXXNullPtrLiteralExpr* cxx_null_ptr_literal_expr =
      dyn_cast<clang::CXXNullPtrLiteralExpr>(stmt)) {
    return "nullptr";
  }

  clang::LangOptions lo;
  std::string out_str;
  llvm::raw_string_ostream outstream(out_str);
  clang::PrintingPolicy printPolicy(lo);
  printPolicy.SuppressTagKeyword = suppressTagKeyword;
  stmt->printPretty(outstream, nullptr, printPolicy);
  return out_str;
}

clang::Expr* get_first_caller(clang::Expr* initExpr, const Env* env_ptr) {
  if (initExpr == nullptr) {
    LOG(INFO) << "initExpr is null";
    return nullptr;
  }

  if (clang::ImplicitCastExpr* castExpr = dyn_cast<clang::ImplicitCastExpr>(initExpr)) {
    return get_first_caller(castExpr->getSubExpr(), env_ptr);

  } else if (clang::MaterializeTemporaryExpr* materialExpr =
             dyn_cast<clang::MaterializeTemporaryExpr>(initExpr)) {
    return get_first_caller(materialExpr->getSubExpr(), env_ptr);

  } else if (clang::ParenExpr* parenExpr = dyn_cast<clang::ParenExpr>(initExpr)) {
    return get_first_caller(parenExpr->getSubExpr(), env_ptr);

  } else if (clang::UnaryOperator* unary_operator = dyn_cast<clang::UnaryOperator>(initExpr)) {
    return get_first_caller(unary_operator->getSubExpr(), env_ptr);

  } else if (clang::DeclRefExpr* declRef = dyn_cast<clang::DeclRefExpr>(initExpr)) {
    if (env_ptr == nullptr) {
      return declRef;
    } else {
      clang::Expr* env_value = env_ptr->find(stmt_to_string(declRef));
      if (env_value != nullptr) {
        return get_first_caller(env_value, env_ptr);
      }
      return declRef;
    }

  } else if (clang::CXXMemberCallExpr* memberCallExpr = dyn_cast<clang::CXXMemberCallExpr>(initExpr)) {
    clang::Expr* caller = memberCallExpr->getImplicitObjectArgument();
    return get_first_caller(caller, env_ptr);

  } else if (clang::MemberExpr* memberExpr = dyn_cast<clang::MemberExpr>(initExpr)) {
    clang::Expr* caller = memberExpr->getBase();
    return get_first_caller(caller, env_ptr);

  } else if (clang::CXXOperatorCallExpr* operatorCallExpr = dyn_cast<clang::CXXOperatorCallExpr>(initExpr)) {
    clang::Expr* last_expr = nullptr;
    for (auto it = operatorCallExpr->child_begin(); it != operatorCallExpr->child_end(); it++) {
      if (clang::ImplicitCastExpr* tmpExpr = dyn_cast<clang::ImplicitCastExpr>(*it)) {
        last_expr = tmpExpr;
      }
    }
    return get_first_caller(last_expr, env_ptr);

  } else {
    return initExpr;
  }
}

const clang::TemplateArgumentList* get_proto_map_args_list(clang::QualType qualType) {
  const clang::Type* type = qualType.getTypePtr();
  if (type == nullptr) {
    return nullptr;
  }

  const clang::CXXRecordDecl* typeDecl = nullptr;
  if (type->isRecordType()) {
    typeDecl = type->getAsCXXRecordDecl();
  } else if (type->isReferenceType()) {
    typeDecl =	type->getPointeeCXXRecordDecl();
  } else {
    return nullptr;
  }

  if (const clang::ClassTemplateSpecializationDecl* templateDecl =
      dyn_cast<clang::ClassTemplateSpecializationDecl>(typeDecl)) {
    const clang::TemplateArgumentList& paramList = templateDecl->getTemplateArgs();
    return &paramList;
  }

  return nullptr;
}

bool is_map_value_builtin(clang::QualType qualType) {
  const clang::Type* type = qualType.getTypePtr();
  if (type == nullptr) {
    return false;
  }

  const clang::TemplateArgumentList* paramList = get_proto_map_args_list(qualType);
  if (paramList != nullptr && paramList->size() > 0) {
    const clang::Type* valueType = paramList->get(paramList->size() - 1).getAsType().getTypePtr();
    if (valueType->isBuiltinType()) {
      return true;
    } else {
      const clang::CXXRecordDecl* typeDecl = nullptr;
      if (valueType->isRecordType()) {
        typeDecl = valueType->getAsCXXRecordDecl();
      } else if (valueType->isReferenceType()) {
        typeDecl =	valueType->getPointeeCXXRecordDecl();
      } else {
        return false;
      }

      if (typeDecl->getNameAsString() == "std::string") {
        return true;
      } else {
        return false;
      }
    }
  }
  return false;
}

bool is_check_item_pos(clang::IfStmt* ifStmt) {
  clang::Expr* condExpr = ifStmt->getCond();
  condExpr = condExpr->IgnoreImpCasts();
  if (clang::BinaryOperator* binaryOperator = dyn_cast<clang::BinaryOperator>(condExpr)) {
    std::string op = binaryOperator->getOpcodeStr().str();
    std::string lhs = stmt_to_string(binaryOperator->getLHS());
    std::string rhs = stmt_to_string(binaryOperator->getRHS());
    if (lhs == "adlog.item_size()" && rhs == "pos") {
      return true;
    }else if (lhs == "pos" && rhs == "adlog.item_size()") {
      return true;
    } else {
      return false;
    }
  } else {
    return false;
  }
}

bool is_prefix(const std::string& s) {
  if (s.substr(0, 1) == "$" && s.find("$FeaturePrefix::") != std::string::npos) {
    return true;
  }
  return false;
}

std::map<std::string, std::set<std::string>> collect_prefix(json d) {
  std::map<std::string, std::set<std::string>> res;
  for (auto it = d.begin(); it != d.end(); it++) {
    res[it.key()] = std::set<std::string>();
    collect_prefix_recursive(it.value(), &(res[it.key()]));
  }

  return res;
}

void collect_prefix_recursive(json d, std::set<std::string>* str_set) {
  if (d.is_string()) {
    std::string x = d.get<std::string>();
    if (x.substr(0, 1) == "$" && x.find("$FeaturePrefix::") != std::string::npos) {
      str_set->insert(x);
    }
  } else if (d.is_array() && d.size() > 0) {
    if (d[0].is_string()) {
      std::string name = d[0].get<std::string>();
      for (size_t i = 1; i < d.size(); i++) {
        collect_prefix_recursive(d[i], str_set);
      }
    } else {
      for (size_t i = 0; i < d.size(); i++) {
        collect_prefix_recursive(d[i], str_set);
      }
    }
  } else if (d.is_object()) {
    for (auto it = d.begin(); it != d.end(); it++) {
      collect_prefix_recursive(it.value(), str_set);
    }
  }
}

json split_feature_def(json d,
                       const std::map<std::string, std::set<std::string>>& prefixs,
                       const std::map<std::string, int>& enum_map) {
  json res = json::object();
  for (auto it = d.begin(); it != d.end(); it++) {
    std::string name = it.key();
    res[name] = json::object();
    res[name][name + ".same"] = replace_enum(it.value(), enum_map);

    const std::set<std::string>& prefix_set = prefixs.at(name);

    for (const auto& prefix: prefix_set) {
      std::string prefix_name = prefix.substr(prefix.rfind(":") + 1);
      if (prefix == "$FeaturePrefix::PHOTO_ACCOUNT_ID") {
        for (auto it_enum = enum_map.begin(); it_enum != enum_map.end(); it_enum++) {
          LOG(INFO) << "name: " << it_enum->first << ", v: " << it_enum->second;
        }
      }
      res[name][name + "." + prefix_name] =
          split_feature_def_recursive(it.value(), prefix, enum_map);
    }
  }

  return res;
}

json split_feature_def_recursive(const json& d,
                                  std::string prefix,
                                  const std::map<std::string, int>& enum_map) {
  json res;
  if (d.is_array() && d.size() > 0) {
    res = json::array();
    for (size_t i = 0; i < d.size(); i++) {
      bool is_rm = false;
      if (d[i].is_array()) {
        for (size_t j = 0; j < d[i].size(); j++) {
          if (d[i][j].is_string() && is_prefix(d[i][j].get<std::string>())) {
            if (d[i][j].get<std::string>() != prefix) {
              is_rm = true;
              break;
            }
          }
        }
      }

      if (!is_rm) {
        json x = split_feature_def_recursive(d[i], prefix, enum_map);
        if (x.is_array()) {
          for (size_t j = 0; j < x.size(); j++) {
            if (x[j].is_string()) {
              std::string word = x[j].get<std::string>();
              if (is_prefix(word) && word == prefix) {
                if (enum_map.find(prefix) == enum_map.end()) {
                  LOG(INFO) << "cannot find enum for prefix: " << prefix;
                }
                x[j] = enum_map.at(prefix);
              } else if (enum_map.find(word) != enum_map.end()) {
                x[j] = enum_map.at(word);
              }
            }
          }
        }
        if (!x.is_array() || x.size() > 0) {
          res.push_back(x);
        }
      }
    }
  } else if (d.is_object()) {
    res = json::object();
    for (auto it = d.begin(); it != d.end(); it++) {
      json x = split_feature_def_recursive(it.value(), prefix, enum_map);
      if (!x.is_array() || x.size() > 0) {
        res[it.key()] = x;
      }
    }
  } else {
    res = json::parse(d.dump());
  }

  return res;
}

json replace_enum(const json& d, const std::map<std::string, int>& enum_map) {
  json res;
  if (d.is_array()) {
    res = json::array();
    for (size_t i = 0; i < d.size(); i++) {
      res.push_back(replace_enum(d[i], enum_map));
    }
  } else if (d.is_object()) {
    res = json::object();
    for (auto it = d.begin(); it != d.end(); it++) {
      res[it.key()] = replace_enum(it.value(), enum_map);
    }
  } else if (d.is_string()) {
    std::string word = d.get<std::string>();
    if (enum_map.find(word) != enum_map.end()) {
      res = enum_map.at(word);
    } else {
      res = word;
    }
  } else {
    res = json::parse(d.dump());
  }

  return res;
}

bool is_basic_type(clang::QualType qual_type) {
  if (tool::is_ad_enum(qual_type)) {
    return true;
  }

  std::string type_name = qual_type.getAsString();
  if (type_name.find("algorithm") != std::string::npos) {
    return false;
  }

  if (type_name.find("auto_cpp_rewriter") != std::string::npos) {
    return false;
  }

  if (type_name.find("const") != std::string::npos &&
      type_name.find("*") != std::string::npos) {
    return false;
  }

  return true;
}

bool is_integer(const std::string & s){
  static const std::regex p(" ?[\\-\\+]?[0-9]+ ?");
  return std::regex_match(s, p);
}

bool starts_with(const std::string& s, const std::string& x) {
  if (s.size() < x.size()) {
    return false;
  }

  return s.substr(0, x.size()) == x;
}

bool ends_with(const std::string& s, const std::string& x) {
  if (s.size() < x.size()) {
    return false;
  }

  return s.substr(s.size() - x.size()) == x;
}

std::string lower(const std::string& s) {
  std::string s1 = s;
  std::transform(s1.begin(), s1.end(), s1.begin(), tolower);
  return s1;
}

absl::optional<int> find_common_attr_int_value(clang::Expr* expr) {
  if (expr != nullptr && tool::is_pointer(expr->getType())) {
    return absl::nullopt;
  }

  if (clang::DeclRefExpr* decl_ref_expr = dyn_cast<clang::DeclRefExpr>(expr)) {
    return find_common_attr_int_value_from_expr(decl_ref_expr);
  }

  if (tool::is_enum_decl(expr)) {
    return find_common_attr_int_value_from_expr(expr);
  }

  return absl::nullopt;
}

absl::optional<int> find_common_attr_int_value_from_expr(clang::Expr* expr) {
  absl::optional<std::string> enum_name = find_common_attr_enum_name_from_expr(expr);
  if (!enum_name) {
    LOG(INFO) << "cannot find enum_name from expr: " << stmt_to_string(expr);
    return absl::nullopt;
  }

  const clang::Type* type = expr->getType().getTypePtr();
  const clang::EnumDecl* enumDecl = type->getAs<clang::EnumType>()->getDecl();
  if (enumDecl != nullptr) {
    for (auto it = enumDecl->enumerator_begin(); it != enumDecl->enumerator_end(); it++) {
      std::string itName = it->getNameAsString();
      auto pos = itName.find(*enum_name);
      if (pos != std::string::npos) {
        if (itName.substr(pos) == *enum_name) {
          int enum_value = std::stoi(stmt_to_string(it->getInitExpr()));
          LOG(INFO) << "find enum, name: " << it->getNameAsString()
                    << ", value: " << stmt_to_string(it->getInitExpr());
          return absl::optional<int>(enum_value);
        }
      }
    }
  }

  return absl::nullopt;
}

absl::optional<std::string> find_common_attr_enum_name_from_expr(clang::Expr* expr) {
  std::vector<std::string> arr = absl::StrSplit(stmt_to_string(expr), "::");
  if (arr.size() == 0) {
    return absl::nullopt;
  }

  static std::regex p("([A-Z0-9][A-Z0-9_]+)", std::regex::extended);
  std::smatch match_res;
  std::string enum_name;
  if (std::regex_search(arr.back(), match_res, p)) {
    if (match_res.size() > 0) {
      enum_name = match_res[match_res.size() - 1];
    }
  }

  if (enum_name.size() == 0) {
    LOG(INFO) << "cannot find enum_name from expr: " << stmt_to_string(expr);
    return absl::nullopt;
  }

  return absl::make_optional(enum_name);
}

bool ends_with_ignore_space(const std::string& s, char c) {
  int n = s.size();
  for (int i = n - 1; i >= 0; i--) {
    if (std::isspace(s[i])) {
      continue;
    }
    if (s[i] == c) {
      return true;
    } else {
      return false;
    }
  }

  return false;
}

bool is_only_space(const std::string& s) {
  for (size_t i = 0; i < s.size(); i++) {
    if (!std::isspace(s[i])) {
      return false;
    }
  }

  return true;
}

bool is_only_semicolon(const std::string& s) {
  for (size_t i = 0; i < s.size(); i++) {
    if (std::isspace(s[i]) || s[i] == ';') {
      continue;
    }
    return false;
  }

  return true;
}

std::string fix_semicolon(const std::string& s) {
  if (is_only_space(s)) {
    return "";
  }

  if (ends_with_ignore_space(s, ';')) {
    if (is_only_semicolon(s)) {
      return "";
    } else {
      return s;
    }
  } else {
    if (ends_with_ignore_space(s, '}')) {
      return s;
    } else {
      return s + ";";
    }
  }
}

namespace tool {

bool is_middle_node_expr(clang::Expr* expr, const Env& env) {
  clang::Expr* first_caller = get_first_caller(expr, &env);
  std::string expr_str = stmt_to_string(first_caller);
  return starts_with(expr_str, "GetPhotoInfo") || starts_with(expr_str, "GetAuthorInfo");
}

bool is_middle_node_root(const std::string& s) {
  static std::unordered_set<std::string> middle_names = {
    "GetPhotoInfo",
    "GetAuthorInfo",
    "GetLiveInfo",
    "GetCommonInfoAttrNew"
  };

  std::string s1 = trim_this(s);
  for (auto it = middle_names.begin(); it != middle_names.end(); it++) {
    if (starts_with(s1, *it)) {
      return true;
    }
  }

  return false;
}

// live_info, 或者 GetLiveInfo(adlog.item(pos))
bool is_middle_node_expr_root(clang::Expr* expr, const Env& env) {
  if (clang::DeclRefExpr* decl_ref_expr = dyn_cast<clang::DeclRefExpr>(expr)) {
    clang::Expr* value_expr = env.find(stmt_to_string(decl_ref_expr));
    if (value_expr != nullptr) {
      if (is_middle_node_root(stmt_to_string(value_expr))) {
        return true;
      }
    }
  }

  return is_middle_node_root(stmt_to_string(expr));
}

bool is_common_info_vector(clang::QualType qual_type) {
  bool is_vector = qual_type.getAsString().find("std::vector") != std::string::npos;
  return is_common_info_enum(qual_type) && is_vector;
}

bool is_common_info_enum(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();

  if (type_str.find("auto_cpp_rewriter::") != std::string::npos &&
      type_str.find("CommonInfo") != std::string::npos &&
      type_str.find("Attr") != std::string::npos) {
    return true;
  }

  if (type_str.find("auto_cpp_rewriter::") != std::string::npos &&
      type_str.find("InfoCommon") != std::string::npos &&
      type_str.find("Attr") != std::string::npos) {
    return true;
  }

  return false;
}

bool is_common_info_struct(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();

  if (type_str.find("auto_cpp_rewriter::") != std::string::npos &&
      type_str.find("CommonInfo") != std::string::npos &&
      type_str.find("Attr") != std::string::npos) {
    return true;
  }

  if (type_str.find("auto_cpp_rewriter::") != std::string::npos &&
      type_str.find("InfoCommon") != std::string::npos &&
      type_str.find("Attr") != std::string::npos) {
    return true;
  }

  return false;
}

bool is_repeated_common_info(clang::QualType qualType) {
  std::string type_str = qualType.getAsString();
  bool is_repeated = (type_str.find("::RepeatedPtr") != std::string::npos);
  is_repeated &= (type_str.find("google::protobuf") != std::string::npos);
  return is_repeated && is_common_info_enum(qualType);
}

bool is_repeated_common_info_size(const std::string& method_name) {
  return CommonAttrInfo::is_repeated_common_info_size(method_name);
}

bool is_combine_feature(const std::string& feature_type) {
  return lower(feature_type).find("combine") != std::string::npos;
}

bool is_user_feature(const std::string& feature_type) {
  return lower(feature_type).find("user") != std::string::npos;
}

bool is_item_feature(const std::string& feature_type) {
  return lower(feature_type).find("photo") != std::string::npos ||
    lower(feature_type).find("item") != std::string::npos;
}

bool is_sparse_feature(const std::string& feature_type) {
  return !is_dense_feature(feature_type);
}

bool is_dense_feature(const std::string& feature_type) {
  return lower(feature_type).find("dense") != std::string::npos;
}

bool is_item_field(const std::string& s) {
  return s.size() > 10 && s.substr(0, 10) == "adlog_item";
}

bool is_adlog_user_field(const std::string& s) {
  return s.size() > 10 && starts_with(s, "adlog_user");
}

bool is_adlog_field(const std::string& s) {
  if (s.size() > 5 && starts_with(s, "adlog")) {
    return true;
  }
  if (s.size() > 6 && starts_with(s, "ad_log")) {
    return true;
  }

  return false;
}

bool is_reco_user_field(const std::string& s) {
  return s.size() >= 15 && starts_with(s, "adlog_reco_user");
}

bool is_item_type_enum(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();
  LOG(INFO) << "type_str: " << type_str;
  if (type_str.find("algorithm::ItemType") != std::string::npos) {
    return true;
  }

  return false;
}

bool is_ad_callback_log_enum(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();
  if (type_str.find("auto_cpp_rewriter::AdCallbackLog") != std::string::npos) {
    return true;
  }

  return false;
}

bool is_ad_action_type_enum(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();
  if (type_str.find("auto_cpp_rewriter::AdActionType") != std::string::npos) {
    return true;
  }

  return false;
}

bool is_ad_enum(clang::QualType qual_type) {
  const clang::Type* type_ptr = qual_type.getTypePtr();

  std::string type_str = qual_type.getAsString();
  bool is_from_ad_proto = type_str.find("auto_cpp_rewriter::") != std::string::npos;

  return is_from_ad_proto && type_ptr->isEnumeralType();
}

bool is_basic_array(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();

  static std::regex p("(int|int64_t|uint64_t|float|double) \\[\\d+\\]");
  std::smatch sm;
  return std::regex_match(type_str, sm, p);
}

bool is_builtin_simple_type(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();

  static std::regex p("(int|int64_t|uint32|uint64_t|float|double)");
  std::smatch sm;
  return std::regex_match(type_str, sm, p);
}

std::string get_bs_scalar_exists_expr(Env* env_ptr,
                                            const std::string& common_info_prefix,
                                            const std::string& value_type) {
  std::ostringstream oss;

  oss << "BSFieldHelper::HasSingular<" << value_type;
  if (env_ptr->is_combine_feature() && !is_item_field(common_info_prefix)) {
    oss << ", true";
  }
  oss << ">(*bs, BSFieldEnum::" << common_info_prefix << ", pos)";

  return oss.str();
}

bool is_int32_type(const std::string &type_str) {
  if (type_str == "int" ||
      type_str == "const int" ||
      type_str == "int32_t" ||
      type_str == "const int32_t") {
    return true;
  }

  return false;
}

std::string get_builtin_type_str(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();
  static std::regex p(" ?\\&$");
  type_str = std::regex_replace(type_str, p, "");

  if (is_int32_type(type_str)) {
    return "int32_t";
  }

  const clang::Type* type_ptr = qual_type.getTypePtr();
  if (type_ptr == nullptr) {
    return "";
  }

  if (type_ptr->isBooleanType()) {
    return "bool";
  } else if (type_ptr->isIntegerType()) {
    if (is_int32_type(type_str)) {
      return "int32_t";
    } else if (type_str.find("uint64") != std::string::npos) {
      return "uint64_t";
    } else {
      return "int64_t";
    }
  } else if (type_ptr->isFloatingType()) {
    if (type_str.find("double") != std::string::npos) {
      return "double";
    } else {
      return "float";
    }
  } else {
    return "absl::string_view";
  }
}

std::string get_exists_name(const std::string& name) {
  return name + "_exists";
}

std::string fix_std_string(const std::string& s) {
  if (s.find("include<") != std::string::npos ||
      s.find("include <") != std::string::npos ||
      s.find("std::string") != std::string::npos) {
    return s;
  }

  static std::regex p_string("([ <]?)string([ >,])");
  return std::regex_replace(s, p_string, "$1std::string$2");
}

std::string fix_string_view(const std::string& s) {
  static std::regex p_string("std::string");
  return std::regex_replace(s, p_string, "absl::string_view");
}

std::string get_bs_correspond_path(const std::string& filename) {
  static std::regex p(".*teams/ad/ad_algorithm/feature/fast/impl/");
  return std::regex_replace(filename, p, "teams/ad/ad_algorithm/bs_feature/fast/impl/bs_");
}

std::string read_file_to_string(const std::string& filename) {
  std::ifstream infile(filename);
  if (!infile.is_open()) {
    LOG(INFO) << "cannot open file, filename: " << filename;
    return "";
  }

  std::stringstream buffer;
  buffer << infile.rdbuf();
  std::string s(buffer.str());

  return s;
}

bool is_bs_already_rewritten(const std::string& filename) {
  std::string s = read_file_to_string(filename);
  if (s.size() == 0) {
    return false;
  }

  bool is_rewritten = (s.find("getBS()") != std::string::npos &&
                       s.find("BSFieldEnum::") != std::string::npos);
  if (is_rewritten) {
    return true;
  }

  if (ends_with(filename, ".h")) {
    std::string cc_filename = filename.substr(0, filename.size() - 1) + "cc";
    if (is_file_exists(cc_filename)) {
      return is_bs_already_rewritten(cc_filename);
    }
  }

  return false;
}

bool is_file_exists(const std::string& name) {
  struct stat buffer;
  return (stat(name.c_str(), &buffer) == 0);
}

std::string rm_continue_break(const std::string& s) {
  static std::regex p_continue("continue ?;");
  static std::regex p_break("break ?;");

  std::string res = std::regex_replace(s, p_continue, "");
  res = std::regex_replace(res, p_break, "");

  return res;
}

std::string find_last_include(const std::string& content) {
  static std::regex p_include("(#include ?[\"<].*[\">])");
  std::smatch m;

  std::string s = content;
  std::string res;

  while (std::regex_search (s, m, p_include)) {
    if (m.size() > 0) {
      res = m[0];
    }
    s = m.suffix().str();
  }

  return res;
}

bool is_string(const std::string& type_str) {
  if (type_str == "std::string" || type_str == "string" || type_str == "const std::string" ||
      type_str == "absl::string_view" || type_str == "char *" || type_str == "const basic_string<char>" ||
      type_str == "basic_string<char>" || type_str == "std::basic_string<char>" ||
      type_str == "google::protobuf::string") {
    return true;
  }

  return false;
}

// TODO(liuzhishan): fix
bool is_string(clang::QualType qual_type) {
  if (is_char_arr(qual_type)) {
    return true;
  }

  std::string type_str = lower(qual_type.getAsString());
  return is_string(type_str);
}

bool is_char_arr(clang::QualType qual_type) {
  std::string type_str = lower(qual_type.getAsString());

  std::regex p("(const )?char \\[.+?\\]");
  std::smatch m;
  if (std::regex_match(type_str, m, p)) {
    return true;
  }

  return false;
}

std::string add_quote(const std::string& s) {
  std::ostringstream oss;
  oss << "\"" << s << "\"";
  return oss.str();
}

bool is_from_info_util(const std::string& s) {
  return starts_with(s, "BSGet") || starts_with(s, "BSHas") || starts_with(s, "bs_util.BS");
}

std::string fix_ad_enum(const std::string& s) {
  static std::regex p("ad::class ");
  return std::regex_replace(s, p, "ad::");
}

std::string adlog_to_bs_enum_str(const std::string& s) {
  std::string s1 = std::regex_replace(s, std::regex("\\."), "_");
  return std::regex_replace(s1, std::regex(":"), "_");
}

std::string dot_underscore_to_camel(const std::string& s) {
  std::ostringstream oss;

  std::vector<std::string> dot_tokens = absl::StrSplit(s, ".");
  for (const auto& dot_token : dot_tokens) {
    std::vector<std::string> underscore_tokens = absl::StrSplit(dot_token, "_");
    for (const auto& underscore_token : underscore_tokens) {
      if (starts_with(underscore_token, "Get") ||
          underscore_token == "exists" ||
          underscore_token.size() == 0) {
        continue;
      }

      oss << char(toupper(underscore_token[0])) << underscore_token.substr(1);
    }
  }

  return oss.str();
}

bool is_var_proto_list(clang::QualType qualType) {
  const clang::Type* type = qualType.getTypePtr();
  if (type == nullptr) {
    return false;
  }
  if (type->isRecordType() || type->isReferenceType()) {
    return qualType.getAsString().find("google::protobuf::Repeated") != std::string::npos;
  }

  return false;
}

bool is_var_proto_map(clang::QualType qualType) {
  const clang::Type* type = qualType.getTypePtr();

  if (type == nullptr) {
    return false;
  }
  if (type->isRecordType() || type->isReferenceType()) {
    return qualType.getAsString().find("google::protobuf::Map") != std::string::npos;
  }

  return false;
}

bool is_var_proto_message(clang::QualType qual_type) {
  const clang::Type *type = qual_type.getTypePtr();
  std::string type_str = qual_type.getAsString();

  if (type == nullptr) {
    return false;
  }

  if (type->isRecordType() || type->isReferenceType()) {
    if (type_str.find("auto_cpp_rewriter::") != std::string::npos) {
      if (type_str.find("Map") == std::string::npos &&
          type_str.find("Repeated") == std::string::npos) {
        return true;
      }
    }
  }

  return false;
}

bool is_repeated_proto_message(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();
  if (is_repeated_proto(qual_type)) {
    if (type_str.find("auto_cpp_rewriter::") != std::string::npos) {
      if (type_str.find("Map") == std::string::npos) {
        return true;
      }
    }
  }

  return false;
}

std::vector<int> find_common_info_values_in_file(const std::string& filename) {
  std::vector<int> res;

  std::string s = read_file_to_string(filename);
  if (s.size() == 0) {
    return res;
  }

  static std::regex p("CommonInfoAttr_Name[A-Za-z0-9]+_([A-Z0-9_]+)");
  std::smatch m;

  while (std::regex_search (s, m, p)) {
    if (m.size() > 0) {
    }
    s = m.suffix().str();
  }

  return res;
}

bool is_skip(const std::string& feature_name) {
  static std::unordered_set<std::string> names = {
    "RecoFastFeature",
    "BSRecoFastFeature",
    "FastFeatureNoPrefix",
    "BSFastFeatureNoPrefix",
    "CommonInfoAttrFeature",
    "BSCommonInfoAttrFeature",
    "ExtractMultiAttrFastFeatureNoPrefix",
    "BSExtractMultiAttrFastFeatureNoPrefix",
  };

  return names.find(feature_name) != names.end();
}

bool is_enum_decl(clang::Expr* expr) {
  if (expr == nullptr) {
    return false;
  }

  const clang::Type* type = expr->getType().getTypePtr();
  const clang::EnumDecl* enumDecl = type->getAs<clang::EnumType>()->getDecl();
  if (enumDecl != nullptr) {
    return true;
    // for (auto it = enumDecl->enumerator_begin(); it != enumDecl->enumerator_end(); it++) {
    //   std::string itName = it->getNameAsString();
    //   auto pos = itName.find(enum_name);
    //   if (pos != std::string::npos) {
    //     if (itName.substr(pos) == enum_name) {
    //       int enum_value = std::stoi(stmt_to_string(it->getInitExpr()));
    //       LOG(INFO) << "find enum, name: " << it->getNameAsString()
    //                 << ", value: " << stmt_to_string(it->getInitExpr());
    //       return absl::optional<int>(enum_value);
    //     }
    //   }
    // }
  }

  return false;
}

bool is_implicit_loop_var(const std::string& s) {
  static const std::regex p("__(begin|range|end)\\d+");
  return std::regex_match(s, p);
}

bool is_from_implicit_loop_var(const std::string &s) {
  if (is_implicit_loop_var(s)) {
    return true;
  }

  static const std::regex p("__(begin|range|end).*\\.(begin|end)\\(\\)");
  return std::regex_match(s, p);
}

bool is_deref_implicit_loop_begin(const std::string s) {
  static const std::regex p("\\* ?__begin");
  return std::regex_match(s, p);
}

bool is_cxx_default_arg_expr(clang::Expr* expr) {
  if (expr == nullptr) {
    return false;
  }

  if (clang::CXXDefaultArgExpr* cxx_default_arg_expr = dyn_cast<clang::CXXDefaultArgExpr>(expr)) {
    return true;
  }

  return false;
}

std::string trim_this(const std::string& s) {
  static const std::regex p("this\\->");
  return std::regex_replace(s, p, "");
}

bool is_int_vector(clang::QualType qual_type) {
  const clang::Type* type = qual_type.getTypePtr();
  if (type != nullptr) {
    if (qual_type.getAsString().find("vector<int") != std::string::npos) {
      return true;
    }
  }

  return false;
}

std::string rm_surround_big_parantheses(const std::string& s) {
  if (s.size() == 0) {
    return s;
  }

  int left_pos = -1;
  int right_pos = -1;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '{') {
      left_pos = i;
      break;
    } else if (isspace(s[i])) {
      continue;
    } else {
      break;
    }
  }

  for (int i = s.size() - 1; i >= 0; i--) {
    if (s[i] == '}') {
      right_pos = i;
      break;
    } else if (isspace(s[i])) {
      continue;
    } else {
      break;
    }
  }

  if (left_pos >= 0 && right_pos > 0) {
    return s.substr(left_pos + 1, right_pos - left_pos - 1);
  } else {
    return s;
  }
}

std::string add_surround_big_parantheses(const std::string& s) {
  std::ostringstream oss;
  oss << "{" << s << "}";

  return oss.str();
}

bool is_action_info(const std::string& type_str) {
  static std::unordered_set<std::string> action_info_types = {
      "AdActionInfoList",
      "::auto_cpp_rewriter::AdActionInfoList",
      "const class auto_cpp_rewriter::AdActionInfoList",
      "AdActionBaseInfo",
      "::auto_cpp_rewriter::AdActionBaseInfo",
      "const class auto_cpp_rewriter::AdActionBaseInfo",
      "SimpleAdDspInfos",
      "::auto_cpp_rewriter::SimpleAdDspInfos",
      "const class auto_cpp_rewriter::SimpleAdDspInfos",
      "SimpleLiveInfos",
      "::auto_cpp_rewriter::SimpleLiveInfos",
      "const class auto_cpp_rewriter::SimpleLiveInfos",
      "SimpleLiveInfo",
      "::auto_cpp_rewriter::SimpleLiveInfo",
      "const class auto_cpp_rewriter::SimpleLiveInfo"};

  return action_info_types.find(type_str) != action_info_types.end();
}

bool is_action_info(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();
  return is_action_info(type_str);
}

bool is_repeated_proto(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();

  static std::regex p("(const )?::google::protobuf::Repeated(Ptr)?Field< ?.*> ?\\&?");
  std::smatch m;
  return std::regex_match(type_str, m, p);
}

bool is_repeated_proto_iterator(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();

  static std::regex p("(const )?(::)?google::protobuf::Repeated(Ptr)?Field< ?.*>::(const_)?iterator");
  std::smatch m;
  return std::regex_match(type_str, m, p);
}

bool is_map_proto(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();

  static std::regex p("(const )?::google::protobuf::Map< ?(.*)> ?\&?");
  std::smatch m;
  return std::regex_match(type_str, m, p);
}

bool is_map_proto_iterator(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();

  LOG(INFO) << "type_str: " << type_str;

  static std::regex p("(const )?(::)?google::protobuf::Map< ?.*>::(const_)?(iterator|pointer)");
  std::smatch m;
  return std::regex_match(type_str, m, p);
}

bool is_repeated_action_info(clang::QualType qual_type) {
  if (is_repeated_proto(qual_type)) {
    std::string type_str = qual_type.getAsString();
    static std::regex p("(const )?::google::protobuf::Repeated(Ptr)?Field< ?(.*)::(.*)> ?\\&?");
    std::smatch m;
    if (std::regex_match(type_str, m, p)) {
      if (m.size() > 0) {
        if (is_action_info(m[m.size() - 1])) {
          return true;
        }
      }
    }
  }

  return false;
}

bool is_action_detail_map(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();

  if (type_str.find("google::protobuf::Map") != std::string::npos) {
    if (type_str.find("ad::algorithm::AdActionInfoList") != std::string::npos ||
        type_str.find("ad::algorithm::SimpleAdDspInfos") != std::string::npos ||
        type_str.find("ad::algorithm::AdActionBaseInfo") != std::string::npos ||
        type_str.find("ad::algorithm::SimpleLiveInfos") != std::string::npos) {
      return true;
    }
  }

  return false;
}

std::string get_bs_type_str(clang::QualType qual_type, bool is_combine_user) {
  std::string type_str = qual_type.getAsString();

  if (is_reco_proto(qual_type)) {
    return type_str;
  }

  if (is_repeated_proto(qual_type) ||
      is_repeated_proto_ptr(qual_type)) {
    if (auto inner_type = get_repeated_proto_inner_type(qual_type)) {
      return get_bs_repeated_field_type(*inner_type, is_combine_user);
    }
  } else if (is_repeated_proto_iterator(qual_type)) {
    if (auto inner_type = get_repeated_proto_iterator_inner_type(qual_type)) {
      return get_bs_repeated_field_type(*inner_type, is_combine_user);
    }
  } else if (is_map_proto(qual_type)) {
    if (auto inner_type = get_map_proto_inner_type(qual_type)) {
      return get_bs_map_field_type(inner_type->first, inner_type->second, is_combine_user);
    }
  }

  return get_builtin_type_str(qual_type);
}

absl::optional<std::string> get_repeated_proto_inner_type(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();
  static std::regex p("(const )?(::)?google::protobuf::Repeated(Ptr)?Field< ?(.*)> ?[\\&\\*]?");
  std::smatch m;
  if (std::regex_match(type_str, m, p)) {
    if (m.size() > 0) {
      LOG(INFO) << "match inner_type: " << m[m.size() - 1];
      std::string last_type = get_last_type_str(m[m.size() - 1]);
      if (last_type == "string" || last_type == "std::string") {
        return absl::make_optional("absl::string_view");
      } else {
        return absl::make_optional(last_type);
      }
    }
  }

  if (absl::optional<std::string> inner_type = get_repeated_proto_iterator_inner_type(qual_type)) {
    return absl::make_optional(*inner_type);
  }

  return absl::nullopt;
}

absl::optional<std::string> get_repeated_proto_iterator_inner_type(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();
  static std::regex p("(const )?(::)?google::protobuf::Repeated(Ptr)?Field< ?(.*)>::const_iterator");
  std::smatch m;
  if (std::regex_match(type_str, m, p)) {
    if (m.size() > 0) {
      std::string last_type = get_last_type_str(m[m.size() - 1]);
      if (last_type == "string" || last_type == "std::string") {
        return absl::make_optional("absl::string_view");
      } else {
        return absl::make_optional(last_type);
      }
    }
  }

  return absl::nullopt;
}

absl::optional<std::pair<std::string, std::string>> get_map_proto_inner_type(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();
  static std::regex p("(const )?(class )?(::)?google::protobuf::Map< ?(.*), (.*) ?> ?(\\&\\*)?");
  std::smatch m;

  if (std::regex_match(type_str, m, p)) {
    if (m.size() > 3) {
      std::string param0 = m[m.size() - 2];
      std::string param1 = m[m.size() - 1];

      if (param0 == "string" || param0 == "std::string") {
        param0 = "absl::string_view";
      }

      if (param1 == "string" || param1 == "std::string") {
        param1 = "absl::string_view";
      }

      return absl::make_optional(std::make_pair(param0, param1));
    }
  }

  return absl::nullopt;
}

std::string get_bs_repeated_field_type(const std::string &type_str,
                                       bool is_combine_user) {
  std::ostringstream oss;
  oss << "BSRepeatedField<" << type_str;
  if (is_combine_user) {
    oss << ", true";
  }
  oss << ">";

  return oss.str();
}

std::string get_bs_map_field_type(const std::string &key_type_str,
                                  const std::string &value_type_str,
                                  bool is_combine_user) {
  std::ostringstream oss;
  oss << "BSMapField<" << key_type_str << ", " << value_type_str;
  if(is_combine_user) {
    oss << ", true";
  }
  oss << ">";

  return oss.str();
}

bool is_repeated_proto_ptr(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();

  static std::regex p("(const )?(class )?(::)?google::protobuf::Repeated(Ptr)?Field< ?.*> ?\\*");
  std::smatch m;
  if (std::regex_match(type_str, m, p)) {
    return true;
  }

  static std::regex p_mapped_type("std::(unordered_)?map<int, (const )?(class )?(::)?google::protobuf::Repeated(Ptr)?Field< ?.*> ?\\*>::mapped_type");
  return std::regex_match(type_str, m, p_mapped_type);
}

bool is_reco_proto(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();
  return type_str.find("google::protobuf") != std::string::npos &&
    type_str.find("ks::reco") != std::string::npos;
}

bool is_reco_proto_type(clang::QualType qual_type) {
  return is_reco_proto(qual_type);
}

std::string replace_adlog_to_bslog(const std::string &s) {
  static std::regex p("(adlog|ad_log)");
  std::string new_str = std::regex_replace(s, p, "bslog");
  return new_str;
}

bool is_map_repeated_int_list_type(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();

  static std::regex p("std::(unordered_)?map<int,[ \\n]?(const )?google::protobuf::RepeatedField< ?::google::protobuf::int64> ?\\*>");
  std::smatch m;
  return std::regex_match(type_str, m, p);
}

bool is_map_int_int_type(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();

  static std::regex p("std::(unordered_)?map<int, int>");
  std::smatch m;
  return std::regex_match(type_str, m, p);
}

clang::Expr* get_inner_expr(clang::Expr* expr) {
  if (expr == nullptr) {
    return expr;
  }

  if (clang::MaterializeTemporaryExpr* materialize_temporary_expr =
      dyn_cast<clang::MaterializeTemporaryExpr>(expr)) {
    return get_inner_expr(materialize_temporary_expr->getSubExpr());
  }

  if (clang::ImplicitCastExpr *implicit_cast_expr = dyn_cast<clang::ImplicitCastExpr>(expr)) {
    return get_inner_expr(implicit_cast_expr->getSubExpr());
  }

  return expr;
}

std::string rm_empty_line(const std::string& s) {
  static std::regex p_empty_line("\n *;");
  return std::regex_replace(s, p_empty_line, "");
}

std::string trim_tail_underscore(const std::string& s) {
  static std::regex p("_+$");
  return std::regex_replace(s, p, "");
}

bool is_common_info_list_or_map_loop_stmt(clang::Stmt* stmt) {
  if (stmt == nullptr) {
    return false;
  }

  std::string stmt_str = stmt_to_string(stmt);

  if (clang::CXXForRangeStmt* cxx_for_range_stmt = dyn_cast<clang::CXXForRangeStmt>(stmt)) {
    if (CommonAttrInfo::is_common_info_list_or_map_loop(stmt_str)) {
      return true;
    }
  }

  if (clang::ForStmt* for_stmt = dyn_cast<clang::ForStmt>(stmt)) {
    if (CommonAttrInfo::is_common_info_list_or_map_loop(stmt_str)) {
      return true;
    }
  }

  return false;
}

std::string trim_tail_size(const std::string& s) {
  if (ends_with(s, "_size")) {
    std::string prefix = s.substr(0, s.size() - std::string("_size").size());
    return prefix;
  }

  return s;
}

bool contains_var(const std::string& expr_str, const std::string& var_name) {
  std::regex p(std::string("([^a-zA-Z0-9_])?") + var_name + std::string("([^a-zA-Z0-9_])?"));

  std::smatch match_res;
  if (std::regex_search(expr_str, match_res, p)) {
    if (expr_str.size() != var_name.size()) {
      return true;
    }
  }

  return false;
}

bool is_map_item_type_int_type(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();

  static std::regex p("std::(unordered_)?map<ItemType, int>");
  std::smatch m;
  return std::regex_match(type_str, m, p);
}

std::string get_last_type_str(const std::string& s) {
  size_t pos = s.find("::");
  if (pos != std::string::npos) {
    return s.substr(pos + 2);
  } else {
    return s;
  }
}

bool has_cc_file(const std::string& filename) {
  if (ends_with(filename, ".h")) {
    std::string cc_file = filename.substr(0, filename.size() - 1) + std::string("cc");
    return is_file_exists(cc_file);
  }

  return false;
}

bool is_pointer(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();
  return ends_with(type_str, "*");
}

bool is_proto_map_string_float(clang::QualType qual_type) {
  if (auto inner_type = get_map_proto_inner_type(qual_type)) {
    if (inner_type->first == "absl::string_view" && inner_type->second == "float") {
      return true;
    }
  }

  return false;
}

bool is_proto_map_string_float_ptr(clang::QualType qual_type) {
  return is_proto_map_string_float(qual_type) && is_pointer(qual_type);
}

bool is_proto_map_string_float_iter(clang::QualType qual_type) {
  std::string type_str = qual_type.getAsString();

  static std::regex p("(const )?(class )?(::)?google::protobuf::Map< ?(::)?std::string, float ?>::(const_)?iterator");
  std::smatch m;
  return std::regex_match(type_str, m, p);
}

std::string trim_exists(const std::string& s) {
  if (ends_with(s, ".exists")) {
    return s.substr(0, s.size() - 7);
  }

  return s;
}

bool is_int_type(const std::string& type_str) {
  static std::regex p("(::)?google::protobuf::");
  static std::regex p_const("const ?");

  static std::regex p_int("u?int(32|64)_?t?");

  std::string s = std::regex_replace(lower(type_str), p, "");
  s = std::regex_replace(s, p_const, "");

  if (s == "size_t") {
    return true;
  }

  std::smatch m;
  if (std::regex_match(s, m, p_int)) {
    return true;
  }

  return false;
}

bool is_bool_type(const std::string& type_str) {
  return lower(type_str) == "bool";
}

bool is_float_type(const std::string& type_str) {
  if (type_str == "float" || type_str == "double") {
    return true;
  }

  return false;
}

bool is_repeated_proto_list_leaf_type(clang::QualType qual_type) {
  if (is_var_proto_list(qual_type)) {
    if (absl::optional<std::string> inner_type = get_repeated_proto_inner_type(qual_type)) {
      if (is_string(*inner_type)) {
        return true;
      }
      if (is_int_type(*inner_type) || is_bool_type(*inner_type) || is_float_type(*inner_type)) {
        return true;
      }
    }
  }

  return false;
}

std::vector<int> get_int_list_values_from_init_str(const std::string& s) {
  static std::regex p_space(" +");
  static std::regex p("\\{([\\d ,]+)\\}");

  std::vector<int> res;

  LOG(INFO) << "s: " << s;
  std::smatch m;
  if (std::regex_match(s, m, p)) {
    if (m.size() > 0) {
      std::string value_str = m[m.size() - 1];
      LOG(INFO) << "match value: " << value_str;

      value_str = std::regex_replace(value_str, p_space, "");
      if (value_str.size() == 0) {
        LOG(INFO) << "cannot find init values from: " << s;
        return  res;
      }

      std::vector<std::string> arr = absl::StrSplit(value_str, ",");
      for (size_t i = 0; i < arr.size(); i++) {
        if (is_integer(arr[i])) {
          res.push_back(std::stoi(arr[i]));
        }
      }

      return res;
    }
  }

  return res;
}

std::string add_filename_suffix(const std::string& filename, const std::string& suffix) {
  std::vector<std::string> arr = absl::StrSplit(filename, ".");
  if (arr.size() == 2) {
    return arr[0] + suffix + "." + arr[1];
  } else {
    return arr[0] + suffix;
  }
}

bool is_str_from_reco_user_info(const std::string& bs_enum_str) {
  return bs_enum_str.find("reco_user_info") != std::string::npos;
}

std::string replace_simple_text(const std::string &s) {
  static std::regex p("_Bool");
  static std::regex p_empty_line("\n *\n");
  static std::regex p_empty_line_semicoln("\n *;");

  std::string s1 = std::regex_replace(s, p, "bool");
  s1 = std::regex_replace(s1, p_empty_line_semicoln, "");

  return s1;
}

std::string insert_str_at_block_begin(const std::string& s, const std::string& new_str) {
  std::ostringstream oss;

  if (new_str.size() > 0) {
    size_t pos = s.find("{");
    if (pos != std::string::npos) {
      oss << s.substr(0, pos + 1) << "\n"
          << new_str << "\n"
          << s.substr(pos + 1);
    }
  }

  return oss.str();
}

size_t find_bs_equal_end(const std::string &s) {
  size_t pos = s.find("bs == nullptr");

  if (pos == std::string::npos) {
    pos = s.find("bs== nullptr");
  }

  if (pos == std::string::npos) {
    pos = s.find("bs ==nullptr");
  }

  if (pos == std::string::npos) {
    return pos;
  }

  for (; pos < s.size(); pos++) {
    if (s[pos] == '}') {
      break;
    }
  }

  return pos;
}

std::string insert_str_after_bs_equal_end(const std::string &s, const std::string &new_str) {
  std::ostringstream oss;

  if (new_str.size() > 0) {
    size_t pos = find_bs_equal_end(s);
    if (pos != std::string::npos && pos + 1 < s.size()) {
      oss << s.substr(0, pos + 1) << "\n"
          << new_str << "\n"
          << s.substr(pos + 1);
    }
  } else {
    oss << s;
  }

  return oss.str();
}

std::string strip_suffix_semicolon_newline(const std::string& s) {
  int pos = s.size() - 1;
  for (; pos >= 0; pos--) {
    if (s[pos] == '\n' || s[pos] == ';') {
      continue;
    }

    break;
  }

  if (pos >= 0 && pos < s.size()) {
    return s.substr(0, pos + 1);
  } else {
    return s;
  }
}

}  // tool
}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
