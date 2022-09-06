// Copyright 2021 The Dawn Authors
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

#ifndef SRC_DAWN_NATIVE_TINTUTILS_H_
#define SRC_DAWN_NATIVE_TINTUTILS_H_

#include "dawn/common/NonCopyable.h"

namespace dawn::native {

class DeviceBase;

// Indicates that for the lifetime of this object tint internal compiler errors should be
// reported to the given device.
class ScopedTintICEHandler : public NonCopyable {
  public:
    explicit ScopedTintICEHandler(DeviceBase* device);
    ~ScopedTintICEHandler();

  private:
    ScopedTintICEHandler(ScopedTintICEHandler&&) = delete;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_TINTUTILS_H_
