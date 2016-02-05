{% include 'copyright_block.txt' %}
#include "{{v8_class_or_partial}}.h"

{% for filename in cpp_includes if filename != '%s.h' % cpp_class_or_partial %}
#include "{{filename}}"
{% endfor %}

namespace blink {
{% set to_active_dom_object = '%s::toActiveDOMObject' % v8_class
                              if is_active_dom_object else '0' %}
{% set visit_dom_wrapper = '%s::visitDOMWrapper' % v8_class
                           if has_visit_dom_wrapper else '0' %}
{% set parent_wrapper_type_info = '&V8%s::wrapperTypeInfo' % parent_interface
                                  if parent_interface else '0' %}
{% set wrapper_type_prototype = 'WrapperTypeExceptionPrototype' if is_exception else
                                'WrapperTypeObjectPrototype' %}
{% set dom_template = '%s::domTemplate' % v8_class if not is_array_buffer_or_view else '0' %}

{% set wrapper_type_info_const = '' if has_partial_interface else 'const ' %}
{% if not is_partial %}
// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
{{wrapper_type_info_const}}WrapperTypeInfo {{v8_class}}::wrapperTypeInfo = { gin::kEmbedderBlink, {{dom_template}}, {{v8_class}}::refObject, {{v8_class}}::derefObject, {{v8_class}}::trace, {{to_active_dom_object}}, {{visit_dom_wrapper}}, {{v8_class}}::preparePrototypeAndInterfaceObject, {{v8_class}}::installConditionallyEnabledProperties, "{{interface_name}}", {{parent_wrapper_type_info}}, WrapperTypeInfo::{{wrapper_type_prototype}}, WrapperTypeInfo::{{wrapper_class_id}}, WrapperTypeInfo::{{event_target_inheritance}}, WrapperTypeInfo::{{lifetime}}, WrapperTypeInfo::{{gc_type}} };
#if defined(COMPONENT_BUILD) && defined(WIN32) && COMPILER(CLANG)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in {{cpp_class}}.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// bindings/core/v8/ScriptWrappable.h.
{% if not is_typed_array_type %}
const WrapperTypeInfo& {{cpp_class}}::s_wrapperTypeInfo = {{v8_class}}::wrapperTypeInfo;
{% endif %}

{% endif %}
{% if not is_array_buffer_or_view %}
namespace {{cpp_class_or_partial}}V8Internal {
{% if has_partial_interface %}
{% for method in methods if method.overloads and method.overloads.has_partial_overloads %}
static void (*{{method.name}}MethodForPartialInterface)(const v8::FunctionCallbackInfo<v8::Value>&) = 0;
{% endfor %}
{% endif %}

{# Constants #}
{% from 'constants.cpp' import constant_getter_callback
       with context %}
{% for constant in special_getter_constants %}
{{constant_getter_callback(constant)}}
{% endfor %}
{# Attributes #}
{% block replaceable_attribute_setter_and_callback %}
{% if has_replaceable_attributes or has_constructor_attributes %}
template<class CallbackInfo>
static bool {{cpp_class}}CreateDataProperty(v8::Local<v8::Name> name, v8::Local<v8::Value> v8Value, const CallbackInfo& info)
{
    {% if is_check_security %}
    {{cpp_class}}* impl = {{v8_class}}::toImpl(info.Holder());
    v8::String::Utf8Value attributeName(name);
    ExceptionState exceptionState(ExceptionState::SetterContext, *attributeName, "{{interface_name}}", info.Holder(), info.GetIsolate());
    if (!BindingSecurity::shouldAllowAccessTo(info.GetIsolate(), callingDOMWindow(info.GetIsolate()), impl, exceptionState)) {
        exceptionState.throwIfNeeded();
        return false;
    }
    {% endif %}
    ASSERT(info.This()->IsObject());
    return v8CallBoolean(v8::Local<v8::Object>::Cast(info.This())->CreateDataProperty(info.GetIsolate()->GetCurrentContext(), name, v8Value));
}

{% if needs_constructor_setter_callback %}
static void {{cpp_class}}ConstructorAttributeSetterCallback(v8::Local<v8::Name>, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<void>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("blink", "DOMSetter");
    do {
        v8::Local<v8::Value> data = info.Data();
        ASSERT(data->IsExternal());
        V8PerContextData* perContextData = V8PerContextData::from(info.Holder()->CreationContext());
        if (!perContextData)
            break;
        const WrapperTypeInfo* wrapperTypeInfo = WrapperTypeInfo::unwrap(data);
        if (!wrapperTypeInfo)
            break;
        {{cpp_class}}CreateDataProperty(v8String(info.GetIsolate(), wrapperTypeInfo->interfaceName), v8Value, info);
    } while (false); // do ... while (false) just for use of break
    TRACE_EVENT_SET_SAMPLING_STATE("v8", "V8Execution");
}

{% endif %}
{% endif %}
{% endblock %}
{##############################################################################}
{% from 'attributes.cpp' import constructor_getter_callback,
       attribute_getter, attribute_getter_callback,
       attribute_setter, attribute_setter_callback,
       attribute_getter_implemented_in_private_script,
       attribute_setter_implemented_in_private_script
       with context %}
{% for attribute in attributes if attribute.should_be_exposed_to_script %}
{% for world_suffix in attribute.world_suffixes %}
{% if not attribute.constructor_type %}
{% if not attribute.has_custom_getter %}
{{attribute_getter(attribute, world_suffix)}}
{% endif %}
{{attribute_getter_callback(attribute, world_suffix)}}
{% endif %}
{% if attribute.has_setter %}
{% if not attribute.has_custom_setter and
       (not attribute.constructor_type or attribute.needs_constructor_setter_callback) %}
{{attribute_setter(attribute, world_suffix)}}
{% endif %}
{% if not attribute.constructor_type or attribute.needs_constructor_setter_callback %}
{{attribute_setter_callback(attribute, world_suffix)}}
{% endif %}
{% endif %}
{% endfor %}
{% endfor %}
{##############################################################################}
{% for attribute in attributes if attribute.needs_constructor_getter_callback %}
{% for world_suffix in attribute.world_suffixes %}
{{constructor_getter_callback(attribute, world_suffix)}}
{% endfor %}
{% endfor %}
{##############################################################################}
{% block security_check_functions %}
{% if has_access_check_callbacks %}
bool securityCheck(v8::Local<v8::Context> accessingContext, v8::Local<v8::Object> accessedObject)
{
    // TODO(jochen): Take accessingContext into account.
    {{cpp_class}}* impl = {{v8_class}}::toImpl(accessedObject);
    return BindingSecurity::shouldAllowAccessTo(v8::Isolate::GetCurrent(), callingDOMWindow(v8::Isolate::GetCurrent()), impl, DoNotReportSecurityError);
}

{% endif %}
{% endblock %}
{##############################################################################}
{# Methods #}
{% from 'methods.cpp' import generate_method, overload_resolution_method,
       method_callback, origin_safe_method_getter, generate_constructor,
       method_implemented_in_private_script, generate_post_message_impl,
       runtime_determined_length_method, runtime_determined_maxarg_method
       with context %}
{% for method in methods %}
{% if method.should_be_exposed_to_script %}
{% for world_suffix in method.world_suffixes %}
{% if not method.is_custom and not method.is_post_message and method.visible %}
{{generate_method(method, world_suffix)}}
{% endif %}
{% if method.is_post_message %}
{{generate_post_message_impl()}}
{% endif %}
{% if method.overloads and method.overloads.visible %}
{% if method.overloads.runtime_determined_lengths %}
{{runtime_determined_length_method(method.overloads)}}
{% endif %}
{% if method.overloads.runtime_determined_maxargs %}
{{runtime_determined_maxarg_method(method.overloads)}}
{% endif %}
{{overload_resolution_method(method.overloads, world_suffix)}}
{% endif %}
{% if not method.overload_index or method.overloads %}
{# Document about the following condition: #}
{# https://docs.google.com/document/d/1qBC7Therp437Jbt_QYAtNYMZs6zQ_7_tnMkNUG_ACqs/edit?usp=sharing #}
{% if (method.overloads and method.overloads.visible and
        (not method.overloads.has_partial_overloads or not is_partial)) or
      (not method.overloads and method.visible) %}
{# A single callback is generated for overloaded methods #}
{# with considering partial overloads #}
{{method_callback(method, world_suffix)}}
{% endif %}
{% endif %}
{% if method.is_do_not_check_security and method.visible %}
{{origin_safe_method_getter(method, world_suffix)}}
{% endif %}
{% endfor %}
{% endif %}
{% endfor %}
{% if iterator_method %}
{{generate_method(iterator_method)}}
{{method_callback(iterator_method)}}
{% endif %}
{% block origin_safe_method_setter %}{% endblock %}
{# Constructors #}
{% for constructor in constructors %}
{{generate_constructor(constructor)}}
{% endfor %}
{% block overloaded_constructor %}{% endblock %}
{% block event_constructor %}{% endblock %}
{# Special operations (methods) #}
{% block indexed_property_getter %}{% endblock %}
{% block indexed_property_getter_callback %}{% endblock %}
{% block indexed_property_setter %}{% endblock %}
{% block indexed_property_setter_callback %}{% endblock %}
{% block indexed_property_deleter %}{% endblock %}
{% block indexed_property_deleter_callback %}{% endblock %}
{% block named_property_getter %}{% endblock %}
{% block named_property_getter_callback %}{% endblock %}
{% block named_property_setter %}{% endblock %}
{% block named_property_setter_callback %}{% endblock %}
{% block named_property_query %}{% endblock %}
{% block named_property_query_callback %}{% endblock %}
{% block named_property_deleter %}{% endblock %}
{% block named_property_deleter_callback %}{% endblock %}
{% block named_property_enumerator %}{% endblock %}
{% block named_property_enumerator_callback %}{% endblock %}
} // namespace {{cpp_class_or_partial}}V8Internal

{% block visit_dom_wrapper %}{% endblock %}
{##############################################################################}
{% block install_attributes %}
{% from 'attributes.cpp' import attribute_configuration with context %}
{% if has_attribute_configuration %}
// Suppress warning: global constructors, because AttributeConfiguration is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const V8DOMConfiguration::AttributeConfiguration {{v8_class}}Attributes[] = {
    {% for attribute in attributes
       if not (attribute.exposed_test or
               attribute.runtime_enabled_function) and
          attribute.is_data_type_property and
          attribute.should_be_exposed_to_script %}
    {{attribute_configuration(attribute)}},
    {% endfor %}
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && COMPILER(CLANG)
#pragma clang diagnostic pop
#endif

{% endif %}
{% endblock %}
{##############################################################################}
{% block install_accessors %}
{% from 'attributes.cpp' import attribute_configuration with context %}
{% if has_accessor_configuration %}
const V8DOMConfiguration::AccessorConfiguration {{v8_class}}Accessors[] = {
    {% for attribute in attributes
       if not (attribute.exposed_test or
               attribute.runtime_enabled_function) and
          not attribute.is_data_type_property and
          attribute.should_be_exposed_to_script %}
    {{attribute_configuration(attribute)}},
    {% endfor %}
};

{% endif %}
{% endblock %}
{##############################################################################}
{% block install_methods %}
{% from 'methods.cpp' import method_configuration with context %}
{% if method_configuration_methods %}
const V8DOMConfiguration::MethodConfiguration {{v8_class}}Methods[] = {
    {% for method in method_configuration_methods %}
    {{method_configuration(method)}},
    {% endfor %}
};

{% endif %}
{% endblock %}
{% endif %}{# not is_array_buffer_or_view #}
{##############################################################################}
{% block named_constructor %}{% endblock %}
{% block constructor_callback %}{% endblock %}
{##############################################################################}
{% block install_dom_template %}
{% if not is_array_buffer_or_view %}
{% from 'methods.cpp' import install_custom_signature with context %}
{% from 'attributes.cpp' import attribute_configuration with context %}
{% from 'constants.cpp' import install_constants with context %}
{% if has_partial_interface or is_partial %}
void {{v8_class_or_partial}}::install{{v8_class}}Template(v8::Local<v8::FunctionTemplate> functionTemplate, v8::Isolate* isolate)
{% else %}
static void install{{v8_class}}Template(v8::Local<v8::FunctionTemplate> functionTemplate, v8::Isolate* isolate)
{% endif %}
{
    {% if is_partial %}
    {{v8_class}}::install{{v8_class}}Template(functionTemplate, isolate);
    {% else %}
    functionTemplate->ReadOnlyPrototype();
    {% endif %}

    v8::Local<v8::Signature> defaultSignature;
    {% set parent_template =
           '%s::domTemplateForNamedPropertiesObject(isolate)' % v8_class
           if has_named_properties_object else
           'V8%s::domTemplate(isolate)' % parent_interface
           if parent_interface else 'v8::Local<v8::FunctionTemplate>()' %}
    {% if runtime_enabled_function %}
    if (!{{runtime_enabled_function}}())
        defaultSignature = V8DOMConfiguration::installDOMClassTemplate(isolate, functionTemplate, "{{interface_name}}", {{parent_template}}, {{v8_class}}::internalFieldCount, 0, 0, 0, 0, 0, 0);
    else
    {% endif %}
    {% set runtime_enabled_indent = 4 if runtime_enabled_function else 0 %}
    {% filter indent(runtime_enabled_indent, true) %}
    defaultSignature = V8DOMConfiguration::installDOMClassTemplate(isolate, functionTemplate, "{{interface_name}}", {{parent_template}}, {{v8_class}}::internalFieldCount,
        {# Test needed as size 0 arrays definitions are not allowed per standard
           (so objects have distinct addresses), which is enforced by MSVC.
           8.5.1 Aggregates [dcl.init.aggr]
           An array of unknown size initialized with a brace-enclosed
           initializer-list containing n initializer-clauses, where n shall be
           greater than zero, is defined as having n elements (8.3.4). #}
        {% set attributes_name, attributes_length =
               ('%sAttributes' % v8_class,
                'WTF_ARRAY_LENGTH(%sAttributes)' % v8_class)
           if has_attribute_configuration else (0, 0) %}
        {% set accessors_name, accessors_length =
               ('%sAccessors' % v8_class,
                'WTF_ARRAY_LENGTH(%sAccessors)' % v8_class)
           if has_accessor_configuration else (0, 0) %}
        {% set methods_name, methods_length =
               ('%sMethods' % v8_class,
                'WTF_ARRAY_LENGTH(%sMethods)' % v8_class)
           if method_configuration_methods else (0, 0) %}
        {{attributes_name}}, {{attributes_length}},
        {{accessors_name}}, {{accessors_length}},
        {{methods_name}}, {{methods_length}});
    {% endfilter %}

    {% if constructors or has_custom_constructor or has_event_constructor %}
    functionTemplate->SetCallHandler({{v8_class}}::constructorCallback);
    functionTemplate->SetLength({{interface_length}});
    {% endif %}
    v8::Local<v8::ObjectTemplate> instanceTemplate = functionTemplate->InstanceTemplate();
    ALLOW_UNUSED_LOCAL(instanceTemplate);
    v8::Local<v8::ObjectTemplate> prototypeTemplate = functionTemplate->PrototypeTemplate();
    ALLOW_UNUSED_LOCAL(prototypeTemplate);
    {% if custom_registration_methods %}
    ExecutionContext* context = currentExecutionContext(isolate);
    ALLOW_UNUSED_LOCAL(context);
    {% endif %}
    {% if has_access_check_callbacks %}
    instanceTemplate->SetAccessCheckCallback({{cpp_class}}V8Internal::securityCheck, v8::External::New(isolate, const_cast<WrapperTypeInfo*>(&{{v8_class}}::wrapperTypeInfo)));
    {% endif %}
    {% if has_array_iterator %}
    {% filter runtime_enabled('RuntimeEnabledFeatures::iterableCollectionsEnabled') %}
    {% if is_global %}
    instanceTemplate->SetIntrinsicDataProperty(v8::Symbol::GetIterator(isolate), v8::kArrayProto_values, v8::DontEnum);
    {% else %}
    prototypeTemplate->SetIntrinsicDataProperty(v8::Symbol::GetIterator(isolate), v8::kArrayProto_values, v8::DontEnum);
    {% endif %}
    {% endfilter %}{# runtime_enabled() #}
    {% endif %}
    {% set runtime_enabled_features = dict() %}
    {% for attribute in attributes
       if attribute.runtime_enabled_function and
          not attribute.exposed_test %}
        {% if attribute.runtime_enabled_function not in runtime_enabled_features %}
            {% set unused = runtime_enabled_features.update({attribute.runtime_enabled_function: []}) %}
        {% endif %}
        {% set unused = runtime_enabled_features.get(attribute.runtime_enabled_function).append(attribute) %}
    {% endfor %}
    {% for runtime_enabled_feature in runtime_enabled_features | sort %}
    if ({{runtime_enabled_feature}}()) {
        {% set distinct_attributes = [] %}
        {% for attribute in runtime_enabled_features.get(runtime_enabled_feature) | sort
           if attribute.name not in distinct_attributes %}
        {% set unused = distinct_attributes.append(attribute.name) %}
        {% if attribute.is_data_type_property %}
        const V8DOMConfiguration::AttributeConfiguration attribute{{attribute.name}}Configuration = \
        {{attribute_configuration(attribute)}};
        V8DOMConfiguration::installAttribute(isolate, instanceTemplate, prototypeTemplate, attribute{{attribute.name}}Configuration);
        {% else %}
        const V8DOMConfiguration::AccessorConfiguration accessor{{attribute.name}}Configuration = \
        {{attribute_configuration(attribute)}};
        V8DOMConfiguration::installAccessor(isolate, instanceTemplate, prototypeTemplate, functionTemplate, defaultSignature, accessor{{attribute.name}}Configuration);
        {% endif %}
        {% endfor %}
    }
    {% endfor %}
    {% if constants %}
    {{install_constants() | indent}}
    {% endif %}
    {# Special operations #}
    {% if indexed_property_getter %}
    {{install_indexed_property_handler('instanceTemplate') | indent}}
    {% endif %}
    {% if named_property_getter and not has_named_properties_object %}
    {{install_named_property_handler('instanceTemplate') | indent}}
    {% endif %}
    {% if iterator_method %}
    {% filter exposed(iterator_method.exposed_test) %}
    {% filter runtime_enabled(iterator_method.runtime_enabled_function) %}
    const V8DOMConfiguration::SymbolKeyedMethodConfiguration symbolKeyedIteratorConfiguration = { v8::Symbol::GetIterator, {{cpp_class_or_partial}}V8Internal::iteratorMethodCallback, 0, v8::DontDelete, V8DOMConfiguration::ExposedToAllScripts, V8DOMConfiguration::OnPrototype };
    V8DOMConfiguration::installMethod(isolate, prototypeTemplate, defaultSignature, symbolKeyedIteratorConfiguration);
    {% endfilter %}{# runtime_enabled() #}
    {% endfilter %}{# exposed() #}
    {% endif %}
    {# End special operations #}
    {% if has_custom_legacy_call_as_function %}
    functionTemplate->InstanceTemplate()->SetCallAsFunctionHandler({{v8_class}}::legacyCallCustom);
    {% endif %}
    {% if interface_name == 'HTMLAllCollection' %}
    {# Needed for legacy support of document.all #}
    functionTemplate->InstanceTemplate()->MarkAsUndetectable();
    {% endif %}
    {% for method in custom_registration_methods %}
    {# install_custom_signature #}
    {% filter exposed(method.overloads.exposed_test_all
                      if method.overloads else
                      method.exposed_test) %}
    {% filter runtime_enabled(method.overloads.runtime_enabled_function_all
                              if method.overloads else
                              method.runtime_enabled_function) %}
    {% if method.is_do_not_check_security %}
    {{install_do_not_check_security_method(method, '', 'instanceTemplate', 'prototypeTemplate') | indent}}
    {% else %}{# is_do_not_check_security #}
    {% set signature = 'v8::Local<v8::Signature>()' if method.is_do_not_check_signature else 'defaultSignature' %}
    {{install_custom_signature(method, 'instanceTemplate', 'prototypeTemplate', 'functionTemplate', signature) | indent}}
    {% endif %}{# is_do_not_check_security #}
    {% endfilter %}{# runtime_enabled() #}
    {% endfilter %}{# exposed() #}
    {% endfor %}
    {# Special interfaces #}
    {% if not is_partial %}
    {% if interface_name == 'Window' %}

    instanceTemplate->SetInternalFieldCount(V8Window::internalFieldCount);
    prototypeTemplate->SetInternalFieldCount(V8Window::internalFieldCount);
    {% endif %}
    {% if is_global or interface_name == 'HTMLDocument' %}
    functionTemplate->SetHiddenPrototype(true);
    {% endif %}
    {% endif %}
}

{% endif %}{# not is_array_buffer_or_view #}
{% endblock %}
{##############################################################################}
{% block get_dom_template %}{% endblock %}
{% block get_dom_template_for_named_properties_object %}{% endblock %}
{% block has_instance %}{% endblock %}
{% block to_impl %}{% endblock %}
{% block to_impl_with_type_check %}{% endblock %}
{% block install_conditional_attributes %}{% endblock %}
{##############################################################################}
{% block prepare_prototype_and_interface_object %}{% endblock %}
{##############################################################################}
{% block to_active_dom_object %}{% endblock %}
{% block ref_object_and_deref_object %}{% endblock %}
{% for method in methods if method.is_implemented_in_private_script and method.visible %}
{{method_implemented_in_private_script(method)}}
{% endfor %}
{% for attribute in attributes if attribute.is_implemented_in_private_script %}
{{attribute_getter_implemented_in_private_script(attribute)}}
{% if attribute.has_setter %}
{{attribute_setter_implemented_in_private_script(attribute)}}
{% endif %}
{% endfor %}
{% block partial_interface %}{% endblock %}
} // namespace blink
