// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlinkGCMemoryDumpProvider_h
#define BlinkGCMemoryDumpProvider_h

#include "platform/PlatformExport.h"
#include "public/platform/WebMemoryDumpProvider.h"

namespace blink {

class PLATFORM_EXPORT BlinkGCMemoryDumpProvider final : public WebMemoryDumpProvider {
public:
    static BlinkGCMemoryDumpProvider* instance();
    ~BlinkGCMemoryDumpProvider() override;

    // WebMemoryDumpProvider implementation.
    bool onMemoryDump(WebProcessMemoryDump*) override;

private:
    BlinkGCMemoryDumpProvider();
};

} // namespace blink

#endif
