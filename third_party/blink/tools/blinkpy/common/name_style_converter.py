# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error,print-statement,relative-import

import re

SPECIAL_TOKENS = [
    # This list should be sorted by length.
    'Float32',
    'Float64',
    'IFrame',
    'Uint16',
    'Uint32',
    'WebGL2',
    'DList',
    'Int16',
    'Int32',
    'MPath',
    'TSpan',
    'UList',
    'UTF16',
    'Uint8',
    'WebGL',
    'XPath',
    'ETC1',
    'HTML',
    'Int8',
    'S3TC',
    'UTF8',
    'CSS',
    'EXT',
    'RTC',
    'SVG',
    'V8',
]

MATCHING_EXPRESSION = '((?:[A-Z][a-z]+)|[0-9]D?$)'


class SmartTokenizer(object):
    """Detects special cases that are not easily discernible without additional
       knowledge, such as recognizing that in SVGSVGElement, the first two SVGs
       are separate tokens, but WebGL is one token."""

    def __init__(self, name):
        self.remaining = name

    def tokenize(self):
        name = self.remaining
        tokens = []
        while len(name) > 0:
            matched_token = None
            for token in SPECIAL_TOKENS:
                if name.startswith(token):
                    matched_token = token
                    break
            if not matched_token:
                match = re.search(MATCHING_EXPRESSION, name)
                if not match:
                    matched_token = name
                elif match.start(0) != 0:
                    matched_token = name[:match.start(0)]
                else:
                    matched_token = match.group(0)
            tokens.append(name[:len(matched_token)])
            name = name[len(matched_token):]
        return tokens


class NameStyleConverter(object):
    """Converts names from camelCase to various other styles.
    """

    def __init__(self, name):
        self.tokens = self.tokenize(name)

    def tokenize(self, name):
        tokenizer = SmartTokenizer(name)
        return tokenizer.tokenize()

    def to_snake_case(self):
        """Snake case is the file and variable name style per Google C++ Style
           Guide:
           https://google.github.io/styleguide/cppguide.html#Variable_Names

           Also known as the hacker case.
           https://en.wikipedia.org/wiki/Snake_case
        """
        return '_'.join([token.lower() for token in self.tokens])

    def to_upper_camel_case(self):
        """Upper-camel case is the class and function name style per
           Google C++ Style Guide:
           https://google.github.io/styleguide/cppguide.html#Function_Names

           Also known as the PascalCase.
           https://en.wikipedia.org/wiki/Camel_case.
        """
        return ''.join([token[0].upper() + token[1:] for token in self.tokens])

    def to_macro_case(self):
        """Macro case is the macro name style per Google C++ Style Guide:
           https://google.github.io/styleguide/cppguide.html#Macro_Names
        """
        return '_'.join([token.upper() for token in self.tokens])

    def to_all_cases(self):
        return {
            'snake_case': self.to_snake_case(),
            'upper_camel_case': self.to_upper_camel_case(),
            'macro_case': self.to_macro_case(),
        }
