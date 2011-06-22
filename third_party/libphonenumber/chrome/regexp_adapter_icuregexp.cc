// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libphonenumber/cpp/src/regexp_adapter.h"

#include <map>

// Setup all of the Chromium and WebKit defines
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "unicode/regex.h"
#include "unicode/stringpiece.h"
#include "unicode/unistr.h"

namespace {

// Converts |source| to UTF-8 string, returns it starting at position |pos|.
std::string UnicodeStringToUtf8String(icu::UnicodeString const& source,
                                      int pos) {
  std::string data;
  source.toUTF8String<std::string>(data);
  return data.substr(pos);
}

}  // namespace

// Implementation of the abstract classes RegularExpressionInput and
// RegularExpression using ICU regular expression capabilities.

// The Regular Expression input class.
class IcuRegularExpressionInput : public reg_exp::RegularExpressionInput {
 public:
  explicit IcuRegularExpressionInput(const char* utf8_input);

  // RegularExpressionInput implementation:
  // Matches string to regular expression, returns true if expression was
  // matched, false otherwise, advances position in the match.
  // |reg_exp| - expression to be matched.
  // |beginning_only| - if true match would be successfull only if appears at
  // the beginning of the tested region of the string.
  // |matched_string1| - successfully matched first string. Can be NULL.
  // |matched_string2| - successfully matched second string. Can be NULL.
  virtual bool ConsumeRegExp(std::string const& reg_exp,
                             bool beginning_only,
                             std::string* matched_string1,
                             std::string* matched_string2);

  // Convert unmatched input to a string.
  virtual std::string ToString() const;

  icu::UnicodeString* Data() { return &utf8_input_; }

  // Position in the input. For the newly created input position is 0,
  // each call to ConsumeRegExp() or RegularExpression::Consume() advances
  // position in the case of the successful match to be after the match.
  int pos() const { return pos_; }
  void set_pos(int pos) { pos_ = pos; }

 private:
  icu::UnicodeString utf8_input_;
  int pos_;

  DISALLOW_COPY_AND_ASSIGN(IcuRegularExpressionInput);
};

// The regular expression class.
class IcuRegularExpression : public reg_exp::RegularExpression {
 public:
  explicit IcuRegularExpression(const char* utf8_regexp);

  // RegularExpression implementation:
  // Matches string to regular expression, returns true if expression was
  // matched, false otherwise, advances position in the match.
  // |input_string| - string to be searched.
  // |beginning_only| - if true match would be successfull only if appears at
  // the beginning of the tested region of the string.
  // |matched_string1| - successfully matched first string. Can be NULL.
  // |matched_string2| - successfully matched second string. Can be NULL.
  // |matched_string3| - successfully matched third string. Can be NULL.
  virtual bool Consume(reg_exp::RegularExpressionInput* input_string,
                       bool beginning_only,
                       std::string* matched_string1,
                       std::string* matched_string2,
                       std::string* matched_string3) const;

  // Matches string to regular expression, returns true if expression was
  // matched, false otherwise.
  // |input_string| - string to be searched.
  // |full_match| - if true match would be successfull only if it matches the
  // complete string.
  // |matched_string| - successfully matched string. Can be NULL.
  virtual bool Match(const char* input_string,
                     bool full_match,
                     std::string* matched_string) const;

  // Replaces match(es) in the |string_to_process|. if |global| is true,
  // replaces all the matches, only the first match otherwise.
  // |replacement_string| - text the matches are replaced with.
  // Returns true if expression successfully processed through the string,
  // even if no actual replacements were made. Returns false in case of an
  // error.
  virtual bool Replace(std::string* string_to_process,
                       bool global,
                       const char* replacement_string) const;
 private:
  icu::RegexPattern utf8_regexp_;

  DISALLOW_COPY_AND_ASSIGN(IcuRegularExpression);
};

class RegExpCache {
 public:
  static RegExpCache* GetInstance();
  const icu::RegexPattern& GetRegExp(const std::string& reg_exp);

 private:
  std::map<std::string, icu::RegexPattern> reg_exp_cache_;
  icu::RegexPattern empty_regexp_;
};

RegExpCache* RegExpCache::GetInstance() {
  return Singleton<RegExpCache>::get();
}

const icu::RegexPattern& RegExpCache::GetRegExp(const std::string& reg_exp) {
  std::map<std::string, icu::RegexPattern>::iterator it =
      reg_exp_cache_.find(reg_exp);
  UParseError pe;
  UErrorCode status = U_ZERO_ERROR;
  if (it == reg_exp_cache_.end()) {
    scoped_ptr<icu::RegexPattern> pattern(icu::RegexPattern::compile(
        icu::UnicodeString::fromUTF8(reg_exp), 0, pe, status));
    if (U_SUCCESS(status)) {
      it = reg_exp_cache_.insert(std::make_pair(reg_exp, *pattern)).first;
    } else {
      // All of the passed regular expressions should compile correctly.
      NOTREACHED();
      return empty_regexp_;
    }
  }
  return it->second;
}

IcuRegularExpressionInput::IcuRegularExpressionInput(const char* utf8_input)
    : pos_(0) {
  DCHECK(utf8_input);
  utf8_input_ = icu::UnicodeString::fromUTF8(utf8_input);
}

bool IcuRegularExpressionInput::ConsumeRegExp(std::string const& reg_exp,
                                              bool beginning_only,
                                              std::string* matched_string1,
                                              std::string* matched_string2) {
  IcuRegularExpression re(reg_exp.c_str());

  return re.Consume(this, beginning_only, matched_string1, matched_string2,
                    NULL);
}

