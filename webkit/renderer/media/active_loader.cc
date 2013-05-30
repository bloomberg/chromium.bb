// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/renderer/media/active_loader.h"

#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "webkit/renderer/media/buffered_resource_loader.h"

namespace webkit_media {

ActiveLoader::ActiveLoader(scoped_ptr<WebKit::WebURLLoader> loader)
    : loader_(loader.Pass()),
      deferred_(false) {
}

ActiveLoader::~ActiveLoader() {
  loader_->cancel();
}

void ActiveLoader::SetDeferred(bool deferred) {
  deferred_ = deferred;
  loader_->setDefersLoading(deferred);
}

}  // namespace webkit_media
