// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "tools/gn/commands.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parser.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/setup.h"
#include "tools/gn/source_file.h"
#include "tools/gn/tokenizer.h"

namespace commands {

const char kSwitchDumpTree[] = "dump-tree";

const char kFormat[] = "format";
const char kFormat_HelpShort[] =
    "format: Format .gn file.";
const char kFormat_Help[] =
    "gn format: Format .gn file. (ALPHA, WILL CURRENTLY DESTROY DATA!)\n"
    "\n"
    "  gn format //some/BUILD.gn\n"
    "  gn format some\\BUILD.gn\n"
    "\n"
    "  Formats .gn file to a standard format. THIS IS NOT FULLY IMPLEMENTED\n"
    "  YET! IT WILL EAT YOUR BEAUTIFUL .GN FILES. AND YOUR LAUNDRY.\n"
    "  At a minimum, make sure everything is `git commit`d so you can\n"
    "  `git checkout -f` to recover.\n";

namespace {

const int kIndentSize = 2;
const int kMaximumWidth = 80;

class Printer {
 public:
  Printer();
  ~Printer();

  void Block(const ParseNode* file);

  std::string String() const { return output_; }

 private:
  // Format a list of values using the given style.
  enum SequenceStyle {
    kSequenceStyleFunctionCall,
    kSequenceStyleList,
    kSequenceStyleBlock,
    kSequenceStyleBracedBlock,
  };

  enum ExprStyle {
    kExprStyleRegular,
    kExprStyleComment,
  };

  struct Metrics {
    Metrics() : length(-1), multiline(false) {}
    int length;
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

  // Get the 0-based x position on the current line.
  int CurrentColumn();

  // Print the expression to the output buffer. Returns the type of element
  // added to the output.
  ExprStyle Expr(const ParseNode* root);

  // Use a sub-Printer recursively to figure out the size that an expression
  // would be before actually adding it to the output.
  Metrics GetLengthOfExpr(const ParseNode* expr);

  // Format a list of values using the given style.
  // |end| holds any trailing comments to be printed just before the closing
  // bracket.
  template <class PARSENODE>  // Just for const covariance.
  void Sequence(SequenceStyle style,
                const std::vector<PARSENODE*>& list,
                const ParseNode* end);

  std::string output_;           // Output buffer.
  std::vector<Token> comments_;  // Pending end-of-line comments.
  int margin_;                   // Left margin (number of spaces).

  DISALLOW_COPY_AND_ASSIGN(Printer);
};

Printer::Printer() : margin_(0) {
  output_.reserve(100 << 10);
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
    Expr(stmt);
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
    if (i < block->statements().size() - 1)
      Newline();
    ++i;
  }

  if (block->comments()) {
    for (const auto& c : block->comments()->after()) {
      TrimAndPrintToken(c);
      Newline();
    }
  }
}

Printer::Metrics Printer::GetLengthOfExpr(const ParseNode* expr) {
  Metrics result;
  Printer sub;
  sub.Expr(expr);
  std::vector<std::string> lines;
  base::SplitStringDontTrim(sub.String(), '\n', &lines);
  result.multiline = lines.size() > 1;
  for (const auto& line : lines)
    result.length = std::max(result.length, static_cast<int>(line.size()));
  return result;
}

Printer::ExprStyle Printer::Expr(const ParseNode* root) {
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

  if (const AccessorNode* accessor = root->AsAccessor()) {
    Print(accessor->base().value());
    if (accessor->member()) {
      Print(".");
      Expr(accessor->member());
    } else {
      CHECK(accessor->index());
      Print("[");
      Expr(accessor->index());
      Print("]");
    }
  } else if (const BinaryOpNode* binop = root->AsBinaryOp()) {
    // TODO(scottmg): Lots to do here for complex if expressions: reflowing,
    // parenthesizing, etc.
    Metrics left = GetLengthOfExpr(binop->left());
    Metrics right = GetLengthOfExpr(binop->right());
    int total_width = left.length +
                      static_cast<int>(binop->op().value().size()) + 2 +
                      right.length;
    if (CurrentColumn() + total_width < kMaximumWidth ||
        binop->right()->AsList()) {
      // If it just fits normally, put it here.
      Expr(binop->left());
      Print(" ");
      Print(binop->op().value());
      Print(" ");
      Expr(binop->right());
    } else {
      // Otherwise, put first argument and op, and indent next.
      Expr(binop->left());
      Print(" ");
      Print(binop->op().value());
      int old_margin = margin_;
      margin_ += kIndentSize * 2;
      Newline();
      Expr(binop->right());
      margin_ = old_margin;
    }
  } else if (const BlockNode* block = root->AsBlock()) {
    Sequence(kSequenceStyleBracedBlock, block->statements(), block->End());
  } else if (const ConditionNode* condition = root->AsConditionNode()) {
    Print("if (");
    Expr(condition->condition());
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
        Expr(condition->if_false());
      } else {
        Sequence(kSequenceStyleBracedBlock,
                 condition->if_false()->AsBlock()->statements(),
                 condition->if_false()->AsBlock()->End());
      }
    }
  } else if (const FunctionCallNode* func_call = root->AsFunctionCall()) {
    Print(func_call->function().value());
    Sequence(kSequenceStyleFunctionCall,
             func_call->args()->contents(),
             func_call->args()->End());
    if (func_call->block()) {
      Print(" ");
      Sequence(kSequenceStyleBracedBlock,
               func_call->block()->statements(),
               func_call->block()->End());
    }
  } else if (const IdentifierNode* identifier = root->AsIdentifier()) {
    Print(identifier->value().value());
  } else if (const ListNode* list = root->AsList()) {
    Sequence(kSequenceStyleList, list->contents(), list->End());
  } else if (const LiteralNode* literal = root->AsLiteral()) {
    // TODO(scottmg): Quoting?
    Print(literal->value().value());
  } else if (const UnaryOpNode* unaryop = root->AsUnaryOp()) {
    Print(unaryop->op().value());
    Expr(unaryop->operand());
  } else if (const BlockCommentNode* block_comment = root->AsBlockComment()) {
    Print(block_comment->comment().value());
    result = kExprStyleComment;
  } else if (const EndNode* end = root->AsEnd()) {
    Print(end->value().value());
  } else {
    CHECK(false) << "Unhandled case in Expr.";
  }

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
  int old_margin = margin_;
  int indent =
      style == kSequenceStyleFunctionCall ? kIndentSize * 2 : kIndentSize;
  bool force_multiline = false;
  if (style == kSequenceStyleFunctionCall)
    Print("(");
  else if (style == kSequenceStyleList)
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

