// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Changes Blink-style names to Chrome-style names. Currently transforms:
//   fields:
//     int m_operationCount => int operation_count_
//   variables (including parameters):
//     int mySuperVariable => int my_super_variable
//   constants:
//     const int maxThings => const int kMaxThings
//   free functions and methods:
//     void doThisThenThat() => void DoThisAndThat()

#include <assert.h>
#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>

#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchersMacros.h"
#include "clang/Basic/CharInfo.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/file.h>
#include <unistd.h>
#endif

using namespace clang::ast_matchers;
using clang::tooling::CommonOptionsParser;
using clang::tooling::Replacement;
using clang::tooling::Replacements;
using llvm::StringRef;

namespace {

// Hack: prevent the custom isDefaulted() from conflicting with the one defined
// in newer revisions of Clang.
namespace internal_hack {

// This is available in newer clang revisions... but alas, Chrome has not rolled
// that far yet.
AST_MATCHER(clang::FunctionDecl, isDefaulted) {
  return Node.isDefaulted();
}

}  // namespace internal_hack

const char kBlinkFieldPrefix[] = "m_";
const char kBlinkStaticMemberPrefix[] = "s_";

AST_MATCHER(clang::FunctionDecl, isOverloadedOperator) {
  return Node.isOverloadedOperator();
}

// A method is from Blink if it is from the Blink namespace or overrides a
// method from the Blink namespace.
bool IsBlinkMethod(const clang::CXXMethodDecl& decl) {
  auto* namespace_decl = clang::dyn_cast_or_null<clang::NamespaceDecl>(
      decl.getParent()->getEnclosingNamespaceContext());
  if (namespace_decl && namespace_decl->getParent()->isTranslationUnit() &&
      (namespace_decl->getName() == "blink" ||
       namespace_decl->getName() == "WTF"))
    return true;

  for (auto it = decl.begin_overridden_methods();
       it != decl.end_overridden_methods(); ++it) {
    if (IsBlinkMethod(**it))
      return true;
  }
  return false;
}

AST_MATCHER(clang::CXXMethodDecl, isBlinkMethod) {
  return IsBlinkMethod(Node);
}

// Helper to convert from a camelCaseName to camel_case_name. It uses some
// heuristics to try to handle acronyms in camel case names correctly.
std::string CamelCaseToUnderscoreCase(StringRef input) {
  std::string output;
  bool needs_underscore = false;
  bool was_lowercase = false;
  bool was_uppercase = false;
  // Iterate in reverse to minimize the amount of backtracking.
  for (const unsigned char* i = input.bytes_end() - 1; i >= input.bytes_begin();
       --i) {
    char c = *i;
    bool is_lowercase = clang::isLowercase(c);
    bool is_uppercase = clang::isUppercase(c);
    c = clang::toLowercase(c);
    // Transitioning from upper to lower case requires an underscore. This is
    // needed to handle names with acronyms, e.g. handledHTTPRequest needs a '_'
    // in 'dH'. This is a complement to the non-acronym case further down.
    if (needs_underscore || (was_uppercase && is_lowercase)) {
      output += '_';
      needs_underscore = false;
    }
    output += c;
    // Handles the non-acronym case: transitioning from lower to upper case
    // requires an underscore when emitting the next character, e.g. didLoad
    // needs a '_' in 'dL'.
    if (i != input.bytes_end() - 1 && was_lowercase && is_uppercase)
      needs_underscore = true;
    was_lowercase = is_lowercase;
    was_uppercase = is_uppercase;
  }
  std::reverse(output.begin(), output.end());
  return output;
}

bool IsProbablyConst(const clang::VarDecl& decl,
                     const clang::ASTContext& context) {
  clang::QualType type = decl.getType();
  if (!type.isConstQualified())
    return false;

  if (type.isVolatileQualified())
    return false;

  // http://google.github.io/styleguide/cppguide.html#Constant_Names
  // Static variables that are const-qualified should use kConstantStyle naming.
  if (decl.getStorageDuration() == clang::SD_Static)
    return true;

  const clang::Expr* initializer = decl.getInit();
  if (!initializer)
    return false;

  // If the expression is dependent on a template input, then we are not
  // sure if it can be compile-time generated as calling isEvaluatable() is
  // not valid on |initializer|.
  // TODO(crbug.com/581218): We could probably look at each compiled
  // instantiation of the template and see if they are all compile-time
  // isEvaluable().
  if (initializer->isInstantiationDependent())
    return false;

  // If the expression can be evaluated at compile time, then it should have a
  // kFoo style name. Otherwise, not.
  return initializer->isEvaluatable(context);
}

bool GetNameForDecl(const clang::FunctionDecl& decl,
                    const clang::ASTContext& context,
                    std::string& name) {
  name = decl.getName().str();
  name[0] = clang::toUppercase(name[0]);
  return true;
}

bool GetNameForDecl(const clang::CXXMethodDecl& decl,
                    const clang::ASTContext& context,
                    std::string& name) {
  StringRef original_name = decl.getName();

  if (!decl.isStatic()) {
    // Some methods shouldn't be renamed because reasons.
    static const char* kBlacklist[] = {"begin", "end",  "rbegin", "rend",
                                       "trace", "lock", "unlock", "try_lock"};
    for (const auto& b : kBlacklist) {
      if (original_name == b)
        return false;
    }
  }

  name = decl.getName().str();
  name[0] = clang::toUppercase(name[0]);
  return true;
}

bool GetNameForDecl(const clang::FieldDecl& decl,
                    const clang::ASTContext& context,
                    std::string& name) {
  StringRef original_name = decl.getName();
  // Blink style field names are prefixed with `m_`. If this prefix isn't
  // present, assume it's already been converted to Google style.
  if (original_name.size() < strlen(kBlinkFieldPrefix) ||
      !original_name.startswith(kBlinkFieldPrefix))
    return false;
  name = CamelCaseToUnderscoreCase(
      original_name.substr(strlen(kBlinkFieldPrefix)));
  // The few examples I could find used struct-style naming with no `_` suffix
  // for unions.
  bool c = decl.getParent()->isClass();
  // There appears to be a GCC bug that makes this branch incorrectly if we
  // don't use a temp variable!! Clang works right. crbug.com/580745
  if (c)
    name += '_';
  return true;
}

bool GetNameForDecl(const clang::VarDecl& decl,
                    const clang::ASTContext& context,
                    std::string& name) {
  StringRef original_name = decl.getName();

  // Nothing to do for unnamed parameters.
  if (clang::isa<clang::ParmVarDecl>(decl) && original_name.empty())
    return false;

  // static class members match against VarDecls. Blink style dictates that
  // these should be prefixed with `s_`, so strip that off. Also check for `m_`
  // and strip that off too, for code that accidentally uses the wrong prefix.
  if (original_name.startswith(kBlinkStaticMemberPrefix))
    original_name = original_name.substr(strlen(kBlinkStaticMemberPrefix));
  else if (original_name.startswith(kBlinkFieldPrefix))
    original_name = original_name.substr(strlen(kBlinkFieldPrefix));

  bool is_const = IsProbablyConst(decl, context);
  if (is_const) {
    // Don't try to rename constants that already conform to Chrome style.
    if (original_name.size() >= 2 && original_name[0] == 'k' &&
        clang::isUppercase(original_name[1]))
      return false;
    name = 'k';
    name.append(original_name.data(), original_name.size());
    name[1] = clang::toUppercase(name[1]);
  } else {
    name = CamelCaseToUnderscoreCase(original_name);
  }

  // Static members end with _ just like other members, but constants should
  // not.
  if (!is_const && decl.isStaticDataMember()) {
    name += '_';
  }

  return true;
}

bool GetNameForDecl(const clang::UsingDecl& decl,
                    const clang::ASTContext& context,
                    std::string& name) {
  assert(decl.shadow_size() > 0);

  // If a using declaration's targeted declaration is a set of overloaded
  // functions, it can introduce multiple shadowed declarations. Just using the
  // first one is OK, since overloaded functions have the same name, by
  // definition.
  clang::NamedDecl* shadowed_name = decl.shadow_begin()->getTargetDecl();
  // Note: CXXMethodDecl must be checked before FunctionDecl, because
  // CXXMethodDecl is derived from FunctionDecl.
  if (auto* method = clang::dyn_cast<clang::CXXMethodDecl>(shadowed_name))
    return GetNameForDecl(*method, context, name);
  if (auto* function = clang::dyn_cast<clang::FunctionDecl>(shadowed_name))
    return GetNameForDecl(*function, context, name);
  if (auto* var = clang::dyn_cast<clang::VarDecl>(shadowed_name))
    return GetNameForDecl(*var, context, name);
  if (auto* field = clang::dyn_cast<clang::FieldDecl>(shadowed_name))
    return GetNameForDecl(*field, context, name);

  return false;
}

template <typename Type>
struct TargetNodeTraits;

template <>
struct TargetNodeTraits<clang::NamedDecl> {
  static const char kName[];
  static clang::SourceLocation GetLoc(const clang::NamedDecl& decl) {
    return decl.getLocation();
  }
};
const char TargetNodeTraits<clang::NamedDecl>::kName[] = "decl";

template <>
struct TargetNodeTraits<clang::MemberExpr> {
  static const char kName[];
  static clang::SourceLocation GetLoc(const clang::MemberExpr& expr) {
    return expr.getMemberLoc();
  }
};
const char TargetNodeTraits<clang::MemberExpr>::kName[] = "expr";

template <>
struct TargetNodeTraits<clang::DeclRefExpr> {
  static const char kName[];
  static clang::SourceLocation GetLoc(const clang::DeclRefExpr& expr) {
    return expr.getLocation();
  }
};
const char TargetNodeTraits<clang::DeclRefExpr>::kName[] = "expr";

template <>
struct TargetNodeTraits<clang::CXXCtorInitializer> {
  static const char kName[];
  static clang::SourceLocation GetLoc(const clang::CXXCtorInitializer& init) {
    assert(init.isWritten());
    return init.getSourceLocation();
  }
};
const char TargetNodeTraits<clang::CXXCtorInitializer>::kName[] = "initializer";

template <typename DeclNode, typename TargetNode>
class RewriterBase : public MatchFinder::MatchCallback {
 public:
  explicit RewriterBase(Replacements* replacements)
      : replacements_(replacements) {}

