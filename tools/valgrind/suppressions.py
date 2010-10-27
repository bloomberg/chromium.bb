#!/usr/bin/python
# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# suppressions.py

"""Post-process Valgrind suppression matcher.

Suppressions are defined as follows:

# optional one-line comments anywhere in the suppressions file.
{
  <Short description of the error>
  Toolname:Errortype
  fun:function_name
  obj:object_filename
  fun:wildcarded_fun*_name
  # an ellipsis wildcards zero or more functions in a stack.
  ...
  fun:some_other_function_name
}

If ran from the command line, suppressions.py does a self-test
of the Suppression class.
"""

import re

ELLIPSIS = '...'


class Suppression(object):
  """This class represents a single stack trace suppression.

  Attributes:
    description: A string representing the error description.
    type: A string representing the error type, e.g. Memcheck:Leak.
    stack: a list of "fun:" or "obj:" or ellipsis lines.
  """

  def __init__(self, description, type, stack):
    """Inits Suppression.

    Args: Same as class attributes.
    """
    self.description = description
    self.type = type
    self._stack = stack
    re_line = '{\n.*\n%s\n' % self.type
    re_bucket = ''
    for line in stack:
      if line == ELLIPSIS:
        re_line += re.escape(re_bucket)
        re_bucket = ''
        re_line += '(.*\n)*'
      else:
        for char in line:
          if char == '*':
            re_line += re.escape(re_bucket)
            re_bucket = ''
            re_line += '.*'
          else:  # there can't be any '\*'s in a stack trace
            re_bucket += char
        re_line += re.escape(re_bucket)
        re_bucket = ''
        re_line += '\n'
    re_line += '(.*\n)*'
    re_line += '}'
    self._re = re.compile(re_line, re.MULTILINE)

  def Match(self, suppression_from_report):
    """Returns bool indicating whether this suppression matches
       the suppression generated from Valgrind error report.

       We match our suppressions against generated suppressions
       (not against reports) since they have the same format
       while the reports are taken from XML, contain filenames,
       they are demangled, etc.

    Args:
      suppression_from_report: list of strings (function names).
    Returns:
      True if the suppression is not empty and matches the report.
    """
    if not self._stack:
      return False
    lines = [f.strip() for f in suppression_from_report]
    if self._re.match('\n'.join(lines) + '\n'):
      return True
    else:
      return False


class SuppressionError(Exception):
  def __init__(self, filename, line, message=''):
    Exception.__init__(self, filename, line, message)
    self._file = filename
    self._line = line
    self._message = message

  def __str__(self):
    return 'Error reading suppressions from "%s" (line %d): %s.' % (
        self._file, self._line, self._message)

def ReadSuppressionsFromFile(filename):
  """Read suppressions from the given file and return them as a list"""
  input_file = file(filename, 'r')
  try:
    return ReadSuppressions(input_file, filename)
  except SuppressionError:
    input_file.close()
    raise

def ReadSuppressions(lines, supp_descriptor):
  """Given a list of lines, returns a list of suppressions.

  Args:
    lines: a list of lines containing suppressions.
    supp_descriptor: should typically be a filename.
      Used only when parsing errors happen.
  """
  result = []
  cur_descr = ''
  cur_type = ''
  cur_stack = []
  in_suppression = False
  nline = 0
  for line in lines:
    nline += 1
    line = line.strip()
    if line.startswith('#'):
      continue
    if not in_suppression:
      if not line:
        # empty lines between suppressions
        pass
      elif line.startswith('{'):
        in_suppression = True
        pass
      else:
        raise SuppressionError(supp_descriptor, nline,
                               'Expected: "{"')
    elif line.startswith('}'):
      result.append(Suppression(cur_descr, cur_type, cur_stack))
      cur_descr = ''
      cur_type = ''
      cur_stack = []
      in_suppression = False
    elif not cur_descr:
      cur_descr = line
      continue
    elif not cur_type:
      if not line.startswith("Memcheck:"):
        raise SuppressionError(supp_descriptor, nline,
                               '"Memcheck:TYPE" is expected, got "%s"' % line)
      if not line[9:] in ["Addr1", "Addr2", "Addr4", "Addr8",
                          "Cond", "Free", "Jump", "Leak", "Overlap", "Param",
                          "Value1", "Value2", "Value4", "Value8"]:
        raise SuppressionError(supp_descriptor, nline,
                               'Unknown suppression type "%s"' % line[9:])
      cur_type = line
      continue
    elif re.match("^fun:.*|^obj:.*|^\.\.\.$", line):
      cur_stack.append(line.strip())
    elif len(cur_stack) == 0 and cur_type == "Memcheck:Param":
      cur_stack.append(line.strip())
    else:
      raise SuppressionError(supp_descriptor, nline,
                             '"fun:function_name" or "obj:object_file" ' \
                             'or "..." expected')
  return result


def SelfTest():
  """Tests the Suppression.Match() capabilities."""

  test_stack1 = """{
    test
    Memcheck:Leak
    fun:absolutly
    fun:brilliant
    obj:condition
    fun:detection
    fun:expression
  }""".split("\n")

  positive_suppressions = [
    "{\nzzz\nMemcheck:Leak\nfun:absolutly\n}",
    "{\nzzz\nMemcheck:Leak\nfun:ab*ly\n}",
    "{\nzzz\nMemcheck:Leak\nfun:absolutly\nfun:brilliant\n}",
    "{\nzzz\nMemcheck:Leak\n...\nfun:brilliant\n}",
    "{\nzzz\nMemcheck:Leak\n...\nfun:detection\n}",
    "{\nzzz\nMemcheck:Leak\nfun:absolutly\n...\nfun:detection\n}",
    "{\nzzz\nMemcheck:Leak\nfun:ab*ly\n...\nfun:detection\n}",
    "{\nzzz\nMemcheck:Leak\n...\nobj:condition\n}",
    "{\nzzz\nMemcheck:Leak\n...\nobj:condition\nfun:detection\n}",
    "{\nzzz\nMemcheck:Leak\n...\nfun:brilliant\nobj:condition\n}",
  ]

  negative_suppressions = [
    "{\nzzz\nMemcheck:Leak\nfun:abnormal\n}",
    "{\nzzz\nMemcheck:Leak\nfun:ab*liant\n}",
    "{\nzzz\nMemcheck:Leak\nfun:brilliant\n}",
    "{\nzzz\nMemcheck:Leak\nobj:condition\n}",
    "{\nzzz\nMemcheck:Addr8\nfun:brilliant\n}",
  ]

  for supp in positive_suppressions:
    assert ReadSuppressions(supp.split("\n"), "")[0].Match(test_stack1), \
           "Suppression:\n%s\ndidn't match stack:\n%s" % (supp, test_stack1)
  for supp in negative_suppressions:
    assert not ReadSuppressions(supp.split("\n"), "")[0].Match(test_stack1), \
           "Suppression:\n%s\ndidn't match stack:\n%s" % (supp, test_stack1)

if __name__ == '__main__':
  SelfTest()
  print 'PASS'
