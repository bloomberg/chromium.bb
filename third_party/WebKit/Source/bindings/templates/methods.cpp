{##############################################################################}
{% macro generate_method(method, world_suffix) %}
{% filter conditional(method.conditional_string) %}
static void {{method.name}}{{method.overload_index}}Method{{world_suffix}}(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    {% if method.has_exception_state %}
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "{{method.name}}", "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {% if method.name in ['addEventListener', 'removeEventListener',
                          'dispatchEvent'] %}
    {{add_event_listener_remove_event_listener_dispatch_event() | indent}}
    {% endif %}
    {% if method.name in ['addEventListener', 'removeEventListener'] %}
    {{add_event_listener_remove_event_listener_method(method.name) | indent}}
    {% else %}
    {% if method.number_of_required_arguments %}
    if (UNLIKELY(info.Length() < {{method.number_of_required_arguments}})) {
        {{throw_type_error(method,
              'ExceptionMessages::notEnoughArguments(%s, info.Length())' %
                  method.number_of_required_arguments) | indent(8)}}
        return;
    }
    {% endif %}
    {% if not method.is_static %}
    {{cpp_class}}* imp = {{v8_class}}::toNative(info.Holder());
    {% endif %}
    {% if method.is_custom_element_callbacks %}
    CustomElementCallbackDispatcher::CallbackDeliveryScope deliveryScope;
    {% endif %}
    {% if method.is_check_security_for_frame %}
    if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), imp->frame(), exceptionState)) {
        exceptionState.throwIfNeeded();
        return;
    }
    {% endif %}
    {% if method.is_check_security_for_node %}
    if (!BindingSecurity::shouldAllowAccessToNode(info.GetIsolate(), imp->{{method.name}}(exceptionState), exceptionState)) {
        v8SetReturnValueNull(info);
        exceptionState.throwIfNeeded();
        return;
    }
    {% endif %}
    {% for argument in method.arguments %}
    {{generate_argument(method, argument, world_suffix) | indent}}
    {% endfor %}
    {% if world_suffix %}
    {{cpp_method_call(method, method.v8_set_return_value_for_main_world, method.cpp_value) | indent}}
    {% else %}
    {{cpp_method_call(method, method.v8_set_return_value, method.cpp_value) | indent}}
    {% endif %}
    {% endif %}{# addEventListener, removeEventListener #}
}
{% endfilter %}
{% endmacro %}


{######################################}
{% macro add_event_listener_remove_event_listener_dispatch_event() %}
{# FIXME: Clean up: use the normal |imp| above,
          use the existing shouldAllowAccessToFrame check if possible. #}
EventTarget* impl = {{v8_class}}::toNative(info.Holder());
if (DOMWindow* window = impl->toDOMWindow()) {
    if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), window->frame(), exceptionState)) {
        exceptionState.throwIfNeeded();
        return;
    }
    if (!window->document())
        return;
}
{% endmacro %}


