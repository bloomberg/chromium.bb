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

#ifndef DAWN_DAWNCPP_TRAITS_H_
#define DAWN_DAWNCPP_TRAITS_H_

#include "dawncpp.h"

#include <type_traits>

namespace dawn {

    template<typename Builder>
    using BuiltObject = decltype(std::declval<Builder>().GetResult());

    template<typename BuiltObject>
    struct BuiltObjectTrait {
    };

    {% for type in by_category["object"] if type.is_builder %}
        template<>
        struct BuiltObjectTrait<BuiltObject<{{as_cppType(type.name)}}>> {
            using Builder = {{as_cppType(type.name)}};
        };
    {% endfor %}

    template<typename BuiltObject>
    using Builder = typename BuiltObjectTrait<BuiltObject>::Builder;

}

#endif // DAWN_DAWNCPP_TRAITS_H_
