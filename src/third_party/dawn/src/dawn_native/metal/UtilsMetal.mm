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

#include "dawn_native/metal/UtilsMetal.h"

#include "common/Assert.h"

namespace dawn_native { namespace metal {

    MTLCompareFunction ToMetalCompareFunction(wgpu::CompareFunction compareFunction) {
        switch (compareFunction) {
            case wgpu::CompareFunction::Never:
                return MTLCompareFunctionNever;
            case wgpu::CompareFunction::Less:
                return MTLCompareFunctionLess;
            case wgpu::CompareFunction::LessEqual:
                return MTLCompareFunctionLessEqual;
            case wgpu::CompareFunction::Greater:
                return MTLCompareFunctionGreater;
            case wgpu::CompareFunction::GreaterEqual:
                return MTLCompareFunctionGreaterEqual;
            case wgpu::CompareFunction::NotEqual:
                return MTLCompareFunctionNotEqual;
            case wgpu::CompareFunction::Equal:
                return MTLCompareFunctionEqual;
            case wgpu::CompareFunction::Always:
                return MTLCompareFunctionAlways;
            default:
                UNREACHABLE();
        }
    }

}}  // namespace dawn_native::metal
