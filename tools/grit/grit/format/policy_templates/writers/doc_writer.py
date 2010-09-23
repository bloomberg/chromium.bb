# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import re

from xml.dom import minidom
from grit.format.policy_templates.writers import xml_formatted_writer


def GetWriter(config, messages):
  '''Factory method for creating DocWriter objects.
  See the constructor of TemplateWriter for description of
  arguments.
  '''
  return DocWriter(['mac', 'linux', 'win'], config, messages)


class DocWriter(xml_formatted_writer.XMLFormattedWriter):
  '''Class for generating policy templates in HTML format.
  The intended use of the generated file is to upload it on
  http://dev.chromium.org, therefore its format has some limitations:
  - No HTML and body tags.
  - Restricted set of element attributes: for example no 'class'.
  Because of the latter the output is styled using the 'style'
  attributes of HTML elements. This is supported by the dictionary
  self._STYLES[] and the method self._AddStyledElement(), they try
  to mimic the functionality of CSS classes. (But without inheritance.)

  This class is invoked by PolicyTemplateGenerator to create the HTML
  files.
  '''

  # TODO(gfeher): This function is duplicated in PListWriter.
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
      msg = policy['parent'][msg_id]
    return msg

  def _GetLocalizedMessage(self, msg_id):
    '''Returns a localized message for this writer.

    Args:
      msg_id: The identifier of the message.

    Returns:
      The localized message.
    '''
    return self.messages['IDS_POLICY_DOC_' + msg_id.upper()]

  def _MapListToString(self, item_map, items):
    '''Creates a comma-separated list.

    Args:
      item_map: A dictionary containing all the elements of 'items' as
        keys.
      items: A list of arbitrary items.

    Returns:
      Looks up each item of 'items' in 'item_maps' and concatenates the
      resulting items into a comma-separated list.
    '''
    return ', '.join([item_map[x] for x in items])

  def _AddTextWithLinks(self, parent, text):
    '''Parse a string for URLs and add it to a DOM node with the URLs replaced
    with <a> HTML links.

    Args:
      parent: The DOM node to which the text will be added.
      text: The string to be added.
    '''
    # Iterate through all the URLs and replace them with links.
    out = []
    while True:
      # Look for the first URL.
      res = self._url_matcher.search(text)
      if not res:
        break
      # Calculate positions of the substring of the URL.
      url = res.group(0)
      start = res.start(0)
      end = res.end(0)
      # Add the text prior to the URL.
      self.AddText(parent, text[:start])
      # Add a link for the URL.
      self.AddElement(parent, 'a', {'href': url}, url)
      # Drop the part of text that is added.
      text = text[end:]
    self.AddText(parent, text)


  def _AddStyledElement(self, parent, name, style_ids, attrs=None, text=None):
    '''Adds an XML element to a parent, with CSS style-sheets included.

    Args:
      parent: The parent DOM node.
      name: Name of the element to add.
      style_ids: A list of CSS style strings from self._STYLE[].
      attrs: Dictionary of attributes for the element.
      text: Text content for the element.
    '''
    if attrs == None:
      attrs = {}

    style = ''.join([self._STYLE[x] for x in style_ids])
    if style != '':
      attrs['style'] = style
    return self.AddElement(parent, name, attrs, text)

  def _AddDescription(self, parent, policy):
    '''Adds a string containing the description of the policy. URLs are
    replaced with links and the possible choices are enumerated in case
    of 'enum' type policies.

    Args:
      parent: The DOM node for which the feature list will be added.
      policy: The data structure of a policy.
    '''
    desc = self._GetLocalizedPolicyMessage(policy, 'desc')
    # Replace URLs with links in the description.
    self._AddTextWithLinks(parent, desc)
    # Add list of enum items.
    if policy['type'] == 'enum':
      ul = self.AddElement(parent, 'ul')
      for item in policy['items']:
        self.AddElement(
            ul, 'li', {}, '%s = %s' % (item['value'], item['caption']))

  def _AddFeatures(self, parent, policy):
    '''Adds a string containing the list of supported features of a policy
    to a DOM node. The text will look like as:
      Feature_X: Yes, Feature_Y: No

    Args:
      parent: The DOM node for which the feature list will be added.
      policy: The data structure of a policy.
    '''
    features = []
    for key, value in policy['annotations']['features'].iteritems():
      key_name = self._FEATURE_MAP[key]
      if value == 0:
        value_name = self._GetLocalizedMessage('not_supported')
      else:
        value_name = self._GetLocalizedMessage('supported')
      features.append('%s: %s' % (key_name, value_name))
    self.AddText(parent, ', '.join(features))

  def _AddListExampleMac(self, parent, policy):
    '''Adds an example value for Mac of a 'list' policy to a DOM node.

    Args:
      parent: The DOM node for which the example will be added.
      policy: A policy of type 'list', for which the Mac example value
        is generated.
    '''
    example_value = policy['annotations']['example_value']
    self.AddElement(parent, 'dt', {}, 'Mac:')
    mac = self._AddStyledElement(parent, 'dd', ['.monospace', '.pre'])

    mac_text = ['<array>']
    for item in example_value:
      mac_text.append('  <string>%s</string>' % item)
    mac_text.append('</array>')
    self.AddText(mac, '\n'.join(mac_text))

  def _AddListExampleWindows(self, parent, policy):
    '''Adds an example value for Windows of a 'list' policy to a DOM node.

    Args:
      parent: The DOM node for which the example will be added.
      policy: A policy of type 'list', for which the Windows example value
        is generated.
    '''
    example_value = policy['annotations']['example_value']
    self.AddElement(parent, 'dt', {}, 'Windows:')
    win = self._AddStyledElement(parent, 'dd', ['.monospace', '.pre'])
    win_text = []
    cnt = 0
    for item in example_value:
      win_text.append(
          '%s\\%s\\%d = "%s"' %
          (self.config['win_reg_key_name'], policy['name'], cnt, item))
      cnt = cnt + 1
    self.AddText(win, '\n'.join(win_text))

  def _AddListExampleLinux(self, parent, policy):
    '''Adds an example value for Linux of a 'list' policy to a DOM node.

    Args:
      parent: The DOM node for which the example will be added.
      policy: A policy of type 'list', for which the Linux example value
        is generated.
    '''
    example_value = policy['annotations']['example_value']
    self.AddElement(parent, 'dt', {}, 'Linux:')
    linux = self._AddStyledElement(parent, 'dd', ['.monospace'])
    linux_text = []
    for item in example_value:
      linux_text.append('"%s"' % item)
    self.AddText(linux, '[%s]' % ', '.join(linux_text))


  def _AddListExample(self, parent, policy):
    '''Adds the example value of a 'list' policy to a DOM node. Example output:
    <dl>
      <dt>Windows:</dt>
      <dd>
        Software\Policies\Chromium\DisabledPlugins\0 = "Java"
        Software\Policies\Chromium\DisabledPlugins\1 = "Shockwave Flash"
      </dd>
      <dt>Linux:</dt>
      <dd>["Java", "Shockwave Flash"]</dd>
      <dt>Mac:</dt>
      <dd>
        <array>
          <string>Java</string>
          <string>Shockwave Flash</string>
        </array>
      </dd>
    </dl>

    Args:
      parent: The DOM node for which the example will be added.
      policy: The data structure of a policy.
    '''
    example_value = policy['annotations']['example_value']
    examples = self._AddStyledElement(parent, 'dl', ['dd dl'])
    self._AddListExampleWindows(examples, policy)
    self._AddListExampleLinux(examples, policy)
    self._AddListExampleMac(examples, policy)

  def _AddExample(self, parent, policy):
    '''Adds the HTML DOM representation of the example value of a policy to
    a DOM node. It is simple text for boolean policies, like
    '0x00000001 (Windows), true (Linux), <true /> (Mac)' in case of boolean
    policies, but it may also contain other HTML elements. (See method
    _AddListExample.)

    Args:
      parent: The DOM node for which the example will be added.
      policy: The data structure of a policy.

    Raises:
      Exception: If the type of the policy is unknown or the example value
        of the policy is out of its expected range.
    '''
    example_value = policy['annotations']['example_value']
    policy_type = policy['type']
    if policy_type == 'main':
      if example_value == True:
        self.AddText(
            parent, '0x00000001 (Windows), true (Linux), <true /> (Mac)')
      elif example_value == False:
        self.AddText(
            parent, '0x00000000 (Windows), false (Linux), <false /> (Mac)')
      else:
        raise Exception('Expected boolean value.')
    elif policy_type == 'string':
      self.AddText(parent, '"%s"' % example_value)
    elif policy_type == 'enum':
      self.AddText(
          parent,
          '0x%08x (Windows), %d (Linux/Mac)' % (example_value, example_value))
    elif policy_type == 'list':
      self._AddListExample(parent, policy)
    else:
      raise Exception('Unknown policy type: ' + policy_type)

  def _AddPolicyAttribute(self, dl, term_id,
                          definition=None, definition_style=None):
    '''Adds a term-definition pair to a HTML DOM <dl> node. This method is
    used by _AddPolicyDetails. Its result will have the form of:
      <dt style="...">...</dt>
      <dd style="...">...</dd>

    Args:
      dl: The DOM node of the <dl> list.
      term_id: A key to self._STRINGS[] which specifies the term of the pair.
      definition: The text of the definition. (Optional.)
      definition_style: List of references to values self._STYLE[] that specify
        the CSS stylesheet of the <dd> (definition) element.

    Returns:
      The DOM node representing the definition <dd> element.
    '''
    # Avoid modifying the default value of definition_style.
    if definition_style == None:
      definition_style = []
    term = self._GetLocalizedMessage(term_id)
    self._AddStyledElement(dl, 'dt', ['dt'], {}, term)
    return self._AddStyledElement(dl, 'dd', definition_style, {}, definition)

  def _AddPolicyDetails(self, parent, policy):
    '''Adds the list of attributes of a policy to the HTML DOM node parent.
    It will have the form:
    <dl>
      <dt>Attribute:</dt><dd>Description</dd>
      ...
    </dl>

    Args:
      parent: A DOM element for which the list will be added.
      policy: The data structure of the policy.
    '''
    annotations = policy['annotations']

    dl = self.AddElement(parent, 'dl')
    self._AddPolicyAttribute(
        dl,
        'data_type',
        self._TYPE_MAP[policy['type']])
    self._AddPolicyAttribute(
        dl,
        'win_reg_loc',
        self.config['win_reg_key_name'] + '\\' + policy['name'],
        ['.monospace'])
    self._AddPolicyAttribute(
        dl,
        'mac_linux_pref_name',
        policy['name'],
        ['.monospace'])
    self._AddPolicyAttribute(
        dl,
        'supported_on_platforms',
        self._MapListToString(self._PLATFORM_MAP, annotations['platforms']))
    self._AddPolicyAttribute(
        dl,
        'supported_in_products',
        self._MapListToString(self._PRODUCT_MAP, annotations['products']))
    dd = self._AddPolicyAttribute(dl, 'supported_features')
    self._AddFeatures(dd, policy)
    dd = self._AddPolicyAttribute(dl, 'description')
    self._AddDescription(dd, policy)
    dd = self._AddPolicyAttribute(dl, 'example_value')
    self._AddExample(dd, policy)

  def _AddPolicyNote(self, parent, policy):
    '''If a policy has an additional web page assigned with it, then add
    a link for that page.

    Args:
      policy: The data structure of the policy.
    '''
    if 'problem_href' not in policy['annotations']:
      return
    problem_href = policy['annotations']['problem_href']
    div = self._AddStyledElement(parent, 'div', ['div.note'])
    note = self._GetLocalizedMessage('note').replace('$6', problem_href)
    self._AddTextWithLinks(div, note)

  def _AddPolicyRow(self, parent, policy):
    '''Adds a row for the policy in the summary table.

    Args:
      parent: The DOM node of the summary table.
      policy: The data structure of the policy.
    '''
    caption = self._GetLocalizedPolicyMessage(policy, 'caption')
    tr = self._AddStyledElement(parent, 'tr', ['tr'])
    name_td = self._AddStyledElement(tr, 'td', ['td', 'td.left'])
    self.AddElement(
        name_td, 'a',
        {'href': '#' + policy['name']}, policy['name'])
    self._AddStyledElement(tr, 'td', ['td', 'td.right'], {}, caption)

  def _AddPolicySection(self, parent, policy):
    '''Adds a section about the policy in the detailed policy listing.

    Args:
      parent: The DOM node of the <div> of the detailed policy list.
      policy: The data structure of the policy.
    '''
    h2 = self.AddElement(parent, 'h2')
    caption = self._GetLocalizedPolicyMessage(policy, 'caption')
    self.AddElement(h2, 'a', {'name': policy['name']})
    self.AddText(h2, policy['name'])
    self.AddElement(parent, 'span', {}, caption)
    self._AddPolicyNote(parent, policy)

    self._AddPolicyDetails(parent, policy)
    self.AddElement(
        parent, 'a', {'href': '#top'},
        self._GetLocalizedMessage('back_to_top'))

  #
  # Implementation of abstract methods of TemplateWriter:
  #

  def WritePolicy(self, policy):
    self._AddPolicyRow(self._summary_tbody, policy)
    self._AddPolicySection(self._details_div, policy)

  def BeginTemplate(self):
    # Add a <div> for the summary section.
    summary_div = self.AddElement(self._main_div, 'div')
    self.AddElement(summary_div, 'a', {'name': 'top'})
    self.AddElement(summary_div, 'br')
    self._AddTextWithLinks(
        summary_div,
        self._GetLocalizedMessage('intro'))
    self.AddElement(summary_div, 'br')
    self.AddElement(summary_div, 'br')
    self.AddElement(summary_div, 'br')
    # Add the summary table of policies.
    summary_table = self._AddStyledElement(summary_div, 'table', ['table'])
    # Add the first row.
    thead = self.AddElement(summary_table, 'thead')
    tr = self._AddStyledElement(thead, 'tr', ['tr'])
    self._AddStyledElement(
        tr, 'td', ['td', 'td.left', 'thead td'], {},
        self._GetLocalizedMessage('name_column_title'))
    self._AddStyledElement(
        tr, 'td', ['td', 'td.right', 'thead td'], {},
        self._GetLocalizedMessage('description_column_title'))
    self._summary_tbody = self.AddElement(summary_table, 'tbody')

    # Add a <div> for the detailed policy listing.
    self._details_div = self.AddElement(self._main_div, 'div')

  def Init(self):
    dom_impl = minidom.getDOMImplementation('')
    self._doc = dom_impl.createDocument(None, 'html', None)
    body = self.AddElement(self._doc.documentElement, 'body')
    self._main_div = self.AddElement(body, 'div')

    # Human-readable names of supported platforms.
    self._PLATFORM_MAP = {
      'win': 'Windows',
      'mac': 'Mac',
      'linux': 'Linux',
    }
    # Human-readable names of supported products.
    self._PRODUCT_MAP = {
      'chrome': self.config['app_name'],
      'chrome_frame': self.config['frame_name'],
    }
    # Human-readable names of supported features.
    self._FEATURE_MAP = {
      'dynamic_refresh': 'Dynamic Policy Refresh'
    }
    # Human-readable names of types.
    self._TYPE_MAP = {
      'string': 'String (REG_SZ)',
      'main': 'Boolean (REG_DWORD)',
      'enum': 'Integer (REG_DWORD)',
      'list': 'List of strings',
    }
    # The CSS style-sheet used for the document. It will be used in Google
    # Sites, which strips class attributes from HTML tags. To work around this,
    # the style-sheet is a dictionary and the style attributes will be added
    # "by hand" for each element.
    self._STYLE = {
      'table': 'border-style: none; border-collapse: collapse;',
      'tr': 'height: 0px;',
      'td': 'border: 1px dotted rgb(170, 170, 170); padding: 7px; '
          'vertical-align: top; width: 236px; height: 15px;',
      'thead td': 'font-weight: bold;',
      'td.left': 'width: 200px;',
      'td.right': 'width: 100%;',
      'dt': 'font-weight: bold;',
      'dd dl': 'margin-top: 0px; margin-bottom: 0px;',
      '.monospace': 'font-family: monospace;',
      '.pre': 'white-space: pre;',
      'div.note': 'border: 2px solid black; padding: 5px; margin: 5px;'
    }

    # A simple regexp to search for URLs. It is enough for now.
    self._url_matcher = re.compile('(http://[^\\s]*[^\\s\\.])')

  def GetTemplateText(self):
    # Return the text representation of the main <div> tag.
    return self._main_div.toxml()
    # To get a complete HTML file, use the following.
    # return self._doc.toxml()
