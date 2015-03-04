# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Generates Go source files from a mojom.Module.'''

from itertools import chain
import os
import re

from mojom.generate.template_expander import UseJinja

import mojom.generate.generator as generator
import mojom.generate.module as mojom
import mojom.generate.pack as pack

class KindInfo(object):
  def __init__(self, go_type, encode_suffix, decode_suffix, bit_size):
    self.go_type = go_type
    self.encode_suffix = encode_suffix
    self.decode_suffix = decode_suffix
    self.bit_size = bit_size

_kind_infos = {
  mojom.BOOL:                  KindInfo('bool', 'Bool', 'Bool', 1),
  mojom.INT8:                  KindInfo('int8', 'Int8', 'Int8', 8),
  mojom.UINT8:                 KindInfo('uint8', 'Uint8', 'Uint8', 8),
  mojom.INT16:                 KindInfo('int16', 'Int16', 'Int16', 16),
  mojom.UINT16:                KindInfo('uint16', 'Uint16', 'Uint16', 16),
  mojom.INT32:                 KindInfo('int32', 'Int32', 'Int32', 32),
  mojom.UINT32:                KindInfo('uint32', 'Uint32', 'Uint32', 32),
  mojom.FLOAT:                 KindInfo('float32', 'Float32', 'Float32', 32),
  mojom.HANDLE:                KindInfo(
      'system.Handle', 'Handle', 'Handle', 32),
  mojom.DCPIPE:                KindInfo(
      'system.ConsumerHandle', 'Handle', 'ConsumerHandle', 32),
  mojom.DPPIPE:                KindInfo(
      'system.ProducerHandle', 'Handle', 'ProducerHandle', 32),
  mojom.MSGPIPE:               KindInfo(
      'system.MessagePipeHandle', 'Handle', 'MessagePipeHandle', 32),
  mojom.SHAREDBUFFER:          KindInfo(
      'system.SharedBufferHandle', 'Handle', 'SharedBufferHandle', 32),
  mojom.NULLABLE_HANDLE:       KindInfo(
      'system.Handle', 'Handle', 'Handle', 32),
  mojom.NULLABLE_DCPIPE:       KindInfo(
      'system.ConsumerHandle', 'Handle', 'ConsumerHandle', 32),
  mojom.NULLABLE_DPPIPE:       KindInfo(
      'system.ProducerHandle', 'Handle', 'ProducerHandle', 32),
  mojom.NULLABLE_MSGPIPE:      KindInfo(
      'system.MessagePipeHandle', 'Handle', 'MessagePipeHandle', 32),
  mojom.NULLABLE_SHAREDBUFFER: KindInfo(
      'system.SharedBufferHandle', 'Handle', 'SharedBufferHandle', 32),
  mojom.INT64:                 KindInfo('int64', 'Int64', 'Int64', 64),
  mojom.UINT64:                KindInfo('uint64', 'Uint64', 'Uint64', 64),
  mojom.DOUBLE:                KindInfo('float64', 'Float64', 'Float64', 64),
  mojom.STRING:                KindInfo('string', 'String', 'String', 64),
  mojom.NULLABLE_STRING:       KindInfo('string', 'String', 'String', 64),
}

_imports = {}

def GetBitSize(kind):
  if isinstance(kind, (mojom.Array, mojom.Map, mojom.Struct)):
    return 64
  if isinstance(kind, (mojom.InterfaceRequest, mojom.Interface)):
    kind = mojom.MSGPIPE
  if isinstance(kind, mojom.Enum):
    kind = mojom.INT32
  return _kind_infos[kind].bit_size

# Returns go type corresponding to provided kind. If |nullable| is true
# and kind is nullable adds an '*' to type (example: ?string -> *string).
def GetGoType(kind, nullable = True):
  if nullable and mojom.IsNullableKind(kind):
    return '*%s' % GetNonNullableGoType(kind)
  return GetNonNullableGoType(kind)

# Returns go type corresponding to provided kind. Ignores nullability of
# top-level kind.
def GetNonNullableGoType(kind):
  if mojom.IsStructKind(kind):
    return '%s' % GetFullName(kind)
  if mojom.IsArrayKind(kind):
    if kind.length:
      return '[%s]%s' % (kind.length, GetGoType(kind.kind))
    return '[]%s' % GetGoType(kind.kind)
  if mojom.IsMapKind(kind):
    return 'map[%s]%s' % (GetGoType(kind.key_kind), GetGoType(kind.value_kind))
  if mojom.IsInterfaceKind(kind):
    return '%sPointer' % GetFullName(kind)
  if mojom.IsInterfaceRequestKind(kind):
    return '%sRequest' % GetFullName(kind.kind)
  if mojom.IsEnumKind(kind):
    return GetNameForNestedElement(kind)
  return _kind_infos[kind].go_type

