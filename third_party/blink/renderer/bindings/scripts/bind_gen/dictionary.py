# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path

from . import name_style
from .blink_v8_bridge import blink_class_name
from .blink_v8_bridge import blink_type_info
from .code_node import CodeNode
from .code_node import FunctionDefinitionNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .codegen_context import CodeGenContext
from .codegen_utils import enclose_with_namespace
from .codegen_utils import make_copyright_header
from .codegen_utils import write_code_node_to_file
from .mako_renderer import MakoRenderer

_format = CodeNode.format_template


def make_dict_member_get_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    member = cg_context.dict_member

    func_name = "{}::{}".format(
        blink_class_name(cg_context.dictionary),
        name_style.api_func(member.identifier))
    func_def = FunctionDefinitionNode(
        name=T(func_name),
        arg_decls=[],
        return_type=T(blink_type_info(member.idl_type).ref_t))

    body = func_def.body
    body.add_template_vars(cg_context.template_bindings())

    _1 = name_style.member_var(member.identifier)
    body.append(T(_format("return {_1};", _1=_1)))

    return func_def


def make_dict_member_has_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    member = cg_context.dict_member

    func_name = "{}::{}".format(
        blink_class_name(cg_context.dictionary),
        name_style.api_func("has", member.identifier))
    func_def = FunctionDefinitionNode(
        name=T(func_name), arg_decls=[], return_type=T("bool"))

    body = func_def.body
    body.add_template_vars(cg_context.template_bindings())

    _1 = name_style.member_var("has", member.identifier)
    body.append(T(_format("return {_1};", _1=_1)))

    return func_def


def make_dict_member_set_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    member = cg_context.dict_member

    func_name = "{}::{}".format(
        blink_class_name(cg_context.dictionary),
        name_style.api_func("set", member.identifier))
    func_def = FunctionDefinitionNode(
        name=T(func_name),
        arg_decls=[
            T(_format("{} value",
                      blink_type_info(member.idl_type).ref_t))
        ],
        return_type=T("void"))

    body = func_def.body
    body.add_template_vars(cg_context.template_bindings())

    _1 = name_style.member_var(member.identifier)
    body.append(T(_format("{_1} = value;", _1=_1)))

    return func_def


def make_dict_fill_with_members_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    dictionary = cg_context.dictionary

    func_name = _format(
        "{_1}::FillWithMembers", _1=blink_class_name(dictionary))
    func_def = FunctionDefinitionNode(
        name=T(func_name),
        arg_decls=[
            T("v8::Isolate* isolate"),
            T("v8::Local<v8::Object> creation_context"),
            T("v8::Local<v8::Object> v8_dictionary"),
        ],
        return_type=T("bool"))

    body = func_def.body
    body.add_template_vars(cg_context.template_bindings())

    if dictionary.inherited:
        _1 = blink_class_name(dictionary.inherited)
        pattern = ("if (!{_1}::FillWithMembers("
                   "isolate, creation_context, v8_dictionary))\n"
                   "  return false;")
        body.append(T(_format(pattern, _1=_1)))

    body.append(
        T("return FillWithOwnMembers("
          "isolate, creation_context, v8_dictionary)"))

    return func_def


def make_dict_fill_with_own_members_def(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    dictionary = cg_context.dictionary
    own_members = sorted(dictionary.own_members, key=lambda m: m.identifier)

    func_name = _format(
        "{_1}::FillWithOwnMembers", _1=blink_class_name(dictionary))
    func_def = FunctionDefinitionNode(
        name=T(func_name),
        arg_decls=[
            T("v8::Isolate* isolate"),
            T("v8::Local<v8::Object> creation_context"),
            T("v8::Local<v8::Object> v8_dictionary")
        ],
        return_type=T("bool"))

    body = func_def.body
    body.add_template_vars(cg_context.template_bindings())

    body.extend([
        T("const v8::Eternal<v8::Name>* member_keys = EternalKeys(isolate);"),
        T("v8::Local<v8::Context> current_context = "
          "isolate->GetCurrentContext();"),
    ])

    # TODO(peria): Support runtime enabled / origin trial members.
    for key_index, member in enumerate(own_members):
        _1 = name_style.api_func("has", member.identifier)
        _2 = name_style.api_func(member.identifier)
        _3 = key_index
        pattern = ("""
if ({_1}()) {{
  v8::Local<v8::Value> v8_value = ToV8({_2}(), creation_context, isolate);
  v8::Local<v8::Name> key = member_keys[{_3}].Get(isolate);
  if (!v8_dictionary->CreateDataProperty(current_context, key, v8_value)
           .ToChecked()) {{
    return false;
  }}
}}\
""")
        body.append(T(_format(pattern, _1=_1, _2=_2, _3=_3)))

    body.append(T("return true;"))

    return func_def


def generate_dictionaries(web_idl_database, output_dirs):
    dictionary = web_idl_database.find("ExampleDictionary")
    filename = "{}.cc".format(name_style.file(dictionary.identifier))
    filepath = os.path.join(output_dirs['core'], filename)

    cg_context = CodeGenContext(dictionary=dictionary)

    root_node = SymbolScopeNode(separator_last="\n")
    root_node.set_renderer(MakoRenderer())

    code_node = SymbolScopeNode()

    code_node.append(make_dict_fill_with_members_def(cg_context))
    code_node.append(make_dict_fill_with_own_members_def(cg_context))
    for member in sorted(dictionary.own_members, key=lambda x: x.identifier):
        code_node.extend([
            make_dict_member_get_def(cg_context.make_copy(dict_member=member)),
            make_dict_member_has_def(cg_context.make_copy(dict_member=member)),
            make_dict_member_set_def(cg_context.make_copy(dict_member=member)),
        ])
    root_node.extend([
        make_copyright_header(),
        TextNode(""),
        enclose_with_namespace(code_node, name_style.namespace("blink")),
    ])

    write_code_node_to_file(root_node, filepath)
