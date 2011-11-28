#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
          elif char == '?':
            re_line += re.escape(re_bucket)
            re_bucket = ''
            re_line += '.'
          else:  # there can't be any '\*'s in a stack trace
            re_bucket += char
        re_line += re.escape(re_bucket)
        re_bucket = ''
        re_line += '\n'
    re_line += '(.*\n)*'
    re_line += '}'

    # In the recent version of valgrind-variant we've switched
    # from memcheck's default Addr[1248]/Value[1248]/Cond suppression types
    # to simply Unaddressable/Uninitialized.
    # The suppression generator no longer gives us "old" types thus
    # for the "new-type" suppressions:
    #  * Memcheck:Unaddressable should also match Addr* reports,
    #  * Memcheck:Uninitialized should also match Cond and Value reports,
    #
    # We also want to support legacy suppressions (e.g. copied from
    # upstream bugs etc), so:
    #  * Memcheck:Addr[1248] suppressions should match Unaddressable reports,
    #  * Memcheck:Cond and Memcheck:Value[1248] should match Uninitialized.
    # Please note the latest two rules only apply to the
    # tools/valgrind/waterfall.sh suppression matcher and the real
    # valgrind-variant Memcheck will not suppress
    # e.g. Addr1 printed as Unaddressable with Addr4 suppression.
    # Be careful to check the access size while copying legacy suppressions!
    for sz in [1, 2, 4, 8]:
      re_line = re_line.replace("\nMemcheck:Addr%d\n" % sz,
                                "\nMemcheck:(Addr%d|Unaddressable)\n" % sz)
      re_line = re_line.replace("\nMemcheck:Value%d\n" % sz,
                                "\nMemcheck:(Value%d|Uninitialized)\n" % sz)
    re_line = re_line.replace("\nMemcheck:Cond\n",
                              "\nMemcheck:(Cond|Uninitialized)\n")
    re_line = re_line.replace("\nMemcheck:Unaddressable\n",
                              "\nMemcheck:(Addr.|Unaddressable)\n")
    re_line = re_line.replace("\nMemcheck:Uninitialized\n",
                              "\nMemcheck:(Cond|Value.|Uninitialized)\n")

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
      if (not line.startswith("Memcheck:")) and \
         (not line.startswith("ThreadSanitizer:")) and \
         (line != "Heapcheck:Leak"):
        raise SuppressionError(supp_descriptor, nline,
            '"Memcheck:TYPE" , "ThreadSanitizer:TYPE" or "Heapcheck:Leak '
            'is expected, got "%s"' % line)
      supp_type = line.split(':')[1]
      if not supp_type in ["Addr1", "Addr2", "Addr4", "Addr8",
                           "Cond", "Free", "Jump", "Leak", "Overlap", "Param",
                           "Value1", "Value2", "Value4", "Value8",
                           "Race", "UnlockNonLocked", "InvalidLock",
                           "Unaddressable", "Uninitialized"]:
        raise SuppressionError(supp_descriptor, nline,
                               'Unknown suppression type "%s"' % supp_type)
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


def TestStack(stack, positive, negative):
  """A helper function for SelfTest() that checks a single stack.

  Args:
    stack: the stack to match the suppressions.
    positive: the list of suppressions that must match the given stack.
    negative: the list of suppressions that should not match.
  """
  for supp in positive:
    assert ReadSuppressions(supp.split("\n"), "")[0].Match(stack), \
           "Suppression:\n%s\ndidn't match stack:\n%s" % (supp, stack)
  for supp in negative:
    assert not ReadSuppressions(supp.split("\n"), "")[0].Match(stack), \
           "Suppression:\n%s\ndid match stack:\n%s" % (supp, stack)

