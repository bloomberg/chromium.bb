// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/parser.h"

#include "base/logging.h"
#include "tools/gn/functions.h"
#include "tools/gn/operators.h"
#include "tools/gn/token.h"

// grammar:
//
// file       := (statement)*
// statement  := block | if | assignment
// block      := '{' statement* '}'
// if         := 'if' '(' expr ')' statement [ else ]
// else       := 'else' (if | statement)*
// assignment := ident {'=' | '+=' | '-='} expr

enum Precedence {
  PRECEDENCE_ASSIGNMENT = 1,  // Lowest precedence.
  PRECEDENCE_OR = 2,
  PRECEDENCE_AND = 3,
  PRECEDENCE_EQUALITY = 4,
  PRECEDENCE_RELATION = 5,
  PRECEDENCE_SUM = 6,
  PRECEDENCE_PREFIX = 7,
  PRECEDENCE_CALL = 8,
  PRECEDENCE_DOT = 9,         // Highest precedence.
};

// The top-level for blocks/ifs is recursive descent, the expression parser is
// a Pratt parser. The basic idea there is to have the precedences (and
// associativities) encoded relative to each other and only parse up until you
// hit something of that precedence. There's a dispatch table in expressions_
// at the top of parser.cc that describes how each token dispatches if it's
// seen as either a prefix or infix operator, and if it's infix, what its
// precedence is.
//
// Refs:
// - http://javascript.crockford.com/tdop/tdop.html
// - http://journal.stuffwithstuff.com/2011/03/19/pratt-parsers-expression-parsing-made-easy/

// Indexed by Token::Type.
ParserHelper Parser::expressions_[] = {
  {NULL, NULL, -1},                                             // INVALID
  {&Parser::Literal, NULL, -1},                                 // INTEGER
  {&Parser::Literal, NULL, -1},                                 // STRING
  {&Parser::Literal, NULL, -1},                                 // TRUE_TOKEN
  {&Parser::Literal, NULL, -1},                                 // FALSE_TOKEN
  {NULL, &Parser::Assignment, PRECEDENCE_ASSIGNMENT},           // EQUAL
  {NULL, &Parser::BinaryOperator, PRECEDENCE_SUM},              // PLUS
  {NULL, &Parser::BinaryOperator, PRECEDENCE_SUM},              // MINUS
  {NULL, &Parser::Assignment, PRECEDENCE_ASSIGNMENT},           // PLUS_EQUALS
  {NULL, &Parser::Assignment, PRECEDENCE_ASSIGNMENT},           // MINUS_EQUALS
  {NULL, &Parser::BinaryOperator, PRECEDENCE_EQUALITY},         // EQUAL_EQUAL
  {NULL, &Parser::BinaryOperator, PRECEDENCE_EQUALITY},         // NOT_EQUAL
  {NULL, &Parser::BinaryOperator, PRECEDENCE_RELATION},         // LESS_EQUAL
  {NULL, &Parser::BinaryOperator, PRECEDENCE_RELATION},         // GREATER_EQUAL
  {NULL, &Parser::BinaryOperator, PRECEDENCE_RELATION},         // LESS_THAN
  {NULL, &Parser::BinaryOperator, PRECEDENCE_RELATION},         // GREATER_THAN
  {NULL, &Parser::BinaryOperator, PRECEDENCE_AND},              // BOOLEAN_AND
  {NULL, &Parser::BinaryOperator, PRECEDENCE_OR},               // BOOLEAN_OR
  {&Parser::Not, NULL, -1},                                     // BANG
  {NULL, &Parser::DotOperator, PRECEDENCE_DOT},                 // DOT
  {&Parser::Group, NULL, -1},                                   // LEFT_PAREN
  {NULL, NULL, -1},                                             // RIGHT_PAREN
  {&Parser::List, &Parser::Subscript, PRECEDENCE_CALL},         // LEFT_BRACKET
  {NULL, NULL, -1},                                             // RIGHT_BRACKET
  {NULL, NULL, -1},                                             // LEFT_BRACE
  {NULL, NULL, -1},                                             // RIGHT_BRACE
  {NULL, NULL, -1},                                             // IF
  {NULL, NULL, -1},                                             // ELSE
  {&Parser::Name, &Parser::IdentifierOrCall, PRECEDENCE_CALL},  // IDENTIFIER
  {NULL, NULL, -1},                                             // COMMA
  {NULL, NULL, -1},                   // UNCLASSIFIED_COMMENT
  {NULL, NULL, -1},                   // LINE_COMMENT
  {NULL, NULL, -1},                   // SUFFIX_COMMENT
  {&Parser::BlockComment, NULL, -1},  // BLOCK_COMMENT
};

