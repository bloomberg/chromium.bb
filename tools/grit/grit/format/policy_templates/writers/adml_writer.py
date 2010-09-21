# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from xml.dom import minidom
from grit.format.policy_templates.writers import xml_formatted_writer


def GetWriter(config, messages):
  '''Factory method for instanciating the ADMLWriter. Every Writer needs a
  GetWriter method because the TemplateFormatter uses this method to
  instantiate a Writer.
  '''
  return ADMLWriter(['win'], config, messages)


class ADMLWriter(xml_formatted_writer.XMLFormattedWriter):
  ''' Class for generating an ADML policy template. It is used by the
  PolicyTemplateGenerator to write the ADML file.
  '''

  # DOM root node of the generated ADML document.
  _doc = None

  # The string-table contains all ADML "string" elements.
  _string_table_elem = None

  # The presentation-table is the container for presentation elements, that
  # describe the presentation of Policy-Groups and Policies.
  _presentation_table_elem = None

  # The active ADML "presentation" element. At any given point in time this
  # contains the "presentation" element for the Policy-Group that is processed.
  _active_presentation_elem = None


  def _AddString(self, parent, id, text):
    ''' Adds an ADML "string" element to the passed parent. The following
    ADML snippet contains an example:

    <string id="$(id)">$(text)</string>

    Args:
      parent: Parent element to which the new "string" element is added.
      id: ID of the newly created "string" element.
      text: Value of the newly created "string" element.
    '''
    string_elem = self.AddElement(parent, 'string', {'id': id})
    string_elem.appendChild(self._doc.createTextNode(text))

  def _AddStringPolicy(self, presentation_elem, policy_name, caption):
    '''Generates ADML elements for a String-Policy. A String-Policy does not
    require any ADML "string" elements. The presentation of a String-Policy is
    defined by an ADML "textBox" element. The text-box contains an ADML "label"
    element that contains the text of the text-box label. The following ADML
    snipped shows an example:

    <presentation ...>
      ...
      <textBox refId="$(policy_name)">
        <label>$(caption)</label>
      </textBox>
    </presentation>

    Args:
      presentation_elem: The ADML "presentation" element of the policy group
        that includes the policy.
      policy_name: Policy name.
      caption: Caption assisiated with the Policy.
    '''
    # A String-Policy requires no additional ADML "string" elements.

    # Add ADML "presentation" elements that are required by a String-Policy to
    # the presentation-table.
    textbox_elem = self.AddElement(presentation_elem, 'textBox',
                                   {'refId': policy_name})
    label_elem = self.AddElement(textbox_elem, 'label')
    label_elem.appendChild(self._doc.createTextNode(caption))

  def _AddEnumPolicy(self, string_table_elem, presentation_elem,
                     policy_name, caption, items):
    '''Generates ADML elements for an Enum-Policy. For each enum item an ADML
    "string" element is added to the string-table. The presentation of an
    Enum-Policy is defined by the ADML "dropdownList" element. The following
    ADML snippet shows an example:

    <stringTable>
      ...
      <string id="$(item_name)">$(description)</string>
    </stringTable>

    <presentation ...>
      ...
      <dropdownList refId="$(policy_name)">$(caption)</dropdownlist>
    </presentation>

    Args:
      string_table_elem: The ADML "stringTable" element to which the ADML
        "string" elements are added.
      presentation_elem: The ADML "presentation" element of the policy group
        that includes the policy.
      policy_name: Policy name.
      caption: Caption associated with the Policy.
      items: The enum items.
    '''
    # Add ADML "string" elements to the string-table that are required by an
    # Enum-Policy.
    for item in items:
      self._AddString(string_table_elem, item['name'], item['caption'])

    # Add ADML "presentation" elements to the presentation-table that are
    # required by an Enum-Policy.
    dropdownlist_elem = self.AddElement(presentation_elem, 'dropdownList',
                                        {'refId': policy_name})
    dropdownlist_elem.appendChild(self._doc.createTextNode(caption))

  def _AddListPolicy(self, string_table_elem, presentation_elem, policy_name,
                     caption):
    '''Generates ADML elements for a List-Policy. Each List-Policy requires an
    additional ADML "string" element that contains a description of the items
    that can be added to the list. The presentation of a List-Policy is defined
    by an ADML "listBox" element. The following ADML snippet shows an example:

    <stringTable>
      ...
      <string id="$(policy_name)Desc">$(caption)</string>
    </stringTable>

    <presentation ...>
      ...
      <listBox refId="$(policy_name)Desc">$(caption)</listBox>
    </presentation>

    Args:
      string_table_elem: The ADML "stringTable" element to which the ADML
        "string" elements are added.
      presentation_elem: The ADML "presentation" element of the policy group
        that includes the policy.
      policy_name: Policy name.
        caption: Caption assisiated with the Policy.
    '''
    # Add ADML "string" elements to the string-table that are required by a
    # List-Policy.
    self._AddString(string_table_elem, policy_name + 'Desc', caption)

    # Add ADML "presentation" elements to the presentation-table that are
    # required by a List-Policy.
    listbox_elem = self.AddElement(presentation_elem, 'listBox',
                                   {'refId': policy_name + 'Desc'})
    listbox_elem.appendChild(self._doc.createTextNode(caption))

  def WritePolicy(self, policy):
    '''Generates the ADML elements for a Policy.

    Args:
      policy: The Policy to generate ADML elements for.
    '''
    policy_type = policy['type']
    policy_name = policy['name']
    presentation_elem = self._active_presentation_elem
    string_table_elem = self._string_table_elem
    if policy_type == 'main':
      # Nothing needs to be done for a Main-Policy.
      pass
    elif policy_type == 'string':
      self._AddStringPolicy(presentation_elem, policy_name, policy['caption'])
    elif policy_type == 'enum':
      self._AddEnumPolicy(string_table_elem, presentation_elem, policy_name,
                          policy['caption'], policy['items'])
    elif policy_type == 'list':
      self._AddListPolicy(string_table_elem, presentation_elem, policy_name,
                          policy['caption'])
    else:
      raise Exception('Unknown policy type %s.' % policy_type)

  def BeginPolicyGroup(self, group):
    '''Generates ADML elements for a Policy-Group. For each Policy-Group two
    ADML "string" elements are added to the string-table. One contains the
    caption of the Policy-Group and the other a description. A Policy-Group also
    requires an ADML "presentation" element that must be added to the
    presentation-table. The "presentation" element is the container for the
    elements that define the visual presentation of the Policy-Goup's Policies.
    The following ADML snippet shows an example:

    <stringTable>
      ...
      <string id="$(policy_group_name)">$(caption)</string>
      <string id="$(policy_group_name)_Explain">$(description)</string>
    </stringTable>

    <presentationTables>
      ...
      <presentation id=$(policy_group_name)/>
    </presentationTables>

    Args:
      group: The Policy-Group to generate ADML elements for.
    '''
    # Add ADML "string" elements to the string-table that are required by a
    # Policy-Group.
    id = group['name']
    self._AddString(self._string_table_elem, id, group['caption'])
    self._AddString(self._string_table_elem, id + '_Explain', group['desc'])

    # Add ADML "presentation" elements to the presentation-table that are
    # required by a Policy-Group.
    self._active_presentation_elem  = self.AddElement(
        self._presentation_table_elem, 'presentation', {'id': id})

  def EndPolicyGroup(self):
    pass

  def _AddBaseStrings(self, string_table_elem, build):
    ''' Adds ADML "string" elements to the string-table that are referenced by
    the ADMX file but not related to any specific Policy-Group or Policy.
    '''
    self._AddString(string_table_elem, self.config['win_supported_os'],
                    self.messages[self.config['win_supported_os_msg']])
    if build == 'chrome':
      self._AddString(string_table_elem, self.config['win_category_path'][0],
                      'Google')
      self._AddString(string_table_elem, self.config['win_category_path'][1],
                      self.config['app_name'])
    elif build == 'chromium':
      self._AddString(string_table_elem, self.config['win_category_path'][0],
                      self.config['app_name'])

  def BeginTemplate(self):
    dom_impl = minidom.getDOMImplementation('')
    self._doc = dom_impl.createDocument(None, 'policyDefinitionResources',
                                        None)
    policy_definitions_resources_elem = self._doc.documentElement
    policy_definitions_resources_elem.attributes['revision'] = '1.0'
    policy_definitions_resources_elem.attributes['schemaVersion'] = '1.0'

    self.AddElement(policy_definitions_resources_elem, 'displayName')
    self.AddElement(policy_definitions_resources_elem, 'description')
    resources_elem = self.AddElement(policy_definitions_resources_elem,
                                     'resources')
    self._string_table_elem = self.AddElement(resources_elem, 'stringTable')
    self._AddBaseStrings(self._string_table_elem, self.config['build'])
    self._presentation_table_elem = self.AddElement(resources_elem,
                                                   'presentationTable')

  def EndTemplate(self):
    pass

  def Prepare(self):
    pass

  def GetTemplateText(self):
    # Using "toprettyxml()" confuses the Windows Group Policy Editor
    # (gpedit.msc) because it interprets whitespace characters in text between
    # the "string" tags. This prevents gpedit.msc from displaying the category
    # names correctly.
    # TODO(markusheintz): Find a better formatting that works with gpedit.
    return self._doc.toxml()
