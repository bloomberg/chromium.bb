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

const int kPenaltyLineBreak = 500;
const int kPenaltyHorizontalSeparation = 100;
const int kPenaltyExcess = 10000;
const int kPenaltyBrokenLineOnOneLiner = 1000;

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

  // Flag assignments to sources, deps, etc. to make their RHSs multiline.
  void AnnotatePreferedMultilineAssignment(const BinaryOpNode* binop);

  // Heuristics to decide if there should be a blank line added between two
  // items. For various "small" items, it doesn't look nice if there's too much
  // vertical whitespace added.
  bool ShouldAddBlankLineInBetween(const ParseNode* a, const ParseNode* b);

  // Get the 0-based x position on the current line.
  int CurrentColumn() const;

  // Get the current line in the output;
  int CurrentLine() const;

  // Adds an opening ( if prec is less than the outers (to maintain evalution
  // order for a subexpression). If an opening paren is emitted, *parenthesized
  // will be set so it can be closed at the end of the expression.
  void AddParen(int prec, int outer_prec, bool* parenthesized);

  // Print the expression to the output buffer. Returns the type of element
  // added to the output. The value of outer_prec gives the precedence of the
  // operator outside this Expr. If that operator binds tighter than root's,
  // Expr must introduce parentheses.
  int Expr(const ParseNode* root, int outer_prec, const std::string& suffix);

  // Generic penalties for exceeding maximum width, adding more lines, etc.
  int AssessPenalty(const std::string& output);

  // Format a list of values using the given style.
  // |end| holds any trailing comments to be printed just before the closing
  // bracket.
  template <class PARSENODE>  // Just for const covariance.
  void Sequence(SequenceStyle style,
                const std::vector<PARSENODE*>& list,
                const ParseNode* end,
                bool force_multiline);

  // Returns the penalty.
  int FunctionCall(const FunctionCallNode* func_call);

  // Create a clone of this Printer in a similar state (other than the output,
  // but including margins, etc.) to be used for dry run measurements.
  void InitializeSub(Printer* sub);

  std::string output_;           // Output buffer.
  std::vector<Token> comments_;  // Pending end-of-line comments.
  int margin() const { return stack_.back().margin; }

  int penalty_depth_;
  int GetPenaltyForLineBreak() const {
    return penalty_depth_ * kPenaltyLineBreak;
  }

  struct IndentState {
    IndentState() : margin(0), continuation_requires_indent(false) {}
    IndentState(int margin, bool continuation_requires_indent)
        : margin(margin),
          continuation_requires_indent(continuation_requires_indent) {}

    // The left margin (number of spaces).
    int margin;

    bool continuation_requires_indent;
  };
  // Stack used to track
  std::vector<IndentState> stack_;

  // Gives the precedence for operators in a BinaryOpNode.
  std::map<base::StringPiece, Precedence> precedence_;

  DISALLOW_COPY_AND_ASSIGN(Printer);
};

Printer::Printer() : penalty_depth_(0) {
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
  stack_.push_back(IndentState());
}

Printer::~Printer() {
}

void Printer::Print(base::StringPiece str) {
  str.AppendToString(&output_);
}

void Printer::PrintMargin() {
  output_ += std::string(margin(), ' ');
}

void Printer::TrimAndPrintToken(const Token& token) {
  std::string trimmed;
  TrimWhitespaceASCII(token.value().as_string(), base::TRIM_ALL, &trimmed);
  Print(trimmed);
}

