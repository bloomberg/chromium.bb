// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/attachment_id_proto.h"

#include "base/rand_util.h"

namespace syncer {

sync_pb::AttachmentIdProto CreateAttachmentIdProto() {
  // Only requirement here is that this id must be globally unique.
  // TODO(maniscalco): Consider making this base64 encoded.
  sync_pb::AttachmentIdProto proto;
  proto.set_unique_id(base::RandBytesAsString(16));
  return proto;
}

}  // namespace syncer
