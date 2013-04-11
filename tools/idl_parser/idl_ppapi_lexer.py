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
  # 'tokens' is a value required by lex which specifies the complete list
  # of valid token types.  To WebIDL we add the following token types
  IDLLexer.tokens += [
    # Operators
      'LSHIFT',
      'RSHIFT',

    # Pepper Extras
      'INLINE',
  ]

  # 'keywords' is a map of string to token type.  All tokens matching
  # KEYWORD_OR_SYMBOL are matched against keywords dictionary, to determine
  # if the token is actually a keyword.  Add the new keywords to the
  # dictionary and set of tokens
  ppapi_keywords = ['LABEL', 'NAMESPACE', 'STRUCT']
  for keyword in ppapi_keywords:
    IDLLexer.keywords[ keyword.lower() ] = keyword
    IDLLexer.tokens.append(keyword)

  # Special multi-character operators
  t_LSHIFT = r'<<'
  t_RSHIFT = r'>>'

  # Return a "preprocessor" inline block
  def t_INLINE(self, t):
    r'\#inline (.|\n)*?\#endinl.*'
    self.AddLines(t.value.count('\n'))
    return t