  void run(const MatchFinder::MatchResult& result) override {
    const DeclNode* decl = result.Nodes.getNodeAs<DeclNode>("decl");
    // If the decl originates inside a macro, just skip it completely.
    clang::SourceLocation decl_loc =
        TargetNodeTraits<clang::NamedDecl>::GetLoc(*decl);
    if (decl_loc.isMacroID())
      return;
    // If false, there's no name to be renamed.
    if (!decl->getIdentifier())
      return;
    // If false, the name was not suitable for renaming.
    clang::ASTContext* context = result.Context;
    std::string new_name;
    if (!GetNameForDecl(*decl, *context, new_name))
      return;
    llvm::StringRef old_name = decl->getName();
    if (old_name == new_name)
      return;
    clang::SourceLocation loc = TargetNodeTraits<TargetNode>::GetLoc(
        *result.Nodes.getNodeAs<TargetNode>(
            TargetNodeTraits<TargetNode>::kName));
    clang::CharSourceRange range = clang::CharSourceRange::getTokenRange(loc);
    replacements_->emplace(*result.SourceManager, range, new_name);
    replacement_names_.emplace(old_name.str(), std::move(new_name));
  }

  const std::unordered_map<std::string, std::string>& replacement_names()
      const {
    return replacement_names_;
  }

