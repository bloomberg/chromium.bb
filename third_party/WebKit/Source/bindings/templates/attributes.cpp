{##############################################################################}
{% macro attribute_getter(attribute, world_suffix) %}
{% filter conditional(attribute.conditional_string) %}
static void {{attribute.name}}AttributeGetter{{world_suffix}}(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    {% if attribute.is_unforgeable %}
    v8::Handle<v8::Object> holder = info.This()->FindInstanceInPrototypeChain({{v8_class_name}}::GetTemplate(info.GetIsolate(), worldType(info.GetIsolate())));
    if (holder.IsEmpty())
        return;
    {{cpp_class_name}}* imp = {{v8_class_name}}::toNative(holder);
    {% endif %}
    {% if attribute.cached_attribute_validation_method %}
    v8::Handle<v8::String> propertyName = v8::String::NewSymbol("{{attribute.name}}");
    {{cpp_class_name}}* imp = {{v8_class_name}}::toNative(info.Holder());
    if (!imp->{{attribute.cached_attribute_validation_method}}()) {
        v8::Handle<v8::Value> jsValue = info.Holder()->GetHiddenValue(propertyName);
        if (!jsValue.IsEmpty()) {
            v8SetReturnValue(info, jsValue);
            return;
        }
    }
    {% elif not (attribute.is_static or attribute.is_unforgeable) %}
    {{cpp_class_name}}* imp = {{v8_class_name}}::toNative(info.Holder());
    {% endif %}
    {% if attribute.is_call_with_script_execution_context %}
    ExecutionContext* scriptContext = getExecutionContext();
    {% endif %}
    {# Special cases #}
    {% if attribute.is_check_security_for_node %}
    {# FIXME: consider using a local variable to not call getter twice #}
    ExceptionState es(info.GetIsolate());
    if (!BindingSecurity::shouldAllowAccessToNode({{attribute.cpp_value}}, es)) {
        v8SetReturnValueNull(info);
        es.throwIfNeeded();
        return;
    }
    {% endif %}
    {% if attribute.is_getter_raises_exception %}
    ExceptionState es(info.GetIsolate());
    {{attribute.cpp_type}} {{attribute.cpp_value}} = {{attribute.cpp_value_original}};
    if (UNLIKELY(es.throwIfNeeded()))
        return;
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
    if (result && DOMDataStore::setReturnValueFromWrapper<{{attribute.v8_type}}>(info.GetReturnValue(), result.get()))
        return;
    v8::Handle<v8::Value> wrapper = toV8(result.get(), info.Holder(), info.GetIsolate());
    if (!wrapper.IsEmpty()) {
        V8HiddenPropertyName::setNamedHiddenReference(info.Holder(), "{{attribute.name}}", wrapper);
        {{attribute.v8_set_return_value}};
    }
    {% else %}
    {{attribute.v8_set_return_value}};
    {% endif %}
}
{% endfilter %}
{% endmacro %}


{##############################################################################}
{% macro attribute_getter_callback(attribute, world_suffix) %}
{% filter conditional(attribute.conditional_string) %}
static void {{attribute.name}}AttributeGetterCallback{{world_suffix}}(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMGetter");
    {% if attribute.deprecate_as %}
    UseCounter::countDeprecation(activeExecutionContext(), UseCounter::{{attribute.deprecate_as}});
    {% endif %}
    {% if attribute.measure_as %}
    UseCounter::count(activeDOMWindow(), UseCounter::{{attribute.measure_as}});
    {% endif %}
    {% if world_suffix in attribute.activity_logging_getter %}
    V8PerContextData* contextData = V8PerContextData::from(info.GetIsolate()->GetCurrentContext());
    if (contextData && contextData->activityLogger())
        contextData->activityLogger()->log("{{interface_name}}.{{attribute.name}}", 0, 0, "Getter");
    {% endif %}
    {% if attribute.has_custom_getter %}
    {{v8_class_name}}::{{attribute.name}}AttributeGetterCustom(name, info);
    {% else %}
    {{cpp_class_name}}V8Internal::{{attribute.name}}AttributeGetter{{world_suffix}}(name, info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "Execution");
}
{% endfilter %}
{% endmacro %}


{##############################################################################}
{% macro attribute_setter(attribute, world_suffix) %}
{% filter conditional(attribute.conditional_string) %}
static void {{attribute.name}}AttributeSetter{{world_suffix}}(v8::Local<v8::String> name, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<void>& info)
{
    {{cpp_class_name}}* imp = {{v8_class_name}}::toNative(info.Holder());
    {{attribute.v8_value_to_local_cpp_value}};
    {% if attribute.is_call_with_script_execution_context %}
    ExecutionContext* scriptContext = getExecutionContext();
    {% endif %}
    {{attribute.cpp_setter}};
    {% if attribute.cached_attribute_validation_method %}
    info.Holder()->DeleteHiddenValue(v8::String::NewSymbol("{{attribute.name}}")); // Invalidate the cached value.
    {% endif %}
}
{% endfilter %}
{% endmacro %}


{##############################################################################}
{% macro attribute_setter_callback(attribute, world_suffix) %}
{% filter conditional(attribute.conditional_string) %}
static void {{attribute.name}}AttributeSetterCallback{{world_suffix}}(v8::Local<v8::String> name, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<void>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMSetter");
    {{cpp_class_name}}V8Internal::{{attribute.name}}AttributeSetter{{world_suffix}}(name, jsValue, info);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "Execution");
}
{% endfilter %}
{% endmacro %}
