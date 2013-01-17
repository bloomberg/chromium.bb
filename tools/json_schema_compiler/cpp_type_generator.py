# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from code import Code
from model import Namespace, PropertyType, Type
import cpp_util
import operator
import schema_util

class CppTypeGenerator(object):
  """Manages the types of properties and provides utilities for getting the
  C++ type out of a model.Property
  """
  def __init__(self, root_namespace, namespace=None, cpp_namespace=None):
    """Creates a cpp_type_generator. The given root_namespace should be of the
    format extensions::api::sub. The generator will generate code suitable for
    use in the given namespace.
    """
    self._type_namespaces = {}
    self._root_namespace = root_namespace.split('::')
    self._cpp_namespaces = {}
    if namespace and cpp_namespace:
      self._namespace = namespace
      self.AddNamespace(namespace, cpp_namespace)
    else:
      self._namespace = None

  def AddNamespace(self, namespace, cpp_namespace):
    """Maps a model.Namespace to its C++ namespace name. All mappings are
    beneath the root namespace.
    """
    self._cpp_namespaces[namespace] = cpp_namespace
    for type_name in namespace.types:
      # Allow $refs to refer to just 'Type' within namespaces. Otherwise they
      # must be qualified with 'namespace.Type'.
      type_aliases = ['%s.%s' % (namespace.name, type_name)]
      if namespace is self._namespace:
        type_aliases.append(type_name)
      for alias in type_aliases:
        self._type_namespaces[alias] = namespace

  def GetCppNamespaceName(self, namespace):
    """Gets the mapped C++ namespace name for the given namespace relative to
    the root namespace.
    """
    return self._cpp_namespaces[namespace]

  def GetRootNamespaceStart(self):
    """Get opening root namespace declarations.
    """
    c = Code()
    for namespace in self._root_namespace:
      c.Append('namespace %s {' % namespace)
    return c

  def GetRootNamespaceEnd(self):
    """Get closing root namespace declarations.
    """
    c = Code()
    for namespace in reversed(self._root_namespace):
      c.Append('}  // %s' % namespace)
    return c

  def GetNamespaceStart(self):
    """Get opening self._namespace namespace declaration.
    """
    return Code().Append('namespace %s {' %
        self.GetCppNamespaceName(self._namespace))

  def GetNamespaceEnd(self):
    """Get closing self._namespace namespace declaration.
    """
    return Code().Append('}  // %s' %
        self.GetCppNamespaceName(self._namespace))

  def GetEnumNoneValue(self, type_):
    """Gets the enum value in the given model.Property indicating no value has
    been set.
    """
    return '%s_NONE' % self.FollowRef(type_).unix_name.upper()

  def GetEnumValue(self, type_, enum_value):
    """Gets the enum value of the given model.Property of the given type.

    e.g VAR_STRING
    """
    return '%s_%s' % (self.FollowRef(type_).unix_name.upper(),
                      cpp_util.Classname(enum_value.upper()))

  def GetCppType(self, type_, is_ptr=False, is_in_container=False):
    """Translates a model.Property or model.Type into its C++ type.

    If REF types from different namespaces are referenced, will resolve
    using self._type_namespaces.

    Use |is_ptr| if the type is optional. This will wrap the type in a
    scoped_ptr if possible (it is not possible to wrap an enum).

    Use |is_in_container| if the type is appearing in a collection, e.g. a
    std::vector or std::map. This will wrap it in the correct type with spacing.
    """
    cpp_type = None
    if type_.property_type == PropertyType.REF:
      ref = type_.ref_type
      dependency_namespace = self._ResolveTypeNamespace(ref)
      if not dependency_namespace:
        raise KeyError('Cannot find referenced type: %s' % ref)
      if self._namespace != dependency_namespace:
        cpp_type = '%s::%s' % (self._cpp_namespaces[dependency_namespace],
                               schema_util.StripSchemaNamespace(ref))
      else:
        cpp_type = schema_util.StripSchemaNamespace(ref)
    elif type_.property_type == PropertyType.BOOLEAN:
      cpp_type = 'bool'
    elif type_.property_type == PropertyType.INTEGER:
      cpp_type = 'int'
    elif type_.property_type == PropertyType.INT64:
      cpp_type = 'int64'
    elif type_.property_type == PropertyType.DOUBLE:
      cpp_type = 'double'
    elif type_.property_type == PropertyType.STRING:
      cpp_type = 'std::string'
    elif type_.property_type == PropertyType.ENUM:
      cpp_type = cpp_util.Classname(type_.name)
    elif type_.property_type == PropertyType.ANY:
      cpp_type = 'base::Value'
    elif (type_.property_type == PropertyType.OBJECT or
          type_.property_type == PropertyType.CHOICES):
      cpp_type = cpp_util.Classname(type_.name)
    elif type_.property_type == PropertyType.FUNCTION:
      # Functions come into the json schema compiler as empty objects. We can
      # record these as empty DictionaryValues so that we know if the function
      # was passed in or not.
      cpp_type = 'base::DictionaryValue'
    elif type_.property_type == PropertyType.ARRAY:
      item_cpp_type = self.GetCppType(type_.item_type, is_in_container=True)
      cpp_type = 'std::vector<%s>' % cpp_util.PadForGenerics(item_cpp_type)
    elif type_.property_type == PropertyType.BINARY:
      cpp_type = 'std::string'
    else:
      raise NotImplementedError('Cannot get type of %s' % type_.property_type)

    # HACK: optional ENUM is represented elsewhere with a _NONE value, so it
    # never needs to be wrapped in pointer shenanigans.
    # TODO(kalman): change this - but it's an exceedingly far-reaching change.
    if not self.FollowRef(type_).property_type == PropertyType.ENUM:
      if is_in_container and (is_ptr or not self.IsCopyable(type_)):
        cpp_type = 'linked_ptr<%s>' % cpp_util.PadForGenerics(cpp_type)
      elif is_ptr:
        cpp_type = 'scoped_ptr<%s>' % cpp_util.PadForGenerics(cpp_type)

    return cpp_type

  def IsCopyable(self, type_):
    return not (self.FollowRef(type_).property_type in (PropertyType.ANY,
                                                        PropertyType.ARRAY,
                                                        PropertyType.OBJECT,
                                                        PropertyType.CHOICES))

  def GenerateForwardDeclarations(self):
    """Returns the forward declarations for self._namespace.

    Use after GetRootNamespaceStart. Assumes all namespaces are relative to
    self._root_namespace.
    """
    c = Code()
    namespace_type_dependencies = self._NamespaceTypeDependencies()
    for namespace in sorted(namespace_type_dependencies.keys(),
                            key=operator.attrgetter('name')):
      c.Append('namespace %s {' % namespace.name)
      for type_name in sorted(namespace_type_dependencies[namespace],
                              key=schema_util.StripSchemaNamespace):
        simple_type_name = schema_util.StripSchemaNamespace(type_name)
        type_ = namespace.types[simple_type_name]
        # Add more ways to forward declare things as necessary.
        if type_.property_type == PropertyType.OBJECT:
          c.Append('struct %s;' % simple_type_name)
      c.Append('}')
    c.Concat(self.GetNamespaceStart())
    for (name, type_) in self._namespace.types.items():
      if not type_.functions and type_.property_type == PropertyType.OBJECT:
        c.Append('struct %s;' % schema_util.StripSchemaNamespace(name))
    c.Concat(self.GetNamespaceEnd())
    return c

  def GenerateIncludes(self):
    """Returns the #include lines for self._namespace.
    """
    c = Code()
    for header in sorted(
        ['%s/%s.h' % (dependency.source_file_dir,
                      self._cpp_namespaces[dependency])
         for dependency in self._NamespaceTypeDependencies().keys()]):
      c.Append('#include "%s"' % header)
    c.Append('#include "base/string_number_conversions.h"')

    if self._namespace.events:
      c.Append('#include "base/json/json_writer.h"')
    return c

  def _ResolveTypeNamespace(self, qualified_name):
    """Resolves a type, which must be explicitly qualified, to its enclosing
    namespace.
    """
    namespace = self._type_namespaces.get(qualified_name, None)
    if namespace is None:
      raise KeyError('Cannot resolve type %s. Maybe it needs a prefix '
                     'if it comes from another namespace?' % qualified_type)
    return namespace

  def FollowRef(self, type_):
    """Follows $ref link of types to resolve the concrete type a ref refers to.

    If the property passed in is not of type PropertyType.REF, it will be
    returned unchanged.
    """
    if not type_.property_type == PropertyType.REF:
      return type_
    ref = type_.ref_type

    without_namespace = ref
    if '.' in ref:
      without_namespace = ref.split('.', 1)[1]

    # TODO(kalman): Do we need to keep on resolving?
    return self._ResolveTypeNamespace(ref).types[without_namespace]

  def _NamespaceTypeDependencies(self):
    """Returns a dict containing a mapping of model.Namespace to the C++ type
    of type dependencies for self._namespace.
    """
    dependencies = set()
    for function in self._namespace.functions.values():
      for param in function.params:
        dependencies |= self._TypeDependencies(param.type_)
      if function.callback:
        for param in function.callback.params:
          dependencies |= self._TypeDependencies(param.type_)
    for type_ in self._namespace.types.values():
      for prop in type_.properties.values():
        dependencies |= self._TypeDependencies(prop.type_)
    for event in self._namespace.events.values():
      for param in event.params:
        dependencies |= self._TypeDependencies(param.type_)

    dependency_namespaces = dict()
    for dependency in dependencies:
      namespace = self._ResolveTypeNamespace(dependency)
      if namespace != self._namespace:
        dependency_namespaces.setdefault(namespace, [])
        dependency_namespaces[namespace].append(dependency)
    return dependency_namespaces

  def _TypeDependencies(self, type_):
    """Recursively gets all the type dependencies of a property.
    """
    deps = set()
    if type_.property_type == PropertyType.REF:
      deps.add(type_.ref_type)
    elif type_.property_type == PropertyType.ARRAY:
      deps = self._TypeDependencies(type_.item_type)
    elif type_.property_type == PropertyType.OBJECT:
      for p in type_.properties.values():
        deps |= self._TypeDependencies(p.type_)
    return deps

  def GeneratePropertyValues(self, property, line, nodoc=False):
    """Generates the Code to display all value-containing properties.
    """
    c = Code()
    if not nodoc:
      c.Comment(property.description)

    if property.value is not None:
      c.Append(line % {
          "type": self.GetCppType(property.type_),
          "name": property.name,
          "value": property.value
        })
    else:
      has_child_code = False
      c.Sblock('namespace %s {' % property.name)
      for child_property in property.type_.properties.values():
        child_code = self.GeneratePropertyValues(child_property,
                                                 line,
                                                 nodoc=nodoc)
        if child_code:
          has_child_code = True
          c.Concat(child_code)
      c.Eblock('}  // namespace %s' % property.name)
      if not has_child_code:
        c = None
    return c
