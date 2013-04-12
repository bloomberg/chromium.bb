# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Lexer for PPAPI IDL

The lexer uses the PLY library to build a tokenizer which understands both
WebIDL and Pepper tokens.

WebIDL, and WebIDL regular expressions can be found at:
   http://www.w3.org/TR/2012/CR-WebIDL-20120419/
PLY can be found at:
   http://www.dabeaz.com/ply/
"""

from idl_lexer import IDLLexer
import optparse
import os.path
import sys


#
# IDL PPAPI Lexer
#
class IDLPPAPILexer(IDLLexer):
  # Special multi-character operators
  def t_LSHIFT(self, t):
    r'<<'
    return t;

  def t_RSHIFT(self, t):
    r'>>'
    return t;

  def t_INLINE(self, t):
    r'\#inline (.|\n)*?\#endinl.*'
    self.AddLines(t.value.count('\n'))
    return t

  # Return a "preprocessor" inline block
  def __init__(self):
    IDLLexer.__init__(self)
    self._AddTokens(['LSHIFT', 'RSHIFT', 'INLINE'])
    self._AddKeywords(['label', 'namespace', 'struct'])


# If run by itself, attempt to build the lexer
if __name__ == '__main__':
  lexer = IDLPPAPILexer()
