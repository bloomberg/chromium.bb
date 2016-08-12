# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A 2to3 fixer that reformats docstrings.

This should transform docstrings to be closer to the conventions in pep-0257;
see https://www.python.org/dev/peps/pep-0257/.
"""

import re

from lib2to3.fixer_base import BaseFix
from lib2to3.pgen2 import token


class FixDocstrings(BaseFix):

    explicit = True
    _accept_type = token.STRING

    def match(self, node):
        return node.value.startswith('"""') and node.prev_sibling is None

    def transform(self, node, results):
        # First, strip whitespace at the beginning and end.
        node.value = re.sub(r'^"""\s+', '"""', node.value)
        node.value = re.sub(r'\s+"""$', '"""', node.value)

        # For multi-line docstrings, the closing quotes should go on their own line.
        if '\n' in node.value:
            indent = re.search(r'\n( *)\S', node.value).group(1)
            node.value = re.sub(r'"""$', '\n' + indent + '"""', node.value)

        node.changed()
