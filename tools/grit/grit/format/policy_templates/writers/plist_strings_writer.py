# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from grit.format.policy_templates.writers import template_writer


def GetWriter(config, messages):
  '''Factory method for creating PListStringsWriter objects.
  See the constructor of TemplateWriter for description of
  arguments.
  '''
  return PListStringsWriter(['mac'], config, messages)


class PListStringsWriter(template_writer.TemplateWriter):
  '''Outputs localized string table files for the Mac policy file.
  These files are named Localizable.strings and they are in the
  [lang].lproj subdirectories of the manifest bundle.
  '''

  def _GetLocalizedPolicyMessage(self, policy, msg_id):
    '''Looks up localized caption or description for a policy.
    If the policy does not have the required message, then it is
    inherited from the group.

    Args:
      policy: The data structure of the policy.
      msg_id: Either 'caption' or 'desc'.

    Returns:
      The corresponding message for the policy.
    '''
    if msg_id in policy:
      msg = policy[msg_id]
    else:
      msg = self._policy_group[msg_id]
    return msg

  def _AddToStringTable(self, item_name, caption, desc):
    '''Add a title and a description of an item to the string table.

    Args:
      item_name: The name of the item that will get the title and the
        description.
      title: The text of the title to add.
      desc: The text of the description to add.
    '''
    caption = caption.replace('"', '\\"')
    caption = caption.replace('\n','\\n')
    desc = desc.replace('"', '\\"')
    desc = desc.replace('\n','\\n')
    self._out.append('%s.pfm_title = \"%s\";' % (item_name, caption))
    self._out.append('%s.pfm_description = \"%s\";' % (item_name, desc))

  def WritePolicy(self, policy):
    '''Add strings to the stringtable corresponding a given policy.

    Args:
      policy: The policy for which the strings will be added to the
        string table.
    '''
    desc = self._GetLocalizedPolicyMessage(policy, 'desc')
    caption = self._GetLocalizedPolicyMessage(policy, 'caption')
    if (policy['type'] == 'enum'):
      # Append the captions of enum items to the description string.
      item_descs = []
      for item in policy['items']:
        item_descs.append(item['value'] + ' - ' + item['caption'])
      desc = '\n'.join(item_descs) + '\n' + desc

    self._AddToStringTable(policy['name'], caption, desc)

  def BeginPolicyGroup(self, group):
    self._policy_group = group

  def EndPolicyGroup(self):
    self._policy_group = None

  def BeginTemplate(self):
    self._AddToStringTable(
        self.config['app_name'],
        self.config['app_name'],
        self.messages['IDS_POLICY_MAC_CHROME_PREFERENCES'])

  def EndTemplate(self):
    pass

  def Prepare(self):
    # A buffer for the lines of the string table being generated.
    self._out = []

  def GetTemplateText(self):
    return '\n'.join(self._out)
