// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_content_renderer_client.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "ipc/message_filter.h"

namespace chromecast {
namespace shell {

// static
std::unique_ptr<CastContentRendererClient> CastContentRendererClient::Create() {
  return base::WrapUnique(new CastContentRendererClient());
}

}  // namespace shell
}  // namespace chromecast
