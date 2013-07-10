// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageBitmapCallback_h
#define ImageBitmapCallback_h

#include "core/dom/ScriptExecutionContext.h"
#include "core/page/ImageBitmap.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class ImageBitmap;

class ImageBitmapCallback : public RefCounted<ImageBitmapCallback> {
public:
    virtual ~ImageBitmapCallback() { }
    virtual bool handleEvent(ImageBitmap*) = 0;

    class CallbackTask : public ScriptExecutionContext::Task {
    public:
        static PassOwnPtr<CallbackTask> create(PassRefPtr<ImageBitmap> bitmap, PassRefPtr<ImageBitmapCallback> callback)
        {
            return adoptPtr(new CallbackTask(bitmap, callback));
        }

        virtual void performTask(ScriptExecutionContext*);

    private:
        CallbackTask(PassRefPtr<ImageBitmap>, PassRefPtr<ImageBitmapCallback>);

        RefPtr<ImageBitmap> m_bitmap;
        RefPtr<ImageBitmapCallback> m_callback;
    };
};

} // namespace Webcore

#endif // ImageBitmapCallback_h
