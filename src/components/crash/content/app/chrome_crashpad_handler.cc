// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "components/gwp_asan/crash_handler/crash_handler.h"
#include "third_party/crashpad/crashpad/handler/handler_main.h"
#include "third_party/crashpad/crashpad/handler/user_stream_data_source.h"

int main(int argc, char* argv[]) {
  crashpad::UserStreamDataSources user_stream_data_sources;
  user_stream_data_sources.push_back(
      std::make_unique<gwp_asan::UserStreamDataSource>());

  return crashpad::HandlerMain(argc, argv, &user_stream_data_sources);
}