std::string IcuRegularExpressionInput::ToString() const {
  if (pos_ < 0 || pos_ > utf8_input_.length())
    return std::string();
  return UnicodeStringToUtf8String(utf8_input_, pos_);
}

IcuRegularExpression::IcuRegularExpression(const char* utf8_regexp) {
  DCHECK(utf8_regexp);
  std::string regexp_key(utf8_regexp);
  utf8_regexp_ = RegExpCache::GetInstance()->GetRegExp(regexp_key);
}

bool IcuRegularExpression::Consume(
    reg_exp::RegularExpressionInput* input_string,
    bool beginning_only,
    std::string* matched_string1,
    std::string* matched_string2,
    std::string* matched_string3) const {
  DCHECK(input_string);
  // matched_string1 may be NULL
  // matched_string2 may be NULL
  // matched_string3 may be NULL
  IcuRegularExpressionInput* input =
      reinterpret_cast<IcuRegularExpressionInput *>(input_string);
  UErrorCode status = U_ZERO_ERROR;
  scoped_ptr<icu::RegexMatcher> matcher(utf8_regexp_.matcher(*(input->Data()),
                                                             status));

  if (U_FAILURE(status))
    return false;

  if (beginning_only) {
    if (!matcher->lookingAt(input->pos(), status))
      return false;
  } else {
    if (!matcher->find(input->pos(), status))
      return false;
  }
  if (U_FAILURE(status))
    return false;
  // If less matches than expected - fail.
  if ((matched_string3 && matcher->groupCount() < 3) ||
      (matched_string2 && matcher->groupCount() < 2) ||
      (matched_string1 && matcher->groupCount() < 1)) {
    return false;
  }
  if (matcher->groupCount() > 0 && matched_string1) {
    *matched_string1 = UnicodeStringToUtf8String(matcher->group(1, status), 0);
  }
  if (matcher->groupCount() > 1 && matched_string2) {
    *matched_string2 = UnicodeStringToUtf8String(matcher->group(2, status), 0);
  }
  if (matcher->groupCount() > 2 && matched_string3) {
    *matched_string3 = UnicodeStringToUtf8String(matcher->group(3, status), 0);
  }
  input->set_pos(matcher->end(status));
  return true;
}

bool IcuRegularExpression::Match(const char* input_string,
                                 bool full_match,
                                 std::string* matched_string) const {
  DCHECK(input_string);
  // matched_string may be NULL

  IcuRegularExpressionInput input(input_string);
  UErrorCode status = U_ZERO_ERROR;
  scoped_ptr<icu::RegexMatcher> matcher(utf8_regexp_.matcher(*(input.Data()),
                                                             status));

  if (U_FAILURE(status))
    return false;

  if (full_match) {
    if (!matcher->matches(input.pos(), status))
      return false;
  } else {
    if (!matcher->find(input.pos(), status))
      return false;
  }
  if (U_FAILURE(status))
    return false;
  if (matcher->groupCount() > 0 && matched_string) {
    *matched_string = UnicodeStringToUtf8String(matcher->group(1, status), 0);
  }
  return true;
}

bool IcuRegularExpression::Replace(std::string* string_to_process,
                                   bool global,
                                   const char* replacement_string) const {
  DCHECK(string_to_process);
  DCHECK(replacement_string);

  std::string adapted_replacement(replacement_string);
  // Adapt replacement string from RE2 (\0-9 for matches) format to ICU format
  // ($0-9 for matches). All '$' should be prepended with '\' as well.
  size_t backslash_pos = adapted_replacement.find('\\');
  size_t dollar_pos = adapted_replacement.find('$');
  while (backslash_pos != std::string::npos ||
         dollar_pos != std::string::npos) {
    bool process_dollar = false;
    if (backslash_pos == std::string::npos ||
        (dollar_pos != std::string::npos && dollar_pos < backslash_pos)) {
      process_dollar = true;
    }
    if (process_dollar) {
      adapted_replacement.insert(dollar_pos, "\\");
      dollar_pos = adapted_replacement.find('$', dollar_pos + 2);
      if (backslash_pos != std::string::npos)
        ++backslash_pos;
    } else {
      if (adapted_replacement.length() > backslash_pos + 1) {
        if (adapted_replacement[backslash_pos + 1] >= '0' &&
            adapted_replacement[backslash_pos + 1] <= '9') {
          adapted_replacement[backslash_pos] = '$';
        }
        if (adapted_replacement[backslash_pos + 1] == '\\') {
          // Skip two characters instead of one.
          ++backslash_pos;
        }
      }
      backslash_pos = adapted_replacement.find('\\', backslash_pos + 1);
    }
  }

  IcuRegularExpressionInput input(string_to_process->c_str());
  UErrorCode status = U_ZERO_ERROR;
  scoped_ptr<icu::RegexMatcher> matcher(utf8_regexp_.matcher(*(input.Data()),
                                                             status));
  if (U_FAILURE(status))
    return false;

  icu::UnicodeString result;

  if (global) {
    result = matcher->replaceAll(
        icu::UnicodeString::fromUTF8(adapted_replacement),
        status);
  } else {
    result = matcher->replaceFirst(
        icu::UnicodeString::fromUTF8(adapted_replacement),
        status);
  }
  if (U_FAILURE(status))
    return false;
  *string_to_process = UnicodeStringToUtf8String(result, 0);
  return true;
}

namespace reg_exp {

RegularExpressionInput* CreateRegularExpressionInput(const char* utf8_input) {
  return new IcuRegularExpressionInput(utf8_input);
}

RegularExpression* CreateRegularExpression(const char* utf8_regexp) {
  return new IcuRegularExpression(utf8_regexp);
}

}  // namespace reg_exp
