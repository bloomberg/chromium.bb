// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_PARSER_H_
#define TOOLS_GN_PARSER_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "tools/gn/err.h"
#include "tools/gn/parse_tree.h"

class Parser;
typedef scoped_ptr<ParseNode> (Parser::*PrefixFunc)(Token token);
typedef scoped_ptr<ParseNode> (Parser::*InfixFunc)(scoped_ptr<ParseNode> left,
                                                   Token token);

struct ParserHelper {
  PrefixFunc prefix;
  InfixFunc infix;
  int precedence;
};

// Parses a series of tokens. The resulting AST will refer to the tokens passed
// to the input, so the tokens an the file data they refer to must outlive your
// use of the ParseNode.
class Parser {
 public:
  // Will return a null pointer and set the err on error.
  static scoped_ptr<ParseNode> Parse(const std::vector<Token>& tokens,
                                     Err* err);

  // Alternative to parsing that assumes the input is an expression.
  static scoped_ptr<ParseNode> ParseExpression(const std::vector<Token>& tokens,
                                               Err* err);

  scoped_ptr<ParseNode> ParseExpression();

 private:
  // Vector must be valid for lifetime of call.
  Parser(const std::vector<Token>& tokens, Err* err);
  ~Parser();

  // Parses an expression with the given precedence or higher.
  scoped_ptr<ParseNode> ParseExpression(int precedence);

  // |PrefixFunc|s used in parsing expressions.
  scoped_ptr<ParseNode> Literal(Token token);
  scoped_ptr<ParseNode> Name(Token token);
  scoped_ptr<ParseNode> Group(Token token);
  scoped_ptr<ParseNode> Not(Token token);
  scoped_ptr<ParseNode> List(Token token);

  // |InfixFunc|s used in parsing expressions.
  scoped_ptr<ParseNode> BinaryOperator(scoped_ptr<ParseNode> left, Token token);
  scoped_ptr<ParseNode> IdentifierOrCall(scoped_ptr<ParseNode> left,
                                         Token token);
  scoped_ptr<ParseNode> Assignment(scoped_ptr<ParseNode> left, Token token);
  scoped_ptr<ParseNode> Subscript(scoped_ptr<ParseNode> left, Token token);
  scoped_ptr<ParseNode> DotOperator(scoped_ptr<ParseNode> left, Token token);

  // Helper to parse a comma separated list, optionally allowing trailing
  // commas (allowed in [] lists, not in function calls).
  scoped_ptr<ListNode> ParseList(Token::Type stop_before,
                                 bool allow_trailing_comma);

  scoped_ptr<ParseNode> ParseFile();
  scoped_ptr<ParseNode> ParseStatement();
  scoped_ptr<BlockNode> ParseBlock();
  scoped_ptr<ParseNode> ParseCondition();

  bool IsAssignment(const ParseNode* node) const;
  bool IsStatementBreak(Token::Type token_type) const;

  bool LookAhead(Token::Type type);
  bool Match(Token::Type type);
  Token Consume(Token::Type type, const char* error_message);
  Token Consume(Token::Type* types,
                size_t num_types,
                const char* error_message);
  Token Consume();

  const Token& cur_token() const { return tokens_[cur_]; }

  bool done() const { return at_end() || has_error(); }
  bool at_end() const { return cur_ >= tokens_.size(); }
  bool has_error() const { return err_->has_error(); }

  const std::vector<Token>& tokens_;

  static ParserHelper expressions_[Token::NUM_TYPES];

  Err* err_;

  // Current index into the tokens.
  size_t cur_;

  FRIEND_TEST_ALL_PREFIXES(Parser, BinaryOp);
  FRIEND_TEST_ALL_PREFIXES(Parser, Block);
  FRIEND_TEST_ALL_PREFIXES(Parser, Condition);
  FRIEND_TEST_ALL_PREFIXES(Parser, Expression);
  FRIEND_TEST_ALL_PREFIXES(Parser, FunctionCall);
  FRIEND_TEST_ALL_PREFIXES(Parser, List);
  FRIEND_TEST_ALL_PREFIXES(Parser, ParenExpression);
  FRIEND_TEST_ALL_PREFIXES(Parser, UnaryOp);

  DISALLOW_COPY_AND_ASSIGN(Parser);
};

#endif  // TOOLS_GN_PARSER_H_
