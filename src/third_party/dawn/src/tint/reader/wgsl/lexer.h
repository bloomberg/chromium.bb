// Copyright 2020 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_TINT_READER_WGSL_LEXER_H_
#define SRC_TINT_READER_WGSL_LEXER_H_

#include <string>

#include "src/tint/reader/wgsl/token.h"

namespace tint::reader::wgsl {

/// Converts the input stream into a series of Tokens
class Lexer {
 public:
  /// Creates a new Lexer
  /// @param file the source file
  explicit Lexer(const Source::File* file);
  ~Lexer();

  /// Returns the next token in the input stream.
  /// @return Token
  Token next();

 private:
  /// Advances past blankspace and comments, if present at the current position.
  /// @returns error token, EOF, or uninitialized
  Token skip_blankspace_and_comments();
  /// Advances past a comment at the current position, if one exists.
  /// Returns an error if there was an unterminated block comment,
  /// or a null character was present.
  /// @returns uninitialized token on success, or error
  Token skip_comment();

  Token build_token_from_int_if_possible(Source source,
                                         size_t start,
                                         size_t end,
                                         int32_t base);
  Token check_keyword(const Source&, std::string_view);

  /// The try_* methods have the following in common:
  /// - They assume there is at least one character to be consumed,
  ///   i.e. the input has not yet reached end of file.
  /// - They return an initialized token when they match and consume
  ///   a token of the specified kind.
  /// - Some can return an error token.
  /// - Otherwise they return an uninitialized token when they did not
  ///   match a token of the specfied kind.
  Token try_float();
  Token try_hex_float();
  Token try_hex_integer();
  Token try_ident();
  Token try_integer();
  Token try_punctuation();

  Source begin_source() const;
  void end_source(Source&) const;

  /// @returns true if the end of the input has been reached.
  bool is_eof() const;
  /// @returns true if there is another character on the input and
  /// it is not null.
  bool is_null() const;
  /// @param ch a character
  /// @returns true if 'ch' is a decimal digit
  bool is_digit(char ch) const;
  /// @param ch a character
  /// @returns true if 'ch' is a hexadecimal digit
  bool is_hex(char ch) const;
  bool matches(size_t pos, std::string_view substr);

  /// The source file content
  Source::File const* const file_;
  /// The length of the input
  uint32_t len_ = 0;
  /// The current position in utf-8 code units (bytes) within the input
  uint32_t pos_ = 0;
  /// The current location within the input
  Source::Location location_;
};

}  // namespace tint::reader::wgsl

#endif  // SRC_TINT_READER_WGSL_LEXER_H_
