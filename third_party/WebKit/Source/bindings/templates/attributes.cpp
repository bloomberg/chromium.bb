{##############################################################################}
{% macro attribute_getter(attribute, world_suffix) %}
{% filter conditional(attribute.conditional_string) %}
static void {{attribute.name}}AttributeGetter{{world_suffix}}(
{%- if attribute.is_expose_js_accessors %}
const v8::FunctionCallbackInfo<v8::Value>& info
{%- else %}
const v8::PropertyCallbackInfo<v8::Value>& info
{%- endif %})
{
    {% if attribute.is_reflect and not attribute.is_url and
          attribute.idl_type == 'DOMString' and is_node %}
    {% set cpp_class, v8_class = 'Element', 'V8Element' %}
    {% endif %}
    {# imp #}
    {# FIXME: use a local variable for holder more often and simplify below #}
    {% if attribute.is_unforgeable or
          interface_name == 'Window' and attribute.idl_type == 'EventHandler' %}
    {% if interface_name == 'Window' %}
    v8::Handle<v8::Object> holder = info.Holder();
    {% else %}{# perform lookup first #}
    {# FIXME: can we remove this lookup? #}
    v8::Handle<v8::Object> holder = {{v8_class}}::findInstanceInPrototypeChain(info.This(), info.GetIsolate());
    if (holder.IsEmpty())
        return;
    {% endif %}{# Window #}
    {{cpp_class}}* imp = {{v8_class}}::toNative(holder);
    {% elif attribute.cached_attribute_validation_method %}
    v8::Handle<v8::String> propertyName = v8AtomicString(info.GetIsolate(), "{{attribute.name}}");
    {{cpp_class}}* imp = {{v8_class}}::toNative(info.Holder());
    if (!imp->{{attribute.cached_attribute_validation_method}}()) {
        v8::Handle<v8::Value> jsValue = getHiddenValue(info.GetIsolate(), info.Holder(), propertyName);
        if (!jsValue.IsEmpty()) {
            v8SetReturnValue(info, jsValue);
            return;
        }
    }
    {% elif not (attribute.is_static or attribute.is_unforgeable) %}
    {{cpp_class}}* imp = {{v8_class}}::toNative(info.Holder());
    {% endif %}
    {% if attribute.is_implemented_by and not attribute.is_static %}
    ASSERT(imp);
    {% endif %}
    {% if interface_name == 'Window' and attribute.idl_type == 'EventHandler' %}
    if (!imp->document())
        return;
    {% endif %}
    {# Local variables #}
    {% if attribute.is_call_with_execution_context %}
    ExecutionContext* scriptContext = currentExecutionContext(info.GetIsolate());
    {% endif %}
    {% if attribute.is_check_security_for_node or
          attribute.is_getter_raises_exception %}
    ExceptionState exceptionState(ExceptionState::GetterContext, "{{attribute.name}}", "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {% if attribute.is_nullable %}
    bool isNull = false;
    {% endif %}
    {# FIXME: consider always using a local variable for value #}
    {% if attribute.cached_attribute_validation_method or
          attribute.is_getter_raises_exception or
          attribute.is_nullable or
          attribute.reflect_only or
          attribute.idl_type == 'EventHandler' %}
    {# FIXME: Remove this duplicate #}
    {% if attribute.is_implemented_by and not attribute.is_static %}
    ASSERT(imp);
    {% endif %}
    {{attribute.cpp_type}} {{attribute.cpp_value}} = {{attribute.cpp_value_original}};
    {% endif %}
    {# Checks #}
    {% if attribute.is_getter_raises_exception %}
    if (UNLIKELY(exceptionState.throwIfNeeded()))
        return;
    {% endif %}
    {% if attribute.is_check_security_for_node %}
    {# FIXME: use a local variable to not call getter twice #}
    if (!BindingSecurity::shouldAllowAccessToNode(info.GetIsolate(), {{attribute.cpp_value}}, exceptionState)) {
        v8SetReturnValueNull(info);
        exceptionState.throwIfNeeded();
        return;
    }
    {% endif %}
    {% if attribute.reflect_only %}
    {{release_only_check(attribute.reflect_only, attribute.reflect_missing,
                         attribute.reflect_invalid, attribute.reflect_empty)
      | indent}}
    {% endif %}
    {% if attribute.is_nullable %}
    if (isNull) {
        v8SetReturnValueNull(info);
        return;
    }
    {% endif %}
    {% if attribute.cached_attribute_validation_method %}
    setHiddenValue(info.GetIsolate(), info.Holder(), propertyName, {{attribute.cpp_value}}.v8Value());
    {% endif %}
    {# v8SetReturnValue #}
    {% if attribute.is_keep_alive_for_gc %}
    {# FIXME: merge local variable assignment with above #}
    {{attribute.cpp_type}} result({{attribute.cpp_value}});
    if (result && DOMDataStore::setReturnValueFromWrapper{{world_suffix}}<{{attribute.v8_type}}>(info.GetReturnValue(), result.get()))
        return;
    v8::Handle<v8::Value> wrapper = toV8(result.get(), info.Holder(), info.GetIsolate());
    if (!wrapper.IsEmpty()) {
        setHiddenValue(info.GetIsolate(), info.Holder(), "{{attribute.name}}", wrapper);
        {{attribute.v8_set_return_value}};
    }
    {% elif world_suffix %}
    {{attribute.v8_set_return_value_for_main_world}};
    {% else %}
    {{attribute.v8_set_return_value}};
    {% endif %}
}
{% endfilter %}
{% endmacro %}

{######################################}
{% macro release_only_check(reflect_only_values, reflect_missing,
                            reflect_invalid, reflect_empty) %}
{# Attribute is limited to only known values: check that the attribute value is
   one of those. If not, set it to the empty string.
   http://www.whatwg.org/specs/web-apps/current-work/#limited-to-only-known-values #}
{# FIXME: rename resultValue to jsValue #}
{% if reflect_empty %}
if (resultValue.isNull()) {
{% if reflect_missing %}
    resultValue = "{{reflect_missing}}";
{% else %}
    ;
{% endif %}
} else if (resultValue.isEmpty()) {
    resultValue = "{{reflect_empty}}";
{% else %}
if (resultValue.isEmpty()) {
{# FIXME: should use [ReflectEmpty] instead; need to change IDL files #}
{% if reflect_missing %}
    resultValue = "{{reflect_missing}}";
{% else %}
    ;
{% endif %}
{% endif %}
{% for value in reflect_only_values %}
} else if (equalIgnoringCase(resultValue, "{{value}}")) {
    resultValue = "{{value}}";
{% endfor %}
} else {
    resultValue = "{{reflect_invalid}}";
}
{% endmacro %}


{##############################################################################}
{% macro attribute_getter_callback(attribute, world_suffix) %}
{% filter conditional(attribute.conditional_string) %}
static void {{attribute.name}}AttributeGetterCallback{{world_suffix}}(
{%- if attribute.is_expose_js_accessors %}
const v8::FunctionCallbackInfo<v8::Value>& info
{%- else %}
v8::Local<v8::String>, const v8::PropertyCallbackInfo<v8::Value>& info
{%- endif %})
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMGetter");
    {% if attribute.deprecate_as %}
    UseCounter::countDeprecation(callingExecutionContext(info.GetIsolate()), UseCounter::{{attribute.deprecate_as}});
    {% endif %}
    {% if attribute.measure_as %}
    UseCounter::count(callingExecutionContext(info.GetIsolate()), UseCounter::{{attribute.measure_as}});
    {% endif %}
    {% if world_suffix in attribute.activity_logging_world_list_for_getter %}
    V8PerContextData* contextData = V8PerContextData::from(info.GetIsolate()->GetCurrentContext());
    if (contextData && contextData->activityLogger())
        contextData->activityLogger()->log("{{interface_name}}.{{attribute.name}}", 0, 0, "Getter");
    {% endif %}
    {% if attribute.has_custom_getter %}
    {{v8_class}}::{{attribute.name}}AttributeGetterCustom(info);
    {% else %}
    {{cpp_class}}V8Internal::{{attribute.name}}AttributeGetter{{world_suffix}}(info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8Execution");
}
{% endfilter %}
{% endmacro %}


{##############################################################################}
{% macro constructor_getter_callback(attribute, world_suffix) %}
{% filter conditional(attribute.conditional_string) %}
static void {{attribute.name}}ConstructorGetterCallback{{world_suffix}}(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMGetter");
    {% if attribute.deprecate_as %}
    UseCounter::countDeprecation(callingExecutionContext(info.GetIsolate()), UseCounter::{{attribute.deprecate_as}});
    {% endif %}
    {% if attribute.measure_as %}
    UseCounter::count(callingExecutionContext(info.GetIsolate()), UseCounter::{{attribute.measure_as}});
    {% endif %}
    {{cpp_class}}V8Internal::{{cpp_class}}ConstructorGetter{{world_suffix}}(property, info);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8Execution");
}
{% endfilter %}
{% endmacro %}


{##############################################################################}
{% macro attribute_setter(attribute, world_suffix) %}
{% filter conditional(attribute.conditional_string) %}
static void {{attribute.name}}AttributeSetter{{world_suffix}}(
{%- if attribute.is_expose_js_accessors %}
v8::Local<v8::Value> jsValue, const v8::FunctionCallbackInfo<v8::Value>& info
{%- else %}
v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<void>& info
{%- endif %})
{
    {% if attribute.is_reflect and attribute.idl_type == 'DOMString' and
          is_node %}
    {% set cpp_class, v8_class = 'Element', 'V8Element' %}
    {# FIXME: Perl skips most of function, but this seems unnecessary;
       we only need to skip the CallbackDeliveryScope #}
    {% endif %}
    {% if attribute.has_setter_exception_state %}
    ExceptionState exceptionState(ExceptionState::SetterContext, "{{attribute.name}}", "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {% if attribute.has_strict_type_checking %}
    {# Type checking for interface types (if interface not implemented, throw
       TypeError), per http://www.w3.org/TR/WebIDL/#es-interface #}
    if (!isUndefinedOrNull(jsValue) && !V8{{attribute.idl_type}}::hasInstance(jsValue, info.GetIsolate())) {
        exceptionState.throwTypeError("The provided value is not of type '{{attribute.idl_type}}'.");
        exceptionState.throwIfNeeded();
        return;
    }
    {% endif %}
    {% if attribute.put_forwards %}
    {{cpp_class}}* proxyImp = {{v8_class}}::toNative(info.Holder());
    {{attribute.idl_type}}* imp = WTF::getPtr(proxyImp->{{attribute.name}}());
    if (!imp)
        return;
    {% elif not attribute.is_static %}
    {{cpp_class}}* imp = {{v8_class}}::toNative(info.Holder());
    {% endif %}{# imp #}
    {# FIXME: move ASSERT(imp) here. #}
    {% if attribute.idl_type == 'EventHandler' and interface_name == 'Window' %}
    if (!imp->document())
        return;
    {% endif %}
    {% if attribute.idl_type != 'EventHandler' %}
    {{attribute.v8_value_to_local_cpp_value}};
    {% elif not is_node %}{# EventHandler hack #}
    {# FIXME: duplicate of below; remove #}
    {% if attribute.is_implemented_by and not attribute.is_static %}
    ASSERT(imp);
    {% endif %}
    moveEventListenerToNewWrapper(info.Holder(), {{attribute.event_handler_getter_expression}}, jsValue, {{v8_class}}::eventListenerCacheIndex, info.GetIsolate());
    {% endif %}
    {% if attribute.enum_validation_expression %}
    {# Setter ignores invalid enum values: http://www.w3.org/TR/WebIDL/#idl-enums #}
    String string = cppValue;
    if (!({{attribute.enum_validation_expression}}))
        return;
    {% endif %}
    {% if attribute.is_custom_element_callbacks or
          (attribute.is_reflect and
           not(attribute.idl_type == 'DOMString' and is_node)) %}
    {# Skip on compact node DOMString getters #}
    CustomElementCallbackDispatcher::CallbackDeliveryScope deliveryScope;
    {% endif %}
    {% if attribute.is_implemented_by and not attribute.is_static %}
    ASSERT(imp);
    {% endif %}
    {% if attribute.is_call_with_execution_context or
          attribute.is_setter_call_with_execution_context %}
    ExecutionContext* scriptContext = currentExecutionContext(info.GetIsolate());
    {% endif %}
    {{attribute.cpp_setter}};
    {% if attribute.is_setter_raises_exception %}
    exceptionState.throwIfNeeded();
    {% endif %}
    {% if attribute.cached_attribute_validation_method %}
    deleteHiddenValue(info.GetIsolate(), info.Holder(), "{{attribute.name}}"); // Invalidate the cached value.
    {% endif %}
}
{% endfilter %}
{% endmacro %}


{##############################################################################}
{% macro attribute_setter_callback(attribute, world_suffix) %}
{% filter conditional(attribute.conditional_string) %}
static void {{attribute.name}}AttributeSetterCallback{{world_suffix}}(
{%- if attribute.is_expose_js_accessors %}
const v8::FunctionCallbackInfo<v8::Value>& info
{%- else %}
v8::Local<v8::String>, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<void>& info
{%- endif %})
{
    {% if attribute.is_expose_js_accessors %}
    v8::Local<v8::Value> jsValue = info[0];
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMSetter");
    {% if attribute.deprecate_as %}
    UseCounter::countDeprecation(callingExecutionContext(info.GetIsolate()), UseCounter::{{attribute.deprecate_as}});
    {% endif %}
    {% if attribute.measure_as %}
    UseCounter::count(callingExecutionContext(info.GetIsolate()), UseCounter::{{attribute.measure_as}});
    {% endif %}
    {% if world_suffix in attribute.activity_logging_world_list_for_setter %}
    V8PerContextData* contextData = V8PerContextData::from(info.GetIsolate()->GetCurrentContext());
    if (contextData && contextData->activityLogger()) {
        v8::Handle<v8::Value> loggerArg[] = { jsValue };
        contextData->activityLogger()->log("{{interface_name}}.{{attribute.name}}", 1, &loggerArg[0], "Setter");
    }
    {% endif %}
    {% if attribute.is_custom_element_callbacks or attribute.is_reflect %}
    CustomElementCallbackDispatcher::CallbackDeliveryScope deliveryScope;
    {% endif %}
    {% if attribute.has_custom_setter %}
    {{v8_class}}::{{attribute.name}}AttributeSetterCustom(jsValue, info);
    {% else %}
    {{cpp_class}}V8Internal::{{attribute.name}}AttributeSetter{{world_suffix}}(jsValue, info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8Execution");
}
{% endfilter %}
{% endmacro %}