Parser::Parser(const std::vector<Token>& tokens, Err* err)
    : err_(err), cur_(0) {
  for (std::vector<Token>::const_iterator i(tokens.begin()); i != tokens.end();
       ++i) {
    switch(i->type()) {
      case Token::LINE_COMMENT:
        line_comment_tokens_.push_back(*i);
        break;
      case Token::SUFFIX_COMMENT:
        suffix_comment_tokens_.push_back(*i);
        break;
      default:
        // Note that BLOCK_COMMENTs (top-level standalone comments) are passed
        // through the real parser.
        tokens_.push_back(*i);
        break;
    }
  }
}

Parser::~Parser() {
}

// static
scoped_ptr<ParseNode> Parser::Parse(const std::vector<Token>& tokens,
                                    Err* err) {
  Parser p(tokens, err);
  return p.ParseFile().PassAs<ParseNode>();
}

// static
scoped_ptr<ParseNode> Parser::ParseExpression(const std::vector<Token>& tokens,
                                              Err* err) {
  Parser p(tokens, err);
  return p.ParseExpression().Pass();
}

bool Parser::IsAssignment(const ParseNode* node) const {
  return node && node->AsBinaryOp() &&
         (node->AsBinaryOp()->op().type() == Token::EQUAL ||
          node->AsBinaryOp()->op().type() == Token::PLUS_EQUALS ||
          node->AsBinaryOp()->op().type() == Token::MINUS_EQUALS);
}

bool Parser::IsStatementBreak(Token::Type token_type) const {
  switch (token_type) {
    case Token::IDENTIFIER:
    case Token::LEFT_BRACE:
    case Token::RIGHT_BRACE:
    case Token::IF:
    case Token::ELSE:
      return true;
    default:
      return false;
  }
}

bool Parser::LookAhead(Token::Type type) {
  if (at_end())
    return false;
  return cur_token().type() == type;
}

bool Parser::Match(Token::Type type) {
  if (!LookAhead(type))
    return false;
  Consume();
  return true;
}

Token Parser::Consume(Token::Type type, const char* error_message) {
  Token::Type types[1] = { type };
  return Consume(types, 1, error_message);
}

Token Parser::Consume(Token::Type* types,
                      size_t num_types,
                      const char* error_message) {
  if (has_error()) {
    // Don't overwrite current error, but make progress through tokens so that
    // a loop that's expecting a particular token will still terminate.
    cur_++;
    return Token(Location(), Token::INVALID, base::StringPiece());
  }
  if (at_end()) {
    const char kEOFMsg[] = "I hit EOF instead.";
    if (tokens_.empty())
      *err_ = Err(Location(), error_message, kEOFMsg);
    else
      *err_ = Err(tokens_[tokens_.size() - 1], error_message, kEOFMsg);
    return Token(Location(), Token::INVALID, base::StringPiece());
  }

  for (size_t i = 0; i < num_types; ++i) {
    if (cur_token().type() == types[i])
      return tokens_[cur_++];
  }
  *err_ = Err(cur_token(), error_message);
  return Token(Location(), Token::INVALID, base::StringPiece());
}

Token Parser::Consume() {
  return tokens_[cur_++];
}

scoped_ptr<ParseNode> Parser::ParseExpression() {
  return ParseExpression(0);
}

