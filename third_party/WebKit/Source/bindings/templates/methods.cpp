{##############################################################################}
{% macro generate_method(method, world_suffix) %}
{% filter conditional(method.conditional_string) %}
static void {{method.name}}{{method.overload_index}}Method{{world_suffix}}(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    {# Local variables #}
    {% if method.has_exception_state %}
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "{{method.name}}", "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {% if method.number_of_required_arguments %}
    if (UNLIKELY(info.Length() < {{method.number_of_required_arguments}})) {
        {{throw_arity_type_error(method, method.number_of_required_arguments)}};
        return;
    }
    {% endif %}
    {% if not method.is_static %}
    {{cpp_class}}* impl = {{v8_class}}::toNative(info.Holder());
    {% endif %}
    {% if method.is_custom_element_callbacks %}
    CustomElementCallbackDispatcher::CallbackDeliveryScope deliveryScope;
    {% endif %}
    {# Security checks #}
    {# FIXME: change to method.is_check_security_for_window #}
    {% if interface_name == 'EventTarget' %}
    if (DOMWindow* window = impl->toDOMWindow()) {
        if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), window->frame(), exceptionState)) {
            exceptionState.throwIfNeeded();
            return;
        }
        if (!window->document())
            return;
    }
    {% elif method.is_check_security_for_frame %}
    if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), impl->frame(), exceptionState)) {
        exceptionState.throwIfNeeded();
        return;
    }
    {% endif %}
    {% if method.is_check_security_for_node %}
    if (!BindingSecurity::shouldAllowAccessToNode(info.GetIsolate(), impl->{{method.name}}(exceptionState), exceptionState)) {
        v8SetReturnValueNull(info);
        exceptionState.throwIfNeeded();
        return;
    }
    {% endif %}
    {# Call method #}
    {% for argument in method.arguments %}
    {{generate_argument(method, argument, world_suffix) | indent}}
    {% endfor %}
    {% if world_suffix %}
    {{cpp_method_call(method, method.v8_set_return_value_for_main_world, method.cpp_value) | indent}}
    {% else %}
    {{cpp_method_call(method, method.v8_set_return_value, method.cpp_value) | indent}}
    {% endif %}
    {# Post-call #}
    {% if method.has_event_listener_argument %}
    {{hidden_dependency_action(method.name) | indent}}
    {% endif %}
}
{% endfilter %}
{% endmacro %}


{######################################}
{% macro hidden_dependency_action(method_name) %}
if (listener && !impl->toNode())
    {% if method_name == 'addEventListener' %}
    addHiddenValueToArray(info.Holder(), info[1], {{v8_class}}::eventListenerCacheIndex, info.GetIsolate());
    {% else %}{# method_name == 'removeEventListener' #}
    removeHiddenValueFromArray(info.Holder(), info[1], {{v8_class}}::eventListenerCacheIndex, info.GetIsolate());
    {% endif %}
{% endmacro %}


{######################################}
{% macro generate_argument(method, argument, world_suffix) %}
{% if argument.is_optional and not argument.has_default and
      argument.idl_type != 'Dictionary' and
      not argument.is_callback_interface %}
{# Optional arguments without a default value generate an early call with
   fewer arguments if they are omitted.
   Optional Dictionary arguments default to empty dictionary. #}
if (UNLIKELY(info.Length() <= {{argument.index}})) {
    {% if world_suffix %}
    {{cpp_method_call(method, argument.v8_set_return_value_for_main_world, argument.cpp_value) | indent}}
    {% else %}
    {{cpp_method_call(method, argument.v8_set_return_value, argument.cpp_value) | indent}}
    {% endif %}
    {% if argument.has_event_listener_argument %}
    {{hidden_dependency_action(method.name) | indent}}
    {% endif %}
    return;
}
{% endif %}
{% if method.is_strict_type_checking and argument.is_wrapper_type %}
{# Type checking for wrapper interface types (if interface not implemented,
   throw TypeError), per http://www.w3.org/TR/WebIDL/#es-interface #}
if (info.Length() > {{argument.index}} && {% if argument.is_nullable %}!isUndefinedOrNull(info[{{argument.index}}]) && {% endif %}!V8{{argument.idl_type}}::hasInstance(info[{{argument.index}}], info.GetIsolate())) {
    {{throw_type_error(method, '"parameter %s is not of type \'%s\'."' %
                               (argument.index + 1, argument.idl_type)) | indent}}
    return;
}
{% endif %}
{% if argument.is_callback_interface %}
{# FIXME: remove EventListener special case #}
{% if argument.idl_type == 'EventListener' %}
{% if method.name == 'removeEventListener' %}
RefPtr<{{argument.idl_type}}> {{argument.name}} = V8EventListenerList::getEventListener(info[1], false, ListenerFindOnly);
{% else %}{# method.name == 'addEventListener' #}
RefPtr<{{argument.idl_type}}> {{argument.name}} = V8EventListenerList::getEventListener(info[1], false, ListenerFindOrCreate);
{% endif %}{# method.name #}
{% else %}
{% if argument.is_optional %}
OwnPtr<{{argument.idl_type}}> {{argument.name}};
if (info.Length() > {{argument.index}} && !isUndefinedOrNull(info[{{argument.index}}])) {
    if (!info[{{argument.index}}]->IsFunction()) {
        {{throw_type_error(method,
              '"The callback provided as parameter %s is not a function."' %
                  (argument.index + 1)) | indent(8)}}
        return;
    }
    {{argument.name}} = V8{{argument.idl_type}}::create(v8::Handle<v8::Function>::Cast(info[{{argument.index}}]), currentExecutionContext(info.GetIsolate()));
}
{% else %}
if (info.Length() <= {{argument.index}} || !{% if argument.is_nullable %}(info[{{argument.index}}]->IsFunction() || info[{{argument.index}}]->IsNull()){% else %}info[{{argument.index}}]->IsFunction(){% endif %}) {
    {{throw_type_error(method,
          '"The callback provided as parameter %s is not a function."' %
              (argument.index + 1)) | indent }}
    return;
}
OwnPtr<{{argument.idl_type}}> {{argument.name}} = {% if argument.is_nullable %}info[{{argument.index}}]->IsNull() ? nullptr : {% endif %}V8{{argument.idl_type}}::create(v8::Handle<v8::Function>::Cast(info[{{argument.index}}]), currentExecutionContext(info.GetIsolate()));
{% endif %}{# argument.is_optional #}
{% endif %}{# argument.idl_type == 'EventListener' #}
{% elif argument.is_clamp %}{# argument.is_callback_interface #}
{# NaN is treated as 0: http://www.w3.org/TR/WebIDL/#es-type-mapping #}
{{argument.cpp_type}} {{argument.name}} = 0;
V8TRYCATCH_VOID(double, {{argument.name}}NativeValue, info[{{argument.index}}]->NumberValue());
if (!std::isnan({{argument.name}}NativeValue))
    {# IDL type is used for clamping, for the right bounds, since different
       IDL integer types have same internal C++ type (int or unsigned) #}
    {{argument.name}} = clampTo<{{argument.idl_type}}>({{argument.name}}NativeValue);
{% elif argument.idl_type == 'SerializedScriptValue' %}
{{argument.cpp_type}} {{argument.name}} = SerializedScriptValue::create(info[{{argument.index}}], 0, 0, exceptionState, info.GetIsolate());
if (exceptionState.throwIfNeeded())
    return;
{% elif argument.is_variadic_wrapper_type %}
{{argument.vector_type}}<{{argument.cpp_type}} > {{argument.name}};
for (int i = {{argument.index}}; i < info.Length(); ++i) {
    if (!V8{{argument.idl_type}}::hasInstance(info[i], info.GetIsolate())) {
        {{throw_type_error(method, '"parameter %s is not of type \'%s\'."' %
                                   (argument.index + 1, argument.idl_type)) | indent(8)}}
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
    {{throw_type_error(method,
          '"parameter %s (\'" + string + "\') is not a valid enum value."' %
              (argument.index + 1)) | indent}}
    return;
}
{% endif %}
{% if argument.idl_type in ['Dictionary', 'Promise'] %}
if (!{{argument.name}}.isUndefinedOrNull() && !{{argument.name}}.isObject()) {
    {{throw_type_error(method, '"parameter %s (\'%s\') is not an object."' %
                               (argument.index + 1, argument.name)) | indent}}
    return;
}
{% endif %}
{% endmacro %}


{######################################}
{% macro cpp_method_call(method, v8_set_return_value, cpp_value) %}
{# Local variables #}
{% if method.is_partial_interface_member and not method.is_static %}
{# instance members (non-static members) in partial interface take |impl| #}
ASSERT(impl);
{% endif %}
{% if method.is_call_with_script_state %}
ScriptState* state = ScriptState::current();
if (!state)
    return;
{% endif %}
{% if method.is_call_with_new_script_state %}
NewScriptState* state = NewScriptState::current(info.GetIsolate());
{% endif %}
{% if method.is_call_with_execution_context %}
ExecutionContext* scriptContext = currentExecutionContext(info.GetIsolate());
{% endif %}
{% if method.is_call_with_script_arguments %}
RefPtr<ScriptArguments> scriptArguments(createScriptArguments(info, {{method.number_of_arguments}}));
{% endif %}
{# Call #}
{% if method.idl_type == 'void' %}
{{cpp_value}};
{% elif method.is_constructor %}
{{method.cpp_type}} impl = {{cpp_value}};
{% elif method.is_call_with_script_state or method.is_call_with_new_script_state or method.is_raises_exception %}
{# FIXME: consider always using a local variable #}
{{method.cpp_type}} result = {{cpp_value}};
{% endif %}
{# Post-call #}
{% if method.is_raises_exception %}
if (exceptionState.throwIfNeeded())
    return;
{% endif %}
{# Set return value #}
{% if method.is_constructor %}
{{generate_constructor_wrapper(method)}}{% elif method.union_arguments %}
{{union_type_method_call_and_set_return_value(method)}}
{% elif v8_set_return_value %}{{v8_set_return_value}};{% endif %}{# None for void #}
{% endmacro %}


{######################################}
{% macro union_type_method_call_and_set_return_value(method) %}
{% for cpp_type in method.cpp_type %}
bool result{{loop.index0}}Enabled = false;
{{cpp_type}} result{{loop.index0}};
{% endfor %}
{{method.cpp_value}};
{% if method.is_null_expression %}{# used by getters #}
if ({{method.is_null_expression}})
    return;
{% endif %}
{% for v8_set_return_value in method.v8_set_return_value %}
if (result{{loop.index0}}Enabled) {
    {{v8_set_return_value}};
    return;
}
{% endfor %}
{# Fall back to null if none of the union members results are returned #}
v8SetReturnValueNull(info);
{%- endmacro %}


{######################################}
{% macro throw_type_error(method, error_message) %}
{% if method.has_exception_state %}
exceptionState.throwTypeError({{error_message}});
exceptionState.throwIfNeeded();
{%- elif method.is_constructor %}
throwTypeError(ExceptionMessages::failedToConstruct("{{interface_name}}", {{error_message}}), info.GetIsolate());
{%- else %}
throwTypeError(ExceptionMessages::failedToExecute("{{method.name}}", "{{interface_name}}", {{error_message}}), info.GetIsolate());
{%- endif %}
{% endmacro %}


{######################################}
{% macro throw_arity_type_error(method, number_of_required_arguments) %}
{% if method.has_exception_state %}
throwArityTypeError(exceptionState, {{number_of_required_arguments}}, info.Length())
{%- elif method.is_constructor %}
throwArityTypeErrorForConstructor("{{interface_name}}", {{number_of_required_arguments}}, info.Length(), info.GetIsolate())
{%- else %}
throwArityTypeErrorForMethod("{{method.name}}", "{{interface_name}}", {{number_of_required_arguments}}, info.Length(), info.GetIsolate())
{%- endif %}
{% endmacro %}


{##############################################################################}
{% macro overload_resolution_method(overloads, world_suffix) %}
static void {{overloads.name}}Method{{world_suffix}}(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    {% for method in overloads.methods %}
    if ({{method.overload_resolution_expression}}) {
        {{method.name}}{{method.overload_index}}Method{{world_suffix}}(info);
        return;
    }
    {% endfor %}
    {% if overloads.minimum_number_of_required_arguments %}
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "{{overloads.name}}", "{{interface_name}}", info.Holder(), info.GetIsolate());
    if (UNLIKELY(info.Length() < {{overloads.minimum_number_of_required_arguments}})) {
        {{throw_arity_type_error(overloads, overloads.minimum_number_of_required_arguments)}};
        return;
    }
    {% endif %}
    {{throw_type_error(overloads, '"No function was found that matched the signature provided."') | indent}}
}
{% endmacro %}


{##############################################################################}
{% macro method_callback(method, world_suffix) %}
{% filter conditional(method.conditional_string) %}
static void {{method.name}}MethodCallback{{world_suffix}}(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMMethod");
    {% if method.measure_as %}
    UseCounter::count(callingExecutionContext(info.GetIsolate()), UseCounter::{{method.measure_as}});
    {% endif %}
    {% if method.deprecate_as %}
    UseCounter::countDeprecation(callingExecutionContext(info.GetIsolate()), UseCounter::{{method.deprecate_as}});
    {% endif %}
    {% if world_suffix in method.activity_logging_world_list %}
    V8PerContextData* contextData = V8PerContextData::from(info.GetIsolate()->GetCurrentContext());
    if (contextData && contextData->activityLogger()) {
        {# FIXME: replace toVectorOfArguments with toNativeArguments(info, 0)
           and delete toVectorOfArguments #}
        Vector<v8::Handle<v8::Value> > loggerArgs = toNativeArguments<v8::Handle<v8::Value> >(info, 0);
        contextData->activityLogger()->log("{{interface_name}}.{{method.name}}", info.Length(), loggerArgs.data(), "Method");
    }
    {% endif %}
    {% if method.is_custom %}
    {{v8_class}}::{{method.name}}MethodCustom(info);
    {% else %}
    {{cpp_class}}V8Internal::{{method.name}}Method{{world_suffix}}(info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8Execution");
}
{% endfilter %}
{% endmacro %}


{##############################################################################}
{% macro origin_safe_method_getter(method, world_suffix) %}
static void {{method.name}}OriginSafeMethodGetter{{world_suffix}}(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    {% set signature = 'v8::Local<v8::Signature>()'
                       if method.is_do_not_check_signature else
                       'v8::Signature::New(info.GetIsolate(), %s::domTemplate(info.GetIsolate()))' % v8_class %}
    v8::Isolate* isolate = info.GetIsolate();
    static int domTemplateKey; // This address is used for a key to look up the dom template.
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    {# FIXME: 1 case of [DoNotCheckSignature] in Window.idl may differ #}
    v8::Handle<v8::FunctionTemplate> privateTemplate = data->domTemplate(&domTemplateKey, {{cpp_class}}V8Internal::{{method.name}}MethodCallback{{world_suffix}}, v8Undefined(), {{signature}}, {{method.number_of_required_or_variadic_arguments}});

    v8::Handle<v8::Object> holder = {{v8_class}}::findInstanceInPrototypeChain(info.This(), isolate);
    if (holder.IsEmpty()) {
        // This is only reachable via |object.__proto__.func|, in which case it
        // has already passed the same origin security check
        v8SetReturnValue(info, privateTemplate->GetFunction());
        return;
    }
    {{cpp_class}}* impl = {{v8_class}}::toNative(holder);
    if (!BindingSecurity::shouldAllowAccessToFrame(isolate, impl->frame(), DoNotReportSecurityError)) {
        static int sharedTemplateKey; // This address is used for a key to look up the dom template.
        v8::Handle<v8::FunctionTemplate> sharedTemplate = data->domTemplate(&sharedTemplateKey, {{cpp_class}}V8Internal::{{method.name}}MethodCallback{{world_suffix}}, v8Undefined(), {{signature}}, {{method.number_of_required_or_variadic_arguments}});
        v8SetReturnValue(info, sharedTemplate->GetFunction());
        return;
    }

    v8::Local<v8::Value> hiddenValue = info.This()->GetHiddenValue(v8AtomicString(isolate, "{{method.name}}"));
    if (!hiddenValue.IsEmpty()) {
        v8SetReturnValue(info, hiddenValue);
        return;
    }

    v8SetReturnValue(info, privateTemplate->GetFunction());
}

static void {{method.name}}OriginSafeMethodGetterCallback{{world_suffix}}(v8::Local<v8::String>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMGetter");
    {{cpp_class}}V8Internal::{{method.name}}OriginSafeMethodGetter{{world_suffix}}(info);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8Execution");
}
{% endmacro %}


{##############################################################################}
{% macro generate_constructor(constructor) %}
static void constructor{{constructor.overload_index}}(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* isolate = info.GetIsolate();
    {% if constructor.has_exception_state %}
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "{{interface_name}}", info.Holder(), isolate);
    {% endif %}
    {% if interface_length and not constructor.overload_index %}
    {# FIXME: remove UNLIKELY: constructors are expensive, so no difference. #}
    if (UNLIKELY(info.Length() < {{interface_length}})) {
        {{throw_arity_type_error(constructor, interface_length)}};
        return;
    }
    {% endif %}
    {% for argument in constructor.arguments %}
    {{generate_argument(constructor, argument) | indent}}
    {% endfor %}
    {% if is_constructor_call_with_execution_context %}
    ExecutionContext* context = currentExecutionContext(isolate);
    {% endif %}
    {% if is_constructor_call_with_document %}
    Document& document = *toDocument(currentExecutionContext(isolate));
    {% endif %}
    {{constructor.cpp_type}} impl = {{cpp_class}}::create({{constructor.argument_list | join(', ')}});
    {% if is_constructor_raises_exception %}
    if (exceptionState.throwIfNeeded())
        return;
    {% endif %}

    {{generate_constructor_wrapper(constructor) | indent}}
}
{% endmacro %}


{##############################################################################}
{% macro generate_constructor_wrapper(constructor) %}
{% if has_custom_wrap %}
v8::Handle<v8::Object> wrapper = wrap(impl.get(), info.Holder(), isolate);
{% else %}
{% set constructor_class = v8_class + ('Constructor'
                                       if constructor.is_named_constructor else
                                       '') %}
v8::Handle<v8::Object> wrapper = info.Holder();
V8DOMWrapper::associateObjectWithWrapper<{{v8_class}}>(impl.release(), &{{constructor_class}}::wrapperTypeInfo, wrapper, isolate, {{wrapper_configuration}});
{% endif %}
v8SetReturnValue(info, wrapper);
{% endmacro %}


{##############################################################################}
{% macro named_constructor_callback(constructor) %}
static void {{v8_class}}ConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* isolate = info.GetIsolate();
    if (!info.IsConstructCall()) {
        throwTypeError(ExceptionMessages::constructorNotCallableAsFunction("{{constructor.name}}"), isolate);
        return;
    }

    if (ConstructorMode::current(isolate) == ConstructorMode::WrapExistingObject) {
        v8SetReturnValue(info, info.Holder());
        return;
    }

    Document* document = currentDOMWindow(isolate)->document();
    ASSERT(document);

    // Make sure the document is added to the DOM Node map. Otherwise, the {{cpp_class}} instance
    // may end up being the only node in the map and get garbage-collected prematurely.
    toV8(document, info.Holder(), isolate);

    {% if constructor.has_exception_state %}
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "{{interface_name}}", info.Holder(), isolate);
    {% endif %}
    {% if constructor.number_of_required_arguments %}
    if (UNLIKELY(info.Length() < {{constructor.number_of_required_arguments}})) {
        {{throw_arity_type_error(constructor, constructor.number_of_required_arguments)}};
        return;
    }
    {% endif %}
    {% for argument in constructor.arguments %}
    {{generate_argument(constructor, argument) | indent}}
    {% endfor %}
    {{constructor.cpp_type}} impl = {{cpp_class}}::createForJSConstructor({{constructor.argument_list | join(', ')}});
    {% if is_constructor_raises_exception %}
    if (exceptionState.throwIfNeeded())
        return;
    {% endif %}

    {{generate_constructor_wrapper(constructor) | indent}}
}
{% endmacro %}