# Splits name to lower-cased parts used for camel-casing
# (example: HTTPEntry2FooBar -> ['http', 'entry2', 'foo', 'bar']).
def NameToComponent(name):
  # insert '_' between anything and a Title name (e.g, HTTPEntry2FooBar ->
  # HTTP_Entry2_FooBar)
  name = re.sub('([^_])([A-Z][^A-Z_]+)', r'\1_\2', name)
  # insert '_' between non upper and start of upper blocks (e.g.,
  # HTTP_Entry2_FooBar -> HTTP_Entry2_Foo_Bar)
  name = re.sub('([^A-Z_])([A-Z])', r'\1_\2', name)
  return [x.lower() for x in name.split('_')]

def UpperCamelCase(name):
  return ''.join([x.capitalize() for x in NameToComponent(name)])

# Formats a name. If |exported| is true makes name camel-cased with first
# letter capital, otherwise does no camel-casing and makes first letter
# lower-cased (which is used for making internal names more readable).
def FormatName(name, exported=True):
  if exported:
    return UpperCamelCase(name)
  # Leave '_' symbols for unexported names.
  return name[0].lower() + name[1:]

# Returns full name of an imported element based on prebuilt dict |_imports|.
# If the |element| is not imported returns formatted name of it.
# |element| should have attr 'name'. |exported| argument is used to make
# |FormatName()| calls only.
def GetFullName(element, exported=True):
  if not hasattr(element, 'imported_from') or not element.imported_from:
    return FormatName(element.name, exported)
  path = 'gen/mojom'
  if element.imported_from['namespace']:
    path = '/'.join([path] + element.imported_from['namespace'].split('.'))
  if path in _imports:
    return '%s.%s' % (_imports[path], FormatName(element.name, exported))
  return FormatName(element.name, exported)

# Returns a name for nested elements like enum field or constant.
# The returned name consists of camel-cased parts separated by '_'.
def GetNameForNestedElement(element):
  if element.parent_kind:
    return "%s_%s" % (GetNameForElement(element.parent_kind),
        FormatName(element.name))
  return GetFullName(element)

def GetNameForElement(element, exported=True):
  if (mojom.IsInterfaceKind(element) or mojom.IsStructKind(element)):
    return GetFullName(element, exported)
  if isinstance(element, (mojom.EnumField,
                          mojom.Field,
                          mojom.Method,
                          mojom.Parameter)):
    return FormatName(element.name, exported)
  if isinstance(element, (mojom.Enum,
                          mojom.Constant,
                          mojom.ConstantValue)):
    return GetNameForNestedElement(element)
  raise Exception('Unexpected element: %s' % element)

def ExpressionToText(token):
  if isinstance(token, mojom.EnumValue):
    return "%s_%s" % (GetNameForNestedElement(token.enum),
        FormatName(token.name, True))
  if isinstance(token, mojom.ConstantValue):
    return GetNameForNestedElement(token)
  if isinstance(token, mojom.Constant):
    return ExpressionToText(token.value)
  return token

def DecodeSuffix(kind):
  if mojom.IsEnumKind(kind):
    return DecodeSuffix(mojom.INT32)
  if mojom.IsInterfaceKind(kind) or mojom.IsInterfaceRequestKind(kind):
    return DecodeSuffix(mojom.MSGPIPE)
  return _kind_infos[kind].decode_suffix

def EncodeSuffix(kind):
  if mojom.IsEnumKind(kind):
    return EncodeSuffix(mojom.INT32)
  if mojom.IsInterfaceKind(kind) or mojom.IsInterfaceRequestKind(kind):
    return EncodeSuffix(mojom.MSGPIPE)
  return _kind_infos[kind].encode_suffix

def GetPackage(namespace):
  if namespace:
    return namespace.split('.')[-1]
  return 'mojom'

def GetPackagePath(namespace):
  path = 'mojom'
  for i in namespace.split('.'):
    path = os.path.join(path, i)
  return path

def GetStructFromMethod(method):
  params_class = "%s_%s_Params" % (GetNameForElement(method.interface),
      GetNameForElement(method))
  struct = mojom.Struct(params_class, module=method.interface.module)
  for param in method.parameters:
    struct.AddField("in%s" % GetNameForElement(param),
        param.kind, param.ordinal)
  struct.packed = pack.PackedStruct(struct)
  struct.bytes = pack.GetByteLayout(struct.packed)
  struct.versions = pack.GetVersionInfo(struct.packed)
  return struct

