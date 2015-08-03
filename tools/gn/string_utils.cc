// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/string_utils.h"

#include "tools/gn/err.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parser.h"
#include "tools/gn/scope.h"
#include "tools/gn/token.h"
#include "tools/gn/tokenizer.h"
#include "tools/gn/value.h"

namespace {

// Constructs an Err indicating a range inside a string. We assume that the
// token has quotes around it that are not counted by the offset.
Err ErrInsideStringToken(const Token& token, size_t offset, size_t size,
                         const std::string& msg,
                         const std::string& help = std::string()) {
  // The "+1" is skipping over the " at the beginning of the token.
  int int_offset = static_cast<int>(offset);
  Location begin_loc(token.location().file(),
                     token.location().line_number(),
                     token.location().char_offset() + int_offset + 1,
                     token.location().byte() + int_offset + 1);
  Location end_loc(
      token.location().file(),
      token.location().line_number(),
      token.location().char_offset() + int_offset + 1 + static_cast<int>(size),
      token.location().byte() + int_offset + 1 + static_cast<int>(size));
  return Err(LocationRange(begin_loc, end_loc), msg, help);
}

// Notes about expression interpolation. This is based loosly on Dart but is
// slightly less flexible. In Dart, seeing the ${ in a string is something
// the toplevel parser knows about, and it will recurse into the block
// treating it as a first-class {...} block. So even things like this work:
//   "hello ${"foo}"*2+"bar"}"  =>  "hello foo}foo}bar"
// (you can see it did not get confused by the nested strings or the nested "}"
// inside the block).
//
// This is cool but complicates the parser for almost no benefit for this
// non-general-purpose programming language. The main reason expressions are
// supported here at all are to support "${scope.variable}" and "${list[0]}",
// neither of which have any of these edge-cases.
//
// In this simplified approach, we search for the terminating '}' and execute
// the result. This means we can't support any expressions with embedded '}'
// or '"'. To keep people from getting confusing about what's supported and
// what's not, only identifier and accessor expressions are allowed (neither
// of these run into any of these edge-cases).
bool AppendInterpolatedExpression(Scope* scope,
                                  const Token& token,
                                  const char* input,
                                  size_t begin_offset,
                                  size_t end_offset,
                                  std::string* output,
                                  Err* err) {
  SourceFile empty_source_file;  // Prevent most vexing parse.
  InputFile input_file(empty_source_file);
  input_file.SetContents(
      std::string(&input[begin_offset], end_offset - begin_offset));

  // Tokenize.
  std::vector<Token> tokens = Tokenizer::Tokenize(&input_file, err);
  if (err->has_error()) {
    // The error will point into our temporary buffer, rewrite it to refer
    // to the original token. This will make the location information less
    // precise, but generally there won't be complicated things in string
    // interpolations.
    *err = ErrInsideStringToken(token, begin_offset, end_offset - begin_offset,
                                err->message(), err->help_text());
    return false;
  }

  // Parse.
  scoped_ptr<ParseNode> node = Parser::ParseExpression(tokens, err);
  if (err->has_error()) {
    // Rewrite error as above.
    *err = ErrInsideStringToken(token, begin_offset, end_offset - begin_offset,
                                err->message(), err->help_text());
    return false;
  }
  if (!(node->AsIdentifier() || node->AsAccessor())) {
    *err = ErrInsideStringToken(token, begin_offset, end_offset - begin_offset,
        "Invalid string interpolation.",
        "The thing inside the ${} must be an identifier ${foo},\n"
        "a scope access ${foo.bar}, or a list access ${foo[0]}.");
    return false;
  }

  // Evaluate.
  Value result = node->Execute(scope, err);
  if (err->has_error()) {
    // Rewrite error as above.
    *err = ErrInsideStringToken(token, begin_offset, end_offset - begin_offset,
                                err->message(), err->help_text());
    return false;
  }

  output->append(result.ToString(false));
  return true;
}

bool AppendInterpolatedIdentifier(Scope* scope,
                                  const Token& token,
                                  const char* input,
                                  size_t begin_offset,
                                  size_t end_offset,
                                  std::string* output,
                                  Err* err) {
  base::StringPiece identifier(&input[begin_offset],
                               end_offset - begin_offset);
  const Value* value = scope->GetValue(identifier, true);
  if (!value) {
    // We assume the input points inside the token.
    *err = ErrInsideStringToken(
        token, identifier.data() - token.value().data() - 1, identifier.size(),
        "Undefined identifier in string expansion.",
        std::string("\"") + identifier + "\" is not currently in scope.");
    return false;
  }

  output->append(value->ToString(false));
  return true;
}

// Handles string interpolations: $identifier and ${expression}
//
// |*i| is the index into |input| of the $. This will be updated to point to
// the last character consumed on success. The token is the original string
// to blame on failure.
//
// On failure, returns false and sets the error. On success, appends the
// result of the interpolation to |*output|.
bool AppendStringInterpolation(Scope* scope,
                               const Token& token,
                               const char* input, size_t size,
                               size_t* i,
                               std::string* output,
                               Err* err) {
  size_t dollars_index = *i;
  (*i)++;
  if (*i == size) {
    *err = ErrInsideStringToken(token, dollars_index, 1, "$ at end of string.",
        "I was expecting an identifier or {...} after the $.");
    return false;
  }

  if (input[*i] == '{') {
    // Bracketed expression.
    (*i)++;
    size_t begin_offset = *i;

    // Find the closing } and check for non-identifier chars. Don't need to
    // bother checking for the more-restricted first character of an identifier
    // since the {} unambiguously denotes the range, and identifiers with
    // invalid names just won't be found later.
    bool has_non_ident_chars = false;
    while (*i < size && input[*i] != '}') {
      has_non_ident_chars |= Tokenizer::IsIdentifierContinuingChar(input[*i]);
      (*i)++;
    }
    if (*i == size) {
      *err = ErrInsideStringToken(token, dollars_index, *i - dollars_index,
                                  "Unterminated ${...");
      return false;
    }

    // In the common case, the thing inside the {} will actually be a
    // simple identifier. Avoid all the complicated parsing of accessors
    // in this case.
    if (!has_non_ident_chars) {
      return AppendInterpolatedIdentifier(scope, token, input, begin_offset,
                                          *i, output, err);
    }
    return AppendInterpolatedExpression(scope, token, input, begin_offset, *i,
                                        output, err);
  }

  // Simple identifier.
  // The first char of an identifier is more restricted.
  if (!Tokenizer::IsIdentifierFirstChar(input[*i])) {
    *err = ErrInsideStringToken(
        token, dollars_index, *i - dollars_index + 1,
        "$ not followed by an identifier char.",
        "It you want a literal $ use \"\\$\".");
    return false;
  }
  size_t begin_offset = *i;
  (*i)++;

  // Find the first non-identifier char following the string.
  while (*i < size && Tokenizer::IsIdentifierContinuingChar(input[*i]))
    (*i)++;
  size_t end_offset = *i;
  (*i)--;  // Back up to mark the last character consumed.
  return AppendInterpolatedIdentifier(scope, token, input, begin_offset,
                                      end_offset, output, err);
}

}  // namespace

