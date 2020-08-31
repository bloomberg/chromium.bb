# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools
import multiprocessing
import os.path

import web_idl

from . import name_style
from .blink_v8_bridge import blink_class_name
from .blink_v8_bridge import blink_type_info
from .blink_v8_bridge import make_default_value_expr
from .blink_v8_bridge import make_v8_to_blink_value
from .code_node import CodeNode
from .code_node import Likeliness
from .code_node import ListNode
from .code_node import SequenceNode
from .code_node import SymbolNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .code_node_cxx import CxxClassDefNode
from .code_node_cxx import CxxFuncDeclNode
from .code_node_cxx import CxxFuncDefNode
from .code_node_cxx import CxxIfElseNode
from .code_node_cxx import CxxLikelyIfNode
from .code_node_cxx import CxxNamespaceNode
from .codegen_accumulator import CodeGenAccumulator
from .codegen_context import CodeGenContext
from .codegen_expr import expr_from_exposure
from .codegen_format import format_template as _format
from .codegen_utils import collect_include_headers_of_idl_types
from .codegen_utils import component_export
from .codegen_utils import component_export_header
from .codegen_utils import enclose_with_header_guard
from .codegen_utils import make_copyright_header
from .codegen_utils import make_forward_declarations
from .codegen_utils import make_header_include_directives
from .codegen_utils import write_code_node_to_file
from .mako_renderer import MakoRenderer
from .package_initializer import package_initializer
from .path_manager import PathManager


_DICT_MEMBER_PRESENCE_PREDICATES = {
    "ScriptValue": "!{}.IsEmpty()",
    "ScriptPromise": "!{}.IsEmpty()",
}


def _blink_member_name(member):
    assert isinstance(member, web_idl.DictionaryMember)

    class BlinkMemberName(object):
        def __init__(self, member):
            blink_name = (member.code_generator_info.property_implemented_as
                          or member.identifier)
            self.get_api = name_style.api_func(blink_name)
            self.set_api = name_style.api_func("set", blink_name)
            self.has_api = name_style.api_func("has", blink_name)
            # C++ data member that shows the presence of the IDL member.
            self.presence_var = name_style.member_var("has", blink_name)
            # C++ data member that holds the value of the IDL member.
            self.value_var = name_style.member_var(blink_name)

    return BlinkMemberName(member)


def _is_member_always_present(member):
    assert isinstance(member, web_idl.DictionaryMember)
    return member.is_required or member.default_value is not None


def _does_use_presence_flag(member):
    assert isinstance(member, web_idl.DictionaryMember)
    return (not _is_member_always_present(member) and blink_type_info(
        member.idl_type).member_t not in _DICT_MEMBER_PRESENCE_PREDICATES)


def _member_presence_expr(member):
    assert isinstance(member, web_idl.DictionaryMember)
    if _is_member_always_present(member):
        return "true"
    if _does_use_presence_flag(member):
        return _blink_member_name(member).presence_var
    blink_type = blink_type_info(member.idl_type).member_t
    assert blink_type in _DICT_MEMBER_PRESENCE_PREDICATES
    _1 = _blink_member_name(member).value_var
    return _format(_DICT_MEMBER_PRESENCE_PREDICATES[blink_type], _1)


def bind_member_iteration_local_vars(code_node):
    local_vars = [
        SymbolNode(
            "current_context", "v8::Local<v8::Context> ${current_context} = "
            "${isolate}->GetCurrentContext();"),
        SymbolNode(
            "member_names", "const auto* ${member_names} = "
            "GetV8MemberNames(${isolate}).data();"),
        SymbolNode(
            "is_in_secure_context", "const bool ${is_in_secure_context} = "
            "${execution_context}->IsSecureContext();"),
    ]

    # Execution context
    node = SymbolNode(
        "execution_context", "ExecutionContext* ${execution_context} = "
        "ToExecutionContext(${current_context});")
    node.accumulate(
        CodeGenAccumulator.require_include_headers([
            "third_party/blink/renderer/core/execution_context/execution_context.h"
        ]))
    local_vars.append(node)

    code_node.register_code_symbols(local_vars)


