# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from xml.dom import minidom
from grit.format.policy_templates.writers import template_writer


class XMLFormattedWriter(template_writer.TemplateWriter):
  '''Helper class for generating XML-based templates.
  '''

  def AddElement(self, parent, name, attrs=None, text=None):
    '''
    Adds a new XML Element as a child to an existing element or the Document.

    Args:
      parent: An XML element or the document, where the new element will be
        added.
      name: The name of the new element.
      attrs: A dictionary of the attributes' names and values for the new
        element.
      text: Text content for the new element.

    Returns:
      The created new element.
    '''
    if attrs == None:
      attrs = {}

    doc = parent.ownerDocument
    element = doc.createElement(name)
    for key, value in attrs.iteritems():
      element.setAttribute(key, value)
    if text:
      element.appendChild(doc.createTextNode(text))
    parent.appendChild(element)
    return element

  def AddText(self, parent, text):
    '''Adds text to a parent node.
    '''
    doc = parent.ownerDocument
    parent.appendChild(doc.createTextNode(text))
