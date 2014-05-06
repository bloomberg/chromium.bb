// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/attachment_id_proto.h"

#include "base/guid.h"
#include "base/logging.h"

namespace syncer {

sync_pb::AttachmentIdProto CreateAttachmentIdProto() {
  // Only requirement here is that this id must be globally unique.
  sync_pb::AttachmentIdProto proto;
  std::string guid = base::GenerateGUID();
  DCHECK(!guid.empty());
  proto.set_unique_id(guid);
  return proto;
}

}  // namespace syncer