scoped_ptr<ParseNode> Parser::ParseExpression(int precedence) {
  if (at_end())
    return scoped_ptr<ParseNode>();

  Token token = Consume();
  PrefixFunc prefix = expressions_[token.type()].prefix;

  if (prefix == NULL) {
    *err_ = Err(token,
                std::string("Unexpected token '") + token.value().as_string() +
                    std::string("'"));
    return scoped_ptr<ParseNode>();
  }

  scoped_ptr<ParseNode> left = (this->*prefix)(token);
  if (has_error())
    return left.Pass();

  while (!at_end() && !IsStatementBreak(cur_token().type()) &&
         precedence <= expressions_[cur_token().type()].precedence) {
    token = Consume();
    InfixFunc infix = expressions_[token.type()].infix;
    if (infix == NULL) {
      *err_ = Err(token,
                  std::string("Unexpected token '") +
                      token.value().as_string() + std::string("'"));
      return scoped_ptr<ParseNode>();
    }
    left = (this->*infix)(left.Pass(), token);
    if (has_error())
      return scoped_ptr<ParseNode>();
  }

  return left.Pass();
}

scoped_ptr<ParseNode> Parser::Literal(Token token) {
  return scoped_ptr<ParseNode>(new LiteralNode(token)).Pass();
}

scoped_ptr<ParseNode> Parser::Name(Token token) {
  return IdentifierOrCall(scoped_ptr<ParseNode>(), token).Pass();
}

scoped_ptr<ParseNode> Parser::BlockComment(Token token) {
  scoped_ptr<BlockCommentNode> comment(new BlockCommentNode());
  comment->set_comment(token);
  return comment.PassAs<ParseNode>();
}

scoped_ptr<ParseNode> Parser::Group(Token token) {
  scoped_ptr<ParseNode> expr = ParseExpression();
  if (has_error())
    return scoped_ptr<ParseNode>();
  Consume(Token::RIGHT_PAREN, "Expected ')'");
  return expr.Pass();
}

scoped_ptr<ParseNode> Parser::Not(Token token) {
  scoped_ptr<ParseNode> expr = ParseExpression(PRECEDENCE_PREFIX + 1);
  if (has_error())
    return scoped_ptr<ParseNode>();
  scoped_ptr<UnaryOpNode> unary_op(new UnaryOpNode);
  unary_op->set_op(token);
  unary_op->set_operand(expr.Pass());
  return unary_op.PassAs<ParseNode>();
}

scoped_ptr<ParseNode> Parser::List(Token node) {
  scoped_ptr<ParseNode> list(ParseList(node, Token::RIGHT_BRACKET, true));
  if (!has_error() && !at_end())
    Consume(Token::RIGHT_BRACKET, "Expected ']'");
  return list.Pass();
}

scoped_ptr<ParseNode> Parser::BinaryOperator(scoped_ptr<ParseNode> left,
                                             Token token) {
  scoped_ptr<ParseNode> right =
      ParseExpression(expressions_[token.type()].precedence + 1);
  if (!right) {
    *err_ =
        Err(token,
            "Expected right hand side for '" + token.value().as_string() + "'");
    return scoped_ptr<ParseNode>();
  }
  scoped_ptr<BinaryOpNode> binary_op(new BinaryOpNode);
  binary_op->set_op(token);
  binary_op->set_left(left.Pass());
  binary_op->set_right(right.Pass());
  return binary_op.PassAs<ParseNode>();
}

scoped_ptr<ParseNode> Parser::IdentifierOrCall(scoped_ptr<ParseNode> left,
                                               Token token) {
  scoped_ptr<ListNode> list(new ListNode);
  list->set_begin_token(token);
  list->set_end_token(token);
  scoped_ptr<BlockNode> block;
  bool has_arg = false;
  if (LookAhead(Token::LEFT_PAREN)) {
    Token start_token = Consume();
    // Parsing a function call.
    has_arg = true;
    if (Match(Token::RIGHT_PAREN)) {
      // Nothing, just an empty call.
    } else {
      list = ParseList(start_token, Token::RIGHT_PAREN, false);
      if (has_error())
        return scoped_ptr<ParseNode>();
      Consume(Token::RIGHT_PAREN, "Expected ')' after call");
    }
    // Optionally with a scope.
    if (LookAhead(Token::LEFT_BRACE)) {
      block = ParseBlock();
      if (has_error())
        return scoped_ptr<ParseNode>();
    }
  }

  if (!left && !has_arg) {
    // Not a function call, just a standalone identifier.
    return scoped_ptr<ParseNode>(new IdentifierNode(token)).Pass();
  }
  scoped_ptr<FunctionCallNode> func_call(new FunctionCallNode);
  func_call->set_function(token);
  func_call->set_args(list.Pass());
  if (block)
    func_call->set_block(block.Pass());
  return func_call.PassAs<ParseNode>();
}

