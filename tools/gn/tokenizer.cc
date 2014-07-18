// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/tokenizer.h"

#include "base/logging.h"
#include "tools/gn/input_file.h"

namespace {

bool CouldBeTwoCharOperatorBegin(char c) {
  return c == '<' || c == '>' || c == '!' || c == '=' || c == '-' ||
         c == '+' || c == '|' || c == '&';
}

bool CouldBeTwoCharOperatorEnd(char c) {
  return c == '=' || c == '|' || c == '&';
}

bool CouldBeOneCharOperator(char c) {
  return c == '=' || c == '<' || c == '>' || c == '+' || c == '!' ||
         c == ':' || c == '|' || c == '&' || c == '-';
}

bool CouldBeOperator(char c) {
  return CouldBeOneCharOperator(c) || CouldBeTwoCharOperatorBegin(c);
}

bool IsScoperChar(char c) {
  return c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}';
}

Token::Type GetSpecificOperatorType(base::StringPiece value) {
  if (value == "=")
    return Token::EQUAL;
  if (value == "+")
    return Token::PLUS;
  if (value == "-")
    return Token::MINUS;
  if (value == "+=")
    return Token::PLUS_EQUALS;
  if (value == "-=")
    return Token::MINUS_EQUALS;
  if (value == "==")
    return Token::EQUAL_EQUAL;
  if (value == "!=")
    return Token::NOT_EQUAL;
  if (value == "<=")
    return Token::LESS_EQUAL;
  if (value == ">=")
    return Token::GREATER_EQUAL;
  if (value == "<")
    return Token::LESS_THAN;
  if (value == ">")
    return Token::GREATER_THAN;
  if (value == "&&")
    return Token::BOOLEAN_AND;
  if (value == "||")
    return Token::BOOLEAN_OR;
  if (value == "!")
    return Token::BANG;
  if (value == ".")
    return Token::DOT;
  return Token::INVALID;
}

}  // namespace

Tokenizer::Tokenizer(const InputFile* input_file, Err* err)
    : input_file_(input_file),
      input_(input_file->contents()),
      err_(err),
      cur_(0),
      line_number_(1),
      char_in_line_(1) {
}

Tokenizer::~Tokenizer() {
}

// static
std::vector<Token> Tokenizer::Tokenize(const InputFile* input_file, Err* err) {
  Tokenizer t(input_file, err);
  return t.Run();
}

std::vector<Token> Tokenizer::Run() {
  DCHECK(tokens_.empty());
  while (!done()) {
    AdvanceToNextToken();
    if (done())
      break;
    Location location = GetCurrentLocation();

    Token::Type type = ClassifyCurrent();
    if (type == Token::INVALID) {
      *err_ = GetErrorForInvalidToken(location);
      break;
    }
    size_t token_begin = cur_;
    AdvanceToEndOfToken(location, type);
    if (has_error())
      break;
    size_t token_end = cur_;

    base::StringPiece token_value(&input_.data()[token_begin],
                                  token_end - token_begin);

    if (type == Token::UNCLASSIFIED_OPERATOR)
      type = GetSpecificOperatorType(token_value);
    if (type == Token::IDENTIFIER) {
      if (token_value == "if")
        type = Token::IF;
      else if (token_value == "else")
        type = Token::ELSE;
      else if (token_value == "true")
        type = Token::TRUE_TOKEN;
      else if (token_value == "false")
        type = Token::FALSE_TOKEN;
    }

    // TODO(brettw) This just strips comments from the token stream. This
    // is probably wrong, they should be removed at a later stage so we can
    // do things like rewrite the file. But this makes the parser simpler and
    // is OK for now.
    if (type != Token::COMMENT)
      tokens_.push_back(Token(location, type, token_value));
  }
  if (err_->has_error())
    tokens_.clear();
  return tokens_;
}

// static
size_t Tokenizer::ByteOffsetOfNthLine(const base::StringPiece& buf, int n) {
  DCHECK_GT(n, 0);

  if (n == 1)
    return 0;

  int cur_line = 1;
  size_t cur_byte = 0;
  while (cur_byte < buf.size()) {
    if (IsNewline(buf, cur_byte)) {
      cur_line++;
      if (cur_line == n)
        return cur_byte + 1;
    }
    cur_byte++;
  }
  return static_cast<size_t>(-1);
}

// static
bool Tokenizer::IsNewline(const base::StringPiece& buffer, size_t offset) {
  DCHECK(offset < buffer.size());
  // We may need more logic here to handle different line ending styles.
  return buffer[offset] == '\n';
}


void Tokenizer::AdvanceToNextToken() {
  while (!at_end() && IsCurrentWhitespace())
    Advance();
}

