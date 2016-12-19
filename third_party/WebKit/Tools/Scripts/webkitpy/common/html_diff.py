# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility for outputting a HTML diff of two multi-line strings.

The main purpose of this utility is to show the difference between
text baselines (-expected.txt files) and actual text results.

Note, in the standard library module difflib, there is also a HtmlDiff class,
although it outputs a larger and more complex HTML table than we need.
"""

import difflib

_TEMPLATE = """<html>
<head>
<style>.del { background: #faa; } .add { background: #afa; }</style>
</head>
<body>
<pre>%s</pre>
</body>
</html>
"""


def html_diff(a_text, b_text):
    """Returns a diff between two strings as HTML."""
    # Diffs can be between multiple text files of different encodings
    # so we always want to deal with them as byte arrays, not unicode strings.
    assert isinstance(a_text, str)
    assert isinstance(b_text, str)
    a_lines = a_text.splitlines(True)
    b_lines = b_text.splitlines(True)
    return _TEMPLATE % html_diff_body(a_lines, b_lines)


def html_diff_body(a_lines, b_lines):
    matcher = difflib.SequenceMatcher(None, a_lines, b_lines)
    output = []
    for tag, a_start, a_end, b_start, b_end in matcher.get_opcodes():
        a_chunk = ''.join(a_lines[a_start:a_end])
        b_chunk = ''.join(b_lines[b_start:b_end])
        output.append(_format_chunk(tag, a_chunk, b_chunk))
    return ''.join(output)


def _format_chunk(tag, a_chunk, b_chunk):
    if tag == 'delete':
        return _format_delete(a_chunk)
    if tag == 'insert':
        return _format_insert(b_chunk)
    if tag == 'replace':
        return _format_delete(a_chunk) + _format_insert(b_chunk)
    assert tag == 'equal'
    return a_chunk


def _format_insert(chunk):
    return '<span class="add">%s</span>' % chunk


def _format_delete(chunk):
    return '<span class="del">%s</span>' % chunk