def SelfTest():
  """Tests the Suppression.Match() capabilities."""

  test_memcheck_stack_1 = """{
    test
    Memcheck:Leak
    fun:absolutly
    fun:brilliant
    obj:condition
    fun:detection
    fun:expression
  }""".split("\n")

  test_memcheck_stack_2 = """{
    test
    Memcheck:Uninitialized
    fun:absolutly
    fun:brilliant
    obj:condition
    fun:detection
    fun:expression
  }""".split("\n")

  test_memcheck_stack_3 = """{
    test
    Memcheck:Unaddressable
    fun:absolutly
    fun:brilliant
    obj:condition
    fun:detection
    fun:expression
  }""".split("\n")

  test_memcheck_stack_4 = """{
    test
    Memcheck:Addr4
    fun:absolutly
    fun:brilliant
    obj:condition
    fun:detection
    fun:expression
  }""".split("\n")

  test_heapcheck_stack = """{
    test
    Heapcheck:Leak
    fun:absolutly
    fun:brilliant
    obj:condition
    fun:detection
    fun:expression
  }""".split("\n")

  test_tsan_stack = """{
    test
    ThreadSanitizer:Race
    fun:absolutly
    fun:brilliant
    obj:condition
    fun:detection
    fun:expression
  }""".split("\n")


  positive_memcheck_suppressions_1 = [
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

  positive_memcheck_suppressions_2 = [
    "{\nzzz\nMemcheck:Uninitialized\nfun:absolutly\n}",
    "{\nzzz\nMemcheck:Uninitialized\nfun:ab*ly\n}",
    "{\nzzz\nMemcheck:Uninitialized\nfun:absolutly\nfun:brilliant\n}",
    # Legacy suppression types
    "{\nzzz\nMemcheck:Value1\n...\nfun:brilliant\n}",
    "{\nzzz\nMemcheck:Cond\n...\nfun:detection\n}",
    "{\nzzz\nMemcheck:Value8\nfun:absolutly\nfun:brilliant\n}",
  ]

  positive_memcheck_suppressions_3 = [
    "{\nzzz\nMemcheck:Unaddressable\nfun:absolutly\n}",
    "{\nzzz\nMemcheck:Unaddressable\nfun:absolutly\nfun:brilliant\n}",
    "{\nzzz\nMemcheck:Unaddressable\nfun:absolutly\nfun:brilliant\n}",
    # Legacy suppression types
    "{\nzzz\nMemcheck:Addr1\n...\nfun:brilliant\n}",
    "{\nzzz\nMemcheck:Addr8\n...\nfun:detection\n}",
  ]

  positive_memcheck_suppressions_4 = [
    "{\nzzz\nMemcheck:Addr4\nfun:absolutly\n}",
    "{\nzzz\nMemcheck:Unaddressable\nfun:absolutly\n}",
    "{\nzzz\nMemcheck:Addr4\nfun:absolutly\nfun:brilliant\n}",
    "{\nzzz\nMemcheck:Unaddressable\n...\nfun:brilliant\n}",
    "{\nzzz\nMemcheck:Addr4\n...\nfun:detection\n}",
  ]

  positive_heapcheck_suppressions = [
    "{\nzzz\nHeapcheck:Leak\n...\nobj:condition\n}",
    "{\nzzz\nHeapcheck:Leak\nfun:absolutly\n}",
  ]

  positive_tsan_suppressions = [
    "{\nzzz\nThreadSanitizer:Race\n...\nobj:condition\n}",
    "{\nzzz\nThreadSanitizer:Race\nfun:absolutly\n}",
  ]

  negative_memcheck_suppressions_1 = [
    "{\nzzz\nMemcheck:Leak\nfun:abnormal\n}",
    "{\nzzz\nMemcheck:Leak\nfun:ab*liant\n}",
    "{\nzzz\nMemcheck:Leak\nfun:brilliant\n}",
    "{\nzzz\nMemcheck:Leak\nobj:condition\n}",
    "{\nzzz\nMemcheck:Addr8\nfun:brilliant\n}",
  ]

  negative_memcheck_suppressions_2 = [
    "{\nzzz\nMemcheck:Cond\nfun:abnormal\n}",
    "{\nzzz\nMemcheck:Value2\nfun:abnormal\n}",
    "{\nzzz\nMemcheck:Uninitialized\nfun:ab*liant\n}",
    "{\nzzz\nMemcheck:Value4\nfun:brilliant\n}",
    "{\nzzz\nMemcheck:Leak\nobj:condition\n}",
    "{\nzzz\nMemcheck:Addr8\nfun:brilliant\n}",
    "{\nzzz\nMemcheck:Unaddressable\nfun:brilliant\n}",
  ]

  negative_memcheck_suppressions_3 = [
    "{\nzzz\nMemcheck:Addr1\nfun:abnormal\n}",
    "{\nzzz\nMemcheck:Uninitialized\nfun:absolutly\n}",
    "{\nzzz\nMemcheck:Addr2\nfun:ab*liant\n}",
    "{\nzzz\nMemcheck:Value4\nfun:brilliant\n}",
    "{\nzzz\nMemcheck:Leak\nobj:condition\n}",
    "{\nzzz\nMemcheck:Addr8\nfun:brilliant\n}",
  ]

  negative_memcheck_suppressions_4 = [
    "{\nzzz\nMemcheck:Addr1\nfun:abnormal\n}",
    "{\nzzz\nMemcheck:Addr4\nfun:abnormal\n}",
    "{\nzzz\nMemcheck:Unaddressable\nfun:abnormal\n}",
    "{\nzzz\nMemcheck:Addr1\nfun:absolutly\n}",
    "{\nzzz\nMemcheck:Addr2\nfun:ab*liant\n}",
    "{\nzzz\nMemcheck:Value4\nfun:brilliant\n}",
    "{\nzzz\nMemcheck:Leak\nobj:condition\n}",
    "{\nzzz\nMemcheck:Addr8\nfun:brilliant\n}",
  ]

  negative_heapcheck_suppressions = [
    "{\nzzz\nMemcheck:Leak\nfun:absolutly\n}",
    "{\nzzz\nHeapcheck:Leak\nfun:brilliant\n}",
  ]

  negative_tsan_suppressions = [
    "{\nzzz\nThreadSanitizer:Leak\nfun:absolutly\n}",
    "{\nzzz\nThreadSanitizer:Race\nfun:brilliant\n}",
  ]

  TestStack(test_memcheck_stack_1,
            positive_memcheck_suppressions_1,
            negative_memcheck_suppressions_1)
  TestStack(test_memcheck_stack_2,
            positive_memcheck_suppressions_2,
            negative_memcheck_suppressions_2)
  TestStack(test_memcheck_stack_3,
            positive_memcheck_suppressions_3,
            negative_memcheck_suppressions_3)
  TestStack(test_memcheck_stack_4,
            positive_memcheck_suppressions_4,
            negative_memcheck_suppressions_4)
  TestStack(test_heapcheck_stack, positive_heapcheck_suppressions,
            negative_heapcheck_suppressions)
  TestStack(test_tsan_stack, positive_tsan_suppressions,
            negative_tsan_suppressions)

if __name__ == '__main__':
  SelfTest()
  print 'PASS'
