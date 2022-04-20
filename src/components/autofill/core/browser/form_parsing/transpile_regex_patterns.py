#!/usr/bin/env python

# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import json
import sys

# Generates a set of C++ constexpr constants to facilitate lookup of a set of
# MatchingPatterns by a given tuple (pattern name, language code).
#
# The constants are:
#
# - kPatterns is an array of MatchingPatterns without duplicates.
#   The array is not sorted. The patterns are condensed (see below).
# - kPatterns_<pattern name>_<language code> contains the indices of the
#   pattterns that for that pattern name and language code.
# - kPatternMap is a fixed flat map from (pattern name, language code) to
#   a span of MatchPatternRefs.
#
# This representation has larger binary size on Android than using nested
# lookup pattern name -> language code -> span of MatchPatternRefs, but it's
# significantly simpler.
def generate_cpp_constants(name_to_lang_to_patterns):
  # The enum constant of the MatchAttribute::kName.
  kName = 1
  yield f'static_assert(MatchAttribute::kName == To<MatchAttribute,{kName}>());'
  yield ''

  # Stores a `key` in `dictionary` and assigns it a natural number.
  #
  # For example, after memoize("foo", d) and memoize("bar", d),
  # d = {"foo": 0, "bar": 1}. This is useful to generate a C++ array
  # {"foo", "bar"} and referring to these elements by their indices
  # 0 and 1.
  def memoize(key, dictionary):
    if key not in dictionary:
      dictionary[key] = len(dictionary)
    return dictionary[key]

  # Maps a Python Boolean to a C++ Boolean literal.
  def python_bool_to_cpp(b):
    return 'true' if b else 'false'

  # Maps a string literal to a C++ string literal.
  def json_to_cpp_string_literal(json_string_literal):
    return json.dumps(json_string_literal or '')

  # Maps a string literal to a C++ UTF-16 string literal.
  def json_to_cpp_u16string_literal(json_string_literal):
    return 'u'+ json.dumps(json_string_literal or '')

  # Maps a list of natural numbers to a DenseSet<MatchAttribute> expression.
  def json_to_cpp_match_field_attributes(enum_values):
    return f'DenseSet<MatchAttribute>{{' \
           f"{','.join(f'To<MatchAttribute,{i}>()' for i in enum_values)}" \
           f'}}'

  # Maps a list of natural numbers to a DenseSet<MatchFieldType> expression.
  def json_to_cpp_match_field_input_types(enum_values):
    return f'DenseSet<MatchFieldType>{{' \
           f"{','.join(f'To<MatchFieldType,{i}>()' for i in enum_values)}" \
           f'}}'

  # Maps a JSON object representing a pattern to a C++ MatchingPattern
  # expression.
  def json_to_cpp_pattern(json):
    positive_pattern = json_to_cpp_u16string_literal(json['positive_pattern'])
    negative_pattern = json_to_cpp_u16string_literal(json['negative_pattern'])
    positive_score = json['positive_score']
    match_field_attributes = json_to_cpp_match_field_attributes(
        json['match_field_attributes'])
    match_field_input_types = json_to_cpp_match_field_input_types(
        json['match_field_input_types'])
    return f'MatchingPattern{{\n' \
           f'  .positive_pattern = {positive_pattern},\n' \
           f'  .negative_pattern = {negative_pattern},\n' \
           f'  .positive_score = {positive_score},\n' \
           f'  .match_field_attributes = {match_field_attributes},\n' \
           f'  .match_field_input_types = {match_field_input_types},\n' \
           f'}}'

  # Name of the auxiliary C++ constant.
  def kPatterns(name, lang):
    if lang:
      return f"kPatterns__{name}__{lang.replace('-', '_')}"
    else:
      return f"kPatterns__{name}"

  # Validate the JSON input.
  for name, lang_to_patterns in name_to_lang_to_patterns.items():
    if '' in lang_to_patterns:
      raise Exception('JSON format error: language is ""')

  # Remember each pattern's language.
  #
  # To ease debugging, we shall sort patterns of equal positive_score by their
  # language.
  for lang_to_patterns in name_to_lang_to_patterns.values():
    for lang, patterns in lang_to_patterns.items():
      for pattern in patterns:
        pattern['lang'] = lang

  # For each name, collect the items of all languages and add them as a
  # separate entry for the pseudo-language ''.
  for name, lang_to_patterns in name_to_lang_to_patterns.items():
    lang_to_patterns[''] = [
        p.copy() for ps in lang_to_patterns.values() for p in ps
    ]

  # Add the English patterns to all languages except for English itself and the
  # catch-all language ''.
  #
  # The idea is that these patterns should be applied (only) to the HTML source
  # code, i.e., not the user-visible labels. To this end, we mark the English
  # patterns here "supplementary", which will in the subsequent step be encoded
  # in the MatchPatternRef().
  for lang_to_patterns in name_to_lang_to_patterns.values():
    if 'en' not in lang_to_patterns:
      continue
    def make_supplementary_pattern(p):
      p = p.copy()
      p['supplementary'] = True
      return p
    for patterns in (patterns for lang, patterns in lang_to_patterns.items()
                     if lang not in ['', 'en']):
      patterns.extend(
          make_supplementary_pattern(p)
          for p in lang_to_patterns['en']
          if kName in p['match_field_attributes'])

  # Populate the two maps:
  # - a map from C++ MatchingPattern expressions to their index.
  # - a map from names and languages to the their MatchingPatterns, represented
  #   as list of tuples (is_supplementary, pattern_index).
  pattern_to_index = {}
  name_to_lang_to_patternrefs = {
      name: {lang: [] for lang in lang_to_patterns.keys()
            } for name, lang_to_patterns in name_to_lang_to_patterns.items()
  }
  for name, lang_to_patterns in sorted(name_to_lang_to_patterns.items()):
    for lang, patterns in sorted(lang_to_patterns.items()):
      patternrefs = name_to_lang_to_patternrefs[name][lang]
      sort_key = lambda p: (-p['positive_score'], p['lang'])
      for pattern in sorted(patterns, key=sort_key):
        is_supplementary = ('supplementary' in pattern and
                            pattern['supplementary'])
        index = memoize(json_to_cpp_pattern(pattern), pattern_to_index)
        patternrefs.append((is_supplementary, index))

  # Generate the C++ constants.
  yield '// The patterns. Referred to by their index in MatchPatternRef.'
  yield 'constexpr std::array kPatterns{'
  for cpp_expr, index in sorted(
      pattern_to_index.items(), key=lambda item: item[1]):
    yield f'/*[{index}]=*/{cpp_expr},'
  yield '};'

  yield '\n// The patterns for field types and languages.'
  yield '// They are sorted by the patterns MatchingPattern::positive_score.'
  for name, lang_to_patternrefs in name_to_lang_to_patternrefs.items():
    for lang, patternrefs in lang_to_patternrefs.items():
      yield (f'constexpr std::array {kPatterns(name, lang)}{{' + ', '.join(
          f'MakeMatchPatternRef({python_bool_to_cpp(is_supplementary)},{index})'
          for is_supplementary, index in patternrefs) + f'}};')

  yield '\n// The lookup map for field types and langs.'
  yield '// The keys in the map are essentially const char* pairs.'
  yield '// It also allows for lookup by base::StringPiece pairs (because the'
  yield '// comparator is transparently accepts base::StringPiece pairs).'
  yield 'constexpr auto kPatternMap = base::MakeFixedFlatMap<' \
        'NameAndLanguage, base::span<const MatchPatternRef>>({'
  for name, lang_to_patternrefs in sorted(name_to_lang_to_patternrefs.items()):
    for lang in sorted(lang_to_patternrefs.keys()):
      yield f'  {{{{"{name}", "{lang}"}}, {kPatterns(name, lang)}}},'
  yield '}, NameAndLanguageComparator());'

