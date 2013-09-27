{% extends 'interface_base.cpp' %}


{##############################################################################}
{% block class_attributes %}
{# FIXME: rename to install_attributes and put into configure_class_template #}
{% if attributes %}
static const V8DOMConfiguration::AttributeConfiguration {{v8_class_name}}Attributes[] = {
    {% for attribute in attributes if not attribute.is_static %}
    {"{{attribute.name}}", {{cpp_class_name}}V8Internal::{{attribute.name}}AttributeGetterCallback, 0, 0, 0, 0, static_cast<v8::AccessControl>(v8::DEFAULT), static_cast<v8::PropertyAttribute>(v8::None), 0 /* on instance */},
    {% endfor %}
};

{% endif %}
{% endblock %}


{##############################################################################}
{% block configure_class_template %}
{# FIXME: rename to install_dom_template and Install{{v8_class_name}}DOMTemplate #}
static v8::Handle<v8::FunctionTemplate> Configure{{v8_class_name}}Template(v8::Handle<v8::FunctionTemplate> desc, v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    desc->ReadOnlyPrototype();

    v8::Local<v8::Signature> defaultSignature;
    defaultSignature = V8DOMConfiguration::installDOMClassTemplate(desc, "{{interface_name}}", v8::Local<v8::FunctionTemplate>(), {{v8_class_name}}::internalFieldCount,
        {{attribute_templates}}, {{number_of_attributes}},
        0, 0, isolate, currentWorldType);
    UNUSED_PARAM(defaultSignature);
    {% for attribute in attributes if attribute.is_static %}
    desc->SetNativeDataProperty(v8::String::NewSymbol("{{attribute.name}}"), {{cpp_class_name}}V8Internal::{{attribute.name}}AttributeGetterCallback, 0, v8::External::New(0), static_cast<v8::PropertyAttribute>(v8::None), v8::Handle<v8::AccessorSignature>(), static_cast<v8::AccessControl>(v8::DEFAULT));
    {% endfor %}
    {% if constants %}{# In general more checks than just constants #}
    v8::Local<v8::ObjectTemplate> instance = desc->InstanceTemplate();
    v8::Local<v8::ObjectTemplate> proto = desc->PrototypeTemplate();
    UNUSED_PARAM(instance);
    UNUSED_PARAM(proto);
    {% endif %}
    {% if constants %}
    {{install_constants() | indent}}
    {% endif %}

    // Custom toString template
    desc->Set(v8::String::NewSymbol("toString"), V8PerIsolateData::current()->toStringTemplate());
    return desc;
}

{% endblock %}


{######################################}
{% macro install_constants() %}
{# FIXME: should use reflected_name instead of name #}
{# Normal (always enabled) constants #}
static const V8DOMConfiguration::ConstantConfiguration {{v8_class_name}}Constants[] = {
    {% for constant in constants if not constant.enabled_at_runtime %}
    {"{{constant.name}}", {{constant.value}}},
    {% endfor %}
};
V8DOMConfiguration::installConstants(desc, proto, {{v8_class_name}}Constants, WTF_ARRAY_LENGTH({{v8_class_name}}Constants), isolate);
{# Runtime-enabled constants #}
{% for constant in constants if constant.enabled_at_runtime %}
if ({{constant.runtime_enable_function_name}}()) {
    static const V8DOMConfiguration::ConstantConfiguration constantConfiguration = {"{{constant.name}}", static_cast<signed int>({{constant.value}})};
    V8DOMConfiguration::installConstants(desc, proto, &constantConfiguration, 1, isolate);
}
{% endfor %}
{# Check constants #}
{% if not do_not_check_constants %}
{% for constant in constants %}
COMPILE_ASSERT({{constant.value}} == {{cpp_class_name}}::{{constant.reflected_name}}, TheValueOf{{cpp_class_name}}_{{constant.reflected_name}}DoesntMatchWithImplementation);
{% endfor %}
{% endif %}
{% endmacro %}


{##############################################################################}
{% block get_template %}
{# FIXME: rename to get_dom_template and GetDOMTemplate #}
v8::Handle<v8::FunctionTemplate> {{v8_class_name}}::GetTemplate(v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    V8PerIsolateData::TemplateMap::iterator result = data->templateMap(currentWorldType).find(&info);
    if (result != data->templateMap(currentWorldType).end())
        return result->value.newLocal(isolate);

    TRACE_EVENT_SCOPED_SAMPLING_STATE("Blink", "BuildDOMTemplate");
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::FunctionTemplate> templ =
        Configure{{v8_class_name}}Template(data->rawTemplate(&info, currentWorldType), isolate, currentWorldType);
    data->templateMap(currentWorldType).add(&info, UnsafePersistent<v8::FunctionTemplate>(isolate, templ));
    return handleScope.Close(templ);
}

{% endblock %}


{##############################################################################}
{% block has_instance_and_has_instance_in_any_world %}
bool {{v8_class_name}}::HasInstance(v8::Handle<v8::Value> value, v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    return V8PerIsolateData::from(isolate)->hasInstance(&info, value, currentWorldType);
}

bool {{v8_class_name}}::HasInstanceInAnyWorld(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    return V8PerIsolateData::from(isolate)->hasInstance(&info, value, MainWorld)
        || V8PerIsolateData::from(isolate)->hasInstance(&info, value, IsolatedWorld)
        || V8PerIsolateData::from(isolate)->hasInstance(&info, value, WorkerWorld);
}

{% endblock %}


{##############################################################################}
{% block create_wrapper_and_deref_object %}
v8::Handle<v8::Object> {{v8_class_name}}::createWrapper(PassRefPtr<{{cpp_class_name}}> impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl.get());
    ASSERT(!DOMDataStore::containsWrapper<{{v8_class_name}}>(impl.get(), isolate));
    if (ScriptWrappable::wrapperCanBeStoredInObject(impl.get())) {
        const WrapperTypeInfo* actualInfo = ScriptWrappable::getTypeInfoFromObject(impl.get());
        // Might be a XXXConstructor::info instead of an XXX::info. These will both have
        // the same object de-ref functions, though, so use that as the basis of the check.
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(actualInfo->derefObjectFunction == info.derefObjectFunction);
    }

    v8::Handle<v8::Object> wrapper = V8DOMWrapper::createWrapper(creationContext, &info, toInternalPointer(impl.get()), isolate);
    if (UNLIKELY(wrapper.IsEmpty()))
        return wrapper;

    installPerContextProperties(wrapper, impl.get(), isolate);
    V8DOMWrapper::associateObjectWithWrapper<{{v8_class_name}}>(impl, &info, wrapper, isolate, WrapperConfiguration::Independent);
    return wrapper;
}

void {{v8_class_name}}::derefObject(void* object)
{
    fromInternalPointer(object)->deref();
}

{% endblock %}
