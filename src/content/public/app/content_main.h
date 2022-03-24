// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_CONTENT_MAIN_H_
#define CONTENT_PUBLIC_APP_CONTENT_MAIN_H_

#include <stddef.h>

#include "base/callback.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace base {
namespace mac {
class ScopedNSAutoreleasePool;
}
}

namespace sandbox {
struct SandboxInterfaceInfo;
}

namespace content {

class BrowserMainParts;
class ContentMainDelegate;
class ContentMainRunner;

using CreatedMainPartsClosure = base::OnceCallback<void(BrowserMainParts*)>;

struct CONTENT_EXPORT ContentMainParams {
  explicit ContentMainParams(ContentMainDelegate* delegate);
  ~ContentMainParams();

  // Do not reuse the moved-from ContentMainParams after this call.
  ContentMainParams(ContentMainParams&&);
  ContentMainParams& operator=(ContentMainParams&&);

  ContentMainDelegate* delegate;

#if defined(OS_WIN)
  HINSTANCE instance = nullptr;

  // |sandbox_info| should be initialized using InitializeSandboxInfo from
  // content_main_win.h
  sandbox::SandboxInterfaceInfo* sandbox_info = nullptr;
#elif !defined(OS_ANDROID)
  int argc = 0;
  const char** argv = nullptr;
#endif

  // Used by BrowserTestBase. If set, BrowserMainLoop runs this task instead of
  // the main message loop.
  base::OnceClosure ui_task;

  // Used by BrowserTestBase. If set, this is invoked after BrowserMainParts has
  // been created and before PreEarlyInitialization().
  CreatedMainPartsClosure created_main_parts_closure;

  // Indicates whether to run in a minimal browser mode where most subsystems
  // are left uninitialized.
  bool minimal_browser_mode = false;

#if defined(OS_MAC)
  // The outermost autorelease pool to pass to main entry points.
  base::mac::ScopedNSAutoreleasePool* autorelease_pool = nullptr;
#endif

  // Returns a copy of this ContentMainParams without the move-only data
  // (which is expected to be null when calling this). Used by the TestLauncher
  // to launch main multiple times under the same conditions.
  ContentMainParams ShallowCopyForTesting() const {
    ContentMainParams copy(delegate);
#if defined(OS_WIN)
    copy.instance = instance;
    copy.sandbox_info = sandbox_info;
#elif !defined(OS_ANDROID)
    copy.argc = argc;
    copy.argv = argv;
#endif
    DCHECK(!ui_task);
    DCHECK(!created_main_parts_closure);
    copy.minimal_browser_mode = minimal_browser_mode;
#if defined(OS_MAC)
    copy.autorelease_pool = autorelease_pool;
#endif
    return copy;
  }
};

CONTENT_EXPORT int RunContentProcess(ContentMainParams params,
                                     ContentMainRunner* content_main_runner);

#if defined(OS_ANDROID)
// In the Android, the content main starts from ContentMain.java, This function
// provides a way to set the |delegate| as ContentMainDelegate for
// ContentMainRunner.
// This should only be called once before ContentMainRunner actually running.
// The ownership of |delegate| is transferred.
CONTENT_EXPORT void SetContentMainDelegate(ContentMainDelegate* delegate);

#if defined(CONTENT_IMPLEMENTATION)
// In browser tests, ContentMain.java is not run either, and the browser test
// harness does not run ContentMain() at all. It does need to make use of the
// delegate though while replacing ContentMain().
ContentMainDelegate* GetContentMainDelegate();
#endif

#else
// ContentMain should be called from the embedder's main() function to do the
// initial setup for every process. The embedder has a chance to customize
// startup using the ContentMainDelegate interface. The embedder can also pass
// in null for |delegate| if they don't want to override default startup.
CONTENT_EXPORT int ContentMain(ContentMainParams params);
#endif

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_CONTENT_MAIN_H_
