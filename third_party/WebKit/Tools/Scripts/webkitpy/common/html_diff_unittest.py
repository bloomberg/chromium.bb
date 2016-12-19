# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.html_diff import html_diff, html_diff_body


class TestHtmlDiff(unittest.TestCase):

    def test_html_diff(self):
        self.assertEqual(
            html_diff('one\ntoo\nthree\n', 'one\ntwo\nthree\n'),
            ('<html>\n'
             '<head>\n'
             '<style>.del { background: #faa; } .add { background: #afa; }</style>\n'
             '</head>\n'
             '<body>\n'
             '<pre>one\n'
             '<span class="del">too\n'
             '</span><span class="add">two\n'
             '</span>three\n'
             '</pre>\n'
             '</body>\n'
             '</html>\n'))

    def test_html_diff_same(self):
        self.assertEqual(
            html_diff_body(['one line\n'], ['one line\n']),
            'one line\n')

    def test_html_diff_delete(self):
        self.assertEqual(
            html_diff_body(['one line\n'], []),
            '<span class="del">one line\n</span>')

    def test_html_diff_insert(self):
        self.assertEqual(
            html_diff_body([], ['one line\n']),
            '<span class="add">one line\n</span>')

    def test_html_diff_ending_newline(self):
        self.assertEqual(
            html_diff_body(['one line'], ['one line\n']),
            '<span class="del">one line</span><span class="add">one line\n</span>')

    def test_html_diff_replace_multiple_lines(self):
        a_lines = [
            '1. Beautiful is better than ugly.\n',
            '2. Explicit is better than implicit.\n',
            '3. Simple is better than complex.\n',
            '4. Complex is better than complicated.\n',
        ]
        b_lines = [
            '1. Beautiful is better than ugly.\n',
            '3.   Simple is better than complex.\n',
            '4. Complicated is better than complex.\n',
            '5. Flat is better than nested.\n',
        ]
        self.assertEqual(html_diff_body(a_lines, b_lines), (
            '1. Beautiful is better than ugly.\n'
            '<span class="del">2. Explicit is better than implicit.\n'
            '3. Simple is better than complex.\n'
            '4. Complex is better than complicated.\n'
            '</span><span class="add">3.   Simple is better than complex.\n'
            '4. Complicated is better than complex.\n'
            '5. Flat is better than nested.\n'
            '</span>'))
