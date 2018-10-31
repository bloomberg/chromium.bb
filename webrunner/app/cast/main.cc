// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/service_directory.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "webrunner/app/cast/cast_runner.h"

int main(int argc, char** argv) {
  base::MessageLoopForIO message_loop;
  base::RunLoop run_loop;

  castrunner::CastRunner runner(
      base::fuchsia::ServiceDirectory::GetDefault(),
      webrunner::WebContentRunner::CreateDefaultWebContext(),
      run_loop.QuitClosure());

  // Run until there are no Components, or the last service client channel is
  // closed.
  run_loop.Run();

  return 0;
}