  // Calculate the length of the items for function calls so we can decide to
  // compress them in various nicer ways.
  std::vector<int> natural_lengths;
  bool fits_on_current_line = true;
  int max_item_width = 0;
  if (style == kSequenceStyleFunctionCall) {
    int total_length = 0;
    natural_lengths.reserve(list.size());
    for (size_t i = 0; i < list.size(); ++i) {
      Metrics sub = GetLengthOfExpr(list[i]);
      if (sub.multiline)
        fits_on_current_line = false;
      natural_lengths.push_back(sub.length);
      total_length += sub.length;
      if (i < list.size() - 1)
        total_length += 2;  // ", "
    }
    // Strictly less than kMaximumWidth so there's room for closing ).
    // TODO(scottmg): Need to know if there's an attached block for " {".
    fits_on_current_line =
        fits_on_current_line && CurrentColumn() + total_length < kMaximumWidth;
    max_item_width =
        *std::max_element(natural_lengths.begin(), natural_lengths.end());
  }

  if (list.size() == 0 && !force_multiline) {
    // No elements, and not forcing newlines, print nothing.
  } else if (list.size() == 1 && !force_multiline && fits_on_current_line) {
    if (style != kSequenceStyleFunctionCall)
      Print(" ");
    Expr(list[0]);
    CHECK(!list[0]->comments() || list[0]->comments()->after().empty());
    if (style != kSequenceStyleFunctionCall)
      Print(" ");
  } else {
    // Function calls get to be single line even with multiple arguments, if
    // they fit inside the maximum width.
    if (style == kSequenceStyleFunctionCall && !force_multiline &&
        fits_on_current_line) {
      for (size_t i = 0; i < list.size(); ++i) {
        Expr(list[i]);
        if (i < list.size() - 1)
          Print(", ");
      }
    } else {
      bool should_break_to_next_line = true;
      if (style == kSequenceStyleFunctionCall &&
          (CurrentColumn() + max_item_width < kMaximumWidth ||
           CurrentColumn() < margin_ + indent)) {
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
        // If:
        // - we're going to output some comments, and;
        // - we haven't just started this multiline list, and;
        // - there isn't already a blank line here;
        // Then: insert one.
        if (i != 0 && x->comments() && !x->comments()->before().empty() &&
            !HaveBlankLine()) {
          Newline();
        }
        ExprStyle expr_style = Expr(x);
        CHECK(!x->comments() || x->comments()->after().empty());
        if (i < list.size() - 1 || style == kSequenceStyleList) {
          if ((style == kSequenceStyleList ||
               style == kSequenceStyleFunctionCall) &&
              expr_style == kExprStyleRegular) {
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
      }

      if (style == kSequenceStyleFunctionCall) {
        if (end->comments() && !end->comments()->before().empty()) {
          Newline();
        }
      } else {
        margin_ = old_margin;
        Newline();
      }
    }
  }

  if (style == kSequenceStyleFunctionCall)
    Print(")");
  else if (style == kSequenceStyleList)
    Print("]");
  else if (style == kSequenceStyleBracedBlock)
    Print("}");

  margin_ = old_margin;
}

}  // namespace

bool FormatFileToString(const std::string& input_filename,
                        bool dump_tree,
                        std::string* output) {
  Setup setup;
  Err err;
  SourceFile input_file(input_filename);
  const ParseNode* parse_node =
      setup.scheduler().input_file_manager()->SyncLoadFile(
          LocationRange(), &setup.build_settings(), input_file, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }
  if (dump_tree) {
    std::ostringstream os;
    parse_node->Print(os, 0);
    printf("----------------------\n");
    printf("-- PARSE TREE --------\n");
    printf("----------------------\n");
    printf("%s", os.str().c_str());
    printf("----------------------\n");
  }
  Printer pr;
  pr.Block(parse_node);
  *output = pr.String();
  return true;
}

int RunFormat(const std::vector<std::string>& args) {
  // TODO(scottmg): Eventually, this should be a list/spec of files, and they
  // should all be done in parallel and in-place. For now, we don't want to
  // overwrite good data with mistakenly reformatted stuff, so we just simply
  // print the formatted output to stdout.
  if (args.size() != 1) {
    Err(Location(), "Expecting exactly one argument, see `gn help format`.\n")
        .PrintToStdout();
    return 1;
  }

  bool dump_tree =
      base::CommandLine::ForCurrentProcess()->HasSwitch(kSwitchDumpTree);

  std::string input_name = args[0];
  if (input_name[0] != '/') {
    std::replace(input_name.begin(), input_name.end(), '\\', '/');
    input_name = "//" + input_name;
  }
  std::string output_string;
  if (FormatFileToString(input_name, dump_tree, &output_string)) {
    printf("%s", output_string.c_str());
  }

  return 0;
}

}  // namespace commands