def generate_cpp_lines(input_json):
  yield """// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_REGEX_PATTERNS_INL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_REGEX_PATTERNS_INL_H_

#include <array>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/strings/string_piece.h"

#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"

namespace autofill {

// Wrapper of MatchPatternRef's private constructor.
// It's a friend of MatchPatternRef.
constexpr MatchPatternRef MakeMatchPatternRef(
    bool is_supplementary,
    MatchPatternRef::UnderlyingType index) {
  return MatchPatternRef(is_supplementary, index);
}

// Converts an integer to the associated enum class constant.
template<typename Enum, int i>
constexpr Enum To() {
  static_assert(0 <= i);
  static_assert(static_cast<Enum>(i) <= Enum::kMaxValue);
  return static_cast<Enum>(i);
}

// A pair of const char* used as keys in the `kPatternMap`.
struct NameAndLanguage {
  using StringPiecePair = std::pair<base::StringPiece, base::StringPiece>;

  // By this implicit conversion, the below comparator can be called for
  // NameAndLanguageComparator and StringPiecePairs alike.
  constexpr operator StringPiecePair() const {
    return {base::StringPiece(name), base::StringPiece(lang)};
  }

  const char* name;  // A pattern name.
  const char* lang;  // A language code.
};

// A less-than relation on NameAndLanguage and/or base::StringPiece pairs.
struct NameAndLanguageComparator {
  using is_transparent = void;

  // By way of the implicit conversion from NameAndLanguage to StringPiecePair,
  // this function also accepts NameAndLanguage.
  //
  // To implement constexpr lexicographic comparison of const char* with the
  // standard library, we need to compute both the lengths of the strings before
  // we can actually compare the strings. A simple way of doing so is to convert
  // each const char* to a base::StringPiece and then comparing the
  // base::StringPieces.
  //
  // This is exactly what the comparator does: when an argument is a
  // NameAndLanguage, it is implicitly converted to a StringPiecePair, which
  // is then compared to the other StringPiecePair.
  constexpr bool operator()(
      NameAndLanguage::StringPiecePair a,
      NameAndLanguage::StringPiecePair b) const {
    int cmp = a.first.compare(b.first);
    return cmp < 0 || (cmp == 0 && a.second.compare(b.second) < 0);
  }
};
"""
  yield from generate_cpp_constants(input_json)
  yield """

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_REGEX_PATTERNS_INL_H_
"""

def build_cpp_file(input_json, output_handle):
  for line in generate_cpp_lines(input_json):
    line += '\n'
    # unicode() exists and is necessary only in Python 2, not in Python 3.
    if sys.version_info[0] < 3:
      line = unicode(s, 'utf-8')
    output_handle.write(line)

if __name__ == '__main__':
  input_file = sys.argv[1]
  output_file = sys.argv[2]
  with io.open(input_file, 'r', encoding='utf-8') as input_handle:
    input_json = json.load(input_handle)
    with io.open(output_file, 'w', encoding='utf-8') as output_handle:
      build_cpp_file(input_json, output_handle)
