# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility file for pretty print xml file.

The function PrettyPrintNode will be used for formatting both histograms.xml
and actions.xml.
"""

import logging
import textwrap
import xml.dom.minidom

WRAP_COLUMN = 80


class Error(Exception):
  pass


def LastLineLength(s):
  """Returns the length of the last line in s.

  Args:
    s: A multi-line string, including newlines.

  Returns:
    The length of the last line in s, in characters.
  """
  if s.rfind('\n') == -1: return len(s)
  return len(s) - s.rfind('\n') - len('\n')


def XmlEscape(s):
  """XML-escapes the given string, replacing magic characters (&<>") with their
  escaped equivalents."""
  s = s.replace("&", "&amp;").replace("<", "&lt;")
  s = s.replace("\"", "&quot;").replace(">", "&gt;")
  return s


class XmlStyle(object):
  """A class that stores all style specification for an output xml file."""

  def __init__(self, attribute_order, tags_that_have_extra_newline,
               tags_that_dont_indent, tags_that_allow_single_line):
    # List of tag names for top-level nodes whose children are not indented.
    self.attribute_order = attribute_order
    self.tags_that_have_extra_newline = tags_that_have_extra_newline
    self.tags_that_dont_indent = tags_that_dont_indent
    self.tags_that_allow_single_line = tags_that_allow_single_line

  def PrettyPrintNode(self, node, indent=0):
    """Pretty-prints the given XML node at the given indent level.

    Args:
      node: The minidom node to pretty-print.
      indent: The current indent level.

    Returns:
      The pretty-printed string (including embedded newlines).

    Raises:
      Error if the XML has unknown tags or attributes.
    """
    # Handle the top-level document node.
    if node.nodeType == xml.dom.minidom.Node.DOCUMENT_NODE:
      return '\n'.join([self.PrettyPrintNode(n) for n in node.childNodes])

    # Handle text nodes.
    if node.nodeType == xml.dom.minidom.Node.TEXT_NODE:
      # Wrap each paragraph in the text to fit in the 80 column limit.
      wrapper = textwrap.TextWrapper()
      wrapper.initial_indent = ' ' * indent
      wrapper.subsequent_indent = ' ' * indent
      wrapper.break_on_hyphens = False
      wrapper.break_long_words = False
      wrapper.width = WRAP_COLUMN
      text = XmlEscape(node.data)
      # Remove any common indent.
      text = textwrap.dedent(text.strip('\n'))
      lines = text.split('\n')
      # Split the text into paragraphs at blank line boundaries.
      paragraphs = [[]]
      for l in lines:
        if len(l.strip()) == 0 and len(paragraphs[-1]) > 0:
          paragraphs.append([])
        else:
          paragraphs[-1].append(l)
      # Remove trailing empty paragraph if present.
      if len(paragraphs) > 0 and len(paragraphs[-1]) == 0:
        paragraphs = paragraphs[:-1]
      # Wrap each paragraph and separate with two newlines.
      return '\n\n'.join([wrapper.fill('\n'.join(p)) for p in paragraphs])

    # Handle element nodes.
    if node.nodeType == xml.dom.minidom.Node.ELEMENT_NODE:
      newlines_after_open, newlines_before_close, newlines_after_close = (
          self.tags_that_have_extra_newline.get(node.tagName, (1, 1, 0)))
      # Open the tag.
      s = ' ' * indent + '<' + node.tagName

      # Calculate how much space to allow for the '>' or '/>'.
      closing_chars = 1
      if not node.childNodes:
        closing_chars = 2

      # Pretty-print the attributes.
      attributes = node.attributes.keys()
      if attributes:
        # Reorder the attributes.
        if node.tagName not in self.attribute_order:
          unrecognized_attributes = attributes
        else:
          unrecognized_attributes = (
              [a for a in attributes
               if a not in self.attribute_order[node.tagName]])
          attributes = [a for a in self.attribute_order[node.tagName]
                        if a in attributes]

        for a in unrecognized_attributes:
          logging.error(
              'Unrecognized attribute "%s" in tag "%s"' % (a, node.tagName))
        if unrecognized_attributes:
          raise Error()

        for a in attributes:
          value = XmlEscape(node.attributes[a].value)
          # Replace sequences of whitespace with single spaces.
          words = value.split()
          a_str = ' %s="%s"' % (a, ' '.join(words))
          # Start a new line if the attribute will make this line too long.
          if LastLineLength(s) + len(a_str) + closing_chars > WRAP_COLUMN:
            s += '\n' + ' ' * (indent + 3)
          # Output everything up to the first quote.
          s += ' %s="' % (a)
          value_indent_level = LastLineLength(s)
          # Output one word at a time, splitting to the next line where
          # necessary.
          column = value_indent_level
          for i, word in enumerate(words):
            # This is slightly too conservative since not every word will be
            # followed by the closing characters...
            if i > 0 and (column + len(word) + 1 + closing_chars > WRAP_COLUMN):
              s = s.rstrip()  # remove any trailing whitespace
              s += '\n' + ' ' * value_indent_level
              column = value_indent_level
            s += word + ' '
            column += len(word) + 1
          s = s.rstrip()  # remove any trailing whitespace
          s += '"'
        s = s.rstrip()  # remove any trailing whitespace

      # Pretty-print the child nodes.
      if node.childNodes:
        s += '>'
        # Calculate the new indent level for child nodes.
        new_indent = indent
        if node.tagName not in self.tags_that_dont_indent:
          new_indent += 2
        child_nodes = node.childNodes

        # Recursively pretty-print the child nodes.
        child_nodes = [self.PrettyPrintNode(n, indent=new_indent)
                       for n in child_nodes]
        child_nodes = [c for c in child_nodes if len(c.strip()) > 0]

        # Determine whether we can fit the entire node on a single line.
        close_tag = '</%s>' % node.tagName
        space_left = WRAP_COLUMN - LastLineLength(s) - len(close_tag)
        if (node.tagName in self.tags_that_allow_single_line and
            len(child_nodes) == 1 and
            len(child_nodes[0].strip()) <= space_left):
          s += child_nodes[0].strip()
        else:
          s += '\n' * newlines_after_open + '\n'.join(child_nodes)
          s += '\n' * newlines_before_close + ' ' * indent
        s += close_tag
      else:
        s += '/>'
      s += '\n' * newlines_after_close
      return s

    # Handle comment nodes.
    if node.nodeType == xml.dom.minidom.Node.COMMENT_NODE:
      return '<!--%s-->\n' % node.data

    # Ignore other node types. This could be a processing instruction
    # (<? ... ?>) or cdata section (<![CDATA[...]]!>), neither of which are
    # legal in the histograms XML at present.
    logging.error('Ignoring unrecognized node data: %s' % node.toxml())
    raise Error()
