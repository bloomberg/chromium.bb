{##############################################################################}
{% macro generate_method(method) %}
static void {{method.name}}Method(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    {% if method.number_of_required_arguments %}
    if (UNLIKELY(info.Length() < {{method.number_of_required_arguments}})) {
        throwTypeError(ExceptionMessages::failedToExecute("{{method.name}}", "{{interface_name}}", ExceptionMessages::notEnoughArguments({{method.number_of_required_arguments}}, info.Length())), info.GetIsolate());
        return;
    }
    {% endif %}
    {{cpp_class_name}}* imp = {{v8_class_name}}::toNative(info.Holder());
    {% for argument in method.arguments %}
    {% if argument.is_optional and not argument.has_default and
          argument.idl_type != 'Dictionary' %}
    {# Optional arguments without a default value generate an early call with
       fewer arguments if they are omitted.
       Optional Dictionary arguments default to empty dictionary. #}
    if (UNLIKELY(info.Length() <= {{argument.index}})) {
        {{argument.cpp_method}};
        return;
    }
    {% endif %}
    {% if argument.is_clamp %}
    {# NaN is treated as 0: http://www.w3.org/TR/WebIDL/#es-type-mapping #}
    {{argument.cpp_type}} {{argument.name}} = 0;
    V8TRYCATCH_VOID(double, {{argument.name}}NativeValue, info[{{argument.index}}]->NumberValue());
    if (!std::isnan({{argument.name}}NativeValue))
        {# IDL type is used for clamping, for the right bounds, since different
           IDL integer types have same internal C++ type (int or unsigned) #}
        {{argument.name}} = clampTo<{{argument.idl_type}}>({{argument.name}}NativeValue);
    {% elif argument.idl_type == 'SerializedScriptValue' %}
    bool {{argument.name}}DidThrow = false;
    {{argument.cpp_type}} {{argument.name}} = SerializedScriptValue::create(info[{{argument.index}}], 0, 0, {{argument.name}}DidThrow, info.GetIsolate());
    if ({{argument.name}}DidThrow)
        return;
    {% elif argument.is_variadic_wrapper_type %}
    Vector<{{argument.cpp_type}} > {{argument.name}};
    for (int i = {{argument.index}}; i < info.Length(); ++i) {
        if (!V8{{argument.idl_type}}::HasInstance(info[i], info.GetIsolate(), worldType(info.GetIsolate()))) {
            throwTypeError(ExceptionMessages::failedToExecute("{{method.name}}", "{{interface_name}}", "parameter {{argument.index + 1}} is not of type '{{argument.idl_type}}'."), info.GetIsolate());
            return;
        }
        {{argument.name}}.append(V8{{argument.idl_type}}::toNative(v8::Handle<v8::Object>::Cast(info[i])));
    }
    {% else %}
    {{argument.v8_value_to_local_cpp_value}};
    {% endif %}
    {% if argument.enum_validation_expression %}
    {# Methods throw on invalid enum values: http://www.w3.org/TR/WebIDL/#idl-enums #}
    String string = {{argument.name}};
    if (!({{argument.enum_validation_expression}})) {
        throwTypeError(ExceptionMessages::failedToExecute("{{method.name}}", "{{interface_name}}", "parameter {{argument.index + 1}} ('" + string + "') is not a valid enum value."), info.GetIsolate());
        return;
    }
    {% endif %}
    {% if argument.idl_type in ['Dictionary', 'Promise'] %}
    if (!{{argument.name}}.isUndefinedOrNull() && !{{argument.name}}.isObject()) {
        throwTypeError(ExceptionMessages::failedToExecute("{{method.name}}", "{{interface_name}}", "parameter {{argument.index + 1}} ('{{argument.name}}') is not an object."), info.GetIsolate());
        return;
    }
    {% endif %}
    {% endfor %}
    {{method.cpp_method}};
}
{% endmacro %}


{##############################################################################}
{% macro method_callback(method) %}
static void {{method.name}}MethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMMethod");
    {{cpp_class_name}}V8Internal::{{method.name}}Method(info);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "Execution");
}
{% endmacro %}
