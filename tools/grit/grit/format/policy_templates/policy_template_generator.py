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
        data structure is a list of policy groups. Each group is a dictionary
        and has an embedded list of policies under the key 'policies'.
        Example:
        policy_groups = [
          {
            'name': 'PolicyGroup1',
            'policies': [
              {'name': 'Policy1Name', 'type': 'string'}
              {'name': 'Policy2Name', 'type': 'main'}
              {'name': 'Policy3Name', 'type': 'enum', 'items': [...]}
            ]
          },
          {'name': 'PolicyGroup2', ...}
        ]
        See chrome/app/policy.policy_templates.json for an example and more
        details.
    '''
    # List of all the policies:
    self._policy_groups = policy_groups
    # Localized messages to be inserted to the policy_groups structure:
    self._messages = messages
    self._AddMessagesToPolicies()

  def _AddMessagesToItem(self, item_name, item, needs_caption, needs_desc):
    '''Adds localized message strings to an item of the policy data structure
    (self._policy_groups).

    Args:
      item_name: The base of the grd name of the item.
      item: The item which will get its message strings now.
      needs_caption: A boolean value. If it is True, an exception will be raised
        in case there is no caption string for item in self._messages.
      needs_desc: A boolean value. If it is True, an exception will be raised
        in case there is no description string for item in self._messages.

    Raises:
      Exception() if a required message string was not found.
    '''
    # The keys for the item's messages in self._messages:
    caption_name = 'IDS_POLICY_%s_CAPTION' % item_name
    desc_name = 'IDS_POLICY_%s_DESC' % item_name
    # Copy the messages from self._messages to item:
    if caption_name in self._messages:
      item['caption'] = self._messages[caption_name]
    elif needs_caption:
      raise Exception('No localized caption for %s (missing %s).' %
                      (item_name, caption_name))
    if desc_name in self._messages:
      item['desc'] = self._messages[desc_name]
    elif needs_desc:
      raise Exception('No localized description for %s (missing %s).' %
                      (item_name, desc_name))

  def _AddMessagesToPolicy(self, policy):
    '''Adds localized message strings to a policy.

    Args:
      policy: The data structure of the policy that will get message strings
        here.
    '''
    full_name = policy['name'].upper()
    if policy['type'] == 'main':
      # In this case, both caption and description are optional.
      self._AddMessagesToItem(full_name, policy, False, False)
    else:
      # Add caption for this policy.
      self._AddMessagesToItem(full_name, policy, True, False)
    if policy['type'] == 'enum':
      # Iterate through all the items of an enum-type policy, and add captions.
      for item in policy['items']:
        self._AddMessagesToItem('ENUM_' + item['name'].upper(), item,
                                True, False)

  def _AddMessagesToPolicies(self):
    '''Adds localized message strings to each item of the policy data structure
    (self._policy_groups).
    '''
    # Iterate through all the policy groups.
    for group in self._policy_groups:
      # Get caption and description for this group.
      group_name = 'GROUP_' + group['name'].upper()
      self._AddMessagesToItem(group_name, group, True, True)
      if 'policies' in group:
        # Iterate through all the policies in the current group.
        for policy in group['policies']:
          self._AddMessagesToPolicy(policy)

  def _GetPoliciesForWriter(self, template_writer, group):
    '''Filters the list of policies in a group for a writer.

    Args:
      template_writer: The writer object.
      group: The dictionary of the policy group.

    Returns: The list of policies of the policy group that are compatible
      with the writer.
    '''
    if not 'policies' in group:
      return []
    result = []
    for policy in group['policies']:
      if template_writer.IsPolicySupported(policy):
        result.append(policy)
    return result

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
    for group in self._policy_groups:
      policies = self._GetPoliciesForWriter(template_writer, group)
      if policies:
        # Only write nonempty groups.
        template_writer.BeginPolicyGroup(group)
        for policy in policies:
          template_writer.WritePolicy(policy)
        template_writer.EndPolicyGroup()
    template_writer.EndTemplate()
    return template_writer.GetTemplateText()
