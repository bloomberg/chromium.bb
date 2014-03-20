// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/attachment.h"

#include "base/logging.h"

namespace syncer {

Attachment::~Attachment() {}

// Static.
scoped_ptr<Attachment> Attachment::Create(
    const scoped_refptr<base::RefCountedMemory>& data) {
  return CreateWithId(AttachmentId::Create(), data);
}

// Static.
scoped_ptr<Attachment> Attachment::CreateWithId(
    const AttachmentId& id,
    const scoped_refptr<base::RefCountedMemory>& data) {
  return scoped_ptr<Attachment>(new Attachment(id, data)).Pass();
}

const AttachmentId& Attachment::GetId() const { return id_; }

const scoped_refptr<base::RefCountedMemory>& Attachment::GetData() const {
  return data_;
}

Attachment::Attachment(const AttachmentId& id,
                       const scoped_refptr<base::RefCountedMemory>& data)
    : id_(id), data_(data) {
  DCHECK(data);
}

}  // namespace syncer