 private:
  Replacements* const replacements_;
  std::unordered_map<std::string, std::string> replacement_names_;
};

using FieldDeclRewriter = RewriterBase<clang::FieldDecl, clang::NamedDecl>;
using VarDeclRewriter = RewriterBase<clang::VarDecl, clang::NamedDecl>;
using MemberRewriter = RewriterBase<clang::FieldDecl, clang::MemberExpr>;
using DeclRefRewriter = RewriterBase<clang::VarDecl, clang::DeclRefExpr>;
using FunctionDeclRewriter =
    RewriterBase<clang::FunctionDecl, clang::NamedDecl>;
using FunctionRefRewriter =
    RewriterBase<clang::FunctionDecl, clang::DeclRefExpr>;
using ConstructorInitializerRewriter =
    RewriterBase<clang::FieldDecl, clang::CXXCtorInitializer>;

using MethodDeclRewriter = RewriterBase<clang::CXXMethodDecl, clang::NamedDecl>;
using MethodRefRewriter =
    RewriterBase<clang::CXXMethodDecl, clang::DeclRefExpr>;
using MethodMemberRewriter =
    RewriterBase<clang::CXXMethodDecl, clang::MemberExpr>;

using UsingDeclRewriter = RewriterBase<clang::UsingDecl, clang::NamedDecl>;

}  // namespace

