# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from xml.dom import minidom
from grit.format.policy_templates.writers import xml_formatted_writer


def GetWriter(config, messages):
  '''Factory method for instanciating the ADMXWriter. Every Writer needs a
  GetWriter method because the TemplateFormatter uses this method to
  instantiate a Writer.
  '''
  return ADMXWriter(config, messages)


class ADMXWriter(xml_formatted_writer.XMLFormattedWriter):
  '''Class for generating an ADMX policy template. It is used by the
  PolicyTemplateGenerator to write the admx file.
  '''

  # DOM root node of the generated ADMX document.
  _doc = None

  # The ADMX "policies" element that contains the ADMX "policy" elements that
  # are generated.
  _active_policies_elem = None

  # The active ADMX "policy" element. At any point in time, the active policy
  # element contains the element being generated for the Policy-Group currently
  # being processed.
  _active_policy_elem = None

  def _AdmlString(self, name):
    '''Creates a reference to the named string in an ADML file.
    Args:
      name: Name of the referenced ADML string.
    '''
    return '$(string.' + name + ')'

  def _AdmlStringExplain(self, name):
    '''Creates a reference to the named explanation string in an ADML file.
    Args:
      name: Name of the referenced ADML explanation.
    '''
    return '$(string.' + name + '_Explain)'

  def _AdmlPresentation(self, name):
    '''Creates a reference to the named presentation element in an ADML file.
    Args:
      name: Name of the referenced ADML presentation element.
    '''
    return '$(presentation.' + name + ')'

  def _AddPolicyNamespaces(self, parent, prefix, namespace):
    '''Generates the ADMX "policyNamespace" element and adds the elements to the
    passed parent element. The namespace of the generated ADMX document is
    define via the ADMX "target" element. Used namespaces are declared with an
    ADMX "using" element. ADMX "target" and "using" elements are children of the
    ADMX "policyNamespace" element.

    Args:
      parent: The parent node to which all generated elements are added.
      prefix: A logical name that can be used in the generated ADMX document to
        refere to this namespace.
      namespace: Namespace of the generated ADMX document.
    '''
    policy_namespaces_elem = self.AddElement(parent, 'policyNamespaces')
    attributes = {
      'prefix': prefix,
      'namespace': namespace,
    }
    self.AddElement(policy_namespaces_elem, 'target', attributes)
    attributes = {
      'prefix': 'windows',
      'namespace': 'Microsoft.Policies.Windows',
    }
    self.AddElement(policy_namespaces_elem, 'using', attributes)

  def _AddCategory(self, parent, name, display_name,
                   parent_category_name=None):
    '''Adds an ADMX category element to the passed parent node. The following
    snippet shows an example of a category element where "chromium" is the value
    of the parameter name:

    <category displayName="$(string.chromium)" name="chromium"/>

    Args:
      parent: The parent node to which all generated elements are added.
      name: Name of the category.
      display_name: Display name of the category.
      parent_category_name: Name of the parent category. Defaults to None.
    '''
    attributes = {
      'name': name,
      'displayName': display_name,
    }
    category_elem = self.AddElement(parent, 'category', attributes)
    if parent_category_name:
      attributes = {'ref': parent_category_name}
      self.AddElement(category_elem, 'parentCategory', attributes)

  def _AddCategories(self, parent, categories):
    '''Generates the ADMX "categories" element and adds it to the passed parent
    node. The "categories" element defines the category for the policies defined
    in this ADMX document. Here is an example of an ADMX "categories" element:

    <categories>
      <category displayName="$(string.google)" name="google"/>
      <category displayName="$(string.googlechrome)" name="googlechrome">
        <parentCategory ref="google"/>
      </category>
    </categories>

    Args:
      parent: The parent node to which all generated elements are added.
      categories_path: The categories path e.g. ['google', 'googlechrome']. For
        each level in the path a "category" element will be generated. Except
        for the root level, each level refers to its parent. Since the root
        level category has no parent it does not require a parent reference.
    '''
    categories_elem = self.AddElement(parent, 'categories')
    category_name = None
    for category in categories:
      parent_category_name = category_name
      category_name = category
      self._AddCategory(categories_elem, category_name,
                        self._AdmlString(category_name), parent_category_name)

  def _AddSupportedOn(self, parent, supported_os):
    '''Generates the "supportedOn" ADMX element and adds it to the passed
    parent node. The "supportedOn" element contains information about supported
    Windows OS versions. The following code snippet contains an example of a
    "supportedOn" element:

    <supportedOn>
      <definitions>
        <definition name="SUPPORTED_WINXPSP2"
                    displayName="$(string.SUPPORTED_WINXPSP2)"/>
        </definitions>
        ...
    </supportedOn>

    Args:
      parent: The parent element to which all generated elements are added.
      supported_os: List with all supported Win OSes.
    '''
    supported_on_elem = self.AddElement(parent, 'supportedOn')
    definitions_elem = self.AddElement(supported_on_elem, 'definitions')
    attributes = {
      'name': supported_os,
      'displayName': self._AdmlString(supported_os)
    }
    self.AddElement(definitions_elem, 'definition', attributes)

  def _AddStringPolicy(self, parent, name):
    '''Generates ADMX elements for a String-Policy and adds them to the
    passed parent node. A String-Policy is mapped to an ADMX "text" element.
    The parent of the ADMX "text" element must be an ADMX "elements" element,
    which must by a child of the ADMX "policy" element that corresponds to the
    Policy-Group that contains the String-Policy. The following example shows
    how the JSON specification of the String-Policy "DisabledPluginsList" of the
    Policy-Group "DisabledPlugins" is mapped to ADMX:

    {
      'name': 'DisabledPlugins',
      'policies': [{
        'name': 'DisabledPluginsList',
        'type' : 'string',
      }, ... ]
    }

    <policy ... name="DisabledPlugins" ...>
      ...
      <elements>
        <text id="DisabledPluginsList" valueName="DisabledPluginsList"/>
        ...
      </elements>
    </policy>

    '''
    attributes = {
      'id': name,
      'valueName': name,
    }
    self.AddElement(parent, 'text', attributes)

  def _AddEnumPolicy(self, parent, name, items):
    '''Generates ADMX elements for an Enum-Policy and adds them to the
    passed parent element. An Enum-Policy is mapped to an ADMX "enum" element.
    The parent of the "enum" element must be the ADMX "elements" element of the
    ADMX "policy" element that corresponds to the Policy-Group that contains the
    Enum-Policy. The following example shows how the JSON specification of the
    Enum-Policy "ProxyServerMode" of the Policy-Group "Proxy" is mapped to ADMX:

    {
      'name': 'Proxy',
      'policies': [{
        'name': 'ProxyServerMode',
        'type': 'enum',
        'items': [
          {'name': 'ProxyServerDisabled', 'value': '0'},
          {'name': 'ProxyServerAutoDetect', 'value': '1'},
          ...
        ],
       }, ... ]
    }

    <policy ... name="Proxy" ...>
      ...
      <elements>
        <enum id="ProxyServerMode" valueName="ProxyServerMode">
          <item displayName="$(string.ProxyServerDisabled)">
            <value>
              <decimal value="0"/>
            </value>
          </item>
          <item displayName="$(string.ProxyServerAutoDetect)">
            <value>
              <decimal value="1"/>
            </value>
          </item>
          ...
        </enum>
      ...
      </elements>
    </policy>
    '''
    attributes = {
      'id': name,
      'valueName': name,
    }
    enum_elem = self.AddElement(parent, 'enum', attributes)
    for item in items:
      attributes = {'displayName': self._AdmlString(item['name'])}
      item_elem = self.AddElement(enum_elem, 'item', attributes)
      value_elem = self.AddElement(item_elem, 'value')
      attributes = {'value': str(item['value'])}
      self.AddElement(value_elem, 'decimal', attributes)

  def _AddListPolicy(self, parent, name):
    '''Generates ADMX XML elements for a List-Policy and adds them to the
    passed parent element. A List-Policy is mapped to an ADMX "list" element.
    The parent of the "list" element must be the ADMX "elements" element of the
    ADMX "policy" element that corresponds to the Policy-Group that contains the
    List-Policy. The following example shows how the JSON specification of the
    List-Policy "ExtensionInstallBlacklist" of the Policy-Group
    "ExtensionInstallBlacklist" is mapped to ADMX:

    {
      'name': 'ExtensionInstallBlacklist',
      'policies': [{
        'name': 'ExtensionInstallBlacklist',
        'type': 'list',
      }]
    },

    <policy ... name="ExtensionInstallBlacklist" ...>
      ...
      <elements>
        <list id="ExtensionInstallBlacklistDesc" valuePrefix=""/>
      </elements>
    </policy>
    '''
    attributes = {
      # The ID must be in sync with ID of the corresponding element in the ADML
      # file.
      'id': name + 'Desc',
      'valuePrefix': '',
      'key': self.config['win_reg_key_name'] + '\\' + name,
    }
    self.AddElement(parent, 'list', attributes)

  def _AddMainPolicy(self, parent):
    '''Generates ADMX elements for a Main-Policy amd adds them to the
    passed parent element.
    '''
    enabled_value_elem = self.AddElement(parent, 'enabledValue');
    self.AddElement(enabled_value_elem, 'decimal', {'value': '1'})
    disabled_value_elem = self.AddElement(parent, 'disabledValue');
    self.AddElement(disabled_value_elem, 'decimal', {'value': '0'})

  def _GetElements(self, policy_group_elem):
    '''Returns the ADMX "elements" child from an ADMX "policy" element. If the
    "policy" element has no "elements" child yet, a new child is created.

    Args:
      policy_group_elem: The ADMX "policy" element from which the child element
        "elements" is returned.

    Raises:
      Exception: The policy_group_elem does not contain a ADMX "policy" element.
    '''
    if policy_group_elem.tagName != 'policy':
      raise Exception('Expected a "policy" element but got a "%s" element'
                      % policy_group_elem.tagName)
    elements_list = policy_group_elem.getElementsByTagName('elements');
    if len(elements_list) == 0:
      return self.AddElement(policy_group_elem, 'elements')
    elif len(elements_list) == 1:
      return elements_list[0]
    else:
      raise Exception('There is supposed to be only one "elements" node but'
                      ' there are %s.' % str(len(elements_list)))

  def WritePolicy(self, policy):
    '''Generates AMDX elements for a Policy. There are four different policy
    types: Main-Policy, String-Policy, Enum-Policy and List-Policy.
    '''
    policy_type = policy['type']
    policy_name = policy['name']
    if policy_type == 'main':
      self._AddMainPolicy(self._active_policy_elem)
    elif policy_type == 'string':
      parent = self._GetElements(self._active_policy_elem)
      self._AddStringPolicy(parent, policy_name)
    elif policy_type == 'enum':
      parent = self._GetElements(self._active_policy_elem)
      self._AddEnumPolicy(parent, policy_name, policy['items'])
    elif policy_type == 'list':
      parent = self._GetElements(self._active_policy_elem)
      self._AddListPolicy(parent, policy_name)
    else:
      raise Exception('Unknown policy type %s.' % policy_type)

  def BeginPolicyGroup(self, group):
    '''Generates ADMX elements for a Policy-Group and adds them to the ADMX
    policies element. A Policy-Group is mapped to an ADMX "policy" element. All
    ADMX policy elements are grouped under the ADMX "policies" element.
    '''
    policies_elem = self._active_policies_elem
    policy_group_name = group['name']
    attributes = {
      'name': policy_group_name,
      'class': self.config['win_group_policy_class'],
      'displayName': self._AdmlString(policy_group_name),
      'explainText': self._AdmlStringExplain(policy_group_name),
      'presentation': self._AdmlPresentation(policy_group_name),
      'key': self.config['win_reg_key_name'],
      'valueName': policy_group_name,
    }
    # Store the current "policy" AMDX element in self for later use by the
    # WritePolicy method.
    self._active_policy_elem = self.AddElement(policies_elem, 'policy',
                                               attributes)
    self.AddElement(self._active_policy_elem, 'parentCategory',
                    {'ref': self.config['win_category_path'][-1]})
    self.AddElement(self._active_policy_elem, 'supportedOn',
                    {'ref': self.config['win_supported_os']})

  def EndPolicyGroup(self):
    return

  def BeginTemplate(self):
    '''Generates the skeleton of the ADMX template. An ADMX template contains
    an ADMX "PolicyDefinitions" element with four child nodes: "policies"
    "policyNamspaces", "resources", "supportedOn" and "categories"
    '''
    dom_impl = minidom.getDOMImplementation('')
    self._doc = dom_impl.createDocument(None, 'policyDefinitions', None)
    policy_definitions_elem = self._doc.documentElement

    policy_definitions_elem.attributes['revision'] = '1.0'
    policy_definitions_elem.attributes['schemaVersion'] = '1.0'

    self._AddPolicyNamespaces(policy_definitions_elem,
                              self.config['admx_prefix'],
                              self.config['admx_namespace'])
    self.AddElement(policy_definitions_elem, 'resources',
                    {'minRequiredRevision' : '1.0'})
    self._AddSupportedOn(policy_definitions_elem,
                         self.config['win_supported_os'])
    self._AddCategories(policy_definitions_elem,
                        self.config['win_category_path'])
    self._active_policies_elem = self.AddElement(policy_definitions_elem,
                                                 'policies')

  def EndTemplate(self):
    pass

  def Prepare(self):
    pass

  def GetTemplateText(self):
    return self._doc.toprettyxml(indent='  ')
