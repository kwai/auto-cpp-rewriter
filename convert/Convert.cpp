#include <glog/logging.h>
#include <gflags/gflags.h>
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/ADT/StringRef.h"
#include <iostream>
#include <string>
#include <sstream>
#include <list>
#include <algorithm>

#include "ConvertAction.h"
#include "LogicParser.h"

using namespace llvm;
using namespace clang;
using namespace clang::tooling;

static llvm::cl::OptionCategory MatcherCategory("Matcher");

cl::opt<std::string> Cmd("cmd",
                         cl::desc("cmd (default = 'hello') "),
                         cl::value_desc("cmd"),
                         cl::init("hello"));

cl::opt<bool> RemoveComment("remove-comment",
                            cl::desc("remove comment"),
                            cl::init(false));

cl::opt<bool> DumpAst("dump-ast",
                      cl::desc("dump ast"),
                      cl::init(false));

cl::opt<bool> Overwrite("overwrite",
                        cl::desc("overwrite exists bs file, default false"),
                        cl::init(false));

cl::opt<std::string> Filename("filename",
                              cl::desc("filename"),
                              cl::init(""));

cl::opt<std::string> FieldDetailFilename("field-detail-filename",
                                         cl::desc("field detail filename for parse result"),
                                         cl::init(""));

cl::opt<std::string> MessageDefFilename("message-def-filename",
                                        cl::desc("json filename for message def"),
                                        cl::init(""));

cl::opt<bool> UseRecoUserInfo("use_reco_user_info",
                             cl::desc("use reco user info"),
                             cl::init(false));

DECLARE_bool(logtostderr);

using ks::ad_algorithm::convert::GlobalConfig;
using ks::ad_algorithm::convert::ConvertAction;
using ks::ad_algorithm::convert::LogicParser;

int main(int argc, const char **argv) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = 1;

  auto ExpectedParser = CommonOptionsParser::create(argc, argv, MatcherCategory);
  if (!ExpectedParser) {
    LOG(ERROR) << "ExpectedParser error, return";
    return 1;
  }

  auto config = GlobalConfig::Instance();
  config->cmd = Cmd;
  config->remove_comment = RemoveComment;
  config->dump_ast = DumpAst;
  config->overwrite = Overwrite;
  config->filename = Filename;
  config->field_detail_filename = FieldDetailFilename;
  config->message_def_filename = MessageDefFilename;
  config->use_reco_user_info = UseRecoUserInfo;

  LOG(INFO) << "Cmd: " << config->cmd;

  CommonOptionsParser& op = ExpectedParser.get();
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());

  if (config->cmd == "hello") {
    LOG(INFO) << "hello";
  } else if (config->cmd == "convert") {
    return Tool.run(newFrontendActionFactory<ConvertAction>().get());
  } else if (config->cmd == "parse_logic") {
    return Tool.run(newFrontendActionFactory<LogicParser>().get());
  } else {
    LOG(ERROR) << "unsupported cmd: " << config->cmd;
  }

  return 0;
}

