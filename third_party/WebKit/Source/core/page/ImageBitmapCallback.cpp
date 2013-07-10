// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/page/ImageBitmapCallback.h"

namespace WebCore {

ImageBitmapCallback::CallbackTask::CallbackTask(PassRefPtr<ImageBitmap> bitmap, PassRefPtr<ImageBitmapCallback> callback)
    : m_bitmap(bitmap)
    , m_callback(callback)
{
}

void ImageBitmapCallback::CallbackTask::performTask(ScriptExecutionContext*)
{
    if (!m_callback || !m_bitmap) {
        return;
    }
    m_callback->handleEvent(m_bitmap.get());
}

} // namespace Webcore

