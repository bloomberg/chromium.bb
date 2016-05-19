{% from 'utilities.cpp' import declare_enum_validation_variable %}
{% include 'copyright_block.txt' %}
#include "{{this_include_header_name}}.h"

{% from 'utilities.cpp' import v8_value_to_local_cpp_value %}
{% macro assign_and_return_if_hasinstance(member) %}
if (V8{{member.type_name}}::hasInstance(v8Value, isolate)) {
    {{member.cpp_local_type}} cppValue = V8{{member.type_name}}::toImpl(v8::Local<v8::Object>::Cast(v8Value));
    impl.set{{member.type_name}}(cppValue);
    return;
}
{% endmacro %}
{% for filename in cpp_includes %}
#include "{{filename}}"
{% endfor %}

namespace blink {

{{cpp_class}}::{{cpp_class}}()
    : m_type(SpecificTypeNone)
{
}

{% for member in members %}
{{member.rvalue_cpp_type}} {{cpp_class}}::getAs{{member.type_name}}() const
{
    ASSERT(is{{member.type_name}}());
    return m_{{member.cpp_name}};
}

void {{cpp_class}}::set{{member.type_name}}({{member.rvalue_cpp_type}} value)
{
    ASSERT(isNull());
    {% if member.enum_values %}
    NonThrowableExceptionState exceptionState;
    {{declare_enum_validation_variable(member.enum_values) | indent}}
    if (!isValidEnum(value, validValues, WTF_ARRAY_LENGTH(validValues), "{{member.type_name}}", exceptionState)) {
        ASSERT_NOT_REACHED();
        return;
    }
    {% endif %}
    m_{{member.cpp_name}} = value;
    m_type = {{member.specific_type_enum}};
}

{{cpp_class}} {{cpp_class}}::from{{member.type_name}}({{member.rvalue_cpp_type}} value)
{
    {{cpp_class}} container;
    container.set{{member.type_name}}(value);
    return container;
}

{% endfor %}
{{cpp_class}}::{{cpp_class}}(const {{cpp_class}}&) = default;
{{cpp_class}}::~{{cpp_class}}() = default;
{{cpp_class}}& {{cpp_class}}::operator=(const {{cpp_class}}&) = default;

DEFINE_TRACE({{cpp_class}})
{
    {% for member in members if member.is_traceable %}
    visitor->trace(m_{{member.cpp_name}});
    {% endfor %}
}

void {{v8_class}}::toImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, {{cpp_class}}& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState)
{
    if (v8Value.IsEmpty())
        return;

    {# The numbers in the following comments refer to the steps described in
       http://heycam.github.io/webidl/#es-union #}
    {# 1. null or undefined #}
    if (conversionMode == UnionTypeConversionMode::Nullable && isUndefinedOrNull(v8Value))
        return;

    {# 3. Platform objects (interfaces) #}
    {% for interface in interface_types %}
    {{assign_and_return_if_hasinstance(interface) | indent}}

    {% endfor %}
    {# 8. ArrayBuffer #}
    {% if array_buffer_type %}
    {{assign_and_return_if_hasinstance(array_buffer_type) | indent}}

    {% endif %}
    {# 9., 10. ArrayBufferView #}
    {# FIXME: Individual typed arrays (e.g. Uint8Array) aren't supported yet. #}
    {% if array_buffer_view_type %}
    {{assign_and_return_if_hasinstance(array_buffer_view_type) | indent}}

    {% endif %}
    {% if dictionary_type %}
    {# 12. Dictionaries #}
    {# FIXME: This should also check "object but not Date or RegExp". Add checks
       when we implement conversions for Date and RegExp. #}
    {# TODO(bashi): The spec doesn't say we should check !IsArray() but otherwise
       we can't distinguish a sequence<T> and a dictionary.
       https://github.com/heycam/webidl/issues/123 #}
    if (isUndefinedOrNull(v8Value) || (v8Value->IsObject() && !v8Value->IsArray())) {
        {{v8_value_to_local_cpp_value(dictionary_type) | indent(8)}}
        impl.set{{dictionary_type.type_name}}(cppValue);
        return;
    }

    {% endif %}
    {% if array_or_sequence_type %}
    {# 13.1, 13.2. Arrays and Sequences #}
    {# FIXME: This should also check "object but not Date or RegExp". Add checks
       when we implement conversions for Date and RegExp. #}
    {# FIXME: Should check for sequences too, not just Array instances. #}
    if (v8Value->IsArray()) {
        {{v8_value_to_local_cpp_value(array_or_sequence_type) | indent(8)}}
        impl.set{{array_or_sequence_type.type_name}}(cppValue);
        return;
    }

    {% endif %}
    {# TODO(bashi): Support 13.3 Callback interface when we need it #}
    {# 13.4. Objects #}
    {% if object_type %}
    if (isUndefinedOrNull(v8Value) || v8Value->IsObject()) {
        {{v8_value_to_local_cpp_value(object_type) | indent(8)}}
        impl.set{{object_type.type_name}}(cppValue);
        return;
    }

    {% endif %}
    {# FIXME: In some cases, we can omit boolean and numeric type checks because
       we have fallback conversions. (step 17 and 18) #}
    {% if boolean_type %}
    {# 14. Boolean #}
    if (v8Value->IsBoolean()) {
        impl.setBoolean(v8Value.As<v8::Boolean>()->Value());
        return;
    }

    {% endif %}
    {% if numeric_type %}
    {# 15. Number #}
    if (v8Value->IsNumber()) {
        {{v8_value_to_local_cpp_value(numeric_type) | indent(8)}}
        impl.set{{numeric_type.type_name}}(cppValue);
        return;
    }

    {% endif %}
    {% if string_type %}
    {# 16. String #}
    {
        {{v8_value_to_local_cpp_value(string_type) | indent(8)}}
        {% if string_type.enum_values %}
        {{declare_enum_validation_variable(string_type.enum_values) | indent(8)}}
        if (!isValidEnum(cppValue, validValues, WTF_ARRAY_LENGTH(validValues), "{{string_type.type_name}}", exceptionState))
            return;
        {% endif %}
        impl.set{{string_type.type_name}}(cppValue);
        return;
    }

    {# 17. Number (fallback) #}
    {% elif numeric_type %}
    {
        {{v8_value_to_local_cpp_value(numeric_type) | indent(8)}}
        impl.set{{numeric_type.type_name}}(cppValue);
        return;
    }

    {# 18. Boolean (fallback) #}
    {% elif boolean_type %}
    {
        impl.setBoolean(v8Value->BooleanValue());
        return;
    }

    {% else %}
    {# 19. TypeError #}
    exceptionState.throwTypeError("The provided value is not of type '{{type_string}}'");
    {% endif %}
}

v8::Local<v8::Value> toV8(const {{cpp_class}}& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate)
{
    switch (impl.m_type) {
    case {{cpp_class}}::SpecificTypeNone:
        {# FIXME: We might want to return undefined in some cases #}
        return v8::Null(isolate);
    {% for member in members %}
    case {{cpp_class}}::{{member.specific_type_enum}}:
        return {{member.cpp_value_to_v8_value}};
    {% endfor %}
    default:
        ASSERT_NOT_REACHED();
    }
    return v8::Local<v8::Value>();
}

{{cpp_class}} NativeValueTraits<{{cpp_class}}>::nativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState)
{
    {{cpp_class}} impl;
    {{v8_class}}::toImpl(isolate, value, impl, UnionTypeConversionMode::NotNullable, exceptionState);
    return impl;
}

} // namespace blink
