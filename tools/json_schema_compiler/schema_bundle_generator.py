# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import code
import cpp_util
from model import Platforms
from schema_util import CapitalizeFirstLetter
from schema_util import JsFunctionNameToClassName

import json
import os
import re

# TODO(miket/asargent) - parameterize this.
SOURCE_BASE_PATH = 'chrome/common/extensions/api'

def _RemoveDescriptions(node):
  """Returns a copy of |schema| with "description" fields removed.
  """
  if isinstance(node, dict):
    result = {}
    for key, value in node.items():
      # Some schemas actually have properties called "description", so only
      # remove descriptions that have string values.
      if key == 'description' and isinstance(value, basestring):
        continue
      result[key] = _RemoveDescriptions(value)
    return result
  if isinstance(node, list):
    return [_RemoveDescriptions(v) for v in node]
  return node

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
    c.Append()
    c.Concat(body_code)
    c.Append()
    c.Append('#endif  // %s' % ifndef_name)
    c.Append()
    return c

  def _GetPlatformIfdefs(self, model_object):
    """Generates the "defined" conditional for an #if check if |model_object|
    has platform restrictions. Returns None if there are no restrictions.
    """
    if model_object.platforms is None:
      return None
    ifdefs = []
    for platform in model_object.platforms:
      if platform == Platforms.CHROMEOS:
        ifdefs.append('defined(OS_CHROMEOS)')
      else:
        raise ValueError("Unsupported platform ifdef: %s" % platform.name)
    return ' and '.join(ifdefs)

  def GenerateAPIHeader(self):
    """Generates the header for API registration / declaration"""
    c = code.Code()

    c.Append('#include <string>')
    c.Append()
    c.Append('#include "base/basictypes.h"')

    for namespace in self._model.namespaces.values():
      ifdefs = self._GetPlatformIfdefs(namespace)
      if ifdefs is not None:
        c.Append("#if %s" % ifdefs, indent_level=0)

      namespace_name = namespace.unix_name.replace("experimental_", "")
      implementation_header = namespace.compiler_options.get(
          "implemented_in",
          "chrome/browser/extensions/api/%s/%s_api.h" % (namespace_name,
                                                         namespace_name))
      c.Append('#include "%s"' % implementation_header)

      if ifdefs is not None:
        c.Append("#endif  // %s" % ifdefs, indent_level=0)

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

  def _GetNamespaceFunctions(self, namespace):
    functions = list(namespace.functions.values())
    if namespace.compiler_options.get("generate_type_functions", False):
      for type_ in namespace.types.values():
        functions += list(type_.functions.values())
    return functions

  def GenerateFunctionRegistry(self):
    c = code.Code()
    c.Sblock("class GeneratedFunctionRegistry {")
    c.Append(" public:")
    c.Sblock("static void RegisterAll(ExtensionFunctionRegistry* registry) {")
    for namespace in self._model.namespaces.values():
      namespace_ifdefs = self._GetPlatformIfdefs(namespace)
      if namespace_ifdefs is not None:
        c.Append("#if %s" % namespace_ifdefs, indent_level=0)

      namespace_name = CapitalizeFirstLetter(namespace.name.replace(
          "experimental.", ""))
      for function in self._GetNamespaceFunctions(namespace):
        if function.nocompile:
          continue
        function_ifdefs = self._GetPlatformIfdefs(function)
        if function_ifdefs is not None:
          c.Append("#if %s" % function_ifdefs, indent_level=0)

        function_name = JsFunctionNameToClassName(namespace.name, function.name)
        c.Append("registry->RegisterFunction<%sFunction>();" % (
            function_name))

        if function_ifdefs is not None:
          c.Append("#endif  // %s" % function_ifdefs, indent_level=0)

      if namespace_ifdefs is not None:
        c.Append("#endif  // %s" % namespace_ifdefs, indent_level=0)
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
    c.Append(' public:')
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
      json_content = json.dumps([_RemoveDescriptions(api)],
                                separators=(',', ':'))
      # Escape all double-quotes and backslashes. For this to output a valid
      # JSON C string, we need to escape \ and ".
      json_content = json_content.replace('\\', '\\\\').replace('"', '\\"')
      c.Append('(*schemas)["%s"] = "%s";' % (namespace.name, json_content))
    c.Eblock('}')
    c.Append()
    c.Concat(self._cpp_type_generator.GetRootNamespaceEnd())
    c.Append()
    return c
