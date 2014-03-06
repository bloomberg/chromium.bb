# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Holds the constants for pretty printing actions.xml."""

import os
import sys

# Import the metrics/common module for pretty print xml.
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import pretty_print_xml

# Desired order for tag and tag attributes.
# { tag_name: [attribute_name, ...] }
ATTRIBUTE_ORDER = {
    'action': ['name'],
    'owner': [],
    'description': [],
    'obsolete': [],
}

# Tag names for top-level nodes whose children we don't want to indent.
TAGS_THAT_DONT_INDENT = ['actions']

# Extra vertical spacing rules for special tag names.
# {tag_name: (newlines_after_open, newlines_before_close, newlines_after_close)}
TAGS_THAT_HAVE_EXTRA_NEWLINE = {
    'actions': (2, 1, 1),
    'action': (1, 1, 1),
}

# Tags that we allow to be squished into a single line for brevity.
TAGS_THAT_ALLOW_SINGLE_LINE = ['owner', 'description', 'obsolete']

def GetPrintStyle():
  """Returns an XmlStyle object for pretty printing actions."""
  return pretty_print_xml.XmlStyle(ATTRIBUTE_ORDER,
                                   TAGS_THAT_HAVE_EXTRA_NEWLINE,
                                   TAGS_THAT_DONT_INDENT,
                                   TAGS_THAT_ALLOW_SINGLE_LINE)
