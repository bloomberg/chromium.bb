# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from code import Code
from model import PropertyType
import cpp_util

class CppTypeGenerator(object):
  """Manages the types of properties and provides utilities for getting the
  C++ type out of a model.Property
  """
  def __init__(self, root_namespace, namespace, cpp_namespace):
    """Creates a cpp_type_generator. The given root_namespace should be of the
    format extensions::api::sub. The generator will generate code suitable for
    use in the given namespace.
    """
    self._type_namespaces = {}
    self._namespace = namespace
    self._root_namespace = root_namespace.split('::')
    self._cpp_namespaces = {}
    self.AddNamespace(namespace, cpp_namespace)

  def AddNamespace(self, namespace, cpp_namespace):
    """Maps a model.Namespace to its C++ namespace name. All mappings are
    beneath the root namespace.
    """
    for type_ in namespace.types:
      self._type_namespaces[type_] = namespace
    self._cpp_namespaces[namespace] = cpp_namespace

  def GetCppNamespaceName(self, namespace):
    """Gets the mapped C++ namespace name for the given namespace relative to
    the root namespace.
    """
    return self._cpp_namespaces[namespace]

  def GetCppNamespaceStart(self):
    """Get opening namespace declarations.
    """
    c = Code()
    for n in self._root_namespace:
      c.Append('namespace %s {' % n)
    c.Append('namespace %s {' % self.GetCppNamespaceName(self._namespace))
    return c

  def GetCppNamespaceEnd(self):
    """Get closing namespace declarations.
    """
    c = Code()
    c.Append('}  // %s' % self.GetCppNamespaceName(self._namespace))
    for n in reversed(self._root_namespace):
      c.Append('}  // %s' % n)
    return c

  # TODO(calamity): Handle ANY
  def GetType(self, prop, pad_for_generics=False):
    """Translates a model.Property into its C++ type.

    If REF types from different namespaces are referenced, will resolve
    using self._type_namespaces.

    Use pad_for_generics when using as a generic to avoid operator ambiguity.
    """
    cpp_type = None
    if prop.type_ == PropertyType.REF:
      dependency_namespace = self._type_namespaces.get(prop.ref_type)
      if not dependency_namespace:
        raise KeyError('Cannot find referenced type: %s' % prop.ref_type)
      if self._namespace != dependency_namespace:
        cpp_type = '%s::%s' % (self._cpp_namespaces[dependency_namespace],
                               prop.ref_type)
      else:
        cpp_type = '%s' % prop.ref_type
    elif prop.type_ == PropertyType.BOOLEAN:
      cpp_type = 'bool'
    elif prop.type_ == PropertyType.INTEGER:
      cpp_type = 'int'
    elif prop.type_ == PropertyType.DOUBLE:
      cpp_type = 'double'
    elif prop.type_ == PropertyType.STRING:
      cpp_type = 'std::string'
    elif prop.type_ == PropertyType.ARRAY:
      cpp_type = 'std::vector<%s>' % self.GetType(
          prop.item_type, pad_for_generics=True)
    elif prop.type_ == PropertyType.OBJECT:
      cpp_type = cpp_util.CppName(prop.name)
    # TODO(calamity): choices
    else:
      raise NotImplementedError

    # Add a space to prevent operator ambiguity
    if pad_for_generics and cpp_type[-1] == '>':
      return '%s ' % cpp_type
    return '%s' % cpp_type

  def GenerateCppIncludes(self):
    """Returns the #include lines for self._namespace using the other
    namespaces in self._model.
    """
    dependencies = set()
    for function in self._namespace.functions.values():
      for param in function.params:
        dependencies |= self._TypeDependencies(param)
      dependencies |= self._TypeDependencies(function.callback.param)
    for type_ in self._namespace.types.values():
      for prop in type_.properties.values():
        dependencies |= self._TypeDependencies(prop)

    dependency_namespaces = set()
    for dependency in dependencies:
      dependency_namespace = self._type_namespaces[dependency]
      if dependency_namespace != self._namespace:
        dependency_namespaces.add(dependency_namespace)

    includes = Code()
    for dependency_namespace in sorted(dependency_namespaces):
      includes.Append('#include "%s/%s.h"' % (
            dependency_namespace.source_file_dir,
            self._cpp_namespaces[dependency_namespace]))
    return includes

  def _TypeDependencies(self, prop):
    """Recursively gets all the type dependencies of a property.
    """
    deps = set()
    if prop:
      if prop.type_ == PropertyType.REF:
        deps.add(prop.ref_type)
      elif prop.type_ == PropertyType.ARRAY:
        deps = self._TypeDependencies(prop.item_type)
      elif prop.type_ == PropertyType.OBJECT:
        for p in prop.properties.values():
          deps |= self._TypeDependencies(p)
    return deps