def _make_include_headers(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    dictionary = cg_context.dictionary

    header_includes = set()
    source_includes = set()

    if dictionary.inherited:
        header_includes.add(
            PathManager(dictionary.inherited).api_path(ext="h"))
    else:
        header_includes.add(
            "third_party/blink/renderer/platform/bindings/dictionary_base.h")

    header_includes.update([
        component_export_header(dictionary.components[0]),
        "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h",
        "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h",
        "v8/include/v8.h",
    ])
    source_includes.update([
        "third_party/blink/renderer/platform/bindings/exception_messages.h",
        "third_party/blink/renderer/platform/bindings/exception_state.h",
        "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h",
        "third_party/blink/renderer/platform/heap/visitor.h",
    ])

    header_includes.update(
        collect_include_headers_of_idl_types(
            [member.idl_type for member in dictionary.own_members]))

    return header_includes, source_includes


def _make_forward_declarations(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    dictionary = cg_context.dictionary

    header_class_fwd_decls = set([
        "ExceptionState",
        "Visitor",
    ])
    header_struct_fwd_decls = set()

    source_class_fwd_decls = set()
    source_struct_fwd_decls = set()

    return (header_class_fwd_decls, header_struct_fwd_decls,
            source_class_fwd_decls, source_struct_fwd_decls)


def make_dict_constructors(cg_context):
    decls = ListNode()
    defs = ListNode()

    dictionary = cg_context.dictionary
    class_name = blink_class_name(dictionary)

    ctor_decl = CxxFuncDeclNode(name=class_name,
                                arg_decls=[],
                                return_type="",
                                default=True)
    decls.append(ctor_decl)

    return decls, defs


def make_dict_member_get(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    member = cg_context.dict_member
    blink_member_name = _blink_member_name(member)
    name = blink_member_name.get_api
    blink_type = blink_type_info(member.idl_type)

    decls = ListNode()
    defs = ListNode()

    func_def = CxxFuncDefNode(
        name=name,
        arg_decls=[],
        return_type=blink_type.const_ref_t,
        const=True)
    decls.append(func_def)
    func_def.set_base_template_vars(cg_context.template_bindings())
    func_def.body.extend([
        TextNode(_format("DCHECK({}());", blink_member_name.has_api)),
        TextNode(_format("return {};", blink_member_name.value_var)),
    ])

    if blink_type.ref_t != blink_type.const_ref_t:
        func_def = CxxFuncDefNode(
            name=name, arg_decls=[], return_type=blink_type.ref_t)
        decls.append(func_def)
        func_def.set_base_template_vars(cg_context.template_bindings())
        func_def.body.extend([
            TextNode(_format("DCHECK({}());", blink_member_name.has_api)),
            TextNode(_format("return {};", blink_member_name.value_var)),
        ])

    return decls, defs


def make_dict_member_has(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    member = cg_context.dict_member

    decls = ListNode()
    defs = ListNode()

    func_def = CxxFuncDefNode(
        name=_blink_member_name(member).has_api,
        arg_decls=[],
        return_type="bool",
        const=True)
    decls.append(func_def)
    func_def.set_base_template_vars(cg_context.template_bindings())
    body = func_def.body

    _1 = _member_presence_expr(member)
    body.append(TextNode(_format("return {_1};", _1=_1)))

    return decls, defs


def make_dict_member_set(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    member = cg_context.dict_member
    blink_member_name = _blink_member_name(member)
    real_type = member.idl_type.unwrap(typedef=True)
    type_info = blink_type_info(real_type)

    decls = ListNode()
    defs = ListNode()

    template_func_def = CxxFuncDefNode(
        name=blink_member_name.set_api,
        arg_decls=["T&& value"],
        return_type="void",
        template_params=["typename T"])
    decls.append(template_func_def)

    # This setter with the explicit type declaration makes it possible to set
    # the dictionary member with uniform initialization (especially aggregate
    # initialization), e.g. setIntVector({3, 1, 4, 1, 5}).
    move_func_decl = CxxFuncDeclNode(
        name=blink_member_name.set_api,
        arg_decls=[_format("{}&&", type_info.member_t)],
        return_type="void")
    decls.append(move_func_decl)

    move_func_def = CxxFuncDefNode(
        name=blink_member_name.set_api,
        arg_decls=[_format("{}&& value", type_info.member_t)],
        return_type="void",
        class_name=cg_context.class_name)
    defs.append(move_func_def)

    _1 = blink_member_name.value_var
    template_func_def.body.append(
        T(_format("{_1} = std::forward<T>(value);", _1=_1)))
    move_func_def.body.append(T(_format("{_1} = value;", _1=_1)))

    if _does_use_presence_flag(member):
        set_presense_expr = _format("{} = true;",
                                    blink_member_name.presence_var)
        template_func_def.body.append(T(set_presense_expr))
        move_func_def.body.append(T(set_presense_expr))

    # Migration Adapter
    if (real_type.is_nullable and
            blink_type_info(real_type).typename.startswith("base::Optional")):
        to_null_func_def = CxxFuncDefNode(
            name=_format("{}ToNull", blink_member_name.set_api),
            arg_decls=[],
            return_type="void")
        decls.append(to_null_func_def)
        to_null_func_def.set_base_template_vars(cg_context.template_bindings())
        to_null_func_def.body.append(
            T(_format("{}(base::nullopt);", blink_member_name.set_api)))

    return decls, defs


def make_dict_member_vars(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    member = cg_context.dict_member

    default_value_initializer = ""
    if member.default_value:
        default_expr = make_default_value_expr(member.idl_type,
                                               member.default_value)
        if default_expr.initializer is not None:
            default_value_initializer = _format("{{{}}}",
                                                default_expr.initializer)

    _1 = blink_type_info(member.idl_type).member_t
    _2 = _blink_member_name(member).value_var
    _3 = default_value_initializer
    value_var_def = TextNode(_format("{_1} {_2}{_3};", _1=_1, _2=_2, _3=_3))

    if _does_use_presence_flag(member):
        _1 = _blink_member_name(member).presence_var
        presense_var_def = TextNode(_format("bool {_1} = false;", _1=_1))
    else:
        presense_var_def = None

    return value_var_def, presense_var_def


def make_get_v8_dict_member_names_func(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    dictionary = cg_context.dictionary
    name = "GetV8MemberNames"
    arg_decls = ["v8::Isolate* isolate"]
    return_type = "const base::span<const v8::Eternal<v8::Name>>"

    func_decl = CxxFuncDeclNode(
        name=name, arg_decls=arg_decls, return_type=return_type, static=True)
    func_def = CxxFuncDefNode(
        name=name,
        class_name=cg_context.class_name,
        arg_decls=arg_decls,
        return_type=return_type)
    func_def.set_base_template_vars(cg_context.template_bindings())
    body = func_def.body

    if dictionary.own_members:
        pattern = "static const char* kKeyStrings[] = {{{_1}}};"
        _1 = ", ".join(
            _format("\"{}\"", member.identifier)
            for member in dictionary.own_members)
        body.extend([
            TextNode(_format(pattern, _1=_1)),
            TextNode("return V8PerIsolateData::From(isolate)"
                     "->FindOrCreateEternalNameCache(kKeyStrings, "
                     "kKeyStrings);"),
        ])
    else:
        body.append(TextNode("return {};"))

    return func_decl, func_def


def make_fill_with_dict_members_func(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    dictionary = cg_context.dictionary
    name = "FillWithMembers"
    arg_decls = [
        "v8::Isolate* isolate",
        "v8::Local<v8::Object> creation_context",
        "v8::Local<v8::Object> v8_dictionary",
    ]
    return_type = "bool"

    func_decl = CxxFuncDeclNode(
        name=name,
        arg_decls=arg_decls,
        return_type=return_type,
        const=True,
        override=True)
    func_def = CxxFuncDefNode(
        name=name,
        class_name=cg_context.class_name,
        arg_decls=arg_decls,
        return_type=return_type,
        const=True)
    func_def.set_base_template_vars(cg_context.template_bindings())
    body = func_def.body

    if dictionary.inherited:
        text = """\
if (!BaseClass::FillWithMembers(isolate, creation_context, v8_dictionary)) {
  return false;
}"""
        body.append(TextNode(text))

    body.append(
        TextNode("return FillWithOwnMembers("
                 "isolate, creation_context, v8_dictionary);"))

    return func_decl, func_def


def make_fill_with_own_dict_members_func(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    dictionary = cg_context.dictionary
    own_members = dictionary.own_members
    name = "FillWithOwnMembers"
    arg_decls = [
        "v8::Isolate* isolate",
        "v8::Local<v8::Object> creation_context",
        "v8::Local<v8::Object> v8_dictionary",
    ]
    return_type = "bool"

    func_decl = CxxFuncDeclNode(
        name=name, arg_decls=arg_decls, return_type=return_type, const=True)
    func_def = CxxFuncDefNode(
        name=name,
        class_name=cg_context.class_name,
        arg_decls=arg_decls,
        return_type=return_type,
        const=True)
    func_def.set_base_template_vars(cg_context.template_bindings())
    body = func_def.body
    body.add_template_var("isolate", "isolate")
    bind_member_iteration_local_vars(body)

    for key_index, member in enumerate(own_members):
        _1 = _blink_member_name(member).has_api
        _2 = key_index
        _3 = _blink_member_name(member).get_api
        pattern = ("""\
if ({_1}()) {{
  if (!v8_dictionary
           ->CreateDataProperty(
               ${current_context},
               ${member_names}[{_2}].Get(isolate),
               ToV8({_3}(), creation_context, isolate))
           .ToChecked()) {{
    return false;
  }}
}}\
""")
        node = T(_format(pattern, _1=_1, _2=_2, _3=_3))

        conditional = expr_from_exposure(member.exposure)
        if not conditional.is_always_true:
            node = CxxLikelyIfNode(cond=conditional, body=node)

        body.append(node)

    body.append(T("return true;"))

    return func_decl, func_def


def make_dict_create_funcs(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    name = "Create"
    arg_decls = [
        "v8::Isolate* isolate",
        "v8::Local<v8::Value> v8_value",
        "ExceptionState& exception_state",
    ]
    return_type = "${class_name}*"

    default_create_def = CxxFuncDefNode(
        name=name, arg_decls=[], return_type=return_type, static=True)
    default_create_def.set_base_template_vars(cg_context.template_bindings())

    default_create_def.body.append(
        TextNode("return MakeGarbageCollected<${class_name}>();"))

    create_decl = CxxFuncDeclNode(
        name=name, arg_decls=arg_decls, return_type=return_type, static=True)
    create_def = CxxFuncDefNode(
        name=name,
        class_name=cg_context.class_name,
        arg_decls=arg_decls,
        return_type=return_type)
    create_def.set_base_template_vars(cg_context.template_bindings())

    create_def.body.append(
        TextNode("""\
DCHECK(!v8_value.IsEmpty());

${class_name}* dictionary = MakeGarbageCollected<${class_name}>();
dictionary->FillMembers(isolate, v8_value, exception_state);
if (exception_state.HadException()) {
  return nullptr;
}
return dictionary;"""))

    decls = ListNode([
        default_create_def,
        create_decl,
    ])
    defs = ListNode([
        create_def,
    ])

    return decls, defs


def make_fill_dict_members_func(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    dictionary = cg_context.dictionary
    own_members = dictionary.own_members
    required_own_members = list(
        member for member in own_members if member.is_required)
    name = "FillMembers"
    arg_decls = [
        "v8::Isolate* isolate",
        "v8::Local<v8::Value> v8_value",
        "ExceptionState& exception_state",
    ]
    return_type = "void"

    func_decl = CxxFuncDeclNode(
        name=name, arg_decls=arg_decls, return_type=return_type)
    func_def = CxxFuncDefNode(
        name=name,
        class_name=cg_context.class_name,
        arg_decls=arg_decls,
        return_type=return_type)
    func_def.set_base_template_vars(cg_context.template_bindings())

    if required_own_members:
        check_required_members_node = T("""\
if (v8_value->IsNullOrUndefined()) {
  exception_state.ThrowTypeError(ExceptionMessages::FailedToConstruct(
      "${dictionary.identifier}",
      "has required members, but null/undefined was passed."));
  return;
}""")
    else:
        check_required_members_node = T("""\
if (v8_value->IsNullOrUndefined()) {
  return;
}""")

    # [PermissiveDictionaryConversion]
    if "PermissiveDictionaryConversion" in dictionary.extended_attributes:
        permissive_conversion_node = T("""\
if (!v8_value->IsObject()) {
  // [PermissiveDictionaryConversion]
  return;
}""")
    else:
        permissive_conversion_node = T("""\
if (!v8_value->IsObject()) {
  exception_state.ThrowTypeError(
      ExceptionMessages::FailedToConstruct(
          "${dictionary.identifier}", "The value is not of type Object"));
  return;
}""")

    call_internal_func_node = T("""\
FillMembersInternal(isolate, v8_value.As<v8::Object>(), exception_state);""")

    func_def.body.extend([
        check_required_members_node,
        permissive_conversion_node,
        call_internal_func_node,
    ])

    return func_decl, func_def


def make_fill_dict_members_internal_func(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    dictionary = cg_context.dictionary
    own_members = dictionary.own_members
    name = "FillMembersInternal"
    arg_decls = [
        "v8::Isolate* isolate",
        "v8::Local<v8::Object> v8_dictionary",
        "ExceptionState& exception_state",
    ]
    return_type = "void"
    func_decl = CxxFuncDeclNode(
        name=name, arg_decls=arg_decls, return_type=return_type)
    func_def = CxxFuncDefNode(
        name=name,
        class_name=cg_context.class_name,
        arg_decls=arg_decls,
        return_type=return_type)
    func_def.set_base_template_vars(cg_context.template_bindings())
    body = func_def.body
    body.add_template_var("isolate", "isolate")
    body.add_template_var("exception_state", "exception_state")
    bind_member_iteration_local_vars(body)
    body.register_code_symbols([
        SymbolNode("try_block", "v8::TryCatch ${try_block}(${isolate});"),
        SymbolNode("v8_value", "v8::Local<v8::Value> ${v8_value};"),
    ])

    if dictionary.inherited:
        text = """\
BaseClass::FillMembersInternal(${isolate}, v8_dictionary, ${exception_state});
if (${exception_state}.HadException()) {
  return;
}
"""
        body.append(T(text))

    for key_index, member in enumerate(own_members):
        body.append(make_fill_own_dict_member(key_index, member))

    return func_decl, func_def


def make_fill_own_dict_member(key_index, member):
    assert isinstance(key_index, int)
    assert isinstance(member, web_idl.DictionaryMember)

    T = TextNode

    pattern = """
if (!<% try_block %>v8_dictionary->Get(${current_context}, ${member_names}[{_1}].Get(${isolate}))
         .ToLocal(&${v8_value})) {{
  ${exception_state}.RethrowV8Exception(${try_block}.Exception());
  return;
}}"""
    get_v8_value_node = T(_format(pattern, _1=key_index))

    api_call_node = SymbolScopeNode()
    api_call_node.register_code_symbol(
        make_v8_to_blink_value("blink_value", "${v8_value}", member.idl_type))
    _1 = _blink_member_name(member).set_api
    api_call_node.append(T(_format("{_1}(${blink_value});", _1=_1)))

    if member.is_required:
        exception_pattern = """\
${exception_state}.ThrowTypeError(
    ExceptionMessages::FailedToGet(
        "{}", "${{dictionary.identifier}}",
        "Required member is undefined."));
"""

        check_and_fill_node = CxxIfElseNode(
            cond="!${v8_value}->IsUndefined()",
            then=api_call_node,
            then_likeliness=Likeliness.LIKELY,
            else_=T(_format(exception_pattern, member.identifier)),
            else_likeliness=Likeliness.UNLIKELY)
    else:
        check_and_fill_node = CxxLikelyIfNode(
            cond="!${v8_value}->IsUndefined()", body=api_call_node)

    node = SequenceNode([
        get_v8_value_node,
        check_and_fill_node,
    ])

    conditional = expr_from_exposure(member.exposure)
    if not conditional.is_always_true:
        node = CxxLikelyIfNode(cond=conditional, body=node)

    return node


def make_dict_trace_func(cg_context):
    assert isinstance(cg_context, CodeGenContext)

    T = TextNode

    dictionary = cg_context.dictionary
    own_members = dictionary.own_members
    name = "Trace"
    arg_decls = ["Visitor* visitor"]
    return_type = "void"

    func_decl = CxxFuncDeclNode(
        name=name, arg_decls=arg_decls, return_type=return_type, override=True)
    func_def = CxxFuncDefNode(
        name=name,
        class_name=cg_context.class_name,
        arg_decls=arg_decls,
        return_type=return_type)
    func_def.set_base_template_vars(cg_context.template_bindings())
    body = func_def.body

    def make_trace_member_node(member):
        pattern = "TraceIfNeeded<{_1}>::Trace(visitor, {_2});"
        _1 = blink_type_info(member.idl_type).member_t
        _2 = _blink_member_name(member).value_var
        return TextNode(_format(pattern, _1=_1, _2=_2))

    body.extend(map(make_trace_member_node, own_members))
    body.append(TextNode("BaseClass::Trace(visitor);"))

    return func_decl, func_def


def generate_dictionary(dictionary):
    assert len(dictionary.components) == 1, (
        "We don't support partial dictionaries across components yet.")
    component = dictionary.components[0]

    path_manager = PathManager(dictionary)
    class_name = name_style.class_(blink_class_name(dictionary))
    if dictionary.inherited:
        base_class_name = blink_class_name(dictionary.inherited)
    else:
        base_class_name = "bindings::DictionaryBase"

    cg_context = CodeGenContext(
        dictionary=dictionary,
        class_name=class_name,
        base_class_name=base_class_name)

    # Filepaths
    header_path = path_manager.api_path(ext="h")
    source_path = path_manager.api_path(ext="cc")

    # Root nodes
    header_node = ListNode(tail="\n")
    header_node.set_accumulator(CodeGenAccumulator())
    header_node.set_renderer(MakoRenderer())
    source_node = ListNode(tail="\n")
    source_node.set_accumulator(CodeGenAccumulator())
    source_node.set_renderer(MakoRenderer())

    # Namespaces
    header_blink_ns = CxxNamespaceNode(name_style.namespace("blink"))
    source_blink_ns = CxxNamespaceNode(name_style.namespace("blink"))

    # Class definitions
    class_def = CxxClassDefNode(
        cg_context.class_name,
        base_class_names=[cg_context.base_class_name],
        export=component_export(component))
    class_def.set_base_template_vars(cg_context.template_bindings())
    class_def.top_section.append(
        TextNode("using BaseClass = ${base_class_name};"))

    # Create functions
    create_decl, create_def = make_dict_create_funcs(cg_context)

    # Constructor and destructor
    constructor_decls, constructor_defs = make_dict_constructors(cg_context)
    destructor_decl = CxxFuncDeclNode(
        name="~${class_name}", arg_decls=[], return_type="", default=True)

    # Fill with members (Blink -> V8 conversion)
    (fill_with_members_decl,
     fill_with_members_def) = make_fill_with_dict_members_func(cg_context)
    (fill_with_own_members_decl, fill_with_own_members_def
     ) = make_fill_with_own_dict_members_func(cg_context)

    # Fill members (V8 -> Blink conversion)
    (fill_members_decl,
     fill_members_def) = make_fill_dict_members_func(cg_context)
    (fill_members_internal_decl, fill_members_internal_def
     ) = make_fill_dict_members_internal_func(cg_context)

    # Misc. functions
    (get_v8_member_names_decl,
     get_v8_member_names_def) = make_get_v8_dict_member_names_func(cg_context)
    trace_decl, trace_def = make_dict_trace_func(cg_context)

    member_accessor_decls = ListNode()
    member_accessor_defs = ListNode()
    member_value_var_defs = ListNode()
    member_presense_var_defs = ListNode()
    for member in cg_context.dictionary.own_members:
        member_context = cg_context.make_copy(dict_member=member)
        get_decls, get_defs = make_dict_member_get(member_context)
        has_decls, has_defs = make_dict_member_has(member_context)
        set_decls, set_defs = make_dict_member_set(member_context)
        value_var_def, presense_var_def = make_dict_member_vars(member_context)
        member_accessor_decls.extend([
            TextNode(""),
            get_decls,
            has_decls,
            set_decls,
        ])
        member_accessor_defs.extend([
            TextNode(""),
            get_defs,
            has_defs,
            set_defs,
        ])
        member_value_var_defs.append(value_var_def)
        member_presense_var_defs.append(presense_var_def)

    # Header part (copyright, include directives, and forward declarations)
    header_includes, source_includes = _make_include_headers(cg_context)
    header_node.accumulator.add_include_headers(header_includes)
    source_node.accumulator.add_include_headers(source_includes)

    (header_class_fwd_decls, header_struct_fwd_decls, source_class_fwd_decls,
     source_struct_fwd_decls) = _make_forward_declarations(cg_context)
    header_node.accumulator.add_class_decls(header_class_fwd_decls)
    header_node.accumulator.add_struct_decls(header_struct_fwd_decls)
    source_node.accumulator.add_class_decls(source_class_fwd_decls)
    source_node.accumulator.add_struct_decls(source_struct_fwd_decls)

    header_node.extend([
        make_copyright_header(),
        TextNode(""),
        enclose_with_header_guard(
            ListNode([
                make_header_include_directives(header_node.accumulator),
                TextNode(""),
                header_blink_ns,
            ]), name_style.header_guard(header_path)),
    ])
    header_blink_ns.body.extend([
        make_forward_declarations(header_node.accumulator),
        TextNode(""),
    ])
    source_node.extend([
        make_copyright_header(),
        TextNode(""),
        TextNode("#include \"{}\"".format(header_path)),
        TextNode(""),
        make_header_include_directives(source_node.accumulator),
        TextNode(""),
        source_blink_ns,
    ])
    source_blink_ns.body.extend([
        make_forward_declarations(source_node.accumulator),
        TextNode(""),
    ])

    # Assemble the parts.
    header_blink_ns.body.append(class_def)
    class_def.public_section.extend([
        create_decl,
        constructor_decls,
        destructor_decl,
        TextNode(""),
        trace_decl,
        TextNode(""),
        member_accessor_decls,
    ])
    class_def.protected_section.extend([
        fill_with_members_decl,
        TextNode(""),
        fill_members_internal_decl,
    ])
    class_def.private_section.extend([
        get_v8_member_names_decl,
        TextNode(""),
        fill_with_own_members_decl,
        TextNode(""),
        fill_members_decl,
        TextNode(""),
        member_value_var_defs,
        TextNode(""),
        member_presense_var_defs,
    ])
    source_blink_ns.body.extend([
        constructor_defs,
        TextNode(""),
        get_v8_member_names_def,
        TextNode(""),
        create_def,
        TextNode(""),
        fill_with_members_def,
        TextNode(""),
        fill_with_own_members_def,
        TextNode(""),
        fill_members_def,
        TextNode(""),
        fill_members_internal_def,
        TextNode(""),
        member_accessor_defs,
        TextNode(""),
        trace_def,
    ])

    # Write down to the files.
    write_code_node_to_file(header_node, path_manager.gen_path_to(header_path))
    write_code_node_to_file(source_node, path_manager.gen_path_to(source_path))


def run_multiprocessing_task(args):
    dictionary, package_initializer = args
    package_initializer.init()
    generate_dictionary(dictionary)


def generate_dictionaries(web_idl_database):
    # More processes do not mean better performance.  The default size was
    # chosen heuristically.
    process_pool_size = 8
    cpu_count = multiprocessing.cpu_count()
    process_pool_size = max(1, min(cpu_count / 2, process_pool_size))

    pool = multiprocessing.Pool(process_pool_size)
    # Prior to Python3, Pool.map doesn't support user interrupts (e.g. Ctrl-C),
    # although Pool.map_async(...).get(...) does.
    timeout_in_sec = 3600  # Just enough long time
    pool.map_async(
        run_multiprocessing_task,
        map(lambda dictionary: (dictionary, package_initializer()),
            web_idl_database.dictionaries)).get(timeout_in_sec)

    return

    # When it is difficult to see errors in generator, use following loop
    # instead of parallel runs above.
    for dictionary in web_idl_database.dictionaries:
        generate_dictionary(dictionary)
