# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import json

# FIXME: This is the left-overs from when we used to generate JSON here.
# Now it's a group of functions used by a hodge-podge of different classes.
# This file should just go away entirely.

_JSON_PREFIX = "ADD_RESULTS("
_JSON_SUFFIX = ");"


def has_json_wrapper(string):
    return string.startswith(_JSON_PREFIX) and string.endswith(_JSON_SUFFIX)


def strip_json_wrapper(json_content):
    # FIXME: Kill this code once the server returns json instead of jsonp.
    if has_json_wrapper(json_content):
        return json_content[len(_JSON_PREFIX):len(json_content) - len(_JSON_SUFFIX)]
    return json_content


def load_json(filesystem, file_path):
    content = filesystem.read_text_file(file_path)
    content = strip_json_wrapper(content)
    return json.loads(content)


def write_json(filesystem, json_object, file_path, callback=None):
    # Specify separators in order to get compact encoding.
    json_string = json.dumps(json_object, separators=(',', ':'))
    if callback:
        json_string = callback + "(" + json_string + ");"
    filesystem.write_text_file(file_path, json_string)


def add_path_to_trie(path, value, trie):
    """Inserts a single flat directory path and associated value into a directory trie structure."""
    if not "/" in path:
        trie[path] = value
        return

    directory, slash, rest = path.partition("/")
    if not directory in trie:
        trie[directory] = {}
    add_path_to_trie(rest, value, trie[directory])


def test_timings_trie(port, individual_test_timings):
    """Breaks a test name into chunks by directory and puts the test time as a value in the lowest part, e.g.
    foo/bar/baz.html: 1ms
    foo/bar/baz1.html: 3ms

    becomes
    foo: {
        bar: {
            baz.html: 1,
            baz1.html: 3
        }
    }
    """
    trie = {}
    for test_result in individual_test_timings:
        test = test_result.test_name

        add_path_to_trie(test, int(1000 * test_result.test_run_time), trie)

    return trie