scoped_ptr<ParseNode> Parser::Assignment(scoped_ptr<ParseNode> left,
                                         Token token) {
  if (left->AsIdentifier() == NULL) {
    *err_ = Err(left.get(), "Left-hand side of assignment must be identifier.");
    return scoped_ptr<ParseNode>();
  }
  scoped_ptr<ParseNode> value = ParseExpression(PRECEDENCE_ASSIGNMENT);
  scoped_ptr<BinaryOpNode> assign(new BinaryOpNode);
  assign->set_op(token);
  assign->set_left(left.Pass());
  assign->set_right(value.Pass());
  return assign.PassAs<ParseNode>();
}

scoped_ptr<ParseNode> Parser::Subscript(scoped_ptr<ParseNode> left,
                                        Token token) {
  // TODO: Maybe support more complex expressions like a[0][0]. This would
  // require work on the evaluator too.
  if (left->AsIdentifier() == NULL) {
    *err_ = Err(left.get(), "May only subscript identifiers.",
        "The thing on the left hand side of the [] must be an identifier\n"
        "and not an expression. If you need this, you'll have to assign the\n"
        "value to a temporary before subscripting. Sorry.");
    return scoped_ptr<ParseNode>();
  }
  scoped_ptr<ParseNode> value = ParseExpression();
  Consume(Token::RIGHT_BRACKET, "Expecting ']' after subscript.");
  scoped_ptr<AccessorNode> accessor(new AccessorNode);
  accessor->set_base(left->AsIdentifier()->value());
  accessor->set_index(value.Pass());
  return accessor.PassAs<ParseNode>();
}

scoped_ptr<ParseNode> Parser::DotOperator(scoped_ptr<ParseNode> left,
                                          Token token) {
  if (left->AsIdentifier() == NULL) {
    *err_ = Err(left.get(), "May only use \".\" for identifiers.",
        "The thing on the left hand side of the dot must be an identifier\n"
        "and not an expression. If you need this, you'll have to assign the\n"
        "value to a temporary first. Sorry.");
    return scoped_ptr<ParseNode>();
  }

  scoped_ptr<ParseNode> right = ParseExpression(PRECEDENCE_DOT);
  if (!right || !right->AsIdentifier()) {
    *err_ = Err(token, "Expected identifier for right-hand-side of \".\"",
        "Good: a.cookies\nBad: a.42\nLooks good but still bad: a.cookies()");
    return scoped_ptr<ParseNode>();
  }

  scoped_ptr<AccessorNode> accessor(new AccessorNode);
  accessor->set_base(left->AsIdentifier()->value());
  accessor->set_member(scoped_ptr<IdentifierNode>(
      static_cast<IdentifierNode*>(right.release())));
  return accessor.PassAs<ParseNode>();
}

// Does not Consume the start or end token.
scoped_ptr<ListNode> Parser::ParseList(Token start_token,
                                       Token::Type stop_before,
                                       bool allow_trailing_comma) {
  scoped_ptr<ListNode> list(new ListNode);
  list->set_begin_token(start_token);
  bool just_got_comma = false;
  bool first_time = true;
  while (!LookAhead(stop_before)) {
    if (!first_time) {
      if (!just_got_comma) {
        // Require commas separate things in lists.
        *err_ = Err(cur_token(), "Expected comma between items.");
        return scoped_ptr<ListNode>();
      }
    }
    first_time = false;

    // Why _OR? We're parsing things that are higher precedence than the ,
    // that separates the items of the list. , should appear lower than
    // boolean expressions (the lowest of which is OR), but above assignments.
    list->append_item(ParseExpression(PRECEDENCE_OR));
    if (has_error())
      return scoped_ptr<ListNode>();
    if (at_end()) {
      *err_ =
          Err(tokens_[tokens_.size() - 1], "Unexpected end of file in list.");
      return scoped_ptr<ListNode>();
    }
    if (list->contents().back()->AsBlockComment()) {
      // If there was a comment inside the list, we don't need a comma to the
      // next item, so pretend we got one, if we're expecting one.
      just_got_comma = allow_trailing_comma;
    } else {
      just_got_comma = Match(Token::COMMA);
    }
  }
  if (just_got_comma && !allow_trailing_comma) {
    *err_ = Err(cur_token(), "Trailing comma");
    return scoped_ptr<ListNode>();
  }
  list->set_end_token(cur_token());
  return list.Pass();
}

