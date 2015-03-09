// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/in_memory_attachment_store.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"

namespace syncer {

namespace {

void AppendMetadata(AttachmentMetadataList* list,
                    const Attachment& attachment) {
  list->push_back(
      AttachmentMetadata(attachment.GetId(), attachment.GetData()->size()));
}

}  // namespace

InMemoryAttachmentStore::InMemoryAttachmentStore(
    const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner)
    : AttachmentStoreBackend(callback_task_runner) {
  // Object is created on one thread but used on another.
  DetachFromThread();
}

InMemoryAttachmentStore::~InMemoryAttachmentStore() {
}

void InMemoryAttachmentStore::Init(
    const AttachmentStore::InitCallback& callback) {
  DCHECK(CalledOnValidThread());
  PostCallback(base::Bind(callback, AttachmentStore::SUCCESS));
}

void InMemoryAttachmentStore::Read(
    const AttachmentIdList& ids,
    const AttachmentStore::ReadCallback& callback) {
  DCHECK(CalledOnValidThread());
  AttachmentStore::Result result_code = AttachmentStore::SUCCESS;
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
    result_code = AttachmentStore::UNSPECIFIED_ERROR;
  }
  PostCallback(base::Bind(callback, result_code, base::Passed(&result_map),
                          base::Passed(&unavailable_attachments)));
}

void InMemoryAttachmentStore::Write(
    const AttachmentList& attachments,
    const AttachmentStore::WriteCallback& callback) {
  DCHECK(CalledOnValidThread());
  AttachmentList::const_iterator iter = attachments.begin();
  AttachmentList::const_iterator end = attachments.end();
  for (; iter != end; ++iter) {
    attachments_.insert(std::make_pair(iter->GetId(), *iter));
  }
  PostCallback(base::Bind(callback, AttachmentStore::SUCCESS));
}

void InMemoryAttachmentStore::Drop(
    const AttachmentIdList& ids,
    const AttachmentStore::DropCallback& callback) {
  DCHECK(CalledOnValidThread());
  AttachmentStore::Result result = AttachmentStore::SUCCESS;
  AttachmentIdList::const_iterator ids_iter = ids.begin();
  AttachmentIdList::const_iterator ids_end = ids.end();
  for (; ids_iter != ids_end; ++ids_iter) {
    AttachmentMap::iterator attachments_iter = attachments_.find(*ids_iter);
    if (attachments_iter != attachments_.end()) {
      attachments_.erase(attachments_iter);
    }
  }
  PostCallback(base::Bind(callback, result));
}

void InMemoryAttachmentStore::ReadMetadata(
    const AttachmentIdList& ids,
    const AttachmentStore::ReadMetadataCallback& callback) {
  DCHECK(CalledOnValidThread());
  AttachmentStore::Result result_code = AttachmentStore::SUCCESS;
  scoped_ptr<AttachmentMetadataList> metadata_list(
      new AttachmentMetadataList());
  AttachmentIdList::const_iterator ids_iter = ids.begin();
  AttachmentIdList::const_iterator ids_end = ids.end();

  for (; ids_iter != ids_end; ++ids_iter) {
    AttachmentMap::iterator attachments_iter = attachments_.find(*ids_iter);
    if (attachments_iter != attachments_.end()) {
      AppendMetadata(metadata_list.get(), attachments_iter->second);
    } else {
      result_code = AttachmentStore::UNSPECIFIED_ERROR;
    }
  }
  PostCallback(base::Bind(callback, result_code, base::Passed(&metadata_list)));
}

void InMemoryAttachmentStore::ReadAllMetadata(
    const AttachmentStore::ReadMetadataCallback& callback) {
  DCHECK(CalledOnValidThread());
  AttachmentStore::Result result_code = AttachmentStore::SUCCESS;
  scoped_ptr<AttachmentMetadataList> metadata_list(
      new AttachmentMetadataList());

  for (AttachmentMap::const_iterator iter = attachments_.begin();
       iter != attachments_.end(); ++iter) {
    AppendMetadata(metadata_list.get(), iter->second);
  }
  PostCallback(base::Bind(callback, result_code, base::Passed(&metadata_list)));
}

}  // namespace syncer
