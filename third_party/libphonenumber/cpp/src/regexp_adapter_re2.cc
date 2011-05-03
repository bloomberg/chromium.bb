// Copyright (C) 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: George Yakovlev
#include "regexp_adapter.h"

#include <re2/re2.h>
#include <re2/stringpiece.h>
#include <re2/re2.h>

namespace {
scoped_ptr<RE2Cache> re2_cache;
}  // namespace

class RE2RegularExpressionInput : public RegularExpressionInput {
 public:
  RE2RegularExpressionInput(const char* utf8_input);
  
  virtual bool ConsumeRegExp(std::string const& reg_exp,
                             bool beginning_only,
                             std::string* matched_string1,
                             std::string* matched_string2);
  virtual std::string ToString() const;
 private:
  StringPiece utf8_input_;
};


class RE2RegularExpression : public reg_exp::RegularExpression {
 public:
  RE2RegularExpression(const char* utf8_regexp);

  virtual bool Consume(reg_exp::RegularExpressionInput* input_string,
                       bool beginning_only,
                       std::string* matched_string1,
                       std::string* matched_string2,
                       std::string* matched_string3) const;

  virtual bool Match(const char* input_string,
                     bool full_match,
                     std::string* matched_string) const;

  virtual bool Replace(std::string* string_to_process,
                       bool global,
                       const char* replacement_string) const;
 private:
  RE2 utf8_regexp_;
};

RE2RegularExpressionInput::RE2RegularExpressionInput(const char* utf8_input)
    : utf8_input_(utf8_input) {
  DCHECK(utf8_input);
}
  
bool RE2RegularExpressionInput::ConsumeRegExp(std::string const& reg_exp,
                                              bool beginning_only,
                                              std::string* matched_string1,
                                              std::string* matched_string2) {
  if (beginning_only) {
    if (matched_string2)
      return RE2::Consume(&utf8_input_,
                          RE2Cache::ScopedAccess(re2_cache.get(), reg_exp),
                          matched_string1, matched_string2);
    else if (matched_string1)
      return RE2::Consume(&utf8_input_,
                          RE2Cache::ScopedAccess(re2_cache.get(), reg_exp),
                          matched_string1);
    else
      return RE2::Consume(&utf8_input_,
                          RE2Cache::ScopedAccess(re2_cache.get(), reg_exp));
  } else {
    if (matched_string2)
      return RE2::FindAndConsume(&utf8_input_,
                                 RE2Cache::ScopedAccess(re2_cache.get(),
                                                        reg_exp),
                                 matched_string1, matched_string2);
    else if (matched_string1)
      return RE2::FindAndConsume(&utf8_input_,
                                 RE2Cache::ScopedAccess(re2_cache.get(),
                                                        reg_exp),
                                 matched_string1);
    else
      return RE2::FindAndConsume(&utf8_input_,
                                 RE2Cache::ScopedAccess(re2_cache.get(),
                                                        reg_exp));
  }
}

std::string RE2RegularExpressionInput::ToString() const {
  utf8_input_.ToString();
}

RE2RegularExpression::RE2RegularExpression(const char* utf8_regexp)
    : utf8_regexp_(utf8_regexp) {
  DCHECK(utf8_regexp);
}

bool RE2RegularExpression::Consume(RegularExpressionInput* input_string,
                                   bool beginning_only,
                                   std::string* matched_string1,
                                   std::string* matched_string2,
                                   std::string* matched_string3) const {
  DCHECK(input_string);
  // matched_string1 may be NULL
  // matched_string2 may be NULL
  if (beginning_only) {
    if (matched_string3) {
      return RE2::Consume(input_string, utf8_regexp_,
                          matched_string1, matched_string2, matched_string3);
    } else if (matched_string2) {
      return RE2::Consume(input_string, utf8_regexp_,
                          matched_string1, matched_string2);
    } else if (matched_string1) {
      return RE2::Consume(input_string, utf8_regexp_, matched_string1);
    } else {
      return RE2::Consume(input_string, utf8_regexp_);
    }
  } else {
    if (matched_string3) {
      return RE2::FindAndConsume(input_string, utf8_regexp_,
                                 matched_string1, matched_string2,
                                 matched_string3);
    } else if (matched_string2) {
      return RE2::FindAndConsume(input_string, utf8_regexp_,
                                 matched_string1, matched_string2);
    } else if (matched_string1) {
      return RE2::FindAndConsume(input_string, utf8_regexp_, matched_string1);
    } else {
      return RE2::FindAndConsume(input_string, utf8_regexp_);
    }
  }
}

bool RE2RegularExpression::Match(const char* input_string,
                                 bool full_match,
                                 std::string* matched_string) const {
  DCHECK(input_string);
  // matched_string may be NULL
  if (full_match) {
    if (matched_string)
      return RE2::FullMatch(input_string, matched_string);
    else
      return RE2::FullMatch(input_string);
  } else {
    if (matched_string)
      return RE2::PartialMatch(input_string, matched_string);
    else
      return RE2::PartialMatch(input_string);
  }
}

bool RE2RegularExpression::Replace(std::string* string_to_process,
                                   bool global,
                                   const char* replacement_string) const {
  DCHECK(string_to_process);
  DCHECK(replacement_string);
  if (global) {
    StringPiece str(replacement_string);
    return RE2::GlobalReplace(string_to_process, str);
  } else {
    return RE2::Replace(string_to_process, replacement_string);
  }
}


namespace reg_exp {

RegularExpressionInput* CreateRegularExpressionInput(const char* utf8_input) {
  if (!re2_cache.get())
    re2_cache.reset(new RE2Cache(64));
  return new RE2RegularExpressionInput(utf8_input);
}

RegularExpression* CreateRegularExpression(const char* utf8_regexp) {
  if (!re2_cache.get())
    re2_cache.reset(new RE2Cache(64));
  return new RE2RegularExpression(utf8_regexp);
}

}  // namespace reg_exp

