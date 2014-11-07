// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/string_split.h"
#include "tools/gn/commands.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parser.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/setup.h"
#include "tools/gn/source_file.h"
#include "tools/gn/tokenizer.h"

namespace commands {

const char kSwitchDumpTree[] = "dump-tree";
const char kSwitchInPlace[] = "in-place";
const char kSwitchStdin[] = "stdin";

const char kFormat[] = "format";
const char kFormat_HelpShort[] =
    "format: Format .gn file. (ALPHA, WILL DESTROY DATA!)";
const char kFormat_Help[] =
    "gn format [--dump-tree] [--in-place] [--stdin] BUILD.gn\n"
    "\n"
    "  Formats .gn file to a standard format. THIS IS NOT FULLY IMPLEMENTED\n"
    "  YET! IT WILL EAT YOUR BEAUTIFUL .GN FILES. AND YOUR LAUNDRY.\n"
    "  At a minimum, make sure everything is `git commit`d so you can\n"
    "  `git checkout -f` to recover.\n"
    "\n"
    "Arguments\n"
    "  --dump-tree\n"
    "      For debugging only, dumps the parse tree.\n"
    "\n"
    "  --in-place\n"
    "      Instead writing the formatted file to stdout, replace the input\n"
    "      with the formatted output.\n"
    "\n"
    "  --stdin\n"
    "      Read input from stdin (and write to stdout). Not compatible with\n"
    "      --in-place of course.\n"
    "\n"
    "Examples\n"
    "  gn format //some/BUILD.gn\n"
    "  gn format some\\BUILD.gn\n"
    "  gn format /abspath/some/BUILD.gn\n"
    "  gn format --stdin\n";

namespace {

const int kIndentSize = 2;
const int kMaximumWidth = 80;

enum Precedence {
  kPrecedenceLowest,
  kPrecedenceAssign,
  kPrecedenceOr,
  kPrecedenceAnd,
  kPrecedenceCompare,
  kPrecedenceAdd,
  kPrecedenceSuffix,
  kPrecedenceUnary,
};

class Printer {
 public:
  Printer();
  ~Printer();

  void Block(const ParseNode* file);

  std::string String() const { return output_; }

 private:
  // Format a list of values using the given style.
  enum SequenceStyle {
    kSequenceStyleList,
    kSequenceStyleBlock,
    kSequenceStyleBracedBlock,
  };

  enum ExprStyle {
    kExprStyleRegular,
    kExprStyleComment,
  };

  struct Metrics {
    Metrics() : first_length(-1), longest_length(-1), multiline(false) {}
    int first_length;
    int longest_length;
    bool multiline;
  };

  // Add to output.
  void Print(base::StringPiece str);

  // Add the current margin (as spaces) to the output.
  void PrintMargin();

  void TrimAndPrintToken(const Token& token);

  // End the current line, flushing end of line comments.
  void Newline();

  // Remove trailing spaces from the current line.
  void Trim();

  // Whether there's a blank separator line at the current position.
  bool HaveBlankLine();

  bool IsAssignment(const ParseNode* node);

  // Heuristics to decide if there should be a blank line added between two
  // items. For various "small" items, it doesn't look nice if there's too much
  // vertical whitespace added.
  bool ShouldAddBlankLineInBetween(const ParseNode* a, const ParseNode* b);

  // Get the 0-based x position on the current line.
  int CurrentColumn();

  // Adds an opening ( if prec is less than the outers (to maintain evalution
  // order for a subexpression). If an opening paren is emitted, *parenthesized
  // will be set so it can be closed at the end of the expression.
  void AddParen(int prec, int outer_prec, bool* parenthesized);

  // Print the expression to the output buffer. Returns the type of element
  // added to the output. The value of outer_prec gives the precedence of the
  // operator outside this Expr. If that operator binds tighter than root's,
  // Expr must introduce parentheses.
  ExprStyle Expr(const ParseNode* root, int outer_prec);

  // Use a sub-Printer recursively to figure out the size that an expression
  // would be before actually adding it to the output.
  Metrics GetLengthOfExpr(const ParseNode* expr, int outer_prec);

