// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCompositorImpl_h
#define WebCompositorImpl_h

#include <public/WebCompositor.h>

#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>

namespace WebCore {
class CCThread;
}

namespace WebKit {

class WebThread;

class WebCompositorImpl : public WebCompositor {
    WTF_MAKE_NONCOPYABLE(WebCompositorImpl);
public:
    static bool initialized();

    static void initialize(WebThread* implThread);
    static bool isThreadingEnabled();
    static void shutdown();

private:
    static bool s_initialized;
    static WebCore::CCThread* s_mainThread;
    static WebCore::CCThread* s_implThread;
};

}

#endif // WebCompositorImpl_h
