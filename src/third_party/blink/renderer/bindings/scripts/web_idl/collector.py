# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import blink_idl_parser
from .collection import Collection


class Collector(object):

    def __init__(self, component, parser=blink_idl_parser.BlinkIDLParser()):
        self._component = component
        self._collection = Collection(component)
        self._parser = parser

    def collect_from_idl_files(self, filepaths):
        if isinstance(filepaths, str):
            filepaths = [filepaths]
        for filepath in filepaths:
            try:
                ast = blink_idl_parser.parse_file(self._parser, filepath)
                self.collect_from_ast(ast)
            except ValueError as ve:
                raise ValueError('%s\nin file %s' % (str(ve), filepath))

    def collect_from_idl_text(self, text, filename='TEXT'):
        ast = self._parser.ParseText(filename, text)  # pylint: disable=no-member
        self.collect_from_ast(ast)

    def collect_from_ast(self, node):
        self._collection.add_ast(node)

    def get_collection(self):
        return self._collection
