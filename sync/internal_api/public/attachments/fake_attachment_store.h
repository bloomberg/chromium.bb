// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_FAKE_ATTACHMENT_STORE_H_
#define SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_FAKE_ATTACHMENT_STORE_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "sync/api/attachments/attachment_store.h"
#include "sync/base/sync_export.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace sync_pb {
class AttachmentId;
}  // namespace sync_pb

namespace syncer {

class Attachment;

// A in-memory only implementation of AttachmentStore used for testing.
//
// Requires that the current thread has a MessageLoop.
class SYNC_EXPORT FakeAttachmentStore : public AttachmentStore {
 public:
  // Construct a FakeAttachmentStore whose "IO" will be performed in
  // |backend_task_runner|.
  explicit FakeAttachmentStore(
      const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner);

  virtual ~FakeAttachmentStore();

  // AttachmentStore implementation.
  virtual void Read(const AttachmentIdList& id,
                    const ReadCallback& callback) OVERRIDE;
  virtual void Write(const AttachmentList& attachments,
                     const WriteCallback& callback) OVERRIDE;
  virtual void Drop(const AttachmentIdList& id,
                    const DropCallback& callback) OVERRIDE;

 private:
  class Backend;

  scoped_refptr<Backend> backend_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(FakeAttachmentStore);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_ATTACHMENTS_FAKE_ATTACHMENT_STORE_H_
