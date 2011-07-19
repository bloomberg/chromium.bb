# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from xml.dom import minidom
from grit.format.policy_templates.writers import template_writer


def GetWriter(config):
  '''Factory method for creating RegWriter objects.
  See the constructor of TemplateWriter for description of
  arguments.
  '''
  return RegWriter(['win'], config)


class RegWriter(template_writer.TemplateWriter):
  '''Class for generating policy example files in .reg format (for Windows).
  The generated files will define all the supported policies with example
  values  set for them. This class is used by PolicyTemplateGenerator to
  write .reg  files.
  '''

  NEWLINE = '\r\n'

  def _EscapeRegString(self, string):
    return string.replace('\\', '\\\\').replace('\"', '\\\"')

  def _StartBlock(self, suffix):
    key = 'HKEY_LOCAL_MACHINE\\' + self.config['win_reg_key_name']
    if suffix:
      key = key + '\\' + suffix
    if key != self._last_key:
      self._out.append('')
      self._out.append('[%s]' % key)
      self._last_key = key

  def PreprocessPolicies(self, policy_list):
    return self.FlattenGroupsAndSortPolicies(policy_list,
                                             self.GetPolicySortingKey)

  def GetPolicySortingKey(self, policy):
    '''Extracts a sorting key from a policy. These keys can be used for
    list.sort() methods to sort policies.
    See TemplateWriter.SortPoliciesGroupsFirst for usage.
    '''
    is_list = policy['type'] == 'list'
    # Lists come after regular policies.
    return (is_list, policy['name'])

  def WritePolicy(self, policy):
    example_value = policy['example_value']

    if policy['type'] == 'list':
      self._StartBlock(policy['name'])
      i = 1
      for item in example_value:
        escaped_str = self._EscapeRegString(item)
        self._out.append('"%d"="%s"' % (i, escaped_str))
        i = i + 1
    else:
      self._StartBlock(None)
      if policy['type'] == 'string':
        escaped_str = self._EscapeRegString(example_value)
        example_value_str = '"' + escaped_str + '"'
      elif policy['type'] == 'main':
        if example_value == True:
          example_value_str = 'dword:00000001'
        else:
          example_value_str = 'dword:00000000'
      elif policy['type'] in ('int', 'int-enum'):
        example_value_str = 'dword:%08x' % example_value
      elif policy['type'] == 'string-enum':
        example_value_str = '"%s"' % example_value
      else:
        raise Exception('unknown policy type %s:' % policy['type'])

      self._out.append('"%s"=%s' % (policy['name'], example_value_str))

  def BeginTemplate(self):
    pass

  def EndTemplate(self):
    pass

  def Init(self):
    self._out = ['Windows Registry Editor Version 5.00']
    self._last_key = None

  def GetTemplateText(self):
    return self.NEWLINE.join(self._out)
