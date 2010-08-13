# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class TemplateWriter(object):
  '''Abstract base class for writing policy templates in various formats.
  The methods of this class will be called by PolicyTemplateGenerator.
  '''

  def __init__(self, build):
    '''Initializes a TemplateWriter object.

    Args:
      build: 'chrome' or 'chromium'
    '''
    assert build in ['chrome', 'chromium']
    self.build = build

  def Prepare(self):
    '''Initializes the internal buffer where the template will be
    stored.
    '''
    raise NotImplementedError()

  def WritePolicy(self, policy_name, policy):
    '''Appends the template text corresponding to a policy into the
    internal buffer.

    Args:
      policy_name: The name of the policy that has to be written.
      policy: The policy as it is found in the JSON file.
    '''
    raise NotImplementedError()

  def BeginPolicyGroup(self, group_name, group):
    '''Appends the template text corresponding to the beginning of a
    policy group into the internal buffer.

    Args:
      group_name: The name of the policy group that has to be written.
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