def GetResponseStructFromMethod(method):
  params_class = "%s_%s_ResponseParams" % (GetNameForElement(method.interface),
      GetNameForElement(method))
  struct = mojom.Struct(params_class, module=method.interface.module)
  for param in method.response_parameters:
    struct.AddField("out%s" % GetNameForElement(param),
        param.kind, param.ordinal)
  struct.packed = pack.PackedStruct(struct)
  struct.bytes = pack.GetByteLayout(struct.packed)
  struct.versions = pack.GetVersionInfo(struct.packed)
  return struct

def GetAllConstants(module):
  data = [module] + module.structs + module.interfaces
  constants = [x.constants for x in data]
  return [i for i in chain.from_iterable(constants)]

def GetAllEnums(module):
  data = [module] + module.structs + module.interfaces
  enums = [x.enums for x in data]
  return [i for i in chain.from_iterable(enums)]

# Adds an import required to use the provided |element|.
# The required import is stored at '_imports'.
def AddImport(module, element):
  if (isinstance(element, mojom.Kind) and
      mojom.IsNonInterfaceHandleKind(element)):
    _imports['mojo/public/go/system'] = 'system'
    return
  if isinstance(element, mojom.Kind) and mojom.IsInterfaceRequestKind(element):
    AddImport(module, element.kind)
    return
  if not hasattr(element, 'imported_from') or not element.imported_from:
    return
  imported = element.imported_from
  if imported['namespace'] == module.namespace:
    return
  path = 'gen/mojom'
  name = 'mojom'
  if imported['namespace']:
    path = '/'.join([path] + imported['namespace'].split('.'))
    name = '_'.join([name] + imported['namespace'].split('.'))
  while (name in _imports.values() and _imports[path] != path):
    name += '_'
  _imports[path] = name

# Scans |module| for elements that require imports and adds all found imports
# to '_imports' dict. Returns a list of imports that should include the
# generated go file.
def GetImports(module):
  # Imports can only be used in structs, constants, enums, interfaces.
  all_structs = list(module.structs)
  for i in module.interfaces:
    for method in i.methods:
      all_structs.append(GetStructFromMethod(method))
      if method.response_parameters:
        all_structs.append(GetResponseStructFromMethod(method))

  if len(all_structs) > 0 or len(module.interfaces) > 0:
    _imports['mojo/public/go/bindings'] = 'bindings'
  for struct in all_structs:
    for field in struct.fields:
      AddImport(module, field.kind)
# TODO(rogulenko): add these after generating constants and struct defaults.
#      if field.default:
#        AddImport(module, field.default)

  for enum in GetAllEnums(module):
    for field in enum.fields:
      if field.value:
        AddImport(module, field.value)

# TODO(rogulenko): add these after generating constants and struct defaults.
#  for constant in GetAllConstants(module):
#    AddImport(module, constant.value)

  imports_list = []
  for i in _imports:
    if i.split('/')[-1] == _imports[i]:
      imports_list.append('"%s"' % i)
    else:
      imports_list.append('%s "%s"' % (_imports[i], i))
  return sorted(imports_list)

class Generator(generator.Generator):
  go_filters = {
    'array': lambda kind: mojom.Array(kind),
    'bit_size': GetBitSize,
    'decode_suffix': DecodeSuffix,
    'encode_suffix': EncodeSuffix,
    'go_type': GetGoType,
    'expression_to_text': ExpressionToText,
    'is_array': mojom.IsArrayKind,
    'is_enum': mojom.IsEnumKind,
    'is_handle': mojom.IsAnyHandleKind,
    'is_handle_owner': lambda kind:
        mojom.IsInterfaceKind(kind) or mojom.IsInterfaceRequestKind(kind),
    'is_map': mojom.IsMapKind,
    'is_none_or_empty': lambda array: array == None or len(array) == 0,
    'is_nullable': mojom.IsNullableKind,
    'is_pointer': mojom.IsObjectKind,
    'is_struct': mojom.IsStructKind,
    'name': GetNameForElement,
    'response_struct_from_method': GetResponseStructFromMethod,
    'struct_from_method': GetStructFromMethod,
    'tab_indent': lambda s, size = 1: ('\n' + '\t' * size).join(s.splitlines())
  }

  def GetParameters(self):
    return {
      'enums': GetAllEnums(self.module),
      'imports': GetImports(self.module),
      'interfaces': self.module.interfaces,
      'package': GetPackage(self.module.namespace),
      'structs': self.GetStructs(),
    }

  @UseJinja('go_templates/source.tmpl', filters=go_filters)
  def GenerateSource(self):
    return self.GetParameters()

  def GenerateFiles(self, args):
    self.Write(self.GenerateSource(), os.path.join("go", "src", "gen",
        GetPackagePath(self.module.namespace), '%s.go' % self.module.name))

  def GetJinjaParameters(self):
    return {
      'lstrip_blocks': True,
      'trim_blocks': True,
    }

  def GetGlobals(self):
    return {
      'namespace': self.module.namespace,
      'module': self.module,
    }
