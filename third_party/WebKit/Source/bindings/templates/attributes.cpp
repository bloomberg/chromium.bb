{##############################################################################}
{% macro attribute_getter(attribute) %}
{% filter conditional(attribute.conditional_string) %}
static void {{attribute.name}}AttributeGetter(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    {% if attribute.cached_attribute_validation_method %}
    v8::Handle<v8::String> propertyName = v8::String::NewSymbol("{{attribute.name}}");
    {{cpp_class_name}}* imp = {{v8_class_name}}::toNative(info.Holder());
    if (!imp->{{attribute.cached_attribute_validation_method}}()) {
        v8::Handle<v8::Value> value = info.Holder()->GetHiddenValue(propertyName);
        if (!value.IsEmpty()) {
            v8SetReturnValue(info, value);
            return;
        }
    }
    {% elif not attribute.is_static %}
    {{cpp_class_name}}* imp = {{v8_class_name}}::toNative(info.Holder());
    {% endif %}
    {% if attribute.is_call_with_script_execution_context %}
    ScriptExecutionContext* scriptContext = getScriptExecutionContext();
    {% endif %}
    {# Special cases #}
    {% if attribute.is_check_security_for_node %}
    {# FIXME: consider using a local variable to not call getter twice #}
    if (!BindingSecurity::shouldAllowAccessToNode({{attribute.cpp_value}})) {
        v8SetReturnValueNull(info);
        return;
    }
    {% endif %}
    {% if attribute.is_nullable %}
    bool isNull = false;
    {{attribute.cpp_type}} {{attribute.cpp_value}} = {{attribute.cpp_value_original}};
    if (isNull) {
        v8SetReturnValueNull(info);
        return;
    }
    {% elif attribute.idl_type == 'EventHandler' or
            attribute.cached_attribute_validation_method %}
    {# FIXME: consider merging all these assign to local variable statements #}
    {{attribute.cpp_type}} {{attribute.cpp_value}} = {{attribute.cpp_value_original}};
    {% endif %}
    {% if attribute.cached_attribute_validation_method %}
    info.Holder()->SetHiddenValue(propertyName, {{attribute.cpp_value}}.v8Value());
    {% endif %}
    {# End special cases #}
    {% if attribute.is_keep_alive_for_gc %}
    {{attribute.cpp_type}} result = {{attribute.cpp_value}};
    if (result.get() && DOMDataStore::setReturnValueFromWrapper<{{attribute.v8_type}}>(info.GetReturnValue(), result.get()))
        return;
    v8::Handle<v8::Value> wrapper = toV8(result.get(), info.Holder(), info.GetIsolate());
    if (!wrapper.IsEmpty()) {
        V8HiddenPropertyName::setNamedHiddenReference(info.Holder(), "{{attribute.name}}", wrapper);
        {{attribute.return_v8_value_statement}}
    }
    {% else %}
    {{attribute.return_v8_value_statement}}
    {% endif %}
}
{% endfilter %}
{% endmacro %}


{##############################################################################}
{% macro attribute_getter_callback(attribute) %}
{% filter conditional(attribute.conditional_string) %}
static void {{attribute.name}}AttributeGetterCallback(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMGetter");
    {% if attribute.is_activity_logging_getter %}
    V8PerContextData* contextData = V8PerContextData::from(info.GetIsolate()->GetCurrentContext());
    if (contextData && contextData->activityLogger())
        contextData->activityLogger()->log("{{interface_name}}.{{attribute.name}}", 0, 0, "Getter");
    {% endif %}
    {% if attribute.is_custom_getter %}
    {{v8_class_name}}::{{attribute.name}}AttributeGetterCustom(name, info);
    {% else %}
    {{cpp_class_name}}V8Internal::{{attribute.name}}AttributeGetter(name, info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "Execution");
}
{% endfilter %}
{% endmacro %}
