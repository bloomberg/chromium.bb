# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Process wayland specifications.

Various functions for converting/processing/understanding wayland protocols.
"""

from __future__ import absolute_import
import argparse
import sys
import xml.etree.ElementTree as xml
import gv_diagram
import proto_gen


def strip_protocol(protocol):
  for desc in protocol.findall('description'):
    protocol.remove(desc)
  for copy in protocol.findall('copyright'):
    protocol.remove(copy)
  if 'summary' in protocol.attrib:
    protocol.attrib.pop('summary')
  for c in protocol.getchildren():
    strip_protocol(c)
  return protocol


def dump_protocol(protocol):
  xml.dump(protocol)


def read_protocol(path):
  return xml.parse(path).getroot()


def main(argv):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('-s', '--spec',
                      help='path(s) to the wayland specification(s)',
                      nargs='+', required=True)
  parser.add_argument('-t', '--type',
                      help='Output different types of graph',
                      choices=['interfaces', 'deps',
                               'harness', 'proto'], required=True)
  parser.add_argument('-x', '--extra',
                      help='add extra detail to the normal printout',
                      action='store_true')

  parsed = parser.parse_args(argv[1:])
  protocols = [strip_protocol(read_protocol(path)) for path in parsed.spec]
  extra = parsed.extra
  if parsed.type == 'deps':
    gv_diagram.DepsPrinter(extra).draw(protocols)
  elif parsed.type == 'interfaces':
    gv_diagram.InterfacesPrinter(extra).draw(protocols)
  elif parsed.type == 'harness':
    pass
  elif parsed.type == 'proto':
    proto_gen.generate(protocols)
  else:
    raise Exception('%s not implemented' % parsed.type)


if __name__ == '__main__':
  main(sys.argv)
