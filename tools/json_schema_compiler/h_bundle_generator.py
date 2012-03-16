# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from model import PropertyType
import code
import cpp_util
import model

SOURCE_BASE_PATH = 'chrome/common/extensions/api'

class HBundleGenerator(object):
  """A .h generator for namespace bundle functionality.
  """
  def __init__(self, model, cpp_type_generator):
    self._cpp_type_generator = cpp_type_generator
    self._model = model

  def Generate(self):
    """Generates a code.Code object with the .h for a bundle.
    """
    c = code.Code()
    (c.Append(cpp_util.CHROMIUM_LICENSE)
      .Append()
      .Append(cpp_util.GENERATED_BUNDLE_FILE_MESSAGE % SOURCE_BASE_PATH)
      .Append()
    )

    ifndef_name = cpp_util.GenerateIfndefName(SOURCE_BASE_PATH,
                                              'generated_api')
    (c.Append('#ifndef %s' % ifndef_name)
      .Append('#define %s' % ifndef_name)
      .Append('#pragma once')
      .Append()
      .Append('#include <string>')
      .Append()
      .Append('#include "base/basictypes.h"'))

    for namespace in self._model.namespaces.values():
      namespace_name = namespace.name.replace(
            "experimental.", "")
      c.Append('#include "chrome/browser/extensions/api/%s/%s_api.h"' % (
          namespace_name, namespace_name))

    (c.Append()
      .Append("class ExtensionFunctionRegistry;")
      .Append())

    c.Concat(self._cpp_type_generator.GetRootNamespaceStart())

    for namespace in self._model.namespaces.values():
      c.Append("// TODO(miket): emit code for %s" % (namespace.unix_name))
    c.Append()

    c.Concat(self.GenerateFunctionRegistry())

    (c.Concat(self._cpp_type_generator.GetRootNamespaceEnd())
      .Append()
      .Append('#endif  // %s' % ifndef_name)
      .Append()
    )
    return c

  def GenerateFunctionRegistry(self):
    c = code.Code()
    c.Sblock("class GeneratedFunctionRegistry {")
    c.Append("public:")
    c.Sblock("static void RegisterAll(ExtensionFunctionRegistry* registry) {")
    for namespace in self._model.namespaces.values():
      for function in namespace.functions.values():
        namespace_name = namespace.name.replace(
            "experimental.", "").capitalize()
        function_name = namespace_name + function.name.capitalize()
        c.Append("registry->RegisterFunction<%sFunction>();" % (
            function_name))
    c.Eblock("}")
    c.Eblock("};")
    c.Append()
    return c
