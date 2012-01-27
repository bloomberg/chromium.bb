# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class Code(object):
  """A convenience object for constructing code.

  Logically each object should be a block of code. All methods except |Render|
  and |IsEmpty| return self.
  """
  def __init__(self, indent_size=2, comment_length=80):
    self._code = []
    self._indent_level = 0
    self._indent_size = indent_size
    self._comment_length = comment_length

  def Append(self, line=''):
    """Appends a line of code at the current indent level or just a newline if
    line is not specified. Trailing whitespace is stripped.
    """
    self._code.append(((' ' * self._indent_level) + line).rstrip())
    return self

  def IsEmpty(self):
    """Returns True if the Code object is empty.
    """
    return not bool(self._code)

  def Concat(self, obj):
    """Concatenate another Code object onto this one. Trailing whitespace is
    stripped.

    Appends the code at the current indent level. Will fail if there are any
    un-interpolated format specifiers eg %s, %(something)s which helps
    isolate any strings that haven't been substituted.
    """
    if not isinstance(obj, Code):
      raise TypeError()
    assert self is not obj
    for line in obj._code:
      # line % () will fail if any substitution tokens are left in line
      self._code.append(((' ' * self._indent_level) + line % ()).rstrip())

    return self

  def Sblock(self, line=''):
    """Starts a code block.

    Appends a line of code and then increases the indent level.
    """
    self.Append(line)
    self._indent_level += self._indent_size
    return self

  def Eblock(self, line=''):
    """Ends a code block by decreasing and then appending a line (or a blank
    line if not given).
    """
    # TODO(calamity): Decide if type checking is necessary
    #if not isinstance(line, basestring):
    #  raise TypeError
    self._indent_level -= self._indent_size
    self.Append(line)
    return self

  # TODO(calamity): Make comment its own class or something and Render at
  # self.Render() time
  def Comment(self, comment):
    """Adds the given string as a comment.

    Will split the comment if it's too long. Use mainly for variable length
    comments. Otherwise just use code.Append('// ...') for comments.
    """
    comment_symbol = '// '
    max_len = self._comment_length - self._indent_level - len(comment_symbol)
    while len(comment) >= max_len:
      line = comment[0:max_len]
      last_space = line.rfind(' ')
      if last_space != -1:
        line = line[0:last_space]
        comment = comment[last_space + 1:]
      else:
        comment = comment[max_len:]
      self.Append(comment_symbol + line)
    self.Append(comment_symbol + comment)
    return self

  def Substitute(self, d):
    """Goes through each line and interpolates using the given dict.

    Raises type error if passed something that isn't a dict

    Use for long pieces of code using interpolation with the same variables
    repeatedly. This will reduce code and allow for named placeholders which
    are more clear.
    """
    if not isinstance(d, dict):
      raise TypeError('Passed argument is not a dictionary: ' + d)
    for i, line in enumerate(self._code):
      # Only need to check %s because arg is a dict and python will allow
      # '%s %(named)s' but just about nothing else
      if '%s' in self._code[i] or '%r' in self._code[i]:
        raise TypeError('"%s" or "%r" found in substitution. '
                        'Named arguments only. Use "%" to escape')
      self._code[i] = line % d
    return self

  def Render(self):
    """Renders Code as a string.
    """
    return '\n'.join(self._code)

