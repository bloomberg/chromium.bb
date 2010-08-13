# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class PolicyTemplateGenerator:
  '''Generates template text for a particular platform.

  This class is used to traverse a JSON structure from a .json template
  definition metafile and merge GUI message string definitions that come
  from a .grd resource tree onto it. After this, it can be used to output
  this data to policy template files using TemplateWriter objects.
  '''

  def __init__(self, messages, policy_groups):
    '''Initializes this object with all the data necessary to output a
    policy template.

    Args:
      messages: An identifier to string dictionary of all the localized
        messages that might appear in the policy template.
      policy_groups: The policies are organized into groups, and each policy
        has a name and type. The user will see a GUI interface for assigning
        values to policies, by using the templates generated here. The traversed
        data structure is a dictionary of policy group names to groups. Each
        group is a dictionary and has an embedded dictionary of policies under
        the key 'policies'. Example:
        policy_groups = {
          'PolicyGroup1': {
            'policies': {
              'Policy1Name': {'type': 'string'}
              'Policy2Name': {'type': 'main'}
              'Policy3Name': {'type': 'enum', 'items': {...}}
            }
          },
          'PolicyGroup2': {...}
        }
        See chrome/app/policy.policy_templates.json for an example and more
        details.
    '''
    # List of all the policies:
    self._policy_groups = policy_groups
    # Localized messages to be inserted to the policy_groups structure:
    self._messages = messages
    self._AddMessagesToPolicies()

  def _AddMessagesToItem(self, item_name, item, warn_no_desc):
    '''Adds localized message strings to an item of the policy data structure
    (self._policy_groups).

    Args:
      item_name: The name of the item.
      item: The item which will get its message strings now.
      warn_no_desc: A boolean value. If it is True, warnings will be
        printed in case there are no description strings for item in
        self._messages.
    '''
    # The keys for the item's messages in self._messages:
    caption_id = 'IDS_POLICY_%s_CAPTION' % item_name
    desc_id = 'IDS_POLICY_%s_DESC' % item_name
    # Copy the messages from self._messages to item:
    if caption_id in self._messages:
      item['caption'] = self._messages[caption_id]
    else:
      print 'Warning: no localized message for ' + caption_id
    if desc_id in self._messages:
      item['desc'] = self._messages[desc_id]
    elif warn_no_desc:
      print 'Warning: no localized message for ' + desc_id

  def _AddMessagesToPolicies(self):
    '''Adds localized message strings to each item of the policy data structure
    (self._policy_groups).
    '''
    # Iterate through all the policy groups.
    for group_name, group in self._policy_groups.iteritems():
      # Get caption and description for this group.
      group_name = 'GROUP_' + group_name.upper()
      self._AddMessagesToItem(group_name, group, True)
      if 'policies' in group:
        # Iterate through all the policies in the current group.
        for policy_name, policy in group['policies'].iteritems():
          if policy['type'] == 'enum':
            # Iterate through all the items of an enum-type policy.
            for item_name, item in policy['items'].iteritems():
              self._AddMessagesToItem('ENUM_' + item_name.upper(), item, False)
          elif policy['type'] == 'main':
            # In this case, messages are inherited from the group.
            policy['caption'] = group['caption']
            policy['desc'] = group['desc']
            continue
          # Get caption for this policy.
          full_name = policy_name.upper()
          self._AddMessagesToItem(full_name, policy, False)

  def GetTemplateText(self, template_writer):
    '''Generates the text of the template from the arguments given
    to the constructor, using a given TemplateWriter.

    Args:
      template_writer: An object implementing TemplateWriter. Its methods
        are called here for each item of self._policy_groups.

    Returns:
      The text of the generated template.
    '''
    template_writer.Prepare()
    template_writer.BeginTemplate()
    for group_name, group in self._policy_groups.iteritems():
      template_writer.BeginPolicyGroup(group_name, group)
      if 'policies' in group:
        for policy_name, policy in group['policies'].iteritems():
          template_writer.WritePolicy(policy_name, policy)
      template_writer.EndPolicyGroup()
    template_writer.EndTemplate()
    return template_writer.GetTemplateText()
