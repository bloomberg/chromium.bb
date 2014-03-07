#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import extract_actions

# Empty value to be inserted to |ACTIONS_MOCK|.
NO_VALUE = ''

ONE_OWNER = '<owner>name1@google.com</owner>\n'
TWO_OWNERS = """
<owner>name1@google.com</owner>\n
<owner>name2@google.com</owner>\n
"""

DESCRIPTION = '<description>Description.</description>\n'
TWO_DESCRIPTIONS = """
<description>Description.</description>\n
<description>Description2.</description>\n
"""

OBSOLETE = '<obsolete>Not used anymore. Replaced by action2.</obsolete>\n'
TWO_OBSOLETE = '<obsolete>Obsolete1.</obsolete><obsolete>Obsolete2.</obsolete>'

COMMENT = '<!--comment-->'

# A format string to mock the input action.xml file.
ACTIONS_XML = """
{comment}
<actions>

<action name="action1">
{owners}{obsolete}{description}
</action>

</actions>"""

NO_OWNER_EXPECTED_XML = (
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <owner>Please list the metric\'s owners. '
    'Add more owner tags as needed.</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '</actions>\n'
)

ONE_OWNER_EXPECTED_XML = (
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <owner>name1@google.com</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '</actions>\n'
)

TWO_OWNERS_EXPECTED_XML = (
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <owner>name1@google.com</owner>\n'
    '  <owner>name2@google.com</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '</actions>\n'
)

NO_DESCRIPTION_EXPECTED_XML = (
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <owner>name1@google.com</owner>\n'
    '  <owner>name2@google.com</owner>\n'
    '  <description>Please enter the description of the metric.</description>\n'
    '</action>\n\n'
    '</actions>\n'
)

OBSOLETE_EXPECTED_XML = (
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <owner>name1@google.com</owner>\n'
    '  <owner>name2@google.com</owner>\n'
    '  <description>Description.</description>\n'
    '  <obsolete>Not used anymore. Replaced by action2.</obsolete>\n'
    '</action>\n\n'
    '</actions>\n'
)

ADD_ACTION_EXPECTED_XML = (
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <owner>name1@google.com</owner>\n'
    '  <owner>name2@google.com</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '<action name="action2">\n'
    '  <owner>Please list the metric\'s owners.'
    ' Add more owner tags as needed.</owner>\n'
    '  <description>Please enter the description of the metric.</description>\n'
    '</action>\n\n'
    '</actions>\n'
)

COMMENT_EXPECTED_XML = (
    '<!--comment-->\n\n'
    '<actions>\n\n'
    '<action name="action1">\n'
    '  <owner>name1@google.com</owner>\n'
    '  <owner>name2@google.com</owner>\n'
    '  <description>Description.</description>\n'
    '</action>\n\n'
    '</actions>\n'
)


class ActionXmlTest(unittest.TestCase):

  def _GetProcessedAction(self, owner, description, obsolete, new_actions=[],
                          comment=NO_VALUE):
    """Forms an actions XML string and returns it after processing.

    It parses the original XML string, adds new user actions (if there is any),
    and pretty prints it.

    Args:
      owner: the owner tag to be inserted in the original XML string.
      description: the description tag to be inserted in the original XML
        string.
      obsolete: the obsolete tag to be inserted in the original XML string.
      new_actions: optional. List of new user actions' names to be inserted.
      comment: the comment tag to be inserted in the original XML string.

    Returns:
      An updated and pretty-printed action XML string.
    """
    # Form the actions.xml mock content based on the input parameters.
    current_xml = ACTIONS_XML.format(owners=owner, description=description,
                                     obsolete=obsolete, comment=comment)
    actions, actions_dict, comments = extract_actions.ParseActionFile(
        current_xml)
    for new_action in new_actions:
      actions.add(new_action)
    return extract_actions.PrettyPrint(actions, actions_dict, comments)

  def testNoOwner(self):
    xml_result = self._GetProcessedAction(NO_VALUE, DESCRIPTION, NO_VALUE)
    self.assertEqual(NO_OWNER_EXPECTED_XML, xml_result)

  def testOneOwnerOneDescription(self):
    xml_result = self._GetProcessedAction(ONE_OWNER, DESCRIPTION, NO_VALUE)
    self.assertEqual(ONE_OWNER_EXPECTED_XML, xml_result)

  def testTwoOwners(self):
    xml_result = self._GetProcessedAction(TWO_OWNERS, DESCRIPTION, NO_VALUE)
    self.assertEqual(TWO_OWNERS_EXPECTED_XML, xml_result)

  def testNoDescription(self):
    xml_result = self._GetProcessedAction(TWO_OWNERS, NO_VALUE, NO_VALUE)
    self.assertEqual(NO_DESCRIPTION_EXPECTED_XML, xml_result)

  def testTwoDescriptions(self):
    current_xml = ACTIONS_XML.format(owners=TWO_OWNERS, obsolete=NO_VALUE,
                                     description=TWO_DESCRIPTIONS,
                                     comment=NO_VALUE)
    # Since there are two description tags, the function ParseActionFile will
    # raise SystemExit with exit code 1.
    with self.assertRaises(SystemExit) as cm:
      _, _ = extract_actions.ParseActionFile(current_xml)
    self.assertEqual(cm.exception.code, 1)

  def testObsolete(self):
    xml_result = self._GetProcessedAction(TWO_OWNERS, DESCRIPTION, OBSOLETE)
    self.assertEqual(OBSOLETE_EXPECTED_XML, xml_result)


  def testTwoObsoletes(self):
    current_xml = ACTIONS_XML.format(owners=TWO_OWNERS, obsolete=TWO_OBSOLETE,
                                     description=DESCRIPTION, comment=NO_VALUE)
    # Since there are two obsolete tags, the function ParseActionFile will
    # raise SystemExit with exit code 1.
    with self.assertRaises(SystemExit) as cm:
      _, _ = extract_actions.ParseActionFile(current_xml)
    self.assertEqual(cm.exception.code, 1)

  def testAddNewActions(self):
    xml_result = self._GetProcessedAction(TWO_OWNERS, DESCRIPTION, NO_VALUE,
                                          new_actions=['action2'])
    self.assertEqual(ADD_ACTION_EXPECTED_XML, xml_result)

  def testComment(self):
    xml_result = self._GetProcessedAction(TWO_OWNERS, DESCRIPTION, NO_VALUE,
                                          comment=COMMENT)
    self.assertEqual(COMMENT_EXPECTED_XML, xml_result)


if __name__ == '__main__':
  unittest.main()
