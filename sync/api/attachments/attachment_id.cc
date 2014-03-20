// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/attachment_id.h"

#include "base/logging.h"
#include "base/rand_util.h"

namespace syncer {

AttachmentId::~AttachmentId() {}

bool AttachmentId::operator==(const AttachmentId& other) const {
  return unique_id_ == other.unique_id_;
}

bool AttachmentId::operator!=(const AttachmentId& other) const {
  return !operator==(other);
}

bool AttachmentId::operator<(const AttachmentId& other) const {
  return unique_id_ < other.unique_id_;
}

// Static.
AttachmentId AttachmentId::Create() {
  // Only requirement here is that this id must be globally unique.
  // TODO(maniscalco): Consider making this base64 encoded.
  return AttachmentId(base::RandBytesAsString(16));
}

AttachmentId::AttachmentId(const std::string& unique_id)
    : unique_id_(unique_id) {
  DCHECK(!unique_id_.empty());
}

}  // namespace syncer
