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
    {{argument.v8_value_to_local_cpp_value}};
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
