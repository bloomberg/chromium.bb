# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import code
import cpp_util

import json
import os
import re

# TODO(miket/asargent) - parameterize this.
SOURCE_BASE_PATH = 'chrome/common/extensions/api'

class SchemaBundleGenerator(object):
  """This class contains methods to generate code based on multiple schemas.
  """

  def __init__(self, model, api_defs, cpp_type_generator):
    self._model = model
    self._api_defs = api_defs
    self._cpp_type_generator = cpp_type_generator

  def GenerateHeader(self, file_base, body_code):
    """Generates a code.Code object for a header file

    Parameters:
    - |file_base| - the base of the filename, e.g. 'foo' (for 'foo.h')
    - |body_code| - the code to put in between the multiple inclusion guards"""
    c = code.Code()
    c.Append(cpp_util.CHROMIUM_LICENSE)
    c.Append()
    c.Append(cpp_util.GENERATED_BUNDLE_FILE_MESSAGE % SOURCE_BASE_PATH)
    ifndef_name = cpp_util.GenerateIfndefName(SOURCE_BASE_PATH, file_base)
    c.Append()
    c.Append('#ifndef %s' % ifndef_name)
    c.Append('#define %s' % ifndef_name)
    c.Append('#pragma once')
    c.Append()
    c.Concat(body_code)
    c.Append()
    c.Append('#endif  // %s' % ifndef_name)
    c.Append()
    return c

  def GenerateAPIHeader(self):
    """Generates the header for API registration / declaration"""
    c = code.Code()

    c.Append('#include <string>')
    c.Append()
    c.Append('#include "base/basictypes.h"')

    for namespace in self._model.namespaces.values():
      namespace_name = namespace.unix_name.replace("experimental_", "")
      c.Append('#include "chrome/browser/extensions/api/%s/%s_api.h"' % (
          namespace_name, namespace_name))

    c.Append()
    c.Append("class ExtensionFunctionRegistry;")
    c.Append()

    c.Concat(self._cpp_type_generator.GetRootNamespaceStart())
    for namespace in self._model.namespaces.values():
      c.Append("// TODO(miket): emit code for %s" % (namespace.unix_name))
    c.Append()
    c.Concat(self.GenerateFunctionRegistry())
    c.Concat(self._cpp_type_generator.GetRootNamespaceEnd())
    c.Append()
    return self.GenerateHeader('generated_api', c)

  def CapitalizeFirstLetter(self, value):
    return value[0].capitalize() + value[1:]

  def GenerateFunctionRegistry(self):
    c = code.Code()
    c.Sblock("class GeneratedFunctionRegistry {")
    c.Append("public:")
    c.Sblock("static void RegisterAll(ExtensionFunctionRegistry* registry) {")
    for namespace in self._model.namespaces.values():
      for function in namespace.functions.values():
        if function.nocompile:
          continue
        namespace_name = self.CapitalizeFirstLetter(namespace.name.replace(
            "experimental.", ""))
        function_name = namespace_name + self.CapitalizeFirstLetter(
            function.name)
        c.Append("registry->RegisterFunction<%sFunction>();" % (
            function_name))
    c.Eblock("}")
    c.Eblock("};")
    c.Append()
    return c

  def GenerateSchemasHeader(self):
    """Generates a code.Code object for the generated schemas .h file"""
    c = code.Code()
    c.Append('#include <map>')
    c.Append('#include <string>')
    c.Append();
    c.Append('#include "base/string_piece.h"')
    c.Append()
    c.Concat(self._cpp_type_generator.GetRootNamespaceStart())
    c.Append()
    c.Sblock('class GeneratedSchemas {')
    c.Append('public:')
    c.Append('// Puts all API schemas in |schemas|.')
    c.Append('static void Get('
                 'std::map<std::string, base::StringPiece>* schemas);')
    c.Eblock('};');
    c.Append()
    c.Concat(self._cpp_type_generator.GetRootNamespaceEnd())
    c.Append()
    return self.GenerateHeader('generated_schemas', c)

  def GenerateSchemasCC(self):
    """Generates a code.Code object for the generated schemas .cc file"""
    c = code.Code()
    c.Append(cpp_util.CHROMIUM_LICENSE)
    c.Append()
    c.Append('#include "%s"' % (os.path.join(SOURCE_BASE_PATH,
                                             'generated_schemas.h')))
    c.Append()
    c.Concat(self._cpp_type_generator.GetRootNamespaceStart())
    c.Append()
    c.Append('// static')
    c.Sblock('void GeneratedSchemas::Get('
                 'std::map<std::string, base::StringPiece>* schemas) {')
    for api in self._api_defs:
      namespace = self._model.namespaces[api.get('namespace')]
      # JSON parsing code expects lists of schemas, so dump a singleton list.
      json_content = json.dumps([api], indent=2)
      # Escape all double-quotes. Ignore already-escaped double-quotes.
      json_content = re.sub('(?<!\\\\)"', '\\"', json_content)
      lines = json_content.split('\n')
      c.Append('(*schemas)["%s"] = ' % namespace.name)
      for index, line in enumerate(lines):
        line = '    "%s"' % line
        if index == len(lines) - 1:
          line += ';'
        c.Append(line)
    c.Eblock('}')
    c.Append()
    c.Concat(self._cpp_type_generator.GetRootNamespaceEnd())
    c.Append()
    return c
