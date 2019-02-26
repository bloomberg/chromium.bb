# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class Collection(object):
    """
    Collection class stores ASTs of Web IDL files and meta information
    like component.
    """

    def __init__(self, component=None):
        self._asts = []
        self._component = component

    def add_ast(self, ast):
        assert ast.GetClass() == 'File', (
            'Root node of an AST must be a File node., but is %s.' % ast.GetClass())
        self._asts.append(ast)

    @property
    def asts(self):
        return self._asts

    @property
    def component(self):
        return self._component
