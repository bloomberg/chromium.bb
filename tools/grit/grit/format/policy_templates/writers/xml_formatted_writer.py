# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from xml.dom.minidom import Document
from xml.dom.minidom import Node
from template_writer import TemplateWriter


class XMLFormattedWriter(TemplateWriter):
  '''Helper class for generating XML-based templates.
  '''

  def AddElement(self, parent, name, attrs = {}, text = None):
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
    if isinstance(parent, Document):
      doc = parent
    else:
      doc = parent.ownerDocument
    element = doc.createElement(name)
    for key, value in attrs.iteritems():
      element.attributes[key] = value
    if text:
      element.appendChild( doc.createTextNode(text) )
    parent.appendChild(element)
    return element