Token::Type Tokenizer::ClassifyCurrent() const {
  DCHECK(!at_end());
  char next_char = cur_char();
  if (IsAsciiDigit(next_char))
    return Token::INTEGER;
  if (next_char == '"')
    return Token::STRING;

  // Note: '-' handled specially below.
  if (next_char != '-' && CouldBeOperator(next_char))
    return Token::UNCLASSIFIED_OPERATOR;

  if (IsIdentifierFirstChar(next_char))
    return Token::IDENTIFIER;

  if (next_char == '[')
    return Token::LEFT_BRACKET;
  if (next_char == ']')
    return Token::RIGHT_BRACKET;
  if (next_char == '(')
    return Token::LEFT_PAREN;
  if (next_char == ')')
    return Token::RIGHT_PAREN;
  if (next_char == '{')
    return Token::LEFT_BRACE;
  if (next_char == '}')
    return Token::RIGHT_BRACE;

  if (next_char == '.')
    return Token::DOT;
  if (next_char == ',')
    return Token::COMMA;

  if (next_char == '#')
    return Token::COMMENT;

  // For the case of '-' differentiate between a negative number and anything
  // else.
  if (next_char == '-') {
    if (!CanIncrement())
      return Token::UNCLASSIFIED_OPERATOR;  // Just the minus before end of
                                            // file.
    char following_char = input_[cur_ + 1];
    if (IsAsciiDigit(following_char))
      return Token::INTEGER;
    return Token::UNCLASSIFIED_OPERATOR;
  }

  return Token::INVALID;
}

void Tokenizer::AdvanceToEndOfToken(const Location& location,
                                    Token::Type type) {
  switch (type) {
    case Token::INTEGER:
      do {
        Advance();
      } while (!at_end() && IsAsciiDigit(cur_char()));
      if (!at_end()) {
        // Require the char after a number to be some kind of space, scope,
        // or operator.
        char c = cur_char();
        if (!IsCurrentWhitespace() && !CouldBeOperator(c) &&
            !IsScoperChar(c) && c != ',') {
          *err_ = Err(GetCurrentLocation(),
                      "This is not a valid number.",
                      "Learn to count.");
          // Highlight the number.
          err_->AppendRange(LocationRange(location, GetCurrentLocation()));
        }
      }
      break;

    case Token::STRING: {
      char initial = cur_char();
      Advance();  // Advance past initial "
      for (;;) {
        if (at_end()) {
          *err_ = Err(LocationRange(location, GetCurrentLocation()),
                      "Unterminated string literal.",
                      "Don't leave me hanging like this!");
          break;
        }
        if (IsCurrentStringTerminator(initial)) {
          Advance();  // Skip past last "
          break;
        } else if (cur_char() == '\n') {
          *err_ = Err(LocationRange(location, GetCurrentLocation()),
                      "Newline in string constant.");
        }
        Advance();
      }
      break;
    }

    case Token::UNCLASSIFIED_OPERATOR:
      // Some operators are two characters, some are one.
      if (CouldBeTwoCharOperatorBegin(cur_char())) {
        if (CanIncrement() && CouldBeTwoCharOperatorEnd(input_[cur_ + 1]))
          Advance();
      }
      Advance();
      break;

    case Token::IDENTIFIER:
      while (!at_end() && IsIdentifierContinuingChar(cur_char()))
        Advance();
      break;

    case Token::LEFT_BRACKET:
    case Token::RIGHT_BRACKET:
    case Token::LEFT_BRACE:
    case Token::RIGHT_BRACE:
    case Token::LEFT_PAREN:
    case Token::RIGHT_PAREN:
    case Token::DOT:
    case Token::COMMA:
      Advance();  // All are one char.
      break;

    case Token::COMMENT:
      // Eat to EOL.
      while (!at_end() && !IsCurrentNewline())
        Advance();
      break;

    case Token::INVALID:
    default:
      *err_ = Err(location, "Everything is all messed up",
                  "Please insert system disk in drive A: and press any key.");
      NOTREACHED();
      return;
  }
}

bool Tokenizer::IsCurrentWhitespace() const {
  DCHECK(!at_end());
  char c = input_[cur_];
  // Note that tab (0x09) is illegal.
  return c == 0x0A || c == 0x0B || c == 0x0C || c == 0x0D || c == 0x20;
}

bool Tokenizer::IsCurrentStringTerminator(char quote_char) const {
  DCHECK(!at_end());
  if (cur_char() != quote_char)
    return false;

  // Check for escaping. \" is not a string terminator, but \\" is. Count
  // the number of preceeding backslashes.
  int num_backslashes = 0;
  for (int i = static_cast<int>(cur_) - 1; i >= 0 && input_[i] == '\\'; i--)
    num_backslashes++;

  // Even backslashes mean that they were escaping each other and don't count
  // as escaping this quote.
  return (num_backslashes % 2) == 0;
}

bool Tokenizer::IsCurrentNewline() const {
  return IsNewline(input_, cur_);
}

void Tokenizer::Advance() {
  DCHECK(cur_ < input_.size());
  if (IsCurrentNewline()) {
    line_number_++;
    char_in_line_ = 1;
  } else {
    char_in_line_++;
  }
  cur_++;
}

Location Tokenizer::GetCurrentLocation() const {
  return Location(input_file_, line_number_, char_in_line_);
}

Err Tokenizer::GetErrorForInvalidToken(const Location& location) const {
  std::string help;
  if (cur_char() == ';') {
    // Semicolon.
    help = "Semicolons are not needed, delete this one.";
  } else if (cur_char() == '\t') {
    // Tab.
    help = "You got a tab character in here. Tabs are evil. "
           "Convert to spaces.";
  } else if (cur_char() == '/' && cur_ + 1 < input_.size() &&
      (input_[cur_ + 1] == '/' || input_[cur_ + 1] == '*')) {
    // Different types of comments.
    help = "Comments should start with # instead";
  } else {
    help = "I have no idea what this is.";
  }

  return Err(location, "Invalid token.", help);
}
