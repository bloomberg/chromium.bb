{% extends 'interface_base.cpp' %}


{##############################################################################}
{% block indexed_property_getter %}
{% if indexed_property_getter and not indexed_property_getter.is_custom %}
{% set getter = indexed_property_getter %}
static void indexedPropertyGetter(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    {{cpp_class}}* impl = {{v8_class}}::toImpl(info.Holder());
    {% if getter.is_raises_exception %}
    ExceptionState exceptionState(ExceptionState::IndexedGetterContext, "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {% if getter.is_call_with_script_state %}
    ScriptState* scriptState = ScriptState::current(info.GetIsolate());
    {% endif %}
    {% set getter_name = getter.name or 'anonymousIndexedGetter' %}
    {% set getter_arguments = ['index'] %}
    {% if getter.is_call_with_script_state %}
    {% set getter_arguments = ['scriptState'] + getter_arguments %}
    {% endif %}
    {% if getter.is_raises_exception %}
    {% set getter_arguments = getter_arguments + ['exceptionState'] %}
    {% endif %}
    {{getter.cpp_type}} result = impl->{{getter_name}}({{getter_arguments | join(', ')}});
    {% if getter.is_raises_exception %}
    if (exceptionState.throwIfNeeded())
        return;
    {% endif %}
    if ({{getter.is_null_expression}})
        return;
    {{getter.v8_set_return_value}};
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block indexed_property_getter_callback %}
{% if indexed_property_getter %}
{% set getter = indexed_property_getter %}
static void indexedPropertyGetterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("blink", "DOMIndexedProperty");
    {% if getter.is_custom %}
    {{v8_class}}::indexedPropertyGetterCustom(index, info);
    {% else %}
    {{cpp_class}}V8Internal::indexedPropertyGetter(index, info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("v8", "V8Execution");
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block indexed_property_setter %}
{% from 'conversions.cpp' import v8_value_to_local_cpp_value %}
{% if indexed_property_setter and not indexed_property_setter.is_custom %}
{% set setter = indexed_property_setter %}
static void indexedPropertySetter(uint32_t index, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    {{cpp_class}}* impl = {{v8_class}}::toImpl(info.Holder());
    {{v8_value_to_local_cpp_value(setter) | indent}}
    {% if setter.has_exception_state %}
    ExceptionState exceptionState(ExceptionState::IndexedSetterContext, "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {% if setter.has_type_checking_interface %}
    {# Type checking for interface types (if interface not implemented, throw
       TypeError), per http://www.w3.org/TR/WebIDL/#es-interface #}
    if (!propertyValue{% if setter.is_nullable %} && !isUndefinedOrNull(v8Value){% endif %}) {
        exceptionState.throwTypeError("The provided value is not of type '{{setter.idl_type}}'.");
        exceptionState.throwIfNeeded();
        return;
    }
    {% endif %}
    {% if setter.is_call_with_script_state %}
    ScriptState* scriptState = ScriptState::current(info.GetIsolate());
    {% endif %}
    {% set setter_name = setter.name or 'anonymousIndexedSetter' %}
    {% set setter_arguments = ['index', 'propertyValue'] %}
    {% if setter.is_call_with_script_state %}
    {% set setter_arguments = ['scriptState'] + setter_arguments %}
    {% endif %}
    {% if setter.is_raises_exception %}
    {% set setter_arguments = setter_arguments + ['exceptionState'] %}
    {% endif %}
    bool result = impl->{{setter_name}}({{setter_arguments | join(', ')}});
    {% if setter.is_raises_exception %}
    if (exceptionState.throwIfNeeded())
        return;
    {% endif %}
    if (!result)
        return;
    v8SetReturnValue(info, v8Value);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block indexed_property_setter_callback %}
{% if indexed_property_setter %}
{% set setter = indexed_property_setter %}
static void indexedPropertySetterCallback(uint32_t index, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("blink", "DOMIndexedProperty");
    {% if setter.is_custom %}
    {{v8_class}}::indexedPropertySetterCustom(index, v8Value, info);
    {% else %}
    {{cpp_class}}V8Internal::indexedPropertySetter(index, v8Value, info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("v8", "V8Execution");
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block indexed_property_deleter %}
{% if indexed_property_deleter and not indexed_property_deleter.is_custom %}
{% set deleter = indexed_property_deleter %}
static void indexedPropertyDeleter(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info)
{
    {{cpp_class}}* impl = {{v8_class}}::toImpl(info.Holder());
    {% if deleter.is_raises_exception %}
    ExceptionState exceptionState(ExceptionState::IndexedDeletionContext, "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {% if deleter.is_call_with_script_state %}
    ScriptState* scriptState = ScriptState::current(info.GetIsolate());
    {% endif %}
    {% set deleter_name = deleter.name or 'anonymousIndexedDeleter' %}
    {% set deleter_arguments = ['index'] %}
    {% if deleter.is_call_with_script_state %}
    {% set deleter_arguments = ['scriptState'] + deleter_arguments %}
    {% endif %}
    {% if deleter.is_raises_exception %}
    {% set deleter_arguments = deleter_arguments + ['exceptionState'] %}
    {% endif %}
    DeleteResult result = impl->{{deleter_name}}({{deleter_arguments | join(', ')}});
    {% if deleter.is_raises_exception %}
    if (exceptionState.throwIfNeeded())
        return;
    {% endif %}
    if (result != DeleteUnknownProperty)
        return v8SetReturnValueBool(info, result == DeleteSuccess);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block indexed_property_deleter_callback %}
{% if indexed_property_deleter %}
{% set deleter = indexed_property_deleter %}
static void indexedPropertyDeleterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("blink", "DOMIndexedProperty");
    {% if deleter.is_custom %}
    {{v8_class}}::indexedPropertyDeleterCustom(index, info);
    {% else %}
    {{cpp_class}}V8Internal::indexedPropertyDeleter(index, info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("v8", "V8Execution");
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_property_getter %}
{% if named_property_getter and not named_property_getter.is_custom %}
{% set getter = named_property_getter %}
static void namedPropertyGetter(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    if (!name->IsString())
        return;
    auto nameString = name.As<v8::String>();
    {% if not is_override_builtins %}
    if (info.Holder()->HasRealNamedProperty(nameString))
        return;
    if (!info.Holder()->GetRealNamedPropertyInPrototypeChain(nameString).IsEmpty())
        return;

    {% endif %}
    {{cpp_class}}* impl = {{v8_class}}::toImpl(info.Holder());
    AtomicString propertyName = toCoreAtomicString(nameString);
    {% if getter.is_raises_exception %}
    v8::String::Utf8Value namedProperty(nameString);
    ExceptionState exceptionState(ExceptionState::GetterContext, *namedProperty, "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {% if getter.is_call_with_script_state %}
    ScriptState* scriptState = ScriptState::current(info.GetIsolate());
    {% endif %}
    {% if getter.use_output_parameter_for_result %}
    {{getter.cpp_type}} result;
    {{getter.cpp_value}};
    {% else %}
    {{getter.cpp_type}} result = {{getter.cpp_value}};
    {% endif %}
    {% if getter.is_raises_exception %}
    if (exceptionState.throwIfNeeded())
        return;
    {% endif %}
    if ({{getter.is_null_expression}})
        return;
    {{getter.v8_set_return_value}};
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_property_getter_callback %}
{% if named_property_getter %}
{% set getter = named_property_getter %}
static void namedPropertyGetterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("blink", "DOMNamedProperty");
    {% if getter.is_custom %}
    {{v8_class}}::namedPropertyGetterCustom(name, info);
    {% else %}
    {{cpp_class}}V8Internal::namedPropertyGetter(name, info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("v8", "V8Execution");
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_property_setter %}
{% from 'conversions.cpp' import v8_value_to_local_cpp_value %}
{% if named_property_setter and not named_property_setter.is_custom %}
{% set setter = named_property_setter %}
static void namedPropertySetter(v8::Local<v8::Name> name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    if (!name->IsString())
        return;
    auto nameString = name.As<v8::String>();
    {% if not is_override_builtins %}
    if (info.Holder()->HasRealNamedProperty(nameString))
        return;
    if (!info.Holder()->GetRealNamedPropertyInPrototypeChain(nameString).IsEmpty())
        return;

    {% endif %}
    {% if setter.has_exception_state %}
    v8::String::Utf8Value namedProperty(nameString);
    ExceptionState exceptionState(ExceptionState::SetterContext, *namedProperty, "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {{cpp_class}}* impl = {{v8_class}}::toImpl(info.Holder());
    {# v8_value_to_local_cpp_value('DOMString', 'nameString', 'propertyName') #}
    V8StringResource<> propertyName(nameString);
    if (!propertyName.prepare())
        return;
    {{v8_value_to_local_cpp_value(setter) | indent}}
    {% if setter.has_type_checking_interface %}
    {# Type checking for interface types (if interface not implemented, throw
       TypeError), per http://www.w3.org/TR/WebIDL/#es-interface #}
    if (!propertyValue{% if setter.is_nullable %} && !isUndefinedOrNull(v8Value){% endif %}) {
        exceptionState.throwTypeError("The provided value is not of type '{{setter.idl_type}}'.");
        exceptionState.throwIfNeeded();
        return;
    }
    {% endif %}
    {% if setter.is_call_with_script_state %}
    ScriptState* scriptState = ScriptState::current(info.GetIsolate());
    {% endif %}
    {% set setter_name = setter.name or 'anonymousNamedSetter' %}
    {% set setter_arguments = ['propertyName', 'propertyValue'] %}
    {% if setter.is_call_with_script_state %}
    {% set setter_arguments = ['scriptState'] + setter_arguments %}
    {% endif %}
    {% if setter.is_raises_exception %}
    {% set setter_arguments =  setter_arguments + ['exceptionState'] %}
    {% endif %}
    bool result = impl->{{setter_name}}({{setter_arguments | join(', ')}});
    {% if setter.is_raises_exception %}
    if (exceptionState.throwIfNeeded())
        return;
    {% endif %}
    if (!result)
        return;
    v8SetReturnValue(info, v8Value);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_property_setter_callback %}
{% if named_property_setter %}
{% set setter = named_property_setter %}
static void namedPropertySetterCallback(v8::Local<v8::Name> name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("blink", "DOMNamedProperty");
    {% if setter.is_custom %}
    {{v8_class}}::namedPropertySetterCustom(name, v8Value, info);
    {% else %}
    {{cpp_class}}V8Internal::namedPropertySetter(name, v8Value, info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("v8", "V8Execution");
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_property_query %}
{% if named_property_getter and named_property_getter.is_enumerable and
      not named_property_getter.is_custom_property_query %}
{# If there is an enumerator, there MUST be a query method to properly
   communicate property attributes. #}
static void namedPropertyQuery(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Integer>& info)
{
    if (!name->IsString())
        return;
    {{cpp_class}}* impl = {{v8_class}}::toImpl(info.Holder());
    AtomicString propertyName = toCoreAtomicString(name.As<v8::String>());
    v8::String::Utf8Value namedProperty(name);
    ExceptionState exceptionState(ExceptionState::GetterContext, *namedProperty, "{{interface_name}}", info.Holder(), info.GetIsolate());
    bool result = impl->namedPropertyQuery(propertyName, exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    if (!result)
        return;
    v8SetReturnValueInt(info, v8::None);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_property_query_callback %}
{% if named_property_getter and named_property_getter.is_enumerable %}
{% set getter = named_property_getter %}
static void namedPropertyQueryCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Integer>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("blink", "DOMNamedProperty");
    {% if getter.is_custom_property_query %}
    {{v8_class}}::namedPropertyQueryCustom(name, info);
    {% else %}
    {{cpp_class}}V8Internal::namedPropertyQuery(name, info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("v8", "V8Execution");
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_property_deleter %}
{% if named_property_deleter and not named_property_deleter.is_custom %}
{% set deleter = named_property_deleter %}
static void namedPropertyDeleter(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Boolean>& info)
{
    if (!name->IsString())
        return;
    {{cpp_class}}* impl = {{v8_class}}::toImpl(info.Holder());
    AtomicString propertyName = toCoreAtomicString(name.As<v8::String>());
    {% if deleter.is_raises_exception %}
    v8::String::Utf8Value namedProperty(name);
    ExceptionState exceptionState(ExceptionState::DeletionContext, *namedProperty, "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {% if deleter.is_call_with_script_state %}
    ScriptState* scriptState = ScriptState::current(info.GetIsolate());
    {% endif %}
    {% set deleter_name = deleter.name or 'anonymousNamedDeleter' %}
    {% set deleter_arguments = ['propertyName'] %}
    {% if deleter.is_call_with_script_state %}
    {% set deleter_arguments = ['scriptState'] + deleter_arguments %}
    {% endif %}
    {% if deleter.is_raises_exception %}
    {% set deleter_arguments = deleter_arguments + ['exceptionState'] %}
    {% endif %}
    DeleteResult result = impl->{{deleter_name}}({{deleter_arguments | join(', ')}});
    {% if deleter.is_raises_exception %}
    if (exceptionState.throwIfNeeded())
        return;
    {% endif %}
    if (result != DeleteUnknownProperty)
        return v8SetReturnValueBool(info, result == DeleteSuccess);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_property_deleter_callback %}
{% if named_property_deleter %}
{% set deleter = named_property_deleter %}
static void namedPropertyDeleterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Boolean>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("blink", "DOMNamedProperty");
    {% if deleter.is_custom %}
    {{v8_class}}::namedPropertyDeleterCustom(name, info);
    {% else %}
    {{cpp_class}}V8Internal::namedPropertyDeleter(name, info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("v8", "V8Execution");
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_property_enumerator %}
{% if named_property_getter and named_property_getter.is_enumerable and
      not named_property_getter.is_custom_property_enumerator %}
static void namedPropertyEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info)
{
    {{cpp_class}}* impl = {{v8_class}}::toImpl(info.Holder());
    Vector<String> names;
    ExceptionState exceptionState(ExceptionState::EnumerationContext, "{{interface_name}}", info.Holder(), info.GetIsolate());
    impl->namedPropertyEnumerator(names, exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    v8::Local<v8::Array> v8names = v8::Array::New(info.GetIsolate(), names.size());
    for (size_t i = 0; i < names.size(); ++i)
        v8names->Set(v8::Integer::New(info.GetIsolate(), i), v8String(info.GetIsolate(), names[i]));
    v8SetReturnValue(info, v8names);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_property_enumerator_callback %}
{% if named_property_getter and named_property_getter.is_enumerable %}
{% set getter = named_property_getter %}
static void namedPropertyEnumeratorCallback(const v8::PropertyCallbackInfo<v8::Array>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("blink", "DOMNamedProperty");
    {% if getter.is_custom_property_enumerator %}
    {{v8_class}}::namedPropertyEnumeratorCustom(info);
    {% else %}
    {{cpp_class}}V8Internal::namedPropertyEnumerator(info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("v8", "V8Execution");
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block origin_safe_method_setter %}
{% if has_origin_safe_method_setter %}
static void {{cpp_class}}OriginSafeMethodSetter(v8::Local<v8::Name> name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<void>& info)
{
    v8::Local<v8::Object> holder = {{v8_class}}::findInstanceInPrototypeChain(info.This(), info.GetIsolate());
    if (holder.IsEmpty())
        return;
    {{cpp_class}}* impl = {{v8_class}}::toImpl(holder);
    v8::String::Utf8Value attributeName(name);
    ExceptionState exceptionState(ExceptionState::SetterContext, *attributeName, "{{interface_name}}", info.Holder(), info.GetIsolate());
    if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), impl->frame(), exceptionState)) {
        exceptionState.throwIfNeeded();
        return;
    }

    {# The findInstanceInPrototypeChain() call above only returns a non-empty handle if info.This() is an Object. #}
    V8HiddenValue::setHiddenValue(info.GetIsolate(), v8::Local<v8::Object>::Cast(info.This()), name.As<v8::String>(), v8Value);
}

static void {{cpp_class}}OriginSafeMethodSetterCallback(v8::Local<v8::Name> name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<void>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("blink", "DOMSetter");
    {{cpp_class}}V8Internal::{{cpp_class}}OriginSafeMethodSetter(name, v8Value, info);
    TRACE_EVENT_SET_SAMPLING_STATE("v8", "V8Execution");
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_constructor %}
{% from 'methods.cpp' import generate_constructor with context %}
{% if named_constructor %}
{% set to_active_dom_object = '%s::toActiveDOMObject' % v8_class
                              if is_active_dom_object else '0' %}
const WrapperTypeInfo {{v8_class}}Constructor::wrapperTypeInfo = { gin::kEmbedderBlink, {{v8_class}}Constructor::domTemplate, {{v8_class}}::refObject, {{v8_class}}::derefObject, {{v8_class}}::trace, {{to_active_dom_object}}, 0, {{v8_class}}::installConditionallyEnabledMethods, {{v8_class}}::installConditionallyEnabledProperties, 0, WrapperTypeInfo::WrapperTypeObjectPrototype, WrapperTypeInfo::{{wrapper_class_id}}, WrapperTypeInfo::{{event_target_inheritance}}, WrapperTypeInfo::{{lifetime}}, WrapperTypeInfo::{{gc_type}} };

{{generate_constructor(named_constructor)}}
v8::Local<v8::FunctionTemplate> {{v8_class}}Constructor::domTemplate(v8::Isolate* isolate)
{
    static int domTemplateKey; // This address is used for a key to look up the dom template.
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    v8::Local<v8::FunctionTemplate> result = data->existingDOMTemplate(&domTemplateKey);
    if (!result.IsEmpty())
        return result;

    TRACE_EVENT_SCOPED_SAMPLING_STATE("blink", "BuildDOMTemplate");
    result = v8::FunctionTemplate::New(isolate, {{v8_class}}ConstructorCallback);
    v8::Local<v8::ObjectTemplate> instanceTemplate = result->InstanceTemplate();
    instanceTemplate->SetInternalFieldCount({{v8_class}}::internalFieldCount);
    result->SetClassName(v8AtomicString(isolate, "{{cpp_class}}"));
    result->Inherit({{v8_class}}::domTemplate(isolate));
    data->setDOMTemplate(&domTemplateKey, result);
    return result;
}

{% endif %}
{% endblock %}

{##############################################################################}
{% block overloaded_constructor %}
{% if constructor_overloads %}
static void constructor(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "{{interface_name}}", info.Holder(), info.GetIsolate());
    {# 2. Initialize argcount to be min(maxarg, n). #}
    switch (std::min({{constructor_overloads.maxarg}}, info.Length())) {
    {# 3. Remove from S all entries whose type list is not of length argcount. #}
    {% for length, tests_constructors in constructor_overloads.length_tests_methods %}
    case {{length}}:
        {# Then resolve by testing argument #}
        {% for test, constructor in tests_constructors %}
        {# 10. If i = d, then: #}
        if ({{test}}) {
            {{cpp_class}}V8Internal::constructor{{constructor.overload_index}}(info);
            return;
        }
        {% endfor %}
        break;
    {% endfor %}
    default:
        {# Invalid arity, throw error #}
        {# Report full list of valid arities if gaps and above minimum #}
        {% if constructor_overloads.valid_arities %}
        if (info.Length() >= {{constructor_overloads.minarg}}) {
            throwArityTypeError(exceptionState, "{{constructor_overloads.valid_arities}}", info.Length());
            return;
        }
        {% endif %}
        {# Otherwise just report "not enough arguments" #}
        exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments({{constructor_overloads.minarg}}, info.Length()));
        exceptionState.throwIfNeeded();
        return;
    }
    {# No match, throw error #}
    exceptionState.throwTypeError("No matching constructor signature.");
    exceptionState.throwIfNeeded();
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block event_constructor %}
{% if has_event_constructor %}
static void constructor(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "{{interface_name}}", info.Holder(), info.GetIsolate());
    if (info.Length() < 1) {
        exceptionState.throwTypeError("An event name must be provided.");
        exceptionState.throwIfNeeded();
        return;
    }

    V8StringResource<> type(info[0]);
    if (!type.prepare())
        return;
    {% for attribute in any_type_attributes %}
    v8::Local<v8::Value> {{attribute.name}};
    {% endfor %}
    {{cpp_class}}Init eventInit;
    if (info.Length() >= 2) {
        Dictionary options(info[1], info.GetIsolate(), exceptionState);
        if (!initialize{{cpp_class}}(eventInit, options, exceptionState, info)) {
            exceptionState.throwIfNeeded();
            return;
        }
        {# Store attributes of type |any| on the wrapper to avoid leaking them
           between isolated worlds. #}
        {% for attribute in any_type_attributes %}
        options.get("{{attribute.name}}", {{attribute.name}});
        if (!{{attribute.name}}.IsEmpty())
            V8HiddenValue::setHiddenValue(info.GetIsolate(), info.Holder(), v8AtomicString(info.GetIsolate(), "{{attribute.name}}"), {{attribute.name}});
        {% endfor %}
    }
    {% if is_constructor_raises_exception %}
    RefPtrWillBeRawPtr<{{cpp_class}}> event = {{cpp_class}}::create(type, eventInit, exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    {% else %}
    RefPtrWillBeRawPtr<{{cpp_class}}> event = {{cpp_class}}::create(type, eventInit);
    {% endif %}
    {% if any_type_attributes and not interface_name == 'ErrorEvent' %}
    {# If we're in an isolated world, create a SerializedScriptValue and store
       it in the event for later cloning if the property is accessed from
       another world. The main world case is handled lazily (in custom code).

       We do not clone Error objects (exceptions), for 2 reasons:
       1) Errors carry a reference to the isolated world's global object, and
          thus passing it around would cause leakage.
       2) Errors cannot be cloned (or serialized):
       http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#safe-passing-of-structured-data #}
    if (DOMWrapperWorld::current(info.GetIsolate()).isIsolatedWorld()) {
        {% for attribute in any_type_attributes %}
        if (!{{attribute.name}}.IsEmpty())
            event->setSerialized{{attribute.name | blink_capitalize}}(SerializedScriptValueFactory::instance().createAndSwallowExceptions(info.GetIsolate(), {{attribute.name}}));
        {% endfor %}
    }

    {% endif %}
    v8::Local<v8::Object> wrapper = info.Holder();
    event->associateWithWrapper(info.GetIsolate(), &{{v8_class}}::wrapperTypeInfo, wrapper);
    v8SetReturnValue(info, wrapper);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block visit_dom_wrapper %}
{% if reachable_node_function or set_wrapper_reference_to_list %}
void {{v8_class}}::visitDOMWrapper(v8::Isolate* isolate, ScriptWrappable* scriptWrappable, const v8::Persistent<v8::Object>& wrapper)
{
    {{cpp_class}}* impl = scriptWrappable->toImpl<{{cpp_class}}>();
    {% if set_wrapper_reference_to_list %}
    v8::Local<v8::Object> creationContext = v8::Local<v8::Object>::New(isolate, wrapper);
    V8WrapperInstantiationScope scope(creationContext, isolate);
    {% for set_wrapper_reference_to in set_wrapper_reference_to_list %}
    {{set_wrapper_reference_to.cpp_type}} {{set_wrapper_reference_to.name}} = impl->{{set_wrapper_reference_to.name}}();
    if ({{set_wrapper_reference_to.name}}) {
        if (!DOMDataStore::containsWrapper({{set_wrapper_reference_to.name}}, isolate))
            {{set_wrapper_reference_to.name}}->wrap(creationContext, isolate);
        DOMDataStore::setWrapperReference(wrapper, {{set_wrapper_reference_to.name}}, isolate);
    }
    {% endfor %}
    {% endif %}
    {% if reachable_node_function %}
    // The {{reachable_node_function}}() method may return a reference or a pointer.
    if (Node* owner = WTF::getPtr(impl->{{reachable_node_function}}())) {
        Node* root = V8GCController::opaqueRootForGC(isolate, owner);
        isolate->SetReferenceFromGroup(v8::UniqueId(reinterpret_cast<intptr_t>(root)), wrapper);
        return;
    }
    {% endif %}
    setObjectGroup(isolate, scriptWrappable, wrapper);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block shadow_attributes %}
{% from 'attributes.cpp' import attribute_configuration with context %}
{% if interface_name == 'Window' %}
static const V8DOMConfiguration::AttributeConfiguration shadowAttributes[] = {
    {% for attribute in attributes if attribute.is_unforgeable and attribute.should_be_exposed_to_script %}
    {{attribute_configuration(attribute)}},
    {% endfor %}
};

{% endif %}
{% endblock %}


{##############################################################################}
{% block initialize_event %}
{% if has_event_constructor %}
bool initialize{{cpp_class}}({{cpp_class}}Init& eventInit, const Dictionary& options, ExceptionState& exceptionState, const v8::FunctionCallbackInfo<v8::Value>& info)
{
    Dictionary::ConversionContext conversionContext(exceptionState);
    {% if parent_interface %}{# any Event interface except Event itself #}
    if (!initialize{{parent_interface}}(eventInit, options, exceptionState, info))
        return false;

    {% endif %}
    {% for attribute in attributes
           if (attribute.is_initialized_by_event_constructor and
               not attribute.idl_type == 'any')%}
    {% set is_nullable = 'true' if attribute.is_nullable else 'false' %}
    {% if attribute.deprecate_as %}
    if (DictionaryHelper::convert(options, conversionContext.setConversionType("{{attribute.idl_type}}", {{is_nullable}}), "{{attribute.name}}", eventInit.{{attribute.cpp_name}})) {
        if (options.hasProperty("{{attribute.name}}"))
            UseCounter::countDeprecationIfNotPrivateScript(info.GetIsolate(), callingExecutionContext(info.GetIsolate()), UseCounter::{{attribute.deprecate_as}});
    } else {
        return false;
    }
    {% else %}
    if (!DictionaryHelper::convert(options, conversionContext.setConversionType("{{attribute.idl_type}}", {{is_nullable}}), "{{attribute.name}}", eventInit.{{attribute.cpp_name}}))
        return false;
    {% endif %}
    {% endfor %}
    return true;
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block constructor_callback %}
{% if constructors or has_custom_constructor or has_event_constructor %}
void {{v8_class}}::constructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SCOPED_SAMPLING_STATE("blink", "DOMConstructor");
    {% if measure_as %}
    UseCounter::countIfNotPrivateScript(info.GetIsolate(), callingExecutionContext(info.GetIsolate()), UseCounter::{{measure_as('Constructor')}});
    {% endif %}
    if (!info.IsConstructCall()) {
        V8ThrowException::throwTypeError(info.GetIsolate(), ExceptionMessages::constructorNotCallableAsFunction("{{interface_name}}"));
        return;
    }

    if (ConstructorMode::current(info.GetIsolate()) == ConstructorMode::WrapExistingObject) {
        v8SetReturnValue(info, info.Holder());
        return;
    }

    {% if has_custom_constructor %}
    {{v8_class}}::constructorCustom(info);
    {% else %}
    {{cpp_class}}V8Internal::constructor(info);
    {% endif %}
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block configure_shadow_object_template %}
{% if interface_name == 'Window' %}
static void configureShadowObjectTemplate(v8::Local<v8::ObjectTemplate> templ, v8::Isolate* isolate)
{
    V8DOMConfiguration::installAttributes(isolate, templ, v8::Local<v8::ObjectTemplate>(), shadowAttributes, WTF_ARRAY_LENGTH(shadowAttributes));

    // Install a security handler with V8.
    templ->SetAccessCheckCallbacks(V8Window::namedSecurityCheckCustom, V8Window::indexedSecurityCheckCustom, v8::External::New(isolate, const_cast<WrapperTypeInfo*>(&V8Window::wrapperTypeInfo)));
    templ->SetInternalFieldCount(V8Window::internalFieldCount);
}

{% endif %}
{% endblock %}


{######################################}
{% macro install_do_not_check_security_signature(method, world_suffix) %}
{# Methods that are [DoNotCheckSecurity] are always readable, but if they are
   changed and then accessed from a different origin, we do not return the
   underlying value, but instead return a new copy of the original function.
   This is achieved by storing the changed value as a hidden property. #}
{% set getter_callback =
       '%sV8Internal::%sOriginSafeMethodGetterCallback%s' %
       (cpp_class, method.name, world_suffix) %}
{% set setter_callback =
    '{0}V8Internal::{0}OriginSafeMethodSetterCallback'.format(cpp_class)
    if not method.is_read_only else '0' %}
{% if method.is_per_world_bindings %}
{% set getter_callback_for_main_world = '%sForMainWorld' % getter_callback %}
{% set setter_callback_for_main_world = '%sForMainWorld' % setter_callback
   if not method.is_read_only else '0' %}
{% else %}
{% set getter_callback_for_main_world = '0' %}
{% set setter_callback_for_main_world = '0' %}
{% endif %}
{% set property_attribute =
    'static_cast<v8::PropertyAttribute>(%s)' %
    ' | '.join(method.property_attributes or ['v8::DontDelete']) %}
{% set only_exposed_to_private_script = 'V8DOMConfiguration::OnlyExposedToPrivateScript' if method.only_exposed_to_private_script else 'V8DOMConfiguration::ExposedToAllScripts' %}
static const V8DOMConfiguration::AttributeConfiguration {{method.name}}OriginSafeAttributeConfiguration = {
    "{{method.name}}", {{getter_callback}}, {{setter_callback}}, {{getter_callback_for_main_world}}, {{setter_callback_for_main_world}}, &{{v8_class}}::wrapperTypeInfo, v8::ALL_CAN_READ, {{property_attribute}}, {{only_exposed_to_private_script}}, V8DOMConfiguration::OnInstance,
};
V8DOMConfiguration::installAttribute(isolate, {{method.function_template}}, v8::Local<v8::ObjectTemplate>(), {{method.name}}OriginSafeAttributeConfiguration);
{%- endmacro %}


{##############################################################################}
{% block get_dom_template %}
{% if not is_array_buffer_or_view %}
v8::Local<v8::FunctionTemplate> {{v8_class}}::domTemplate(v8::Isolate* isolate)
{
    {% if has_partial_interface %}
    {% set installTemplateFunction = '%s::install%sTemplateFunction' % (v8_class, v8_class) %}
    ASSERT({{installTemplateFunction}} != {{v8_class}}::install{{v8_class}}Template);
    {% else %}
    {% set installTemplateFunction = 'install%sTemplate' % v8_class %}
    {% endif %}
{% set installTemplateFunction = '%s::install%sTemplateFunction' % (v8_class, v8_class) if has_partial_interface else 'install%sTemplate' % v8_class %}
    return V8DOMConfiguration::domClassTemplate(isolate, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), {{installTemplateFunction}});
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block has_instance %}
bool {{v8_class}}::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate)
{
    {% if is_array_buffer_or_view %}
    return v8Value->Is{{interface_name}}();
    {% else %}
    return V8PerIsolateData::from(isolate)->hasInstance(&wrapperTypeInfo, v8Value);
    {% endif %}
}

{% if not is_array_buffer_or_view %}
v8::Local<v8::Object> {{v8_class}}::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate)
{
    return V8PerIsolateData::from(isolate)->findInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block to_impl %}
{% if interface_name == 'ArrayBuffer' %}
{{cpp_class}}* V8ArrayBuffer::toImpl(v8::Local<v8::Object> object)
{
    ASSERT(object->IsArrayBuffer());
    v8::Local<v8::ArrayBuffer> v8buffer = object.As<v8::ArrayBuffer>();
    if (v8buffer->IsExternal()) {
        const WrapperTypeInfo* wrapperTypeInfo = toWrapperTypeInfo(object);
        RELEASE_ASSERT(wrapperTypeInfo);
        RELEASE_ASSERT(wrapperTypeInfo->ginEmbedder == gin::kEmbedderBlink);
        return toScriptWrappable(object)->toImpl<{{cpp_class}}>();
    }

    // Transfer the ownership of the allocated memory to an ArrayBuffer without
    // copying.
    v8::ArrayBuffer::Contents v8Contents = v8buffer->Externalize();
    WTF::ArrayBufferContents contents(v8Contents.Data(), v8Contents.ByteLength(), 0);
    RefPtr<{{cpp_class}}> buffer = {{cpp_class}}::create(contents);
    // Since this transfer doesn't allocate new memory, do not call
    // DOMArrayBufferDeallocationObserver::blinkAllocatedMemory.
    buffer->buffer()->setDeallocationObserverWithoutAllocationNotification(
        DOMArrayBufferDeallocationObserver::instance());
    buffer->associateWithWrapper(v8::Isolate::GetCurrent(), buffer->wrapperTypeInfo(), object);

    return buffer.get();
}

{% elif interface_name == 'ArrayBufferView' %}
{{cpp_class}}* V8ArrayBufferView::toImpl(v8::Local<v8::Object> object)
{
    ASSERT(object->IsArrayBufferView());
    ScriptWrappable* scriptWrappable = toScriptWrappable(object);
    if (scriptWrappable)
        return scriptWrappable->toImpl<{{cpp_class}}>();

    if (object->IsInt8Array())
        return V8Int8Array::toImpl(object);
    if (object->IsInt16Array())
        return V8Int16Array::toImpl(object);
    if (object->IsInt32Array())
        return V8Int32Array::toImpl(object);
    if (object->IsUint8Array())
        return V8Uint8Array::toImpl(object);
    if (object->IsUint8ClampedArray())
        return V8Uint8ClampedArray::toImpl(object);
    if (object->IsUint16Array())
        return V8Uint16Array::toImpl(object);
    if (object->IsUint32Array())
        return V8Uint32Array::toImpl(object);
    if (object->IsFloat32Array())
        return V8Float32Array::toImpl(object);
    if (object->IsFloat64Array())
        return V8Float64Array::toImpl(object);
    if (object->IsDataView())
        return V8DataView::toImpl(object);

    ASSERT_NOT_REACHED();
    return 0;
}

{% elif is_array_buffer_or_view %}
{{cpp_class}}* {{v8_class}}::toImpl(v8::Local<v8::Object> object)
{
    ASSERT(object->Is{{interface_name}}());
    ScriptWrappable* scriptWrappable = toScriptWrappable(object);
    if (scriptWrappable)
        return scriptWrappable->toImpl<{{cpp_class}}>();

    v8::Local<v8::{{interface_name}}> v8View = object.As<v8::{{interface_name}}>();
    RefPtr<{{cpp_class}}> typedArray = {{cpp_class}}::create(V8ArrayBuffer::toImpl(v8View->Buffer()), v8View->ByteOffset(), v8View->{% if interface_name == 'DataView' %}Byte{% endif %}Length());
    typedArray->associateWithWrapper(v8::Isolate::GetCurrent(), typedArray->wrapperTypeInfo(), object);

    return typedArray->toImpl<{{cpp_class}}>();
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block to_impl_with_type_check %}
{{cpp_class}}* {{v8_class}}::toImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value)
{
    return hasInstance(value, isolate) ? toImpl(v8::Local<v8::Object>::Cast(value)) : 0;
}

{% endblock %}


{##############################################################################}
{% block install_conditional_attributes %}
{% from 'attributes.cpp' import attribute_configuration with context %}
{% if has_conditional_attributes %}
void {{v8_class}}::installConditionallyEnabledProperties(v8::Local<v8::Object> instanceObject, v8::Isolate* isolate)
{
    v8::Local<v8::Object> prototypeObject = v8::Local<v8::Object>::Cast(instanceObject->GetPrototype());
    ExecutionContext* context = toExecutionContext(prototypeObject->CreationContext());

    {% for attribute in attributes if attribute.per_context_enabled_function or attribute.exposed_test %}
    {% filter per_context_enabled(attribute.per_context_enabled_function) %}
    {% filter exposed(attribute.exposed_test) %}
    {% if attribute.is_expose_js_accessors %}
    static const V8DOMConfiguration::AccessorConfiguration accessorConfiguration = {{attribute_configuration(attribute)}};
    V8DOMConfiguration::installAccessor(isolate, prototypeObject, accessorConfiguration);
    {% else %}
    static const V8DOMConfiguration::AttributeConfiguration attributeConfiguration = {{attribute_configuration(attribute)}};
    V8DOMConfiguration::installAttribute(isolate, instanceObject, prototypeObject, attributeConfiguration);
    {% endif %}
    {% endfilter %}
    {% endfilter %}
    {% endfor %}
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block to_active_dom_object %}
{% if is_active_dom_object %}
ActiveDOMObject* {{v8_class}}::toActiveDOMObject(v8::Local<v8::Object> wrapper)
{
    return toImpl(wrapper);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block get_shadow_object_template %}
{% if interface_name == 'Window' %}
v8::Local<v8::ObjectTemplate> V8Window::getShadowObjectTemplate(v8::Isolate* isolate)
{
    if (DOMWrapperWorld::current(isolate).isMainWorld()) {
        DEFINE_STATIC_LOCAL(v8::Persistent<v8::ObjectTemplate>, V8WindowShadowObjectCacheForMainWorld, ());
        if (V8WindowShadowObjectCacheForMainWorld.IsEmpty()) {
            TRACE_EVENT_SCOPED_SAMPLING_STATE("blink", "BuildDOMTemplate");
            v8::Local<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(isolate);
            configureShadowObjectTemplate(templ, isolate);
            V8WindowShadowObjectCacheForMainWorld.Reset(isolate, templ);
            return templ;
        }
        return v8::Local<v8::ObjectTemplate>::New(isolate, V8WindowShadowObjectCacheForMainWorld);
    } else {
        DEFINE_STATIC_LOCAL(v8::Persistent<v8::ObjectTemplate>, V8WindowShadowObjectCacheForNonMainWorld, ());
        if (V8WindowShadowObjectCacheForNonMainWorld.IsEmpty()) {
            TRACE_EVENT_SCOPED_SAMPLING_STATE("blink", "BuildDOMTemplate");
            v8::Local<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(isolate);
            configureShadowObjectTemplate(templ, isolate);
            V8WindowShadowObjectCacheForNonMainWorld.Reset(isolate, templ);
            return templ;
        }
        return v8::Local<v8::ObjectTemplate>::New(isolate, V8WindowShadowObjectCacheForNonMainWorld);
    }
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block ref_object_and_deref_object %}
void {{v8_class}}::refObject(ScriptWrappable* scriptWrappable)
{
{% if gc_type == 'WillBeGarbageCollectedObject' %}
#if !ENABLE(OILPAN)
    scriptWrappable->toImpl<{{cpp_class}}>()->ref();
#endif
{% elif gc_type == 'RefCountedObject' %}
    scriptWrappable->toImpl<{{cpp_class}}>()->ref();
{% endif %}
}

void {{v8_class}}::derefObject(ScriptWrappable* scriptWrappable)
{
{% if gc_type == 'WillBeGarbageCollectedObject' %}
#if !ENABLE(OILPAN)
    scriptWrappable->toImpl<{{cpp_class}}>()->deref();
#endif
{% elif gc_type == 'RefCountedObject' %}
    scriptWrappable->toImpl<{{cpp_class}}>()->deref();
{% endif %}
}

{% endblock %}

{##############################################################################}
{% block partial_interface %}
{% if has_partial_interface %}
InstallTemplateFunction {{v8_class}}::install{{v8_class}}TemplateFunction = (InstallTemplateFunction)&{{v8_class}}::install{{v8_class}}Template;

void {{v8_class}}::updateWrapperTypeInfo(InstallTemplateFunction installTemplateFunction, InstallConditionallyEnabledMethodsFunction installConditionallyEnabledMethodsFunction)
{
    {{v8_class}}::install{{v8_class}}TemplateFunction = installTemplateFunction;
    if (installConditionallyEnabledMethodsFunction)
        {{v8_class}}::wrapperTypeInfo.installConditionallyEnabledMethodsFunction = installConditionallyEnabledMethodsFunction;
}

{% for method in methods if method.overloads and method.overloads.has_partial_overloads %}
void {{v8_class}}::register{{method.name | blink_capitalize}}MethodForPartialInterface(void (*method)(const v8::FunctionCallbackInfo<v8::Value>&))
{
    {{cpp_class}}V8Internal::{{method.name}}MethodForPartialInterface = method;
}
{% endfor %}
{% endif %}
{% endblock %}
