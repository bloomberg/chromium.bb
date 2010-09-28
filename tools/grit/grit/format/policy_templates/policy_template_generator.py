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

  def __init__(self, messages, policy_definitions):
    '''Initializes this object with all the data necessary to output a
    policy template.

    Args:
      messages: An identifier to string dictionary of all the localized
        messages that might appear in the policy template.
      policy_definitions: The list of defined policies and groups, as
        parsed from the policy metafile. Note that this list is passed by
        reference and its contents are modified.
        See chrome/app/policy.policy_templates.json for description and
        content.
    '''
    # List of all the policies:
    self._policy_definitions = policy_definitions
    # Localized messages to be inserted to the policy_definitions structure:
    self._messages = messages
    self._SortPolicies(self._policy_definitions)
    self._AddMessagesToPolicyList(self._policy_definitions)

  def _SortPolicies(self, policy_list):
    '''Sorts a list of policies in-place alphabetically. The order is the
    following: first groups alphabetically (a-z), then other policies
    alphabetically (a-z). The order of policies inside groups is unchanged.

    Args:
      policy_list: The list of policies to sort. Sub-lists in groups will not
        be sorted.
    '''
    policy_list.sort(
        key=lambda(policy): (policy['type'] != 'group', policy['name']))

  def _AddMessageToItem(self, item_name, item, message_name, default=None):
    '''Adds a localized message string to an item of the policy data structure

    Args:
      item_name: The base of the grd name of the item.
      item: The policy, group, or enum item.
      message_name: Identifier of the message: 'desc', 'caption' or 'label'.
      default: If this is specified and the message is not found for item,
        then this string will be added to the item.

    Raises:
      Exception() if the message string was not found and no default was
      specified.
    '''
    # The keys for the item's messages in self._messages:
    long_message_name = ('IDS_POLICY_%s_%s' %
                         (item_name.upper(), message_name.upper()))
    # Copy the messages from self._messages to item:
    if long_message_name in self._messages:
      item[message_name] = self._messages[long_message_name]
    elif default != None:
      item[message_name] = default
    else:
      raise Exception('No localized message for %s (missing %s).' %
                      (item_name, long_message_name))

  def _AddMessagesToPolicy(self, policy):
    '''Adds localized message strings to a policy or group.

    Args:
      policy: The data structure of the policy or group, that will get message
        strings here.
    '''
    self._AddMessageToItem(policy['name'], policy, 'caption')
    self._AddMessageToItem(policy['name'], policy, 'desc')
    if policy['type'] != 'group':
      # Real policies (that are not groups) might also have an optional
      # 'label', that defaults to 'caption'.
      self._AddMessageToItem(
          policy['name'], policy, 'label', policy['caption'])
    if policy['type'] == 'group':
      self._AddMessagesToPolicyList(policy['policies'])
    elif policy['type'] == 'enum':
      # Iterate through all the items of an enum-type policy, and add captions.
      for item in policy['items']:
        self._AddMessageToItem('ENUM_' + item['name'], item, 'caption')

  def _AddMessagesToPolicyList(self, policy_list):
    '''Adds localized message strings to each item in a list of policies and
    groups.

    Args:
      policy_list: A list of policies and groups. Message strings will be added
        for each item and to their child items, recursively.
    '''
    for policy in policy_list:
      self._AddMessagesToPolicy(policy)

  def GetTemplateText(self, template_writer):
    '''Generates the text of the template from the arguments given
    to the constructor, using a given TemplateWriter.

    Args:
      template_writer: An object implementing TemplateWriter. Its methods
        are called here for each item of self._policy_groups.

    Returns:
      The text of the generated template.
    '''
    return template_writer.WriteTemplate(self._policy_definitions)
