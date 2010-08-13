# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from template_writer import TemplateWriter


def GetWriter(build):
  return AdmWriter(build)


class AdmWriter(TemplateWriter):
  '''Class for generating policy templates in Windows ADM format.
  It is used by PolicyTemplateGenerator to write ADM files.
  '''

  TYPE_TO_INPUT = {
    'string': 'EDITTEXT',
    'enum': 'DROPDOWNLIST'}
  NEWLINE = '\r\n'

  def _AddGuiString(self, name, value):
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
    self.policy_list.append(self.indent + string)
    if indent_diff > 0:
      self.indent += ''.ljust(indent_diff)

  def _WriteSupported(self):
    self._PrintLine('#if version >= 4', 1)
    self._PrintLine('SUPPORTED !!SUPPORTED_WINXPSP2')
    self._PrintLine('#endif', -1)

  def WritePolicy(self, policy_name, policy):
    type = policy['type']
    if type == 'main':
      self._PrintLine('VALUENAME "%s"' % policy_name )
      self._PrintLine('VALUEON NUMERIC 1')
      self._PrintLine('VALUEOFF NUMERIC 0')
      return

    policy_part_name = policy_name + '_Part'
    self._AddGuiString(policy_part_name, policy['caption'])

    self._PrintLine()
    self._PrintLine(
        'PART !!%s  %s' % (policy_part_name, self.TYPE_TO_INPUT[type]),
        1)
    self._PrintLine('VALUENAME "%s"' % policy_name)
    if type == 'enum':
      self._PrintLine('ITEMLIST', 1)
      for item_name, item in policy['items'].iteritems():
        self._PrintLine(
            'NAME !!%s_DropDown VALUE NUMERIC %s' % (item_name,  item['value']))
        self._AddGuiString(item_name + '_DropDown', item['caption'])
      self._PrintLine('END ITEMLIST', -1)
    self._PrintLine('END PART', -1)

  def BeginPolicyGroup(self, group_name, group):
    group_explain_name = group_name + '_Explain'
    self._AddGuiString(group_name + '_Policy', group['caption'])
    self._AddGuiString(group_explain_name, group['desc'])

    self._PrintLine('POLICY !!%s_Policy' % group_name, 1)
    self._WriteSupported()
    self._PrintLine('EXPLAIN !!' + group_explain_name)

  def EndPolicyGroup(self):
    self._PrintLine('END POLICY', -1)
    self._PrintLine()

  def BeginTemplate(self):
    # TODO(gfeher): Move this string to .grd.
    self._AddGuiString('SUPPORTED_WINXPSP2',
                       'At least Microsoft Windows XP SP2')
    self._PrintLine('CLASS MACHINE', 1)
    if self.build == 'chrome':
      self._AddGuiString('google', 'Google')
      self._AddGuiString('googlechrome', 'Google Chrome')
      self._PrintLine('CATEGORY !!google', 1)
      self._PrintLine('CATEGORY !!googlechrome', 1)
      self._PrintLine('KEYNAME "Software\\Policies\\Google\\Google Chrome"')
    elif self.build == 'chromium':
      self._AddGuiString('chromium', 'Chromium')
      self._PrintLine('CATEGORY !!chromium', 1)
      self._PrintLine('KEYNAME "Software\\Policies\\Chromium"')
    self._PrintLine()

  def EndTemplate(self):
    if self.build == 'chrome':
      self._PrintLine('END CATEGORY', -1)
      self._PrintLine('END CATEGORY', -1)
      self._PrintLine('', -1)
    elif self.build == 'chromium':
      self._PrintLine('END CATEGORY', -1)
      self._PrintLine('', -1)

  def Prepare(self):
    self.policy_list = []
    self.str_list = ['[Strings]']
    self.indent = ''

  def GetTemplateText(self):
    lines = self.policy_list + self.str_list
    return self.NEWLINE.join(lines)
