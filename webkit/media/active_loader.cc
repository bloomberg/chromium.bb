// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/active_loader.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLLoader.h"
#include "webkit/media/buffered_resource_loader.h"

namespace webkit_media {

ActiveLoader::ActiveLoader(
    const scoped_refptr<BufferedResourceLoader>& parent,
    WebKit::WebURLLoader* loader)
    : parent_(parent),
      loader_(loader),
      deferred_(false) {
}

ActiveLoader::~ActiveLoader() {
  if (parent_)
    Cancel();
}

void ActiveLoader::SetDeferred(bool deferred) {
  deferred_ = deferred;
  loader_->setDefersLoading(deferred);
}

void ActiveLoader::Cancel() {
  // We only need to maintain a reference to our parent while the loader is
  // still active. Failing to do so can result in circular refcounts.
  loader_->cancel();
  parent_ = NULL;
}

}  // namespace webkit_media