scoped_ptr<ParseNode> Parser::ParseFile() {
  scoped_ptr<BlockNode> file(new BlockNode(false));
  for (;;) {
    if (at_end())
      break;
    scoped_ptr<ParseNode> statement = ParseStatement();
    if (!statement)
      break;
    file->append_statement(statement.Pass());
  }
  if (!at_end() && !has_error())
    *err_ = Err(cur_token(), "Unexpected here, should be newline.");
  if (has_error())
    return scoped_ptr<ParseNode>();

  // TODO(scottmg): If this is measurably expensive, it could be done only
  // when necessary (when reformatting, or during tests). Comments are
  // separate from the parse tree at this point, so downstream code can remain
  // ignorant of them.
  AssignComments(file.get());

  return file.PassAs<ParseNode>();
}

scoped_ptr<ParseNode> Parser::ParseStatement() {
  if (LookAhead(Token::LEFT_BRACE)) {
    return ParseBlock().PassAs<ParseNode>();
  } else if (LookAhead(Token::IF)) {
    return ParseCondition();
  } else if (LookAhead(Token::BLOCK_COMMENT)) {
    return BlockComment(Consume());
  } else {
    // TODO(scottmg): Is this too strict? Just drop all the testing if we want
    // to allow "pointless" expressions and return ParseExpression() directly.
    scoped_ptr<ParseNode> stmt = ParseExpression();
    if (stmt) {
      if (stmt->AsFunctionCall() || IsAssignment(stmt.get()))
        return stmt.Pass();
    }
    if (!has_error()) {
      Token token = at_end() ? tokens_[tokens_.size() - 1] : cur_token();
      *err_ = Err(token, "Expecting assignment or function call.");
    }
    return scoped_ptr<ParseNode>();
  }
}

scoped_ptr<BlockNode> Parser::ParseBlock() {
  Token begin_token =
      Consume(Token::LEFT_BRACE, "Expected '{' to start a block.");
  if (has_error())
    return scoped_ptr<BlockNode>();
  scoped_ptr<BlockNode> block(new BlockNode(true));
  block->set_begin_token(begin_token);

  for (;;) {
    if (LookAhead(Token::RIGHT_BRACE)) {
      block->set_end_token(Consume());
      break;
    }

    scoped_ptr<ParseNode> statement = ParseStatement();
    if (!statement)
      return scoped_ptr<BlockNode>();
    block->append_statement(statement.Pass());
  }
  return block.Pass();
}

scoped_ptr<ParseNode> Parser::ParseCondition() {
  scoped_ptr<ConditionNode> condition(new ConditionNode);
  condition->set_if_token(Consume(Token::IF, "Expected 'if'"));
  Consume(Token::LEFT_PAREN, "Expected '(' after 'if'.");
  condition->set_condition(ParseExpression());
  if (IsAssignment(condition->condition()))
    *err_ = Err(condition->condition(), "Assignment not allowed in 'if'.");
  Consume(Token::RIGHT_PAREN, "Expected ')' after condition of 'if'.");
  condition->set_if_true(ParseBlock().Pass());
  if (Match(Token::ELSE))
    condition->set_if_false(ParseStatement().Pass());
  if (has_error())
    return scoped_ptr<ParseNode>();
  return condition.PassAs<ParseNode>();
}

