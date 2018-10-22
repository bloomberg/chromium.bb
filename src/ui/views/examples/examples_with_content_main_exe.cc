// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "ui/views/examples/examples_window_with_content.h"
#include "ui/views_content_client/views_content_client.h"

#if defined(OS_WIN)
#include "content/public/app/sandbox_helper_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

namespace {

void ShowContentExampleWindow(content::BrowserContext* browser_context,
                              gfx::NativeWindow window_context) {
  views::examples::ShowExamplesWindowWithContent(views::examples::QUIT_ON_CLOSE,
                                                 browser_context,
                                                 window_context);

  // These lines serve no purpose other than to introduce an explicit content
  // dependency. If the main executable doesn't have this dependency, the linker
  // has more flexibility to reorder library dependencies in a shared component
  // build. On linux, this can cause libc to appear before libcontent in the
  // dlsym search path, which breaks (usually valid) assumptions made in
  // sandbox::InitLibcUrandomOverrides(). See http://crbug.com/374712.
  if (!browser_context) {
    content::BrowserContext::SaveSessionState(NULL);
    NOTREACHED();
  }
}

}  // namespace

#if defined(OS_WIN)
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  content::InitializeSandboxInfo(&sandbox_info);
  ui::ViewsContentClient views_content_client(instance, &sandbox_info);
#else
int main(int argc, const char** argv) {
  ui::ViewsContentClient views_content_client(argc, argv);
#endif

  views_content_client.set_task(base::Bind(&ShowContentExampleWindow));
  return views_content_client.RunMain();
}
