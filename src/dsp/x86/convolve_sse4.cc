// Copyright 2019 The libgav1 Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/dsp/convolve.h"
#include "src/utils/cpu.h"

#if LIBGAV1_ENABLE_SSE4_1
namespace libgav1 {
namespace dsp {

void ConvolveInit_SSE4_1() {}

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_SSE4_1
namespace libgav1 {
namespace dsp {

void ConvolveInit_SSE4_1() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_SSE4_1
