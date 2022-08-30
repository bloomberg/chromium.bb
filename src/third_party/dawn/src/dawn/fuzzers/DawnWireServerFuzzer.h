// Copyright 2019 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_DAWN_FUZZERS_DAWNWIRESERVERFUZZER_H_
#define SRC_DAWN_FUZZERS_DAWNWIRESERVERFUZZER_H_

#include <cstdint>
#include <functional>

#include "dawn/webgpu_cpp.h"

namespace dawn::native {

class Adapter;

}  // namespace dawn::native

namespace DawnWireServerFuzzer {

int Initialize(int* argc, char*** argv);

int Run(const uint8_t* data,
        size_t size,
        bool (*AdapterSupported)(const dawn::native::Adapter&),
        bool supportsErrorInjection);

}  // namespace DawnWireServerFuzzer

#endif  // SRC_DAWN_FUZZERS_DAWNWIRESERVERFUZZER_H_
