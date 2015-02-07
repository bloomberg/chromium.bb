# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Translates parse tree to Mojom IR."""


from . import ast


def _MapTreeForType(func, tree, type_to_map):
  assert isinstance(type_to_map, type)
  if not tree:
    return []
  return [func(subtree) for subtree in tree if isinstance(subtree, type_to_map)]

def _MapKind(kind):
  map_to_kind = {'bool': 'b',
                 'int8': 'i8',
                 'int16': 'i16',
                 'int32': 'i32',
                 'int64': 'i64',
                 'uint8': 'u8',
                 'uint16': 'u16',
                 'uint32': 'u32',
                 'uint64': 'u64',
                 'float': 'f',
                 'double': 'd',
                 'string': 's',
                 'handle': 'h',
                 'handle<data_pipe_consumer>': 'h:d:c',
                 'handle<data_pipe_producer>': 'h:d:p',
                 'handle<message_pipe>': 'h:m',
                 'handle<shared_buffer>': 'h:s'}
  if kind.endswith('?'):
    base_kind = _MapKind(kind[0:-1])
    # NOTE: This doesn't rule out enum types. Those will be detected later, when
    # cross-reference is established.
    reference_kinds = ('m', 's', 'h', 'a', 'r', 'x')
    if base_kind[0] not in reference_kinds:
      raise Exception(
          'A type (spec "%s") cannot be made nullable' % base_kind)
    return '?' + base_kind
  if kind.endswith('}'):
    lbracket = kind.rfind('{')
    value = kind[0:lbracket]
    return 'm[' + _MapKind(kind[lbracket+1:-1]) + '][' + _MapKind(value) + ']'
  if kind.endswith(']'):
    lbracket = kind.rfind('[')
    typename = kind[0:lbracket]
    return 'a' + kind[lbracket+1:-1] + ':' + _MapKind(typename)
  if kind.endswith('&'):
    return 'r:' + _MapKind(kind[0:-1])
  if kind in map_to_kind:
    return map_to_kind[kind]
  return 'x:' + kind

def _AddOptional(dictionary, key, value):
  if value is not None:
    dictionary[key] = value;

def _AttributeListToDict(attribute_list):
  if attribute_list is None:
    return None
  assert isinstance(attribute_list, ast.AttributeList)
  # TODO(vtl): Check for duplicate keys here.
  return dict([(attribute.key, attribute.value)
                   for attribute in attribute_list])

def _EnumToDict(enum):
  def EnumValueToDict(enum_value):
    assert isinstance(enum_value, ast.EnumValue)
    data = {'name': enum_value.name}
    _AddOptional(data, 'value', enum_value.value)
    _AddOptional(data, 'attributes',
                 _AttributeListToDict(enum_value.attribute_list))
    return data

  assert isinstance(enum, ast.Enum)
  data = {'name': enum.name,
          'fields': map(EnumValueToDict, enum.enum_value_list)}
  _AddOptional(data, 'attributes', _AttributeListToDict(enum.attribute_list))
  return data

def _ConstToDict(const):
  assert isinstance(const, ast.Const)
  return {'name': const.name,
          'kind': _MapKind(const.typename),
          'value': const.value}


class _MojomBuilder(object):
  def __init__(self):
    self.mojom = {}

  def Build(self, tree, name):
    def StructToDict(struct):
      def StructFieldToDict(struct_field):
        assert isinstance(struct_field, ast.StructField)
        data = {'name': struct_field.name,
                'kind': _MapKind(struct_field.typename)}
        _AddOptional(data, 'ordinal',
                     struct_field.ordinal.value
                         if struct_field.ordinal else None)
        _AddOptional(data, 'default', struct_field.default_value)
        _AddOptional(data, 'attributes',
                     _AttributeListToDict(struct_field.attribute_list))
        return data

      assert isinstance(struct, ast.Struct)
      data = {'name': struct.name,
              'fields': _MapTreeForType(StructFieldToDict, struct.body,
                                        ast.StructField),
              'enums': _MapTreeForType(_EnumToDict, struct.body, ast.Enum),
              'constants': _MapTreeForType(_ConstToDict, struct.body,
                                           ast.Const)}
      _AddOptional(data, 'attributes',
                   _AttributeListToDict(struct.attribute_list))
      return data

    def UnionToDict(union):
      def UnionFieldToDict(union_field):
        assert isinstance(union_field, ast.UnionField)
        data = {'name': union_field.name,
                'kind': _MapKind(union_field.typename)}
        _AddOptional(data, 'ordinal',
                     union_field.ordinal.value
                         if union_field.ordinal else None)
        _AddOptional(data, 'attributes',
                     _AttributeListToDict(union_field.attribute_list))
        return data

      assert isinstance(union, ast.Union)
      data = {'name': union.name,
              'fields': _MapTreeForType(UnionFieldToDict, union.body,
                                        ast.UnionField)}
      _AddOptional(data, 'attributes',
                   _AttributeListToDict(union.attribute_list))
      return data

    def InterfaceToDict(interface):
      def MethodToDict(method):
        def ParameterToDict(param):
          assert isinstance(param, ast.Parameter)
          data = {'name': param.name,
                  'kind': _MapKind(param.typename)}
          _AddOptional(data, 'ordinal',
                       param.ordinal.value if param.ordinal else None)
          _AddOptional(data, 'attributes',
                       _AttributeListToDict(param.attribute_list))
          return data

        assert isinstance(method, ast.Method)
        data = {'name': method.name,
                'parameters': map(ParameterToDict, method.parameter_list)}
        if method.response_parameter_list is not None:
          data['response_parameters'] = map(ParameterToDict,
                                            method.response_parameter_list)
        _AddOptional(data, 'ordinal',
                     method.ordinal.value if method.ordinal else None)
        _AddOptional(data, 'attributes',
                     _AttributeListToDict(method.attribute_list))
        return data

      assert isinstance(interface, ast.Interface)
      data = {'name': interface.name,
              'methods': _MapTreeForType(MethodToDict, interface.body,
                                         ast.Method),
              'enums': _MapTreeForType(_EnumToDict, interface.body, ast.Enum),
              'constants': _MapTreeForType(_ConstToDict, interface.body,
                                           ast.Const)}
      _AddOptional(data, 'attributes',
                   _AttributeListToDict(interface.attribute_list))
      return data

    assert isinstance(tree, ast.Mojom)
    self.mojom['name'] = name
    self.mojom['namespace'] = tree.module.name[1] if tree.module else ''
    self.mojom['imports'] = \
        [{'filename': imp.import_filename} for imp in tree.import_list]
    self.mojom['structs'] = \
        _MapTreeForType(StructToDict, tree.definition_list, ast.Struct)
    self.mojom['unions'] = \
        _MapTreeForType(UnionToDict, tree.definition_list, ast.Union)
    self.mojom['interfaces'] = \
        _MapTreeForType(InterfaceToDict, tree.definition_list, ast.Interface)
    self.mojom['enums'] = \
        _MapTreeForType(_EnumToDict, tree.definition_list, ast.Enum)
    self.mojom['constants'] = \
        _MapTreeForType(_ConstToDict, tree.definition_list, ast.Const)
    _AddOptional(self.mojom, 'attributes',
                 _AttributeListToDict(tree.module.attribute_list)
                     if tree.module else None)
    return self.mojom


def Translate(tree, name):
  return _MojomBuilder().Build(tree, name)