{######################################}
{% macro add_event_listener_remove_event_listener_method(method_name) %}
{# Set template values for addEventListener vs. removeEventListener #}
{% set listener_lookup_type, listener, hidden_dependency_action =
    ('ListenerFindOrCreate', 'listener', 'addHiddenValueToArray')
    if method_name == 'addEventListener' else
    ('ListenerFindOnly', 'listener.get()', 'removeHiddenValueFromArray')
%}
RefPtr<EventListener> listener = V8EventListenerList::getEventListener(info[1], false, {{listener_lookup_type}});
if (listener) {
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<WithNullCheck>, eventName, info[0]);
    impl->{{method_name}}(eventName, {{listener}}, info[2]->BooleanValue());
    if (!impl->toNode())
        {{hidden_dependency_action}}(info.Holder(), info[1], {{v8_class}}::eventListenerCacheIndex, info.GetIsolate());
}
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
Vector<{{argument.cpp_type}} > {{argument.name}};
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
{% if method.is_implemented_by and not method.is_static %}
ASSERT(imp);
{% endif %}
{% if method.is_call_with_script_state %}
ScriptState* currentState = ScriptState::current();
if (!currentState)
    return;
ScriptState& state = *currentState;
{% endif %}
{% if method.is_call_with_execution_context %}
ExecutionContext* scriptContext = currentExecutionContext(info.GetIsolate());
{% endif %}
{% if method.is_call_with_script_arguments %}
RefPtr<ScriptArguments> scriptArguments(createScriptArguments(info, {{method.number_of_arguments}}));
{% endif %}
{% if method.idl_type == 'void' %}
{{cpp_value}};
{% elif method.is_call_with_script_state or method.is_raises_exception %}
{# FIXME: consider always using a local variable #}
{{method.cpp_type}} result = {{cpp_value}};
{% endif %}
{% if method.is_raises_exception %}
if (exceptionState.throwIfNeeded())
    return;
{% endif %}
{% if method.is_call_with_script_state %}
if (state.hadException()) {
    v8::Local<v8::Value> exception = state.exception();
    state.clearException();
    throwError(exception, info.GetIsolate());
    return;
}
{% endif %}
{% if method.union_arguments %}
{{union_type_method_call(method)}}
{% elif v8_set_return_value %}{{v8_set_return_value}};{% endif %}{# None for void #}
{% endmacro %}

{######################################}
{% macro union_type_method_call(method) %}
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
        {{throw_type_error(overloads,
              'ExceptionMessages::notEnoughArguments(%s, info.Length())' %
              overloads.minimum_number_of_required_arguments) | indent(8)}}
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
                       'v8::Signature::New(info.GetIsolate(), %s::domTemplate(info.GetIsolate(), currentWorldType))' % v8_class %}
    {# FIXME: don't call GetIsolate() so often #}
    // This is only for getting a unique pointer which we can pass to privateTemplate.
    static int privateTemplateUniqueKey;
    WrapperWorldType currentWorldType = worldType(info.GetIsolate());
    V8PerIsolateData* data = V8PerIsolateData::from(info.GetIsolate());
    {# FIXME: 1 case of [DoNotCheckSignature] in Window.idl may differ #}
    v8::Handle<v8::FunctionTemplate> privateTemplate = data->privateTemplate(currentWorldType, &privateTemplateUniqueKey, {{cpp_class}}V8Internal::{{method.name}}MethodCallback{{world_suffix}}, v8Undefined(), {{signature}}, {{method.number_of_required_or_variadic_arguments}});

    v8::Handle<v8::Object> holder = info.This()->FindInstanceInPrototypeChain({{v8_class}}::domTemplate(info.GetIsolate(), currentWorldType));
    if (holder.IsEmpty()) {
        // This is only reachable via |object.__proto__.func|, in which case it
        // has already passed the same origin security check
        v8SetReturnValue(info, privateTemplate->GetFunction());
        return;
    }
    {{cpp_class}}* imp = {{v8_class}}::toNative(holder);
    if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), imp->frame(), DoNotReportSecurityError)) {
        static int sharedTemplateUniqueKey;
        v8::Handle<v8::FunctionTemplate> sharedTemplate = data->privateTemplate(currentWorldType, &sharedTemplateUniqueKey, {{cpp_class}}V8Internal::{{method.name}}MethodCallback{{world_suffix}}, v8Undefined(), {{signature}}, {{method.number_of_required_or_variadic_arguments}});
        v8SetReturnValue(info, sharedTemplate->GetFunction());
        return;
    }

    v8::Local<v8::Value> hiddenValue = getHiddenValue(info.GetIsolate(), info.This(), "{{method.name}}");
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
    {% if constructor.has_exception_state %}
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {% if interface_length and not constructor.overload_index %}
    {# FIXME: remove this UNLIKELY: constructors are heavy, so no difference. #}
    if (UNLIKELY(info.Length() < {{interface_length}})) {
        {{throw_type_error(constructor,
            'ExceptionMessages::notEnoughArguments(%s, info.Length())' %
                interface_length) | indent(8)}}
        return;
    }
    {% endif %}
    {% for argument in constructor.arguments %}
    {{generate_argument(constructor, argument) | indent}}
    {% endfor %}
    {% if is_constructor_call_with_execution_context %}
    ExecutionContext* context = currentExecutionContext(info.GetIsolate());
    {% endif %}
    {% if is_constructor_call_with_document %}
    Document& document = *toDocument(currentExecutionContext(info.GetIsolate()));
    {% endif %}
    {{ref_ptr}}<{{cpp_class}}> impl = {{cpp_class}}::create({{constructor.argument_list | join(', ')}});
    v8::Handle<v8::Object> wrapper = info.Holder();
    {% if is_constructor_raises_exception %}
    if (exceptionState.throwIfNeeded())
        return;
    {% endif %}

    {# FIXME: Should probably be Independent unless [ActiveDOMObject]
              or [DependentLifetime]. #}
    V8DOMWrapper::associateObjectWithWrapper<{{v8_class}}>(impl.release(), &{{v8_class}}::wrapperTypeInfo, wrapper, info.GetIsolate(), WrapperConfiguration::Dependent);
    v8SetReturnValue(info, wrapper);
}
{% endmacro %}


{##############################################################################}
{% macro named_constructor_callback(constructor) %}
static void {{v8_class}}ConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (!info.IsConstructCall()) {
        throwTypeError(ExceptionMessages::failedToConstruct("{{constructor.name}}", "Please use the 'new' operator, this DOM object constructor cannot be called as a function."), info.GetIsolate());
        return;
    }

    if (ConstructorMode::current() == ConstructorMode::WrapExistingObject) {
        v8SetReturnValue(info, info.Holder());
        return;
    }

    Document* document = currentDocument(info.GetIsolate());
    ASSERT(document);

    // Make sure the document is added to the DOM Node map. Otherwise, the {{cpp_class}} instance
    // may end up being the only node in the map and get garbage-collected prematurely.
    toV8(document, info.Holder(), info.GetIsolate());

    {% if constructor.has_exception_state %}
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {% if constructor.number_of_required_arguments %}
    if (UNLIKELY(info.Length() < {{constructor.number_of_required_arguments}})) {
        {{throw_type_error(constructor,
              'ExceptionMessages::notEnoughArguments(%s, info.Length())' %
                  constructor.number_of_required_arguments) | indent(8)}}
        return;
    }
    {% endif %}
    {% for argument in constructor.arguments %}
    {{generate_argument(constructor, argument) | indent}}
    {% endfor %}
    RefPtr<{{cpp_class}}> impl = {{cpp_class}}::createForJSConstructor({{constructor.argument_list | join(', ')}});
    v8::Handle<v8::Object> wrapper = info.Holder();
    {% if is_constructor_raises_exception %}
    if (exceptionState.throwIfNeeded())
        return;
    {% endif %}

    V8DOMWrapper::associateObjectWithWrapper<{{v8_class}}>(impl.release(), &{{v8_class}}Constructor::wrapperTypeInfo, wrapper, info.GetIsolate(), WrapperConfiguration::Dependent);
    v8SetReturnValue(info, wrapper);
}
{% endmacro %}
