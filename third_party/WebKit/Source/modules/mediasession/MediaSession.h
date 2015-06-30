// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaSession_h
#define MediaSession_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class MediaSession : public ScriptWrappable, public RefCountedWillBeNoBase<MediaSession> {
    DEFINE_WRAPPERTYPEINFO();
public:
    static RefPtrWillBeRawPtr<MediaSession> create();

    void activate();
    void deactivate();

private:
    MediaSession();
};

}

#endif // MediaSession_h
