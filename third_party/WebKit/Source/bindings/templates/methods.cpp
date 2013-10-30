{##############################################################################}
{% macro generate_method(method) %}
static void {{method.name}}Method(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    {% if method.number_of_required_arguments %}
    if (UNLIKELY(args.Length() < {{method.number_of_required_arguments}})) {
        throwTypeError(ExceptionMessages::failedToExecute("{{method.name}}", "{{interface_name}}", ExceptionMessages::notEnoughArguments({{method.number_of_required_arguments}}, args.Length())), args.GetIsolate());
        return;
    }
    {% endif %}
    {{cpp_class_name}}* imp = {{v8_class_name}}::toNative(args.Holder());
    {% for argument in method.arguments %}
    {% if argument.is_optional %}
    if (UNLIKELY(args.Length() <= {{argument.index}})) {
        {{argument.cpp_method}};
        return;
    }
    {% endif %}
    {% if argument.is_variadic_wrapper_type %}
    Vector<{{argument.cpp_type}} > {{argument.name}};
    for (int i = {{argument.index}}; i < args.Length(); ++i) {
        if (!V8{{argument.idl_type}}::HasInstance(args[i], args.GetIsolate(), worldType(args.GetIsolate()))) {
            throwTypeError(ExceptionMessages::failedToExecute("{{method.name}}", "{{interface_name}}", "parameter {{argument.index + 1}} is not of type '{{argument.idl_type}}'."), args.GetIsolate());
            return;
        }
        {{argument.name}}.append(V8{{argument.idl_type}}::toNative(v8::Handle<v8::Object>::Cast(args[i])));
    }
    {% else %}
    {{argument.v8_value_to_local_cpp_value}};
    {% endif %}
    {% if argument.enum_validation_expression %}
    {# Methods throw on invalid enum values: http://www.w3.org/TR/WebIDL/#idl-enums #}
    String string = {{argument.name}};
    if (!({{argument.enum_validation_expression}})) {
        throwTypeError(ExceptionMessages::failedToExecute("{{method.name}}", "{{interface_name}}", "parameter {{argument.index + 1}} ('" + string + "') is not a valid enum value."), args.GetIsolate());
        return;
    }
    {% endif %}
    {% endfor %}
    {{method.cpp_method}};
}
{% endmacro %}


{##############################################################################}
{% macro method_callback(method) %}
static void {{method.name}}MethodCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMMethod");
    {{cpp_class_name}}V8Internal::{{method.name}}Method(args);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "Execution");
}
{% endmacro %}
