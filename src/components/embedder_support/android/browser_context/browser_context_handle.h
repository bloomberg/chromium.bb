// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_BROWSER_CONTEXT_BROWSER_CONTEXT_HANDLE_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_BROWSER_CONTEXT_BROWSER_CONTEXT_HANDLE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"

namespace content {
class BrowserContext;
}

namespace browser_context {

// Returns a pointer to the native BrowserContext wrapped by the given Java
// BrowserContextHandle reference.
content::BrowserContext* BrowserContextFromJavaHandle(
    const base::android::JavaRef<jobject>& jhandle);

}  // namespace browser_context

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ANDROID_BROWSER_CONTEXT_BROWSER_CONTEXT_HANDLE_H_
