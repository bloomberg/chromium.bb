// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/fake_attachment_store.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "sync/api/attachments/attachment.h"

namespace syncer {

// Backend is where all the work happens.
class FakeAttachmentStore::Backend
    : public base::RefCountedThreadSafe<FakeAttachmentStore::Backend> {
 public:
  // Construct a Backend that posts its results to |frontend_task_runner|.
  Backend(
      const scoped_refptr<base::SingleThreadTaskRunner>& frontend_task_runner);

  void Read(const AttachmentIdList& ids, const ReadCallback& callback);
  void Write(const AttachmentList& attachments, const WriteCallback& callback);
  void Drop(const AttachmentIdList& ids, const DropCallback& callback);

 private:
  friend class base::RefCountedThreadSafe<Backend>;

  ~Backend();

  scoped_refptr<base::SingleThreadTaskRunner> frontend_task_runner_;
  AttachmentMap attachments_;
};

FakeAttachmentStore::Backend::Backend(
    const scoped_refptr<base::SingleThreadTaskRunner>& frontend_task_runner)
    : frontend_task_runner_(frontend_task_runner) {}

FakeAttachmentStore::Backend::~Backend() {}

void FakeAttachmentStore::Backend::Read(const AttachmentIdList& ids,
                                        const ReadCallback& callback) {
  Result result_code = SUCCESS;
  AttachmentIdList::const_iterator id_iter = ids.begin();
  AttachmentIdList::const_iterator id_end = ids.end();
  scoped_ptr<AttachmentMap> result_map(new AttachmentMap);
  scoped_ptr<AttachmentIdList> unavailable_attachments(new AttachmentIdList);
  for (; id_iter != id_end; ++id_iter) {
    const AttachmentId& id = *id_iter;
    syncer::AttachmentMap::iterator attachment_iter =
        attachments_.find(*id_iter);
    if (attachment_iter != attachments_.end()) {
      const Attachment& attachment = attachment_iter->second;
      result_map->insert(std::make_pair(id, attachment));
    } else {
      unavailable_attachments->push_back(id);
    }
  }
  if (!unavailable_attachments->empty()) {
    result_code = UNSPECIFIED_ERROR;
  }
  frontend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 result_code,
                 base::Passed(&result_map),
                 base::Passed(&unavailable_attachments)));
}

void FakeAttachmentStore::Backend::Write(const AttachmentList& attachments,
                                         const WriteCallback& callback) {
  AttachmentList::const_iterator iter = attachments.begin();
  AttachmentList::const_iterator end = attachments.end();
  for (; iter != end; ++iter) {
    attachments_.insert(std::make_pair(iter->GetId(), *iter));
  }
  frontend_task_runner_->PostTask(FROM_HERE, base::Bind(callback, SUCCESS));
}

void FakeAttachmentStore::Backend::Drop(const AttachmentIdList& ids,
                                        const DropCallback& callback) {
  Result result = SUCCESS;
  AttachmentIdList::const_iterator ids_iter = ids.begin();
  AttachmentIdList::const_iterator ids_end = ids.end();
  for (; ids_iter != ids_end; ++ids_iter) {
    AttachmentMap::iterator attachments_iter = attachments_.find(*ids_iter);
    if (attachments_iter != attachments_.end()) {
      attachments_.erase(attachments_iter);
    }
  }
  frontend_task_runner_->PostTask(FROM_HERE, base::Bind(callback, result));
}

FakeAttachmentStore::FakeAttachmentStore(
    const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner)
    : backend_(new Backend(base::ThreadTaskRunnerHandle::Get())),
      backend_task_runner_(backend_task_runner) {}

FakeAttachmentStore::~FakeAttachmentStore() {}

void FakeAttachmentStore::Read(const AttachmentIdList& ids,
                               const ReadCallback& callback) {
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&FakeAttachmentStore::Backend::Read, backend_, ids, callback));
}

void FakeAttachmentStore::Write(const AttachmentList& attachments,
                                const WriteCallback& callback) {
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&FakeAttachmentStore::Backend::Write,
                 backend_,
                 attachments,
                 callback));
}

void FakeAttachmentStore::Drop(const AttachmentIdList& ids,
                               const DropCallback& callback) {
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&FakeAttachmentStore::Backend::Drop, backend_, ids, callback));
}

}  // namespace syncer
