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
    {%- if member.annotation in ["*", "const*", "const*const*"] and member.optional -%}
        {{" "}}= nullptr
    {%- elif member.type.category in ["enum", "bitmask"] and member.default_value != None -%}
        {{" "}}= wgpu::{{as_cppType(member.type.name)}}::{{as_cppEnum(Name(member.default_value))}}
    {%- elif member.type.category == "native" and member.default_value != None -%}
        {{" "}}= {{member.default_value}}
    {%- else -%}
        {{assert(member.default_value == None)}}
    {%- endif -%}
{%- endmacro %}

    {% for type in by_category["structure"] %}
        struct {{as_cppType(type.name)}} {
            {% if type.extensible %}
                const void* nextInChain = nullptr;
            {% endif %}
            {% for member in type.members %}
            {{as_annotated_frontendType(member)}} {{render_cpp_default_value(member)}};
            {% endfor %}
        };

    {% endfor %}

} // namespace dawn_native

#endif  // DAWNNATIVE_WGPU_STRUCTS_H_
