// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This clang tool finds all instances of the following functions:
//   - net::DefineNetworkTrafficAnnotation
//   - net::DefinePartialNetworkTrafficAnnotation
//   - net::CompleteNetworkTrafficAnnotation
//   - net::BranchedCompleteNetworkTrafficAnnotation
// It extracts the location info and content of annotation tags, and outputs
// them to llvm::outs. Please refer to README.md for build and usage
// instructions.

#include <memory>
#include <vector>

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

using namespace clang::ast_matchers;

namespace {

// An instance of a call to either of the 4 network traffic annotation
// definition functions.
struct NetworkAnnotationInstance {
  // Information about where the call has happened.
  struct Location {
    std::string file_path;
    int line_number = -1;

    // Name of the function including this line. E.g., in the following code,
    // |function_name| will be 'foo' for all |line_number| values 101-103.
    //
    // 100 void foo() {
    // 101   NetworkTrafficAnnotationTag baz =
    // 102       net::DefineNetworkTrafficAnnotation(...); }
    // 103   bar(baz);
    // 104 }
    // If no function is found, 'Global Namespace' will be returned.
    std::string function_name;
  };

  // Annotation content. These are the arguments of the call to either of the 4
  // network traffic annotation definition functions.
  struct Annotation {
    std::string unique_id;
    std::string text;

    // |extra_id| will have |completing_id| for
    // net::DefinePartialNetworkTrafficAnnotation and |group_id| for
    // net::BranchedCompleteNetworkTrafficAnnotation. It will be empty in other
    // cases.
    std::string extra_id;
  };

  Location location;
  Annotation annotation;

  // Specifying the function type.
  enum FunctionType {
    kDefinition,         // net::DefineNetworkTrafficAnnotation
    kPartial,            // net::DefinePartialNetworkTrafficAnnotation
    kCompleting,         // net::CompleteNetworkTrafficAnnotation
    kBranchedCompleting  // net::BranchedCompleteNetworkTrafficAnnotation
  };

  FunctionType function_type;

  const char* GetTypeName() const {
    switch (function_type) {
      case kDefinition:
        return "Definition";
      case kPartial:
        return "Partial";
      case kCompleting:
        return "Completing";
      case kBranchedCompleting:
        return "BranchedCompleting";
    }
    assert(false);
    return "";
  }
};

using Collector = std::vector<NetworkAnnotationInstance>;

// This class implements the call back functions for AST Matchers. The matchers
// are defined in RunMatchers function. When a pattern is found there,
// the run function in this class is called back with information on the matched
// location and description of the matched pattern.
class NetworkAnnotationTagCallback : public MatchFinder::MatchCallback {
 public:
  explicit NetworkAnnotationTagCallback(Collector* collector)
      : collector_(collector) {}
  ~NetworkAnnotationTagCallback() override = default;

  // Is called on any pattern found by ASTMathers that are defined in RunMathers
  // function.
  virtual void run(const MatchFinder::MatchResult& result) override {
    NetworkAnnotationInstance instance;

    const clang::StringLiteral* unique_id =
        result.Nodes.getNodeAs<clang::StringLiteral>("unique_id");
    const clang::StringLiteral* annotation_text =
        result.Nodes.getNodeAs<clang::StringLiteral>("annotation_text");
    const clang::FunctionDecl* ancestor =
        result.Nodes.getNodeAs<clang::FunctionDecl>("function_context");
    const clang::StringLiteral* group_id =
        result.Nodes.getNodeAs<clang::StringLiteral>("group_id");
    const clang::StringLiteral* completing_id =
        result.Nodes.getNodeAs<clang::StringLiteral>("completing_id");

    const clang::CallExpr* call_expr = nullptr;
    if ((call_expr =
             result.Nodes.getNodeAs<clang::CallExpr>("definition_function"))) {
      instance.function_type = NetworkAnnotationInstance::kDefinition;
    } else if ((call_expr = result.Nodes.getNodeAs<clang::CallExpr>(
                    "partial_function"))) {
      instance.function_type = NetworkAnnotationInstance::kPartial;
      assert(completing_id);
      instance.annotation.extra_id = completing_id->getString();
    } else if ((call_expr = result.Nodes.getNodeAs<clang::CallExpr>(
                    "completing_function"))) {
      instance.function_type = NetworkAnnotationInstance::kCompleting;
    } else if ((call_expr = result.Nodes.getNodeAs<clang::CallExpr>(
                    "branched_completing_function"))) {
      instance.function_type = NetworkAnnotationInstance::kBranchedCompleting;
      assert(group_id);
      instance.annotation.extra_id = group_id->getString();
    } else {
      assert(false);
    }

    assert(unique_id && annotation_text);
    instance.annotation.unique_id = unique_id->getString();
    instance.annotation.text = annotation_text->getString();

    // Get annotation location.
    clang::SourceLocation source_location = call_expr->getLocStart();
    if (source_location.isMacroID()) {
      source_location =
          result.SourceManager->getImmediateMacroCallerLoc(source_location);
    }
    instance.location.file_path =
        result.SourceManager->getFilename(source_location);
    instance.location.line_number =
        result.SourceManager->getSpellingLineNumber(source_location);
    if (ancestor)
      instance.location.function_name = ancestor->getQualifiedNameAsString();
    else
      instance.location.function_name = "Global Namespace";

    // Trim leading "../"s from file path.
    std::replace(instance.location.file_path.begin(),
                 instance.location.file_path.end(), '\\', '/');
    while (instance.location.file_path.length() > 3 &&
           instance.location.file_path.substr(0, 3) == "../") {
      instance.location.file_path = instance.location.file_path.substr(
          3, instance.location.file_path.length() - 3);
    }

    collector_->push_back(instance);
  }

