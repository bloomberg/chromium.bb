# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re


def LoadTestFile(filename):
  r"""Loads and parses .test file.

  Args:
    filename: filename.

  Returns:
    List of string pairs (field name, field content) in order. Field content is
    concatenation of \n-terminated lines, so it's either empty or ends with \n.
  """

  with open(filename) as file_in:
    fields = []
    field_data = {}
    current_field = None
    for line in file_in:
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


def SaveTestFile(items_list, filename):
  r"""Saves .test file

  Args:
    items_list: List of string pairs (field name, field content) in order.
                Field content should either be empty or end with \n.
  Returns:
    None.
  """

  with open(filename, 'w') as file_out:
    for field, content in items_list:
      file_out.write('@%s:\n' % field)
      if content == '':
        return

      assert content.endswith('\n')
      content = content[:-1]

      for line in content.split('\n'):
        file_out.write('  %s\n' % line)
