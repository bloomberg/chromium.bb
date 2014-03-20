// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_ATTACHMENTS_ATTACHMENT_ID_H_
#define SYNC_API_ATTACHMENTS_ATTACHMENT_ID_H_

#include <string>
#include <vector>

#include "sync/base/sync_export.h"

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

 private:
  std::string unique_id_;

  AttachmentId(const std::string& unique_id);
};

typedef std::vector<AttachmentId> AttachmentIdList;

}  // namespace syncer

#endif  // SYNC_API_ATTACHMENTS_ATTACHMENT_ID_H_