  // Format a list of values using the given style.
  // |end| holds any trailing comments to be printed just before the closing
  // bracket.
  template <class PARSENODE>  // Just for const covariance.
  void Sequence(SequenceStyle style,
                const std::vector<PARSENODE*>& list,
                const ParseNode* end);

  void FunctionCall(const FunctionCallNode* func_call);

  std::string output_;           // Output buffer.
  std::vector<Token> comments_;  // Pending end-of-line comments.
  int margin_;                   // Left margin (number of spaces).

  // Gives the precedence for operators in a BinaryOpNode.
  std::map<base::StringPiece, Precedence> precedence_;

  DISALLOW_COPY_AND_ASSIGN(Printer);
};

Printer::Printer() : margin_(0) {
  output_.reserve(100 << 10);
  precedence_["="] = kPrecedenceAssign;
  precedence_["+="] = kPrecedenceAssign;
  precedence_["-="] = kPrecedenceAssign;
  precedence_["||"] = kPrecedenceOr;
  precedence_["&&"] = kPrecedenceAnd;
  precedence_["<"] = kPrecedenceCompare;
  precedence_[">"] = kPrecedenceCompare;
  precedence_["=="] = kPrecedenceCompare;
  precedence_["!="] = kPrecedenceCompare;
  precedence_["<="] = kPrecedenceCompare;
  precedence_[">="] = kPrecedenceCompare;
  precedence_["+"] = kPrecedenceAdd;
  precedence_["-"] = kPrecedenceAdd;
  precedence_["!"] = kPrecedenceUnary;
}

Printer::~Printer() {
}

void Printer::Print(base::StringPiece str) {
  str.AppendToString(&output_);
}

void Printer::PrintMargin() {
  output_ += std::string(margin_, ' ');
}

void Printer::TrimAndPrintToken(const Token& token) {
  std::string trimmed;
  TrimWhitespaceASCII(token.value().as_string(), base::TRIM_ALL, &trimmed);
  Print(trimmed);
}

void Printer::Newline() {
  if (!comments_.empty()) {
    Print("  ");
    int i = 0;
    // Save the margin, and temporarily set it to where the first comment
    // starts so that multiple suffix comments are vertically aligned. This
    // will need to be fancier once we enforce 80 col.
    int old_margin = margin_;
    for (const auto& c : comments_) {
      if (i == 0)
        margin_ = CurrentColumn();
      else {
        Trim();
        Print("\n");
        PrintMargin();
      }
      TrimAndPrintToken(c);
      ++i;
    }
    margin_ = old_margin;
    comments_.clear();
  }
  Trim();
  Print("\n");
  PrintMargin();
}

void Printer::Trim() {
  size_t n = output_.size();
  while (n > 0 && output_[n - 1] == ' ')
    --n;
  output_.resize(n);
}

bool Printer::HaveBlankLine() {
  size_t n = output_.size();
  while (n > 0 && output_[n - 1] == ' ')
    --n;
  return n > 2 && output_[n - 1] == '\n' && output_[n - 2] == '\n';
}

bool Printer::IsAssignment(const ParseNode* node) {
  return node->AsBinaryOp() && (node->AsBinaryOp()->op().value() == "=" ||
                                node->AsBinaryOp()->op().value() == "+=" ||
                                node->AsBinaryOp()->op().value() == "-=");
}

bool Printer::ShouldAddBlankLineInBetween(const ParseNode* a,
                                          const ParseNode* b) {
  LocationRange a_range = a->GetRange();
  LocationRange b_range = b->GetRange();
  // If they're already separated by 1 or more lines, then we want to keep a
  // blank line.
  return b_range.begin().line_number() > a_range.end().line_number() + 1;
}

int Printer::CurrentColumn() {
  int n = 0;
  while (n < static_cast<int>(output_.size()) &&
         output_[output_.size() - 1 - n] != '\n') {
    ++n;
  }
  return n;
}

void Printer::Block(const ParseNode* root) {
  const BlockNode* block = root->AsBlock();

  if (block->comments()) {
    for (const auto& c : block->comments()->before()) {
      TrimAndPrintToken(c);
      Newline();
    }
  }

  size_t i = 0;
  for (const auto& stmt : block->statements()) {
    Expr(stmt, kPrecedenceLowest);
    Newline();
    if (stmt->comments()) {
      // Why are before() not printed here too? before() are handled inside
      // Expr(), as are suffix() which are queued to the next Newline().
      // However, because it's a general expression handler, it doesn't insert
      // the newline itself, which only happens between block statements. So,
      // the after are handled explicitly here.
      for (const auto& c : stmt->comments()->after()) {
        TrimAndPrintToken(c);
        Newline();
      }
    }
    if (i < block->statements().size() - 1 &&
        (ShouldAddBlankLineInBetween(block->statements()[i],
                                     block->statements()[i + 1]))) {
      Newline();
    }
    ++i;
  }

  if (block->comments()) {
    for (const auto& c : block->comments()->after()) {
      TrimAndPrintToken(c);
      Newline();
    }
  }
}

Printer::Metrics Printer::GetLengthOfExpr(const ParseNode* expr,
                                          int outer_prec) {
  Metrics result;
  Printer sub;
  sub.Expr(expr, outer_prec);
  std::vector<std::string> lines;
  base::SplitStringDontTrim(sub.String(), '\n', &lines);
  result.multiline = lines.size() > 1;
  result.first_length = static_cast<int>(lines[0].size());
  for (const auto& line : lines) {
    result.longest_length =
        std::max(result.longest_length, static_cast<int>(line.size()));
  }
  return result;
}

void Printer::AddParen(int prec, int outer_prec, bool* parenthesized) {
  if (prec < outer_prec) {
    Print("(");
    *parenthesized = true;
  }
}

Printer::ExprStyle Printer::Expr(const ParseNode* root, int outer_prec) {
  ExprStyle result = kExprStyleRegular;
  if (root->comments()) {
    if (!root->comments()->before().empty()) {
      Trim();
      // If there's already other text on the line, start a new line.
      if (CurrentColumn() > 0)
        Print("\n");
      // We're printing a line comment, so we need to be at the current margin.
      PrintMargin();
      for (const auto& c : root->comments()->before()) {
        TrimAndPrintToken(c);
        Newline();
      }
    }
  }

  bool parenthesized = false;

  if (const AccessorNode* accessor = root->AsAccessor()) {
    AddParen(kPrecedenceSuffix, outer_prec, &parenthesized);
    Print(accessor->base().value());
    if (accessor->member()) {
      Print(".");
      Expr(accessor->member(), kPrecedenceLowest);
    } else {
      CHECK(accessor->index());
      Print("[");
      Expr(accessor->index(), kPrecedenceLowest);
      Print("]");
    }
  } else if (const BinaryOpNode* binop = root->AsBinaryOp()) {
    CHECK(precedence_.find(binop->op().value()) != precedence_.end());
    Precedence prec = precedence_[binop->op().value()];
    AddParen(prec, outer_prec, &parenthesized);
    Metrics right = GetLengthOfExpr(binop->right(), prec + 1);
    int op_length = static_cast<int>(binop->op().value().size()) + 2;
    Expr(binop->left(), prec);
    if (CurrentColumn() + op_length + right.first_length <= kMaximumWidth) {
      // If it just fits normally, put it here.
      Print(" ");
      Print(binop->op().value());
      Print(" ");
      Expr(binop->right(), prec + 1);
    } else {
      // Otherwise, put first argument and op, and indent next.
      Print(" ");
      Print(binop->op().value());
      int old_margin = margin_;
      margin_ += kIndentSize * 2;
      Newline();
      Expr(binop->right(), prec + 1);
      margin_ = old_margin;
    }
  } else if (const BlockNode* block = root->AsBlock()) {
    Sequence(kSequenceStyleBracedBlock, block->statements(), block->End());
  } else if (const ConditionNode* condition = root->AsConditionNode()) {
    Print("if (");
    Expr(condition->condition(), kPrecedenceLowest);
    Print(") ");
    Sequence(kSequenceStyleBracedBlock,
             condition->if_true()->statements(),
             condition->if_true()->End());
    if (condition->if_false()) {
      Print(" else ");
      // If it's a block it's a bare 'else', otherwise it's an 'else if'. See
      // ConditionNode::Execute.
      bool is_else_if = condition->if_false()->AsBlock() == NULL;
      if (is_else_if) {
        Expr(condition->if_false(), kPrecedenceLowest);
      } else {
        Sequence(kSequenceStyleBracedBlock,
                 condition->if_false()->AsBlock()->statements(),
                 condition->if_false()->AsBlock()->End());
      }
    }
  } else if (const FunctionCallNode* func_call = root->AsFunctionCall()) {
    FunctionCall(func_call);
  } else if (const IdentifierNode* identifier = root->AsIdentifier()) {
    Print(identifier->value().value());
  } else if (const ListNode* list = root->AsList()) {
    Sequence(kSequenceStyleList, list->contents(), list->End());
  } else if (const LiteralNode* literal = root->AsLiteral()) {
    // TODO(scottmg): Quoting?
    Print(literal->value().value());
  } else if (const UnaryOpNode* unaryop = root->AsUnaryOp()) {
    Print(unaryop->op().value());
    Expr(unaryop->operand(), kPrecedenceUnary);
  } else if (const BlockCommentNode* block_comment = root->AsBlockComment()) {
    Print(block_comment->comment().value());
    result = kExprStyleComment;
  } else if (const EndNode* end = root->AsEnd()) {
    Print(end->value().value());
  } else {
    CHECK(false) << "Unhandled case in Expr.";
  }

  if (parenthesized)
    Print(")");

  // Defer any end of line comment until we reach the newline.
  if (root->comments() && !root->comments()->suffix().empty()) {
    std::copy(root->comments()->suffix().begin(),
              root->comments()->suffix().end(),
              std::back_inserter(comments_));
  }

  return result;
}

template <class PARSENODE>
void Printer::Sequence(SequenceStyle style,
                       const std::vector<PARSENODE*>& list,
                       const ParseNode* end) {
  bool force_multiline = false;
  if (style == kSequenceStyleList)
    Print("[");
  else if (style == kSequenceStyleBracedBlock)
    Print("{");

  if (style == kSequenceStyleBlock || style == kSequenceStyleBracedBlock)
    force_multiline = true;

  if (end && end->comments() && !end->comments()->before().empty())
    force_multiline = true;

  // If there's before line comments, make sure we have a place to put them.
  for (const auto& i : list) {
    if (i->comments() && !i->comments()->before().empty())
      force_multiline = true;
  }

  if (list.size() == 0 && !force_multiline) {
    // No elements, and not forcing newlines, print nothing.
  } else if (list.size() == 1 && !force_multiline) {
    Print(" ");
    Expr(list[0], kPrecedenceLowest);
    CHECK(!list[0]->comments() || list[0]->comments()->after().empty());
    Print(" ");
  } else {
    margin_ += kIndentSize;
    size_t i = 0;
    for (const auto& x : list) {
      Newline();
      // If:
      // - we're going to output some comments, and;
      // - we haven't just started this multiline list, and;
      // - there isn't already a blank line here;
      // Then: insert one.
      if (i != 0 && x->comments() && !x->comments()->before().empty() &&
          !HaveBlankLine()) {
        Newline();
      }
      ExprStyle expr_style = Expr(x, kPrecedenceLowest);
      CHECK(!x->comments() || x->comments()->after().empty());
      if (i < list.size() - 1 || style == kSequenceStyleList) {
        if (style == kSequenceStyleList && expr_style == kExprStyleRegular) {
          Print(",");
        } else {
          if (i < list.size() - 1 &&
              ShouldAddBlankLineInBetween(list[i], list[i + 1]))
            Newline();
        }
      }
      ++i;
    }

    // Trailing comments.
    if (end->comments()) {
      if (list.size() >= 2)
        Newline();
      for (const auto& c : end->comments()->before()) {
        Newline();
        TrimAndPrintToken(c);
      }
    }

    margin_ -= kIndentSize;
    Newline();

    // Defer any end of line comment until we reach the newline.
    if (end->comments() && !end->comments()->suffix().empty()) {
      std::copy(end->comments()->suffix().begin(),
        end->comments()->suffix().end(),
        std::back_inserter(comments_));
    }
  }

  if (style == kSequenceStyleList)
    Print("]");
  else if (style == kSequenceStyleBracedBlock)
    Print("}");
}

void Printer::FunctionCall(const FunctionCallNode* func_call) {
  Print(func_call->function().value());
  Print("(");

  int old_margin = margin_;
  bool have_block = func_call->block() != nullptr;
  bool force_multiline = false;

  const std::vector<const ParseNode*>& list = func_call->args()->contents();
  const ParseNode* end = func_call->args()->End();

  if (end && end->comments() && !end->comments()->before().empty())
    force_multiline = true;

  // If there's before line comments, make sure we have a place to put them.
  for (const auto& i : list) {
    if (i->comments() && !i->comments()->before().empty())
      force_multiline = true;
  }

  // Calculate the length of the items for function calls so we can decide to
  // compress them in various nicer ways.
  std::vector<int> natural_lengths;
  bool fits_on_current_line = true;
  int max_item_width = 0;
  int total_length = 0;
  natural_lengths.reserve(list.size());
  std::string terminator = ")";
  if (have_block)
    terminator += " {";
  for (size_t i = 0; i < list.size(); ++i) {
    Metrics sub = GetLengthOfExpr(list[i], kPrecedenceLowest);
    if (sub.multiline)
      fits_on_current_line = false;
    natural_lengths.push_back(sub.longest_length);
    total_length += sub.longest_length;
    if (i < list.size() - 1) {
      total_length += static_cast<int>(strlen(", "));
    }
  }
  fits_on_current_line =
      fits_on_current_line &&
      CurrentColumn() + total_length + terminator.size() <= kMaximumWidth;
  if (natural_lengths.size() > 0) {
    max_item_width =
        *std::max_element(natural_lengths.begin(), natural_lengths.end());
  }

  if (list.size() == 0 && !force_multiline) {
    // No elements, and not forcing newlines, print nothing.
  } else if (list.size() == 1 && !force_multiline && fits_on_current_line) {
    Expr(list[0], kPrecedenceLowest);
    CHECK(!list[0]->comments() || list[0]->comments()->after().empty());
  } else {
    // Function calls get to be single line even with multiple arguments, if
    // they fit inside the maximum width.
    if (!force_multiline && fits_on_current_line) {
      for (size_t i = 0; i < list.size(); ++i) {
        Expr(list[i], kPrecedenceLowest);
        if (i < list.size() - 1)
          Print(", ");
      }
    } else {
      bool should_break_to_next_line = true;
      int indent = kIndentSize * 2;
      if (CurrentColumn() + max_item_width + terminator.size() <=
              kMaximumWidth ||
          CurrentColumn() < margin_ + indent) {
        should_break_to_next_line = false;
        margin_ = CurrentColumn();
      } else {
        margin_ += indent;
      }
      size_t i = 0;
      for (const auto& x : list) {
        // Function calls where all the arguments would fit at the current
        // position should do that instead of going back to margin+4.
        if (i > 0 || should_break_to_next_line)
          Newline();
        ExprStyle expr_style = Expr(x, kPrecedenceLowest);
        CHECK(!x->comments() || x->comments()->after().empty());
        if (i < list.size() - 1) {
          if (expr_style == kExprStyleRegular) {
            Print(",");
          } else {
            Newline();
          }
        }
        ++i;
      }

      // Trailing comments.
      if (end->comments()) {
        if (!list.empty())
          Newline();
        for (const auto& c : end->comments()->before()) {
          Newline();
          TrimAndPrintToken(c);
        }
        if (!end->comments()->before().empty())
          Newline();
      }
    }
  }

  // Defer any end of line comment until we reach the newline.
  if (end->comments() && !end->comments()->suffix().empty()) {
    std::copy(end->comments()->suffix().begin(),
              end->comments()->suffix().end(),
              std::back_inserter(comments_));
  }

  Print(")");
  margin_ = old_margin;

  if (have_block) {
    Print(" ");
    Sequence(kSequenceStyleBracedBlock,
             func_call->block()->statements(),
             func_call->block()->End());
  }
}

void DoFormat(const ParseNode* root, bool dump_tree, std::string* output) {
  if (dump_tree) {
    std::ostringstream os;
    root->Print(os, 0);
    printf("----------------------\n");
    printf("-- PARSE TREE --------\n");
    printf("----------------------\n");
    printf("%s", os.str().c_str());
    printf("----------------------\n");
  }
  Printer pr;
  pr.Block(root);
  *output = pr.String();
}

std::string ReadStdin() {
  static const int kBufferSize = 256;
  char buffer[kBufferSize];
  std::string result;
  while (true) {
    char* input = NULL;
    input = fgets(buffer, kBufferSize, stdin);
    if (input == NULL && feof(stdin))
      return result;
    int length = static_cast<int>(strlen(buffer));
    if (length == 0)
      return result;
    else
      result += std::string(buffer, length);
  }
}

}  // namespace

bool FormatFileToString(Setup* setup,
                        const SourceFile& file,
                        bool dump_tree,
                        std::string* output) {
  Err err;
  const ParseNode* parse_node =
      setup->scheduler().input_file_manager()->SyncLoadFile(
          LocationRange(), &setup->build_settings(), file, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }
  DoFormat(parse_node, dump_tree, output);
  return true;
}

bool FormatStringToString(const std::string& input,
                          bool dump_tree,
                          std::string* output) {
  SourceFile source_file;
  InputFile file(source_file);
  file.SetContents(input);
  Err err;
  // Tokenize.
  std::vector<Token> tokens = Tokenizer::Tokenize(&file, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  // Parse.
  scoped_ptr<ParseNode> parse_node = Parser::Parse(tokens, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  DoFormat(parse_node.get(), dump_tree, output);
  return true;
}

int RunFormat(const std::vector<std::string>& args) {
  bool dump_tree =
      base::CommandLine::ForCurrentProcess()->HasSwitch(kSwitchDumpTree);

  bool from_stdin =
      base::CommandLine::ForCurrentProcess()->HasSwitch(kSwitchStdin);

  if (from_stdin) {
    if (args.size() != 0) {
      Err(Location(), "Expecting no arguments when reading from stdin.\n")
          .PrintToStdout();
      return 1;
    }
    std::string input = ReadStdin();
    std::string output;
    if (!FormatStringToString(input, dump_tree, &output))
      return 1;
    printf("%s", output.c_str());
    return 0;
  }

  // TODO(scottmg): Eventually, this should be a list/spec of files, and they
  // should all be done in parallel.
  if (args.size() != 1) {
    Err(Location(), "Expecting exactly one argument, see `gn help format`.\n")
        .PrintToStdout();
    return 1;
  }

  Setup setup;
  SourceDir source_dir =
      SourceDirForCurrentDirectory(setup.build_settings().root_path());
  SourceFile file = source_dir.ResolveRelativeFile(args[0]);

  std::string output_string;
  if (FormatFileToString(&setup, file, dump_tree, &output_string)) {
    bool in_place =
        base::CommandLine::ForCurrentProcess()->HasSwitch(kSwitchInPlace);
    if (in_place) {
      base::FilePath to_write = setup.build_settings().GetFullPath(file);
      if (base::WriteFile(to_write,
                          output_string.data(),
                          static_cast<int>(output_string.size())) == -1) {
        Err(Location(),
            std::string("Failed to write formatted output back to \"") +
                to_write.AsUTF8Unsafe() + std::string("\".")).PrintToStdout();
        return 1;
      }
      printf("Wrote formatted to '%s'.\n", to_write.AsUTF8Unsafe().c_str());
    } else {
      printf("%s", output_string.c_str());
    }
  }

  return 0;
}

}  // namespace commands
