# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A 2to3 fixer that converts all string literals to use double quotes.

Strings that contain double quotes will not be modified. Prefixed string
literals will also not be modified. This affects both single-quoted strings
and triple-single-quoted strings.
"""

from lib2to3.fixer_base import BaseFix
from lib2to3.pgen2 import token


class FixDoubleQuoteStrings(BaseFix):

    explicit = True
    _accept_type = token.STRING

    def match(self, node):
        res = node.value.startswith("'") and '"' not in node.value[1:-1]
        return res

    def transform(self, node, results):
        node.value = node.value.replace("'", '"')
        node.changed()
