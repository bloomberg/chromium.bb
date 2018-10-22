// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_RENDERER_HEADLESS_CONTENT_RENDERER_CLIENT_H_
#define HEADLESS_LIB_RENDERER_HEADLESS_CONTENT_RENDERER_CLIENT_H_

#include "content/public/renderer/content_renderer_client.h"

namespace headless {

class HeadlessContentRendererClient : public content::ContentRendererClient {
 public:
  HeadlessContentRendererClient();
  ~HeadlessContentRendererClient() override;

 private:
  void RenderFrameCreated(content::RenderFrame* render_frame) override;

  DISALLOW_COPY_AND_ASSIGN(HeadlessContentRendererClient);
};

}  // namespace headless

#endif  // HEADLESS_LIB_RENDERER_HEADLESS_CONTENT_RENDERER_CLIENT_H_
