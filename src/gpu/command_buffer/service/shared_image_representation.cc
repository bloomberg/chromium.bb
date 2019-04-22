// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {

SharedImageRepresentation::SharedImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* owning_tracker)
    : manager_(manager), backing_(backing), tracker_(owning_tracker) {
  DCHECK(tracker_);
  backing_->AddRef(this);
}

SharedImageRepresentation::~SharedImageRepresentation() {
  manager_->OnRepresentationDestroyed(backing_->mailbox(), this);
}

bool SharedImageRepresentationGLTexture::BeginAccess(GLenum mode) {
  return true;
}

bool SharedImageRepresentationGLTexturePassthrough::BeginAccess(GLenum mode) {
  return true;
}

}  // namespace gpu
