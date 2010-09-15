# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class TemplateWriter(object):
  '''Abstract base class for writing policy templates in various formats.
  The methods of this class will be called by PolicyTemplateGenerator.
  '''

  def __init__(self, platforms, config, messages):
    '''Initializes a TemplateWriter object.

    Args:
      platforms: List of platforms for which this writer can write policies.
      config: A dictionary of information required to generate the template.
        It contains some key-value pairs, including the following examples:
          'build': 'chrome' or 'chromium'
          'branding': 'Google Chrome' or 'Chromium'
          'mac_bundle_id': The Mac bundle id of Chrome. (Only set when building
            for Mac.)
      messages: List of all the message strings from the grd file. Most of them
        are also present in the policy data structures that are passed to
        methods. That is the preferred way of accessing them, this should only
        be used in exceptional cases. An example for its use is the
        IDS_POLICY_WIN_SUPPORTED_WINXPSP2 message in ADM files, because that
        cannot be associated with any policy or group.
    '''
    self.platforms = platforms
    self.config = config
    self.messages = messages

  def Prepare(self):
    '''Initializes the internal buffer where the template will be
    stored.
    '''
    raise NotImplementedError()

  def WritePolicy(self, policy):
    '''Appends the template text corresponding to a policy into the
    internal buffer.

    Args:
      policy: The policy as it is found in the JSON file.
    '''
    raise NotImplementedError()

  def BeginPolicyGroup(self, group):
    '''Appends the template text corresponding to the beginning of a
    policy group into the internal buffer.

    Args:
      group: The policy group as it is found in the JSON file.
    '''
    raise NotImplementedError()

  def EndPolicyGroup(self):
    '''Appends the template text corresponding to the end of a
    policy group into the internal buffer.
    '''
    raise NotImplementedError()

  def BeginTemplate(self):
    '''Appends the text corresponding to the beginning of the whole
    template into the internal buffer.
    '''
    raise NotImplementedError()

  def EndTemplate(self):
    '''Appends the text corresponding to the end of the whole
    template into the internal buffer.
    '''
    raise NotImplementedError()


  def GetTemplateText(self):
    '''Gets the content of the internal template buffer.

    Returns:
      The generated template from the the internal buffer as a string.
    '''
    raise NotImplementedError()

  def IsPolicySupported(self, policy):
    '''Checks if the writer is interested in writing a given policy.

    Args:
      policy: The dictionary of the policy.

    Returns:
      True if the writer chooses to include 'policy' in its output.
    '''
    for platform in self.platforms:
      if platform in policy['annotations']['platforms']:
        return True
    return False
