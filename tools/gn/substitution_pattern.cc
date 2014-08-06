// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/substitution_pattern.h"

#include "base/strings/string_number_conversions.h"
#include "tools/gn/err.h"
#include "tools/gn/value.h"

SubstitutionPattern::Subrange::Subrange()
    : type(SUBSTITUTION_LITERAL) {
}

SubstitutionPattern::Subrange::Subrange(SubstitutionType t,
                                        const std::string& l)
    : type(t),
      literal(l) {
}

SubstitutionPattern::Subrange::~Subrange() {
}

SubstitutionPattern::SubstitutionPattern() {
}

SubstitutionPattern::~SubstitutionPattern() {
}

bool SubstitutionPattern::Parse(const Value& value, Err* err) {
  if (!value.VerifyTypeIs(Value::STRING, err))
    return false;
  return Parse(value.string_value(), value.origin(), err);
}

bool SubstitutionPattern::Parse(const std::string& str,
                                const ParseNode* origin,
                                Err* err) {
  DCHECK(ranges_.empty());  // Should only be called once.

  size_t cur = 0;
  while (true) {
    size_t next = str.find("{{", cur);

    // Pick up everything from the previous spot to here as a literal.
    if (next == std::string::npos) {
      if (cur != str.size())
        ranges_.push_back(Subrange(SUBSTITUTION_LITERAL, str.substr(cur)));
      break;
    } else if (next > cur) {
      ranges_.push_back(
          Subrange(SUBSTITUTION_LITERAL, str.substr(cur, next - cur)));
    }

    // Find which specific pattern this corresponds to.
    bool found_match = false;
    for (size_t i = SUBSTITUTION_FIRST_PATTERN;
         i < SUBSTITUTION_NUM_TYPES; i++) {
      const char* cur_pattern = kSubstitutionNames[i];
      size_t cur_len = strlen(cur_pattern);
      if (str.compare(next, cur_len, cur_pattern) == 0) {
        ranges_.push_back(Subrange(static_cast<SubstitutionType>(i)));
        cur = next + cur_len;
        found_match = true;
        break;
      }
    }

    // Expect all occurrances of {{ to resolve to a pattern.
    if (!found_match) {
      // Could make this error message more friendly if it comes up a lot. But
      // most people will not be writing substitution patterns and the code
      // to exactly indicate the error location is tricky.
      *err = Err(origin, "Unknown substitution pattern",
          "Found a {{ at offset " +
          base::SizeTToString(next) +
          " and did not find a known substitution following it.");
      ranges_.clear();
      return false;
    }
  }

  // Fill required types vector.
  bool required_type_bits[SUBSTITUTION_NUM_TYPES] = {0};
  FillRequiredTypes(required_type_bits);

  for (size_t i = SUBSTITUTION_FIRST_PATTERN; i < SUBSTITUTION_NUM_TYPES; i++) {
    if (required_type_bits[i])
      required_types_.push_back(static_cast<SubstitutionType>(i));
  }
  return true;
}

std::string SubstitutionPattern::AsString() const {
  std::string result;
  for (size_t i = 0; i < ranges_.size(); i++) {
    if (ranges_[i].type == SUBSTITUTION_LITERAL)
      result.append(ranges_[i].literal);
    else
      result.append(kSubstitutionNames[ranges_[i].type]);
  }
  return result;
}

void SubstitutionPattern::FillRequiredTypes(
    bool required_types[SUBSTITUTION_NUM_TYPES]) const {
  for (size_t i = 0; i < ranges_.size(); i++) {
    if (ranges_[i].type != SUBSTITUTION_LITERAL)
      required_types[static_cast<size_t>(ranges_[i].type)] = true;
  }
}
