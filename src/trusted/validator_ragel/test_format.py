# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re


def ParseTest(lines):
  r"""Parses section-based test.

  Args:
    lines: list of \n-terminated strings.

  Returns:
    List of string pairs (field name, field content) in order. Field content is
    concatenation of \n-terminated lines, so it's either empty or ends with \n.
  """
  fields = []
  field_data = {}
  current_field = None

  for line in lines:
    if line.startswith('  '):
      assert current_field is not None, line
      field_data[current_field].append(line[2:])
    else:
      match = re.match('@(\S+):$', line)
      if match is None:
        raise Exception('Bad line: %r' % line)
      current_field = match.group(1)
      assert current_field not in field_data, current_field
      field_data[current_field] = []
      fields.append(current_field)

  return [(field, ''.join(field_data[field])) for field in fields]


def SplitLines(lines, separator_regex):
  """Split sequence of lines into sequence of list of lines.

  Args:
    lines: sequence of strings.
    separator_regex: separator regex.

  Yields:
    Nonempty sequence of (possibly empty) lists of strings. Separator lines
    are not included.
  """
  part = []
  for line in lines:
    if re.match(separator_regex, line):
      yield part
      part = []
    else:
      part.append(line)
  yield part


def LoadTestFile(filename):
  r"""Loads and parses .test file.

  Args:
    filename: filename.

  Returns:
    List of tests (see ParseTest).
  """
  with open(filename) as file_in:
    return map(ParseTest, SplitLines(file_in, r'-{3,}\s*$'))


def UnparseTest(items_list):
  """Convert test to sequence of \n-terminated strings

  Args:
    items_list: list of string pairs (see ParseTest).

  Yields:
    Sequence of \n-terminated strings.
  """
  for field, content in items_list:
    yield '@%s:\n' % field
    if content == '':
      continue

    assert content.endswith('\n')
    content = content[:-1]

    for line in content.split('\n'):
      yield '  %s\n' % line


def SaveTestFile(tests, filename):
  r"""Saves .test file

  Args:
    tests: list of tests (see ParseTest).
    filename: filename.
  Returns:
    None.
  """
  with open(filename, 'w') as file_out:
    first = True
    for test in tests:
      if not first:
        file_out.write('-' * 70 + '\n')
      first = False
      for line in UnparseTest(test):
        file_out.write(line)
