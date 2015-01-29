#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys
import re

IMPORT_TEMPLATE = '<import src="/gen/%s.sky" as="%s" />'
PREAMBLE_TEMPLATE = '<script>'
POSTAMBLE_TEMPLATE = '  module.exports = exports;\n</script>'

class Import(object):
  def __init__(self, path, name):
    self.path = path
    self.name = name

class Module(object):
  def __init__(self):
    self.name = ""
    self.imports = []
    self.body = ""

def Serialize(module):
  lines = []
  for i in module.imports:
    lines.append(IMPORT_TEMPLATE % (i.path, i.name))
  lines.append(PREAMBLE_TEMPLATE)
  lines.append(module.body)
  lines.append(POSTAMBLE_TEMPLATE)
  return "\n".join(lines)

name_regex = re.compile(r'define\("([^"]+)"')
import_regex = re.compile(r' +"([^"]+)",')
begin_body_regexp = re.compile(r', function\(([^)]*)\)')
end_body_regexp = re.compile(r'return exports')

def AddImportNames(module, unparsed_names):
  names = [n.strip() for n in unparsed_names.split(',')]
  for i in range(len(module.imports)):
    module.imports[i].name = names[i]

def RewritePathNames(path):
  return path.replace("mojo/public/js", "mojo/public/sky") \
             .replace("mojo/services/public/js", "mojo/services/public/sky")

def Parse(amd_module):
  module = Module()
  body_lines = []
  state = "name"
  for line in amd_module.splitlines():
    if state == "name":
      m = name_regex.search(line)
      if m:
        module.name = m.group(1)
        m = begin_body_regexp.search(line)
        if m:
          AddImportNames(module, m.group(1))
          state = "body"
        else:
          state = "imports"
      continue
    if state == "imports":
      m = import_regex.search(line)
      if m:
        module.imports.append(Import(RewritePathNames(m.group(1)), None))
        continue
      m = begin_body_regexp.search(line)
      if m:
        AddImportNames(module, m.group(1))
        state = "body"
        continue
      raise Exception("Unknown import declaration:" + line)
    if state == "body":
      if end_body_regexp.search(line):
        module.body = "\n".join(body_lines)
        return module
      body_lines.append(line)
      continue
    raise Exception("Unknown parser state")
  raise Exception("End of file reached with finding a module")

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument("--input")
  parser.add_argument("--output")
  args = parser.parse_args()

  module = None
  with open(args.input, "r") as input_file:
    module = Parse(input_file.read())

  with open(args.output, "w+") as output_file:
    output_file.write(Serialize(module))

  return 0

if __name__ == "__main__":
  sys.exit(main())