void Parser::TraverseOrder(const ParseNode* root,
                           std::vector<const ParseNode*>* pre,
                           std::vector<const ParseNode*>* post) {
  if (root) {
    pre->push_back(root);

    if (const AccessorNode* accessor = root->AsAccessor()) {
      TraverseOrder(accessor->index(), pre, post);
      TraverseOrder(accessor->member(), pre, post);
    } else if (const BinaryOpNode* binop = root->AsBinaryOp()) {
      TraverseOrder(binop->left(), pre, post);
      TraverseOrder(binop->right(), pre, post);
    } else if (const BlockNode* block = root->AsBlock()) {
      const std::vector<ParseNode*>& statements = block->statements();
      for (std::vector<ParseNode*>::const_iterator i(statements.begin());
          i != statements.end();
          ++i) {
        TraverseOrder(*i, pre, post);
      }
    } else if (const ConditionNode* condition = root->AsConditionNode()) {
      TraverseOrder(condition->condition(), pre, post);
      TraverseOrder(condition->if_true(), pre, post);
      TraverseOrder(condition->if_false(), pre, post);
    } else if (const FunctionCallNode* func_call = root->AsFunctionCall()) {
      TraverseOrder(func_call->args(), pre, post);
      TraverseOrder(func_call->block(), pre, post);
    } else if (root->AsIdentifier()) {
      // Nothing.
    } else if (const ListNode* list = root->AsList()) {
      const std::vector<const ParseNode*>& contents = list->contents();
      for (std::vector<const ParseNode*>::const_iterator i(contents.begin());
          i != contents.end();
          ++i) {
        TraverseOrder(*i, pre, post);
      }
    } else if (root->AsLiteral()) {
      // Nothing.
    } else if (const UnaryOpNode* unaryop = root->AsUnaryOp()) {
      TraverseOrder(unaryop->operand(), pre, post);
    } else if (root->AsBlockComment()) {
      // Nothing.
    } else {
      CHECK(false) << "Unhandled case in TraverseOrder.";
    }

    post->push_back(root);
  }
}

void Parser::AssignComments(ParseNode* file) {
  // Start by generating a pre- and post- order traversal of the tree so we
  // can determine what's before and after comments.
  std::vector<const ParseNode*> pre;
  std::vector<const ParseNode*> post;
  TraverseOrder(file, &pre, &post);

  // Assign line comments to syntax immediately following.
  int cur_comment = 0;
  for (std::vector<const ParseNode*>::const_iterator i = pre.begin();
       i != pre.end();
       ++i) {
    const Location& start = (*i)->GetRange().begin();
    while (cur_comment < static_cast<int>(line_comment_tokens_.size())) {
      if (start.byte() >= line_comment_tokens_[cur_comment].location().byte()) {
        const_cast<ParseNode*>(*i)->comments_mutable()->append_before(
            line_comment_tokens_[cur_comment]);
        ++cur_comment;
      } else {
        break;
      }
    }
  }

  // Remaining line comments go at end of file.
  for (; cur_comment < static_cast<int>(line_comment_tokens_.size());
       ++cur_comment)
    file->comments_mutable()->append_after(line_comment_tokens_[cur_comment]);

  // Assign suffix to syntax immediately before.
  cur_comment = static_cast<int>(suffix_comment_tokens_.size() - 1);
  for (std::vector<const ParseNode*>::const_reverse_iterator i = post.rbegin();
       i != post.rend();
       ++i) {
    // Don't assign suffix comments to the function call or list, but instead
    // to the last thing inside.
    if ((*i)->AsFunctionCall() || (*i)->AsList())
      continue;

    const Location& start = (*i)->GetRange().begin();
    const Location& end = (*i)->GetRange().end();

    // Don't assign suffix comments to something that starts on an earlier
    // line, so that in:
    //
    // sources = [ "a",
    //     "b" ] # comment
    //
    // it's attached to "b", not sources = [ ... ].
    if (start.line_number() != end.line_number())
      continue;

    while (cur_comment >= 0) {
      if (end.byte() <= suffix_comment_tokens_[cur_comment].location().byte()) {
        const_cast<ParseNode*>(*i)->comments_mutable()->append_suffix(
            suffix_comment_tokens_[cur_comment]);
        --cur_comment;
      } else {
        break;
      }
    }

    // Suffix comments were assigned in reverse, so if there were multiple on
    // the same node, they need to be reversed.
    const_cast<ParseNode*>(*i)->comments_mutable()->ReverseSuffix();
  }
}
