#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Replaces "Chrome Remote Desktop" with "Chromoting" in GRD files"""

import sys
from optparse import OptionParser
import xml.dom.minidom as minidom

def update_xml_node(element):
  for child in element.childNodes:
    if child.nodeType == minidom.Node.ELEMENT_NODE:
      update_xml_node(child)
    elif child.nodeType == minidom.Node.TEXT_NODE:
      child.replaceWholeText(
          child.data.replace("Chrome Remote Desktop", "Chromoting"))

def remove_official_branding(input, output):
  xml = minidom.parse(input)

  # Remove all translations.
  for translations in xml.getElementsByTagName("translations"):
    for translation in xml.getElementsByTagName("translation"):
      translations.removeChild(translation)

  # Update branding.
  update_xml_node(xml)

  out = file(output, "w")
  out.write(xml.toxml(encoding = "UTF-8"))
  out.close()

def main():
  usage = 'Usage: remove_official_branding <input.grd> <output.grd>'
  parser = OptionParser(usage=usage)
  options, args = parser.parse_args()
  if len(args) != 2:
    parser.error('two positional arguments expected')

  return remove_official_branding(args[0], args[1])

if __name__ == '__main__':
  sys.exit(main())

