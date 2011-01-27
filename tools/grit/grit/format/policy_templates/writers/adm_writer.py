# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from grit.format.policy_templates.writers import template_writer


NEWLINE = '\r\n'


def GetWriter(config):
  '''Factory method for creating AdmWriter objects.
  See the constructor of TemplateWriter for description of
  arguments.
  '''
  return AdmWriter(['win'], config)


class IndentedStringBuilder:
  '''Utility class for building text with indented lines.'''

  def __init__(self):
    self.lines = []
    self.indent = ''

  def AddLine(self, string='', indent_diff=0):
    '''Appends a string with indentation and a linebreak to |self.lines|.

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
      self.lines.append(self.indent + string)
    else:
      self.lines.append('')
    if indent_diff > 0:
      self.indent += ''.ljust(indent_diff)

  def AddLines(self, other):
    '''Appends the content of another |IndentedStringBuilder| to |self.lines|.
    Indentation of the added lines will be the sum of |self.indent| and
    their original indentation.

    Args:
      other: The buffer from which lines are copied.
    '''
    for line in other.lines:
      self.AddLine(line)

  def ToString(self):
    '''Returns |self.lines| as text string.'''
    return NEWLINE.join(self.lines)


class AdmWriter(template_writer.TemplateWriter):
  '''Class for generating policy templates in Windows ADM format.
  It is used by PolicyTemplateGenerator to write ADM files.
  '''

  TYPE_TO_INPUT = {
    'string': 'EDITTEXT',
    'int': 'NUMERIC',
    'string-enum': 'DROPDOWNLIST',
    'int-enum': 'DROPDOWNLIST',
    'list': 'LISTBOX'
  }

  def _AddGuiString(self, name, value):
    # Escape newlines in the value.
    value = value.replace('\n', '\\n')
    line = '%s="%s"' % (name, value)
    self.strings.AddLine(line)

  def _WriteSupported(self):
    self.policies.AddLine('#if version >= 4', 1)
    self.policies.AddLine('SUPPORTED !!SUPPORTED_WINXPSP2')
    self.policies.AddLine('#endif', -1)

  def _WritePart(self, policy):
    '''Writes the PART ... END PART section of a policy.

    Args:
      policy: The policy to write to the output.
    '''
    policy_part_name = policy['name'] + '_Part'
    self._AddGuiString(policy_part_name, policy['label'])

    # Print the PART ... END PART section:
    self.policies.AddLine()
    adm_type = self.TYPE_TO_INPUT[policy['type']]
    self.policies.AddLine('PART !!%s  %s' % (policy_part_name, adm_type), 1)
    if policy['type'] == 'list':
      # Note that the following line causes FullArmor ADMX Migrator to create
      # corrupt ADMX files. Please use admx_writer to get ADMX files.
      self.policies.AddLine('KEYNAME "%s\\%s"' %
                      (self.config['win_reg_key_name'], policy['name']))
      self.policies.AddLine('VALUEPREFIX ""')
    else:
      self.policies.AddLine('VALUENAME "%s"' % policy['name'])
    if policy['type'] in ('int-enum', 'string-enum'):
      self.policies.AddLine('ITEMLIST', 1)
      for item in policy['items']:
        if policy['type'] == 'int-enum':
          value_text = 'NUMERIC ' + str(item['value'])
        else:
          value_text = '"' + item['value'] + '"'
        self.policies.AddLine('NAME !!%s_DropDown VALUE %s' %
            (item['name'], value_text))
        self._AddGuiString(item['name'] + '_DropDown', item['caption'])
      self.policies.AddLine('END ITEMLIST', -1)
    self.policies.AddLine('END PART', -1)

  def WritePolicy(self, policy):
    self._AddGuiString(policy['name'] + '_Policy', policy['caption'])
    self.policies.AddLine('POLICY !!%s_Policy' % policy['name'], 1)
    self._WriteSupported()
    policy_explain_name = policy['name'] + '_Explain'
    self._AddGuiString(policy_explain_name, policy['desc'])
    self.policies.AddLine('EXPLAIN !!' + policy_explain_name)

    if policy['type'] == 'main':
      self.policies.AddLine('VALUENAME "%s"' % policy['name'])
      self.policies.AddLine('VALUEON NUMERIC 1')
      self.policies.AddLine('VALUEOFF NUMERIC 0')
    else:
      self._WritePart(policy)

    self.policies.AddLine('END POLICY', -1)
    self.policies.AddLine()

  def BeginPolicyGroup(self, group):
    self._open_category = len(group['policies']) > 1
    # Open a category for the policies if there is more than one in the
    # group.
    if self._open_category:
      category_name = group['name'] + '_Category'
      self._AddGuiString(category_name, group['caption'])
      self.policies.AddLine('CATEGORY !!' + category_name, 1)

  def EndPolicyGroup(self):
    if self._open_category:
      self.policies.AddLine('END CATEGORY', -1)
      self.policies.AddLine('')

  def _CreateTemplateForClass(self, policy_class, policies):
    '''Creates the whole ADM template except for the [Strings] section, and
    returns it as an |IndentedStringBuilder|.

    Args:
      policy_class: USER or MACHINE
      policies: ADM code for all the policies in an |IndentedStringBuilder|.
    '''
    lines = IndentedStringBuilder()
    category_path = self.config['win_category_path']

    lines.AddLine('CLASS ' + policy_class, 1)
    if self.config['build'] == 'chrome':
      lines.AddLine('CATEGORY !!' + category_path[0], 1)
      lines.AddLine('CATEGORY !!' + category_path[1], 1)
    elif self.config['build'] == 'chromium':
      lines.AddLine('CATEGORY !!' + category_path[0], 1)
    lines.AddLine('KEYNAME "%s"' % self.config['win_reg_key_name'])
    lines.AddLine()

    lines.AddLines(policies)

    if self.config['build'] == 'chrome':
      lines.AddLine('END CATEGORY', -1)
      lines.AddLine('END CATEGORY', -1)
      lines.AddLine('', -1)
    elif self.config['build'] == 'chromium':
      lines.AddLine('END CATEGORY', -1)
      lines.AddLine('', -1)

    return lines

  def BeginTemplate(self):
    category_path = self.config['win_category_path']
    self._AddGuiString(self.config['win_supported_os'],
                       self.messages['win_supported_winxpsp2']['text'])
    if self.config['build'] == 'chrome':
      self._AddGuiString(category_path[0], 'Google')
      self._AddGuiString(category_path[1], self.config['app_name'])
    elif self.config['build'] == 'chromium':
      self._AddGuiString(category_path[0], self.config['app_name'])
    # All the policies will be written into self.policies.
    # The final template text will be assembled into self.lines by
    # self.EndTemplate().

  def EndTemplate(self):
    # Copy policies into self.lines.
    policy_class = self.config['win_group_policy_class'].upper()
    if policy_class in ('BOTH', 'MACHINE'):
      self.lines.AddLines(self._CreateTemplateForClass('MACHINE',
                                                       self.policies))
    if policy_class in ('BOTH', 'USER'):
      self.lines.AddLines(self._CreateTemplateForClass('USER',
                                                       self.policies))
    # Copy user strings into self.lines.
    self.lines.AddLine('[Strings]')
    self.lines.AddLines(self.strings)

  def Init(self):
    # String buffer for building the whole ADM file.
    self.lines = IndentedStringBuilder()
    # String buffer for building the strings section of the ADM file.
    self.strings = IndentedStringBuilder()
    # String buffer for building the policies of the ADM file.
    self.policies = IndentedStringBuilder()

  def GetTemplateText(self):
    return self.lines.ToString()
