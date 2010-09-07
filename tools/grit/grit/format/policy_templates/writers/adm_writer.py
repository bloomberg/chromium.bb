# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from grit.format.policy_templates.writers import template_writer


def GetWriter(info, messages):
  '''Factory method for creating AdmWriter objects.
  See the constructor of TemplateWriter for description of
  arguments.
  '''
  return AdmWriter(info, messages)


class AdmWriter(template_writer.TemplateWriter):
  '''Class for generating policy templates in Windows ADM format.
  It is used by PolicyTemplateGenerator to write ADM files.
  '''

  TYPE_TO_INPUT = {
    'string': 'EDITTEXT',
    'enum': 'DROPDOWNLIST',
    'list': 'LISTBOX'}
  NEWLINE = '\r\n'

  def _AddGuiString(self, name, value):
    # Escape newlines in the value.
    value = value.replace('\n','\\n')
    line = '%s="%s"' % (name, value)
    self.str_list.append(line)

  def _PrintLine(self, string='', indent_diff=0):
    '''Prints a string with indents and a linebreak to the output.

    Args:
      string: The string to print.
      indent_diff: the difference of indentation of the printed line,
        compared to the next/previous printed line. Increment occurs
        after printing the line, while decrement occurs before that.
    '''
    indent_diff *= 2
    if indent_diff < 0:
      self.indent = self.indent[(-indent_diff):]
    if string != '':
      self.policy_list.append(self.indent + string)
    else:
      self.policy_list.append('')
    if indent_diff > 0:
      self.indent += ''.ljust(indent_diff)

  def _WriteSupported(self):
    self._PrintLine('#if version >= 4', 1)
    self._PrintLine('SUPPORTED !!SUPPORTED_WINXPSP2')
    self._PrintLine('#endif', -1)

  def WritePolicy(self, policy):
    policy_type = policy['type']
    policy_name = policy['name']
    if policy_type == 'main':
      self._PrintLine('VALUENAME "%s"' % policy_name )
      self._PrintLine('VALUEON NUMERIC 1')
      self._PrintLine('VALUEOFF NUMERIC 0')
      return

    policy_part_name = policy_name + '_Part'
    self._AddGuiString(policy_part_name, policy['caption'])

    self._PrintLine()
    # Print the PART ... END PART section:
    self._PrintLine(
        'PART !!%s  %s' % (policy_part_name, self.TYPE_TO_INPUT[policy_type]),
        1)
    if policy_type == 'list':
      # Note that the following line causes FullArmor ADMX Migrator to create
      # corrupt ADMX files. Please use admx_writer to get ADMX files.
      self._PrintLine('KEYNAME "%s\\%s"' %
                      (self.config['win_reg_key_name'], policy_name))
      self._PrintLine('VALUEPREFIX ""')
    else:
      self._PrintLine('VALUENAME "%s"' % policy_name)
    if policy_type == 'enum':
      self._PrintLine('ITEMLIST', 1)
      for item in policy['items']:
        self._PrintLine('NAME !!%s_DropDown VALUE NUMERIC %s' %
                        (item['name'],  item['value']))
        self._AddGuiString(item['name'] + '_DropDown', item['caption'])
      self._PrintLine('END ITEMLIST', -1)
    self._PrintLine('END PART', -1)

  def BeginPolicyGroup(self, group):
    group_explain_name = group['name'] + '_Explain'
    self._AddGuiString(group['name'] + '_Policy', group['caption'])
    self._AddGuiString(group_explain_name, group['desc'])

    self._PrintLine('POLICY !!%s_Policy' % group['name'], 1)
    self._WriteSupported()
    self._PrintLine('EXPLAIN !!' + group_explain_name)

  def EndPolicyGroup(self):
    self._PrintLine('END POLICY', -1)
    self._PrintLine()

  def BeginTemplate(self):
    category_path = self.config['win_category_path']
    self._AddGuiString(self.config['win_supported_os'],
                       self.messages[self.config['win_supported_os_msg']])
    self._PrintLine(
        'CLASS ' + self.config['win_group_policy_class'].upper(),
        1)
    if self.config['build'] == 'chrome':
      self._AddGuiString(category_path[0], 'Google')
      self._AddGuiString(category_path[1], self.config['app_name'])
      self._PrintLine('CATEGORY !!' + category_path[0], 1)
      self._PrintLine('CATEGORY !!' + category_path[1], 1)
    elif self.config['build'] == 'chromium':
      self._AddGuiString(category_path[0], self.config['app_name'])
      self._PrintLine('CATEGORY !!' + category_path[0], 1)
    self._PrintLine('KEYNAME "%s"' % self.config['win_reg_key_name'])
    self._PrintLine()

  def EndTemplate(self):
    if self.config['build'] == 'chrome':
      self._PrintLine('END CATEGORY', -1)
      self._PrintLine('END CATEGORY', -1)
      self._PrintLine('', -1)
    elif self.config['build'] == 'chromium':
      self._PrintLine('END CATEGORY', -1)
      self._PrintLine('', -1)

  def Prepare(self):
    self.policy_list = []
    self.str_list = ['[Strings]']
    self.indent = ''

  def GetTemplateText(self):
    lines = self.policy_list + self.str_list
    return self.NEWLINE.join(lines)