 private:
  Collector* collector_;
};

// Sets up an ASTMatcher and runs clang tool to populate collector. Returns the
// result of running the clang tool.
int RunMatchers(clang::tooling::ClangTool* clang_tool, Collector* collector) {
  NetworkAnnotationTagCallback callback(collector);
  MatchFinder match_finder;

  // Set up patterns to find network traffic annotation definition functions,
  // their arguments, and their ancestor function (when possible).
  match_finder.addMatcher(
      callExpr(hasDeclaration(functionDecl(
                   anyOf(hasName("DefineNetworkTrafficAnnotation"),
                         hasName("net::DefineNetworkTrafficAnnotation")))),
               hasArgument(0, stringLiteral().bind("unique_id")),
               hasArgument(1, stringLiteral().bind("annotation_text")),
               anyOf(hasAncestor(functionDecl().bind("function_context")),
                     unless(hasAncestor(functionDecl()))))
          .bind("definition_function"),
      &callback);
  match_finder.addMatcher(
      callExpr(hasDeclaration(functionDecl(anyOf(
                   hasName("DefinePartialNetworkTrafficAnnotation"),
                   hasName("net::DefinePartialNetworkTrafficAnnotation")))),
               hasArgument(0, stringLiteral().bind("unique_id")),
               hasArgument(1, stringLiteral().bind("completing_id")),
               hasArgument(2, stringLiteral().bind("annotation_text")),
               anyOf(hasAncestor(functionDecl().bind("function_context")),
                     unless(hasAncestor(functionDecl()))))
          .bind("partial_function"),
      &callback);
  match_finder.addMatcher(
      callExpr(hasDeclaration(functionDecl(
                   anyOf(hasName("CompleteNetworkTrafficAnnotation"),
                         hasName("net::CompleteNetworkTrafficAnnotation")))),
               hasArgument(0, stringLiteral().bind("unique_id")),
               hasArgument(2, stringLiteral().bind("annotation_text")),
               anyOf(hasAncestor(functionDecl().bind("function_context")),
                     unless(hasAncestor(functionDecl()))))
          .bind("completing_function"),
      &callback);
  match_finder.addMatcher(
      callExpr(hasDeclaration(functionDecl(anyOf(
                   hasName("BranchedCompleteNetworkTrafficAnnotation"),
                   hasName("net::BranchedCompleteNetworkTrafficAnnotation")))),
               hasArgument(0, stringLiteral().bind("unique_id")),
               hasArgument(1, stringLiteral().bind("group_id")),
               hasArgument(3, stringLiteral().bind("annotation_text")),
               anyOf(hasAncestor(functionDecl().bind("function_context")),
                     unless(hasAncestor(functionDecl()))))
          .bind("branched_completing_function"),
      &callback);
  std::unique_ptr<clang::tooling::FrontendActionFactory> frontend_factory =
      clang::tooling::newFrontendActionFactory(&match_finder);
  return clang_tool->run(frontend_factory.get());
}

}  // namespace

static llvm::cl::OptionCategory ToolCategory(
    "traffic_annotation_extractor: Extract traffic annotation texts");
static llvm::cl::extrahelp CommonHelp(
    clang::tooling::CommonOptionsParser::HelpMessage);

int main(int argc, const char* argv[]) {
  clang::tooling::CommonOptionsParser options(argc, argv, ToolCategory);
  clang::tooling::ClangTool tool(options.getCompilations(),
                                 options.getSourcePathList());
  Collector collector;

  int result = RunMatchers(&tool, &collector);

  if (result != 0)
    return result;

  // For each call to any of the functions that define a network traffic
  // annotation, write annotation text and relevant meta data into llvm::outs().
  for (const NetworkAnnotationInstance& call : collector) {
    llvm::outs() << "==== NEW ANNOTATION ====\n";
    llvm::outs() << call.location.file_path << "\n";
    llvm::outs() << call.location.function_name << "\n";
    llvm::outs() << call.location.line_number << "\n";
    llvm::outs() << call.GetTypeName() << "\n";
    llvm::outs() << call.annotation.unique_id << "\n";
    llvm::outs() << call.annotation.extra_id << "\n";
    llvm::outs() << call.annotation.text << "\n";
    llvm::outs() << "==== ANNOTATION ENDS ====\n";
  }

  return 0;
}