void Printer::Newline() {
  if (!comments_.empty()) {
    Print("  ");
    // Save the margin, and temporarily set it to where the first comment
    // starts so that multiple suffix comments are vertically aligned. This
    // will need to be fancier once we enforce 80 col.
    stack_.push_back(IndentState(CurrentColumn(), false));
    int i = 0;
    for (const auto& c : comments_) {
      if (i > 0) {
        Trim();
        Print("\n");
        PrintMargin();
      }
      TrimAndPrintToken(c);
      ++i;
    }
    stack_.pop_back();
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

void Printer::AnnotatePreferedMultilineAssignment(const BinaryOpNode* binop) {
  const IdentifierNode* ident = binop->left()->AsIdentifier();
  const ListNode* list = binop->right()->AsList();
  // This is somewhat arbitrary, but we include the 'deps'- and 'sources'-like
  // things, but not flags things.
  if (binop->op().value() == "=" && ident && list &&
      (ident->value().value() == "data" ||
       ident->value().value() == "datadeps" ||
       ident->value().value() == "deps" || ident->value().value() == "inputs" ||
       ident->value().value() == "public" ||
       ident->value().value() == "public_deps" ||
       ident->value().value() == "sources")) {
    const_cast<ListNode*>(list)->set_prefer_multiline(true);
  }
}

bool Printer::ShouldAddBlankLineInBetween(const ParseNode* a,
                                          const ParseNode* b) {
  LocationRange a_range = a->GetRange();
  LocationRange b_range = b->GetRange();
  // If they're already separated by 1 or more lines, then we want to keep a
  // blank line.
  return b_range.begin().line_number() > a_range.end().line_number() + 1;
}

int Printer::CurrentColumn() const {
  int n = 0;
  while (n < static_cast<int>(output_.size()) &&
         output_[output_.size() - 1 - n] != '\n') {
    ++n;
  }
  return n;
}

int Printer::CurrentLine() const {
  int count = 1;
  for (const char* p = output_.c_str(); (p = strchr(p, '\n')) != NULL;) {
    ++count;
    ++p;
  }
  return count;
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
    Expr(stmt, kPrecedenceLowest, std::string());
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

int Printer::AssessPenalty(const std::string& output) {
  int penalty = 0;
  std::vector<std::string> lines;
  base::SplitStringDontTrim(output, '\n', &lines);
  penalty += static_cast<int>(lines.size() - 1) * GetPenaltyForLineBreak();
  for (const auto& line : lines) {
    if (line.size() > kMaximumWidth)
      penalty += static_cast<int>(line.size() - kMaximumWidth) * kPenaltyExcess;
  }
  return penalty;
}

void Printer::AddParen(int prec, int outer_prec, bool* parenthesized) {
  if (prec < outer_prec) {
    Print("(");
    *parenthesized = true;
  }
}

int Printer::Expr(const ParseNode* root,
                  int outer_prec,
                  const std::string& suffix) {
  int penalty = 0;
  penalty_depth_++;

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
      Expr(accessor->member(), kPrecedenceLowest, std::string());
    } else {
      CHECK(accessor->index());
      Print("[");
      Expr(accessor->index(), kPrecedenceLowest, "]");
    }
  } else if (const BinaryOpNode* binop = root->AsBinaryOp()) {
    CHECK(precedence_.find(binop->op().value()) != precedence_.end());
    AnnotatePreferedMultilineAssignment(binop);
    Precedence prec = precedence_[binop->op().value()];
    AddParen(prec, outer_prec, &parenthesized);
    int start_line = CurrentLine();
    int start_column = CurrentColumn();
    int indent_column =
        (binop->op().value() == "=" || binop->op().value() == "+=" ||
         binop->op().value() == "-=")
            ? margin() + kIndentSize * 2
            : start_column;
    if (stack_.back().continuation_requires_indent)
      indent_column += kIndentSize * 2;

    Expr(binop->left(),
         prec,
         std::string(" ") + binop->op().value().as_string());

    // Single line.
    Printer sub1;
    InitializeSub(&sub1);
    sub1.stack_.push_back(IndentState(indent_column, false));
    sub1.Print(" ");
    int penalty_current_line =
        sub1.Expr(binop->right(), prec + 1, std::string());
    sub1.Print(suffix);
    penalty_current_line += AssessPenalty(sub1.String());

    // Break after operator.
    Printer sub2;
    InitializeSub(&sub2);
    sub2.stack_.push_back(IndentState(indent_column, false));
    sub2.Newline();
    int penalty_next_line = sub2.Expr(binop->right(), prec + 1, std::string());
    sub2.Print(suffix);
    penalty_next_line += AssessPenalty(sub2.String());

    if (penalty_current_line < penalty_next_line) {
      Print(" ");
      Expr(binop->right(), prec + 1, std::string());
    } else {
      // Otherwise, put first argument and op, and indent next.
      stack_.push_back(IndentState(indent_column, false));
      Newline();
      penalty += std::abs(CurrentColumn() - start_column) *
                 kPenaltyHorizontalSeparation;
      Expr(binop->right(), prec + 1, std::string());
      stack_.pop_back();
    }
    penalty += (CurrentLine() - start_line) * GetPenaltyForLineBreak();
  } else if (const BlockNode* block = root->AsBlock()) {
    Sequence(
        kSequenceStyleBracedBlock, block->statements(), block->End(), false);
  } else if (const ConditionNode* condition = root->AsConditionNode()) {
    Print("if (");
    // TODO(scottmg): The { needs to be included in the suffix here.
    Expr(condition->condition(), kPrecedenceLowest, ") ");
    Sequence(kSequenceStyleBracedBlock,
             condition->if_true()->statements(),
             condition->if_true()->End(),
             false);
    if (condition->if_false()) {
      Print(" else ");
      // If it's a block it's a bare 'else', otherwise it's an 'else if'. See
      // ConditionNode::Execute.
      bool is_else_if = condition->if_false()->AsBlock() == NULL;
      if (is_else_if) {
        Expr(condition->if_false(), kPrecedenceLowest, std::string());
      } else {
        Sequence(kSequenceStyleBracedBlock,
                 condition->if_false()->AsBlock()->statements(),
                 condition->if_false()->AsBlock()->End(), false);
      }
    }
  } else if (const FunctionCallNode* func_call = root->AsFunctionCall()) {
    penalty += FunctionCall(func_call);
  } else if (const IdentifierNode* identifier = root->AsIdentifier()) {
    Print(identifier->value().value());
  } else if (const ListNode* list = root->AsList()) {
    bool force_multiline =
        list->prefer_multiline() && !list->contents().empty();
    Sequence(
        kSequenceStyleList, list->contents(), list->End(), force_multiline);
  } else if (const LiteralNode* literal = root->AsLiteral()) {
    Print(literal->value().value());
  } else if (const UnaryOpNode* unaryop = root->AsUnaryOp()) {
    Print(unaryop->op().value());
    Expr(unaryop->operand(), kPrecedenceUnary, std::string());
  } else if (const BlockCommentNode* block_comment = root->AsBlockComment()) {
    Print(block_comment->comment().value());
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

  Print(suffix);

  penalty_depth_--;
  return penalty;
}

template <class PARSENODE>
void Printer::Sequence(SequenceStyle style,
                       const std::vector<PARSENODE*>& list,
                       const ParseNode* end,
                       bool force_multiline) {
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
    Expr(list[0], kPrecedenceLowest, std::string());
    CHECK(!list[0]->comments() || list[0]->comments()->after().empty());
    Print(" ");
  } else {
    stack_.push_back(
        IndentState(margin() + kIndentSize, style == kSequenceStyleList));
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
      bool body_of_list = i < list.size() - 1 || style == kSequenceStyleList;
      bool want_comma =
          body_of_list && (style == kSequenceStyleList && !x->AsBlockComment());
      Expr(x, kPrecedenceLowest, want_comma ? "," : std::string());
      CHECK(!x->comments() || x->comments()->after().empty());
      if (body_of_list) {
        if (!want_comma) {
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

    stack_.pop_back();
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

int Printer::FunctionCall(const FunctionCallNode* func_call) {
  int start_line = CurrentLine();
  int start_column = CurrentColumn();
  Print(func_call->function().value());
  Print("(");

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

  // Calculate the penalties for 3 possible layouts:
  // 1. all on same line;
  // 2. starting on same line, broken at each comma but paren aligned;
  // 3. broken to next line + 4, broken at each comma.
  std::string terminator = ")";
  if (have_block)
    terminator += " {";

  // 1: Same line.
  Printer sub1;
  InitializeSub(&sub1);
  sub1.stack_.push_back(IndentState(CurrentColumn(), true));
  int penalty_one_line = 0;
  for (size_t i = 0; i < list.size(); ++i) {
    penalty_one_line += sub1.Expr(list[i], kPrecedenceLowest,
                                  i < list.size() - 1 ? ", " : std::string());
  }
  sub1.Print(terminator);
  penalty_one_line += AssessPenalty(sub1.String());
  std::vector<std::string> lines;
  base::SplitStringDontTrim(sub1.String(), '\n', &lines);
  // This extra penalty prevents a short second argument from being squeezed in
  // after a first argument that went multiline (and instead preferring a
  // variant below).
  if (lines.size() > 1)
    penalty_one_line += kPenaltyBrokenLineOnOneLiner;

  // 2: Starting on same line, broken at commas.
  Printer sub2;
  InitializeSub(&sub2);
  sub2.stack_.push_back(IndentState(CurrentColumn(), true));
  int penalty_multiline_start_same_line = 0;
  for (size_t i = 0; i < list.size(); ++i) {
    penalty_multiline_start_same_line += sub2.Expr(
        list[i], kPrecedenceLowest, i < list.size() - 1 ? "," : std::string());
    if (i < list.size() - 1) {
      sub2.Newline();
    }
  }
  sub2.Print(terminator);
  penalty_multiline_start_same_line += AssessPenalty(sub2.String());

  // 3: Starting on next line, broken at commas.
  Printer sub3;
  InitializeSub(&sub3);
  sub3.stack_.push_back(IndentState(margin() + kIndentSize * 2, true));
  sub3.Newline();
  int penalty_multiline_start_next_line = 0;
  for (size_t i = 0; i < list.size(); ++i) {
    if (i == 0) {
      penalty_multiline_start_next_line +=
          std::abs(sub3.CurrentColumn() - start_column) *
          kPenaltyHorizontalSeparation;
    }
    penalty_multiline_start_next_line += sub3.Expr(
        list[i], kPrecedenceLowest, i < list.size() - 1 ? "," : std::string());
    if (i < list.size() - 1) {
      sub3.Newline();
    }
  }
  sub3.Print(terminator);
  penalty_multiline_start_next_line += AssessPenalty(sub3.String());

  int penalty = penalty_multiline_start_next_line;
  bool fits_on_current_line = false;
  if (penalty_one_line < penalty_multiline_start_next_line ||
      penalty_multiline_start_same_line < penalty_multiline_start_next_line) {
    fits_on_current_line = true;
    penalty = penalty_one_line;
    if (penalty_multiline_start_same_line < penalty_one_line) {
      penalty = penalty_multiline_start_same_line;
      force_multiline = true;
    }
  } else {
    force_multiline = true;
  }

  if (list.size() == 0 && !force_multiline) {
    // No elements, and not forcing newlines, print nothing.
  } else {
    if (penalty_multiline_start_next_line < penalty_multiline_start_same_line) {
      stack_.push_back(IndentState(margin() + kIndentSize * 2, true));
      Newline();
    } else {
      stack_.push_back(IndentState(CurrentColumn(), true));
    }

    for (size_t i = 0; i < list.size(); ++i) {
      const auto& x = list[i];
      if (i > 0) {
        if (fits_on_current_line && !force_multiline)
          Print(" ");
        else
          Newline();
      }
      bool want_comma = i < list.size() - 1 && !x->AsBlockComment();
      Expr(x, kPrecedenceLowest, want_comma ? "," : std::string());
      CHECK(!x->comments() || x->comments()->after().empty());
      if (i < list.size() - 1) {
        if (!want_comma)
          Newline();
      }
    }

    // Trailing comments.
    if (end->comments() && !end->comments()->before().empty()) {
      if (!list.empty())
        Newline();
      for (const auto& c : end->comments()->before()) {
        Newline();
        TrimAndPrintToken(c);
      }
      Newline();
    }
    stack_.pop_back();
  }

  // Defer any end of line comment until we reach the newline.
  if (end->comments() && !end->comments()->suffix().empty()) {
    std::copy(end->comments()->suffix().begin(),
              end->comments()->suffix().end(), std::back_inserter(comments_));
  }

  Print(")");

  if (have_block) {
    Print(" ");
    Sequence(kSequenceStyleBracedBlock,
             func_call->block()->statements(),
             func_call->block()->End(),
             false);
  }
  return penalty + (CurrentLine() - start_line) * GetPenaltyForLineBreak();
}

void Printer::InitializeSub(Printer* sub) {
  sub->stack_ = stack_;
  sub->comments_ = comments_;
  sub->penalty_depth_ = penalty_depth_;
  sub->Print(std::string(CurrentColumn(), 'x'));
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
  SourceFile file = source_dir.ResolveRelativeFile(args[0],
      setup.build_settings().root_path_utf8());

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
