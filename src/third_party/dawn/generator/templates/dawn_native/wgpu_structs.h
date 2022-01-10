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

#ifndef DAWNNATIVE_WGPU_STRUCTS_H_
#define DAWNNATIVE_WGPU_STRUCTS_H_

#include "dawn/webgpu_cpp.h"
#include "dawn_native/Forward.h"

namespace dawn_native {

{% macro render_cpp_default_value(member) -%}
    {%- if member.annotation in ["*", "const*"] and member.optional -%}
        {{" "}}= nullptr
    {%- elif member.type.category == "object" and member.optional -%}
        {{" "}}= nullptr
    {%- elif member.type.category in ["enum", "bitmask"] and member.default_value != None -%}
        {{" "}}= wgpu::{{as_cppType(member.type.name)}}::{{as_cppEnum(Name(member.default_value))}}
    {%- elif member.type.category == "native" and member.default_value != None -%}
        {{" "}}= {{member.default_value}}
    {%- else -%}
        {{assert(member.default_value == None)}}
    {%- endif -%}
{%- endmacro %}

    struct ChainedStruct {
        ChainedStruct const * nextInChain = nullptr;
        wgpu::SType sType = wgpu::SType::Invalid;
    };

    {% for type in by_category["structure"] %}
        {% if type.chained %}
            struct {{as_cppType(type.name)}} : ChainedStruct {
                {{as_cppType(type.name)}}() {
                    sType = wgpu::SType::{{type.name.CamelCase()}};
                }
        {% else %}
            struct {{as_cppType(type.name)}} {
        {% endif %}
            {% if type.extensible %}
                ChainedStruct const * nextInChain = nullptr;
            {% endif %}
            {% for member in type.members %}
                {% set member_declaration = as_annotated_frontendType(member) + render_cpp_default_value(member) %}
                {% if type.chained and loop.first %}
                    //* Align the first member to ChainedStruct to match the C struct layout.
                    alignas(ChainedStruct) {{member_declaration}};
                {% else %}
                    {{member_declaration}};
                {% endif %}
            {% endfor %}

            // Equality operators, mostly for testing. Note that this tests
            // strict pointer-pointer equality if the struct contains member pointers.
            bool operator==(const {{as_cppType(type.name)}}& rhs) const;
        };

    {% endfor %}

    {% for typeDef in by_category["typedef"] if typeDef.type.category == "structure" %}
        using {{as_cppType(typeDef.name)}} = {{as_cppType(typeDef.type.name)}};
    {% endfor %}

} // namespace dawn_native

#endif  // DAWNNATIVE_WGPU_STRUCTS_H_
