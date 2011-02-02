// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gfx/test_suite.h"

#include "gfx/gfx_paths.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/mac/scoped_nsautorelease_pool.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

GfxTestSuite::GfxTestSuite(int argc, char** argv) : TestSuite(argc, argv) {}

void GfxTestSuite::Initialize() {
  base::mac::ScopedNSAutoreleasePool autorelease_pool;

  TestSuite::Initialize();

  gfx::RegisterPathProvider();

#if defined(OS_MACOSX)
  // Look in the framework bundle for resources.
  // TODO(port): make a resource bundle for non-app exes.  What's done here
  // isn't really right because this code needs to depend on chrome_dll
  // being built.  This is inappropriate in app.
  FilePath path;
  PathService::Get(base::DIR_EXE, &path);
#if defined(GOOGLE_CHROME_BUILD)
  path = path.AppendASCII("Google Chrome Framework.framework");
#elif defined(CHROMIUM_BUILD)
  path = path.AppendASCII("Chromium Framework.framework");
#else
#error Unknown branding
#endif
  base::mac::SetOverrideAppBundlePath(path);
#endif  // OS_MACOSX
}

void GfxTestSuite::Shutdown() {
#if defined(OS_MACOSX)
  base::mac::SetOverrideAppBundle(NULL);
#endif
  TestSuite::Shutdown();
}
