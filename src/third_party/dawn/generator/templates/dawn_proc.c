//* Copyright 2017 The Dawn Authors
//*
//* Licensed under the Apache License, Version 2.0 (the "License");
//* you may not use this file except in compliance with the License.
//* You may obtain a copy of the License at
//*
//*     http://www.apache.org/licenses/LICENSE-2.0
//*
//* Unless required by applicable law or agreed to in writing, software
//* distributed under the License is distributed on an "AS IS" BASIS,
//* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//* See the License for the specific language governing permissions and
//* limitations under the License.

{% set Prefix = metadata.proc_table_prefix %}
{% set prefix = Prefix.lower() %}
#include "dawn/{{prefix}}_proc.h"

static {{Prefix}}ProcTable procs;

static {{Prefix}}ProcTable nullProcs;

void {{prefix}}ProcSetProcs(const {{Prefix}}ProcTable* procs_) {
    if (procs_) {
        procs = *procs_;
    } else {
        procs = nullProcs;
    }
}

{% for function in by_category["function"] %}
    {{as_cType(function.return_type.name)}} {{as_cMethod(None, function.name)}}(
        {%- for arg in function.arguments -%}
            {% if not loop.first %}, {% endif %}{{as_annotated_cType(arg)}}
        {%- endfor -%}
    ) {
        {% if function.return_type.name.canonical_case() != "void" %}return {% endif %}
        procs.{{as_varName(function.name)}}(
            {%- for arg in function.arguments -%}
                {% if not loop.first %}, {% endif %}{{as_varName(arg.name)}}
            {%- endfor -%}
        );
    }
{% endfor %}

{% for type in by_category["object"] %}
    {% for method in c_methods(type) %}
        {{as_cType(method.return_type.name)}} {{as_cMethod(type.name, method.name)}}(
            {{-as_cType(type.name)}} {{as_varName(type.name)}}
            {%- for arg in method.arguments -%}
                , {{as_annotated_cType(arg)}}
            {%- endfor -%}
        ) {
            {% if method.return_type.name.canonical_case() != "void" %}return {% endif %}
            procs.{{as_varName(type.name, method.name)}}({{as_varName(type.name)}}
                {%- for arg in method.arguments -%}
                    , {{as_varName(arg.name)}}
                {%- endfor -%}
            );
        }
    {% endfor %}

{% endfor %}
