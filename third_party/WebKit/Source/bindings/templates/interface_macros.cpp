{# FIXME: should use reflected_name instead of name #}
{% macro install_constants() %}
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
