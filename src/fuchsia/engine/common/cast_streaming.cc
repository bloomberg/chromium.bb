// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/common/cast_streaming.h"

#include "base/command_line.h"
#include "fuchsia/engine/switches.h"
#include "url/gurl.h"

namespace {
constexpr char kCastStreamingReceiverUrl[] = "data:cast_streaming_receiver";
}  // namespace

bool IsCastStreamingEnabled() {
  static bool is_cast_streaming_enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCastStreamingReceiver);
  return is_cast_streaming_enabled;
}

bool IsCastStreamingMediaSourceUrl(const GURL& url) {
  return url == kCastStreamingReceiverUrl;
}
