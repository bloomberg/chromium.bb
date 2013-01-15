// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/active_loader.h"

#include "third_party/WebKit/Source/Platform/chromium/public/WebURLLoader.h"
#include "webkit/media/buffered_resource_loader.h"

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
