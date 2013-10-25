{##############################################################################}
{% macro generate_method(method) %}
static void {{method.name}}Method(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    {{cpp_class_name}}* imp = {{v8_class_name}}::toNative(args.Holder());
    imp->{{method.name}}();

    return;
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
