// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_process_host_impl.h"

namespace content {

RenderProcessHostInternalObserver::~RenderProcessHostInternalObserver() {
  DCHECK(!IsInObserverList());
}

}  // namespace content
