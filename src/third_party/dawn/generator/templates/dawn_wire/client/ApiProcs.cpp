//* Copyright 2019 The Dawn Authors
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

#include "dawn_wire/client/ApiObjects.h"
#include "dawn_wire/client/ApiProcs_autogen.h"
#include "dawn_wire/client/Client.h"

#include <algorithm>
#include <string>
#include <vector>

namespace dawn_wire { namespace client {
    //* Implementation of the client API functions.
    {% for type in by_category["object"] %}
        {% set Type = type.name.CamelCase() %}
        {% set cType = as_cType(type.name) %}

        {% for method in type.methods %}
            {% set Suffix = as_MethodSuffix(type.name, method.name) %}
            {% if Suffix not in client_handwritten_commands %}
                {{as_cType(method.return_type.name)}} Client{{Suffix}}(
                    {{-cType}} cSelf
                    {%- for arg in method.arguments -%}
                        , {{as_annotated_cType(arg)}}
                    {%- endfor -%}
                ) {
                    auto self = reinterpret_cast<{{as_wireType(type)}}>(cSelf);
                    Device* device = self->device;
                    {{Suffix}}Cmd cmd;

                    //* Create the structure going on the wire on the stack and fill it with the value
                    //* arguments so it can compute its size.
                    cmd.self = cSelf;

                    //* For object creation, store the object ID the client will use for the result.
                    {% if method.return_type.category == "object" %}
                        auto* allocation = self->device->GetClient()->{{method.return_type.name.CamelCase()}}Allocator().New(self->device);
                        cmd.result = ObjectHandle{allocation->object->id, allocation->serial};
                    {% endif %}

                    {% for arg in method.arguments %}
                        cmd.{{as_varName(arg.name)}} = {{as_varName(arg.name)}};
                    {% endfor %}

                    //* Allocate space to send the command and copy the value args over.
                    size_t requiredSize = cmd.GetRequiredSize();
                    char* allocatedBuffer = static_cast<char*>(device->GetClient()->GetCmdSpace(requiredSize));
                    cmd.Serialize(allocatedBuffer, *device->GetClient());

                    {% if method.return_type.category == "object" %}
                        return reinterpret_cast<{{as_cType(method.return_type.name)}}>(allocation->object.get());
                    {% endif %}
                }
            {% endif %}
        {% endfor %}

        {% if not type.name.canonical_case() == "device" %}
            //* When an object's refcount reaches 0, notify the server side of it and delete it.
            void Client{{as_MethodSuffix(type.name, Name("release"))}}({{cType}} cObj) {
                {{Type}}* obj = reinterpret_cast<{{Type}}*>(cObj);
                obj->refcount --;

                if (obj->refcount > 0) {
                    return;
                }

                DestroyObjectCmd cmd;
                cmd.objectType = ObjectType::{{type.name.CamelCase()}};
                cmd.objectId = obj->id;

                size_t requiredSize = cmd.GetRequiredSize();
                char* allocatedBuffer = static_cast<char*>(obj->device->GetClient()->GetCmdSpace(requiredSize));
                cmd.Serialize(allocatedBuffer);

                obj->device->GetClient()->{{type.name.CamelCase()}}Allocator().Free(obj);
            }

            void Client{{as_MethodSuffix(type.name, Name("reference"))}}({{cType}} cObj) {
                {{Type}}* obj = reinterpret_cast<{{Type}}*>(cObj);
                obj->refcount ++;
            }
        {% endif %}
    {% endfor %}

    namespace {
        struct ProcEntry {
            WGPUProc proc;
            const char* name;
        };
        static const ProcEntry sProcMap[] = {
            {% for (type, method) in c_methods_sorted_by_name %}
                { reinterpret_cast<WGPUProc>(Client{{as_MethodSuffix(type.name, method.name)}}), "{{as_cMethod(type.name, method.name)}}" },
            {% endfor %}
        };
        static constexpr size_t sProcMapSize = sizeof(sProcMap) / sizeof(sProcMap[0]);
    }  // anonymous namespace

    WGPUProc ClientGetProcAddress(WGPUDevice, const char* procName) {
        if (procName == nullptr) {
            return nullptr;
        }

        const ProcEntry* entry = std::lower_bound(&sProcMap[0], &sProcMap[sProcMapSize], procName,
            [](const ProcEntry &a, const char *b) -> bool {
                return strcmp(a.name, b) < 0;
            }
        );

        if (entry != &sProcMap[sProcMapSize] && strcmp(entry->name, procName) == 0) {
            return entry->proc;
        }

        if (strcmp(procName, "wgpuGetProcAddress") == 0) {
            return reinterpret_cast<WGPUProc>(ClientGetProcAddress);
        }

        return nullptr;
    }

    std::vector<const char*> GetProcMapNamesForTesting() {
        std::vector<const char*> result;
        result.reserve(sProcMapSize);
        for (const ProcEntry& entry : sProcMap) {
            result.push_back(entry.name);
        }
        return result;
    }

    //* Some commands don't have a custom wire format, but need to be handled manually to update
    //* some client-side state tracking. For these we have two functions:
    //*  - An autogenerated Client{{suffix}} method that sends the command on the wire
    //*  - A manual ProxyClient{{suffix}} method that will be inserted in the proctable instead of
    //*    the autogenerated one, and that will have to call Client{{suffix}}
    DawnProcTable GetProcs() {
        DawnProcTable table;
        table.getProcAddress = ClientGetProcAddress;
        {% for type in by_category["object"] %}
            {% for method in c_methods(type) %}
                {% set suffix = as_MethodSuffix(type.name, method.name) %}
                table.{{as_varName(type.name, method.name)}} = Client{{suffix}};
            {% endfor %}
        {% endfor %}
        return table;
    }
}}  // namespace dawn_wire::client
