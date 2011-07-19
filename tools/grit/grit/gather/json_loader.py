# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import types

from grit.gather import interface
from grit import util


class JsonLoader(interface.GathererBase):
  '''A simple gatherer that loads and parses a JSON file.'''

  def __init__(self, json_text):
    '''Initializes a gatherer object with JSON input.

    Args:
      json_text: A string containing a JSON expression.
    '''
    super(type(self), self).__init__()
    self._json_text = json_text
    self._data = None

  def Parse(self):
    '''Parses the text of self._json_text into the data structure in
    self._data.
    '''
    globs = {}
    exec('data = ' + self._json_text, globs)
    self._data = globs['data']

  def GetData(self):
    '''Returns the parsed JSON data.'''
    return self._data

  def FromFile(filename_or_stream, extkey, encoding):
    '''Creates a JSONLoader instance from a file or stream.

    Args:
      filename_or_stream: The source of JSON data.
      extkey: Unused, see interface.py.
      encoding: The encoding used in the JSON file. (Note that it should
        not contain localized strings.)

    Returns:
      The JSONLoader instance holding the JSON data unparsed.
    '''
    if isinstance(filename_or_stream, types.StringTypes):
      filename_or_stream = \
          util.WrapInputStream(file(filename_or_stream, 'rU'), encoding)
    return JsonLoader(filename_or_stream.read())
  FromFile = staticmethod(FromFile)