bool ExpandStringLiteral(Scope* scope,
                         const Token& literal,
                         Value* result,
                         Err* err) {
  DCHECK(literal.type() == Token::STRING);
  DCHECK(literal.value().size() > 1);  // Should include quotes.
  DCHECK(result->type() == Value::STRING);  // Should be already set.

  // The token includes the surrounding quotes, so strip those off.
  const char* input = &literal.value().data()[1];
  size_t size = literal.value().size() - 2;

  std::string& output = result->string_value();
  output.reserve(size);
  for (size_t i = 0; i < size; i++) {
    if (input[i] == '\\') {
      if (i < size - 1) {
        switch (input[i + 1]) {
          case '\\':
          case '"':
          case '$':
            output.push_back(input[i + 1]);
            i++;
            continue;
          default:  // Everything else has no meaning: pass the literal.
            break;
        }
      }
      output.push_back(input[i]);
    } else if (input[i] == '$') {
      if (!AppendStringInterpolation(scope, literal, input, size, &i,
                                     &output, err))
        return false;
    } else {
      output.push_back(input[i]);
    }
  }
  return true;
}

std::string RemovePrefix(const std::string& str, const std::string& prefix) {
  CHECK(str.size() >= prefix.size() &&
        str.compare(0, prefix.size(), prefix) == 0);
  return str.substr(prefix.size());
}

void TrimTrailingSlash(std::string* str) {
  if (!str->empty()) {
    DCHECK((*str)[str->size() - 1] == '/');
    str->resize(str->size() - 1);
  }
}
