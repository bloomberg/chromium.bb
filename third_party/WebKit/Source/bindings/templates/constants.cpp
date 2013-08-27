{# FIXME: remove all the conditional_string blocks once test removed #}
{# FIXME: should use reflected_name instead of name #}
{% macro class_constants() %}
static const V8DOMConfiguration::ConstantConfiguration {{v8_class_name}}Constants[] = {
{% for constant in constants %}
{% if not constant.enabled_at_runtime %}
{% if constant.conditional_string %}
#if {{constant.conditional_string}}
{% endif %}
    {"{{constant.name}}", {{constant.value}}},
{% if constant.conditional_string %}
#endif // {{constant.conditional_string}}
{% endif %}
{% endif %}{# not constant.enabled_at_runtime #}
{% endfor %}
};
{% if not do_not_check_constants %}

{% for constant in constants %}
{% if constant.conditional_string %}
#if {{constant.conditional_string}}
{% endif %}
{# FIXME
{{cpp_class_name}}Enum{{constant.name_reflect}}IsWrongUseDoNotCheckConstants
=>
TheValueOf{{cpp_class_name}}::{{constant.name_reflect}}Doesn'tMatchWithImplementation
#}
COMPILE_ASSERT({{constant.value}} == {{cpp_class_name}}::{{constant.reflected_name}}, {{cpp_class_name}}Enum{{constant.reflected_name}}IsWrongUseDoNotCheckConstants);
{% if constant.conditional_string %}
#endif // {{constant.conditional_string}}
{% endif %}
{% endfor %}
{% endif %}{# not do_not_check_constants #}
{% endmacro %}


{##############################################################################}
{% macro install_constants() %}
{# Runtime-enabled constants #}
{% for constant in constants %}
{% if constant.enabled_at_runtime %}
{% if constant.conditional_string %}
#if {{constant.conditional_string}}
{% endif %}
    if ({{constant.runtime_enable_function_name}}()) {
        static const V8DOMConfiguration::ConstantConfiguration constantConfiguration = {"{{constant.name}}", static_cast<signed int>({{constant.value}})};
        V8DOMConfiguration::installConstants(desc, proto, &constantConfiguration, 1, isolate);
    }
{% if constant.conditional_string %}
#endif // {{constant.conditional_string}}
{%- endif %}
{% endif %}
{% endfor %}
{# Normal (always enabled) constants #}
    V8DOMConfiguration::installConstants(desc, proto, {{v8_class_name}}Constants, WTF_ARRAY_LENGTH({{v8_class_name}}Constants), isolate);
{%- endmacro %}
