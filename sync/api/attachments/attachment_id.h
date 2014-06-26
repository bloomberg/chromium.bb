// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_ATTACHMENTS_ATTACHMENT_ID_H_
#define SYNC_API_ATTACHMENTS_ATTACHMENT_ID_H_

#include <set>
#include <string>
#include <vector>

#include "sync/base/sync_export.h"
#include "sync/internal_api/public/util/immutable.h"

namespace sync_pb {
class AttachmentIdProto;
}  // namespace sync_pb

namespace syncer {

// Uniquely identifies an attachment.
//
// Two attachments with equal (operator==) AttachmentIds are considered
// equivalent.
class SYNC_EXPORT AttachmentId {
 public:
  ~AttachmentId();

  // Default copy and assignment are welcome.

  bool operator==(const AttachmentId& other) const;

  bool operator!=(const AttachmentId& other) const;

  // Needed for using AttachmentId as key in std::map.
  bool operator<(const AttachmentId& other) const;

  // Creates a unique attachment id.
  static AttachmentId Create();

  // Creates an attachment id from an initialized proto.
  static AttachmentId CreateFromProto(const sync_pb::AttachmentIdProto& proto);

  const sync_pb::AttachmentIdProto& GetProto() const;

 private:
  // Necessary since we forward-declare sync_pb::AttachmentIdProto; see comments
  // in immutable.h.
  struct SYNC_EXPORT ImmutableAttachmentIdProtoTraits {
    typedef sync_pb::AttachmentIdProto* Wrapper;
    static void InitializeWrapper(Wrapper* wrapper);
    static void DestroyWrapper(Wrapper* wrapper);
    static const sync_pb::AttachmentIdProto& Unwrap(const Wrapper& wrapper);
    static sync_pb::AttachmentIdProto* UnwrapMutable(Wrapper* wrapper);
    static void Swap(sync_pb::AttachmentIdProto* t1,
                     sync_pb::AttachmentIdProto* t2);
  };

  typedef Immutable<sync_pb::AttachmentIdProto,
                    ImmutableAttachmentIdProtoTraits>
      ImmutableAttachmentIdProto;

  ImmutableAttachmentIdProto proto_;

  AttachmentId(sync_pb::AttachmentIdProto* proto);
};

typedef std::vector<AttachmentId> AttachmentIdList;
typedef std::set<AttachmentId> AttachmentIdSet;

}  // namespace syncer

#endif  // SYNC_API_ATTACHMENTS_ATTACHMENT_ID_H_
