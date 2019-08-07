// Copyright 2017 The Dawn Authors
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

#include "dawn_native/RefCounted.h"

#include "common/Assert.h"

namespace dawn_native {

    RefCounted::RefCounted() {
    }

    RefCounted::~RefCounted() {
    }

    void RefCounted::ReferenceInternal() {
        ASSERT(mInternalRefs != 0);

        // TODO(cwallez@chromium.org): what to do on overflow?
        mInternalRefs++;
    }

    void RefCounted::ReleaseInternal() {
        ASSERT(mInternalRefs != 0);

        mInternalRefs--;

        if (mInternalRefs == 0) {
            ASSERT(mExternalRefs == 0);
            // TODO(cwallez@chromium.org): would this work with custom allocators?
            delete this;
        }
    }

    uint32_t RefCounted::GetExternalRefs() const {
        return mExternalRefs;
    }

    uint32_t RefCounted::GetInternalRefs() const {
        return mInternalRefs;
    }

    void RefCounted::Reference() {
        ASSERT(mInternalRefs != 0);

        // mExternalRefs != 0 counts as one internal ref.
        if (mExternalRefs == 0) {
            ReferenceInternal();
        }

        // TODO(cwallez@chromium.org): what to do on overflow?
        mExternalRefs++;
    }

    void RefCounted::Release() {
        ASSERT(mInternalRefs != 0);
        ASSERT(mExternalRefs != 0);

        mExternalRefs--;
        // mExternalRefs != 0 counts as one internal ref.
        if (mExternalRefs == 0) {
            ReleaseInternal();
        }
    }

}  // namespace dawn_native
