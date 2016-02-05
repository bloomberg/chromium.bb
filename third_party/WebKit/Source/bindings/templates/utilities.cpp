{% macro v8_value_to_local_cpp_value(thing) %}
{# This indirection is just to avoid spurious white-space lines. #}
{{generate_v8_value_to_local_cpp_value(thing) | trim}}
{%- endmacro %}


{% macro generate_v8_value_to_local_cpp_value(thing) %}
{% set item = thing.v8_value_to_local_cpp_value or thing %}
{% if item.error_message %}
/* {{item.error_message}} */
{% else %}
{% if item.declare_variable %}
{% if item.assign_expression %}
{{item.cpp_type}} {{item.cpp_name}} = {{item.assign_expression}};
{% else %}
{{item.cpp_type}} {{item.cpp_name}};
{% endif %}
{% else %}{# item.declare_variable #}
{% if item.assign_expression %}
{{item.cpp_name}} = {{item.assign_expression}};
{% endif %}
{% endif %}{# item.declare_variable #}
{% if item.set_expression %}
{{item.set_expression}};
{% endif %}
{% if item.check_expression %}
if ({{item.check_expression}})
    return{% if item.return_expression %} {{item.return_expression}}{% endif %};
{% endif %}{# item.check_expression #}
{% endif %}{# item.error_message #}
{% endmacro %}


{% macro declare_enum_validation_variable(enum_values) %}
const char* validValues[] = {
{% for enum_value in enum_values %}
    "{{enum_value}}",
{% endfor %}
};
{%-endmacro %}


{% macro property_location(member) %}
{% set property_location_list = [] %}
{% if member.on_instance %}
{% set property_location_list = property_location_list + ['V8DOMConfiguration::OnInstance'] %}
{% endif %}
{% if member.on_prototype %}
{% set property_location_list = property_location_list + ['V8DOMConfiguration::OnPrototype'] %}
{% endif %}
{% if member.on_interface %}
{% set property_location_list = property_location_list + ['V8DOMConfiguration::OnInterface'] %}
{% endif %}
{{property_location_list | join(' | ')}}
{%- endmacro %}


{% macro check_api_experiment_internal(errorName, experiment_name) %}
{% if experiment_name %}
String {{errorName}};
if (!{{experiment_name}}(executionContext, {{errorName}})) {
     v8SetReturnValue(info, v8::Undefined(info.GetIsolate()));
     if (!{{errorName}}.isEmpty()) {
         toDocument(executionContext)->addConsoleMessage(ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, {{errorName}}));
     }
     return;
}
{% endif %}
{% endmacro %}


{% macro check_api_experiment(member, isolate="info.GetIsolate()") -%}
ExecutionContext* executionContext = currentExecutionContext({{isolate}});
{{check_api_experiment_internal("errorMessage", member.api_experiment_enabled_per_interface) -}}
{{check_api_experiment_internal("memberErrorMessage", member.api_experiment_enabled) -}}
{% endmacro %}
