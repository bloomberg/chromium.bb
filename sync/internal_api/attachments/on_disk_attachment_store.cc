// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/on_disk_attachment_store.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "sync/protocol/attachments.pb.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace syncer {

namespace {

// Prefix for records containing attachment data.
const char kDataPrefix[] = "data-";

const base::FilePath::CharType kLeveldbDirectory[] =
    FILE_PATH_LITERAL("leveldb");
}  // namespace

OnDiskAttachmentStore::OnDiskAttachmentStore(
    const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner)
    : callback_task_runner_(callback_task_runner) {
}

OnDiskAttachmentStore::~OnDiskAttachmentStore() {
}

void OnDiskAttachmentStore::Read(const AttachmentIdList& ids,
                                 const ReadCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_);
  scoped_ptr<AttachmentMap> result_map(new AttachmentMap());
  scoped_ptr<AttachmentIdList> unavailable_attachments(new AttachmentIdList());

  leveldb::ReadOptions read_options;
  // Attachment content is typically large and only read once. Don't cache it on
  // db level.
  read_options.fill_cache = false;
  read_options.verify_checksums = true;

  AttachmentIdList::const_iterator iter = ids.begin();
  const AttachmentIdList::const_iterator end = ids.end();
  for (; iter != end; ++iter) {
    const std::string key = CreateDataKeyFromAttachmentId(*iter);
    std::string data_str;
    leveldb::Status status = db_->Get(read_options, key, &data_str);
    if (!status.ok()) {
      DVLOG(1) << "DB::Get failed: status=" << status.ToString();
      unavailable_attachments->push_back(*iter);
      continue;
    }
    scoped_refptr<base::RefCountedMemory> data =
        base::RefCountedString::TakeString(&data_str);
    Attachment attachment = Attachment::CreateWithId(*iter, data);
    result_map->insert(std::make_pair(*iter, attachment));
  }

  Result result_code =
      unavailable_attachments->empty() ? SUCCESS : UNSPECIFIED_ERROR;
  callback_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 result_code,
                 base::Passed(&result_map),
                 base::Passed(&unavailable_attachments)));
}

void OnDiskAttachmentStore::Write(const AttachmentList& attachments,
                                  const WriteCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_);
  Result result_code = SUCCESS;

  leveldb::ReadOptions read_options;
  read_options.fill_cache = false;
  read_options.verify_checksums = true;

  leveldb::WriteOptions write_options;
  write_options.sync = true;

  AttachmentList::const_iterator iter = attachments.begin();
  const AttachmentList::const_iterator end = attachments.end();
  for (; iter != end; ++iter) {
    const std::string key = CreateDataKeyFromAttachmentId(iter->GetId());

    std::string data_str;
    // TODO(pavely): crbug/424304 This read is expensive. When I add metadata
    // records this read will target metadata record instead of payload record.
    leveldb::Status status = db_->Get(read_options, key, &data_str);
    if (status.ok()) {
      // Entry exists, don't overwrite.
      continue;
    } else if (!status.IsNotFound()) {
      // Entry exists but failed to read.
      DVLOG(1) << "DB::Get failed: status=" << status.ToString();
      result_code = UNSPECIFIED_ERROR;
      continue;
    }
    DCHECK(status.IsNotFound());

    scoped_refptr<base::RefCountedMemory> data = iter->GetData();
    leveldb::Slice data_slice(data->front_as<char>(), data->size());
    status = db_->Put(write_options, key, data_slice);
    if (!status.ok()) {
      // Failed to write.
      DVLOG(1) << "DB::Put failed: status=" << status.ToString();
      result_code = UNSPECIFIED_ERROR;
    }
  }
  callback_task_runner_->PostTask(FROM_HERE, base::Bind(callback, result_code));
}

void OnDiskAttachmentStore::Drop(const AttachmentIdList& ids,
                                 const DropCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_);
  Result result_code = SUCCESS;
  leveldb::WriteOptions write_options;
  write_options.sync = true;
  AttachmentIdList::const_iterator iter = ids.begin();
  const AttachmentIdList::const_iterator end = ids.end();
  for (; iter != end; ++iter) {
    const std::string key = CreateDataKeyFromAttachmentId(*iter);
    leveldb::Status status = db_->Delete(write_options, key);
    if (!status.ok()) {
      // DB::Delete doesn't check if record exists, it returns ok just like
      // AttachmentStore::Drop should.
      DVLOG(1) << "DB::Delete failed: status=" << status.ToString();
      result_code = UNSPECIFIED_ERROR;
    }
  }
  callback_task_runner_->PostTask(FROM_HERE, base::Bind(callback, result_code));
}

AttachmentStore::Result OnDiskAttachmentStore::OpenOrCreate(
    const base::FilePath& path) {
  DCHECK(CalledOnValidThread());
  DCHECK(!db_);
  Result result_code = UNSPECIFIED_ERROR;
  base::FilePath leveldb_path = path.Append(kLeveldbDirectory);

  leveldb::DB* db;
  leveldb::Options options;
  options.create_if_missing = true;
  // TODO(pavely): crbug/424287 Consider adding info_log, block_cache and
  // filter_policy to options.
  leveldb::Status status =
      leveldb::DB::Open(options, leveldb_path.AsUTF8Unsafe(), &db);
  if (!status.ok()) {
    DVLOG(1) << "DB::Open failed: status=" << status.ToString()
             << ", path=" << path.AsUTF8Unsafe();
  } else {
    db_.reset(db);
    result_code = SUCCESS;
  }
  return result_code;
}

std::string OnDiskAttachmentStore::CreateDataKeyFromAttachmentId(
    const AttachmentId& attachment_id) {
  std::string key = kDataPrefix + attachment_id.GetProto().unique_id();
  return key;
}

}  // namespace syncer
