// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockThreadableLoader_h
#define MockThreadableLoader_h

#include "core/loader/ThreadableLoader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class MockThreadableLoader : public ThreadableLoader {
public:
    static PassRefPtr<MockThreadableLoader> create() { return adoptRef(new testing::StrictMock<MockThreadableLoader>); }

    MOCK_METHOD1(overrideTimeout, void(unsigned long));
    MOCK_METHOD0(cancel, void());

protected:
    MockThreadableLoader() = default;
};

} // namespace blink

#endif // MockThreadableLoader_h