static llvm::cl::extrahelp common_help(CommonOptionsParser::HelpMessage);

int main(int argc, const char* argv[]) {
  // TODO(dcheng): Clang tooling should do this itself.
  // http://llvm.org/bugs/show_bug.cgi?id=21627
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  llvm::cl::OptionCategory category(
      "rewrite_to_chrome_style: convert Blink style to Chrome style.");
  CommonOptionsParser options(argc, argv, category);
  clang::tooling::ClangTool tool(options.getCompilations(),
                                 options.getSourcePathList());

  MatchFinder match_finder;
  Replacements replacements;

  auto in_blink_namespace =
      decl(hasAncestor(namespaceDecl(anyOf(hasName("blink"), hasName("WTF")),
                                     hasParent(translationUnitDecl()))));
  // The ^gen/ rule is used for production code, but the /gen/ one exists here
  // too for making testing easier.
  auto is_generated = decl(isExpansionInFileMatching("^gen/|/gen/"));

  // Field and variable declarations ========
  // Given
  //   int x;
  //   struct S {
  //     int y;
  //   };
  // matches |x| and |y|.
  auto field_decl_matcher =
      id("decl", fieldDecl(in_blink_namespace, unless(is_generated)));
  auto var_decl_matcher =
      id("decl", varDecl(in_blink_namespace, unless(is_generated)));

  FieldDeclRewriter field_decl_rewriter(&replacements);
  match_finder.addMatcher(field_decl_matcher, &field_decl_rewriter);

  VarDeclRewriter var_decl_rewriter(&replacements);
  match_finder.addMatcher(var_decl_matcher, &var_decl_rewriter);

  // Field and variable references ========
  // Given
  //   bool x = true;
  //   if (x) {
  //     ...
  //   }
  // matches |x| in if (x).
  auto member_matcher = id(
      "expr",
      memberExpr(
          member(field_decl_matcher),
          // Needed to avoid matching member references in functions (which will
          // be an ancestor of the member reference) synthesized by the
          // compiler, such as a synthesized copy constructor.
          // This skips explicitly defaulted functions as well, but that's OK:
          // there's nothing interesting to rewrite in those either.
          unless(hasAncestor(functionDecl(internal_hack::isDefaulted())))));
  auto decl_ref_matcher = id("expr", declRefExpr(to(var_decl_matcher)));

  MemberRewriter member_rewriter(&replacements);
  match_finder.addMatcher(member_matcher, &member_rewriter);

  DeclRefRewriter decl_ref_rewriter(&replacements);
  match_finder.addMatcher(decl_ref_matcher, &decl_ref_rewriter);

  // Non-method function declarations ========
  // Given
  //   void f();
  //   struct S {
  //     void g();
  //   };
  // matches |f| but not |g|.
  auto function_decl_matcher = id(
      "decl",
      functionDecl(
          unless(anyOf(
              // Methods are covered by the method matchers.
              cxxMethodDecl(),
              // Out-of-line overloaded operators have special names and should
              // never be renamed.
              isOverloadedOperator())),
          in_blink_namespace, unless(is_generated)));
  FunctionDeclRewriter function_decl_rewriter(&replacements);
  match_finder.addMatcher(function_decl_matcher, &function_decl_rewriter);

  // Non-method function references ========
  // Given
  //   f();
  //   void (*p)() = &f;
  // matches |f()| and |&f|.
  auto function_ref_matcher =
      id("expr", declRefExpr(to(function_decl_matcher)));
  FunctionRefRewriter function_ref_rewriter(&replacements);
  match_finder.addMatcher(function_ref_matcher, &function_ref_rewriter);

  // Method declarations ========
  // Given
  //   struct S {
  //     void g();
  //   };
  // matches |g|.
  auto method_decl_matcher =
      id("decl",
         cxxMethodDecl(isBlinkMethod(),
                       unless(anyOf(is_generated,
                                    // Overloaded operators have special names
                                    // and should never be renamed.
                                    isOverloadedOperator(),
                                    // Similarly, constructors, destructors, and
                                    // conversion functions should not be
                                    // considered for renaming.
                                    cxxConstructorDecl(), cxxDestructorDecl(),
                                    cxxConversionDecl()))));
  MethodDeclRewriter method_decl_rewriter(&replacements);
  match_finder.addMatcher(method_decl_matcher, &method_decl_rewriter);

  // Method references in a non-member context ========
  // Given
  //   S s;
  //   s.g();
  //   void (S::*p)() = &S::g;
  // matches |&S::g| but not |s.g()|.
  auto method_ref_matcher = id("expr", declRefExpr(to(method_decl_matcher)));

  MethodRefRewriter method_ref_rewriter(&replacements);
  match_finder.addMatcher(method_ref_matcher, &method_ref_rewriter);

  // Method references in a member context ========
  // Given
  //   S s;
  //   s.g();
  //   void (S::*p)() = &S::g;
  // matches |s.g()| but not |&S::g|.
  auto method_member_matcher =
      id("expr", memberExpr(member(method_decl_matcher)));

  MethodMemberRewriter method_member_rewriter(&replacements);
  match_finder.addMatcher(method_member_matcher, &method_member_rewriter);

  // Initializers ========
  // Given
  //   struct S {
  //     int x;
  //     S() : x(2) {}
  //   };
  // matches each initializer in the constructor for S.
  auto constructor_initializer_matcher =
      cxxConstructorDecl(forEachConstructorInitializer(
          id("initializer",
             cxxCtorInitializer(forField(field_decl_matcher), isWritten()))));

  ConstructorInitializerRewriter constructor_initializer_rewriter(
      &replacements);
  match_finder.addMatcher(constructor_initializer_matcher,
                          &constructor_initializer_rewriter);

  // Using declarations ========
  // Given
  //   using blink::X;
  // matches |using blink::X|.
  UsingDeclRewriter using_decl_rewriter(&replacements);
  match_finder.addMatcher(
      id("decl", usingDecl(hasAnyUsingShadowDecl(hasTargetDecl(
                     anyOf(var_decl_matcher, field_decl_matcher,
                           function_decl_matcher, method_decl_matcher))))),
      &using_decl_rewriter);

  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      clang::tooling::newFrontendActionFactory(&match_finder);
  int result = tool.run(factory.get());
  if (result != 0)
    return result;

#if defined(_WIN32)
  HANDLE lockfd = CreateFile("rewrite-sym.lock", GENERIC_READ, FILE_SHARE_READ,
                             NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  OVERLAPPED overlapped = {};
  LockFileEx(lockfd, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &overlapped);
#else
  int lockfd = open("rewrite-sym.lock", O_RDWR | O_CREAT, 0666);
  while (flock(lockfd, LOCK_EX)) {  // :D
  }
#endif

  std::ofstream replacement_db_file("rewrite-sym.txt",
                                    std::ios_base::out | std::ios_base::app);
  for (const auto& p : field_decl_rewriter.replacement_names())
    replacement_db_file << "var:" << p.first << ":" << p.second << "\n";
  for (const auto& p : var_decl_rewriter.replacement_names())
    replacement_db_file << "var:" << p.first << ":" << p.second << "\n";
  for (const auto& p : function_decl_rewriter.replacement_names())
    replacement_db_file << "fun:" << p.first << ":" << p.second << "\n";
  for (const auto& p : method_decl_rewriter.replacement_names())
    replacement_db_file << "fun:" << p.first << ":" << p.second << "\n";
  replacement_db_file.close();

#if defined(_WIN32)
  UnlockFileEx(lockfd, 0, 1, 0, &overlapped);
  CloseHandle(lockfd);
#else
  flock(lockfd, LOCK_UN);
  close(lockfd);
#endif

  // Serialization format is documented in tools/clang/scripts/run_tool.py
  llvm::outs() << "==== BEGIN EDITS ====\n";
  for (const auto& r : replacements) {
    std::string replacement_text = r.getReplacementText().str();
    std::replace(replacement_text.begin(), replacement_text.end(), '\n', '\0');
    llvm::outs() << "r:::" << r.getFilePath() << ":::" << r.getOffset()
                 << ":::" << r.getLength() << ":::" << replacement_text << "\n";
  }
  llvm::outs() << "==== END EDITS ====\n";

  return 0;
}
