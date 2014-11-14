// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/on_disk_attachment_store.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "sync/internal_api/attachments/proto/attachment_store.pb.h"
#include "sync/internal_api/public/attachments/attachment_util.h"
#include "sync/protocol/attachments.pb.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace syncer {

namespace {

// Prefix for records containing attachment data.
const char kDataPrefix[] = "data-";

// Prefix for records containing attachment metadata.
const char kMetadataPrefix[] = "metadata-";

const char kDatabaseMetadataKey[] = "database-metadata";

const int32 kCurrentSchemaVersion = 1;

const base::FilePath::CharType kLeveldbDirectory[] =
    FILE_PATH_LITERAL("leveldb");

leveldb::WriteOptions MakeWriteOptions() {
  leveldb::WriteOptions write_options;
  write_options.sync = true;
  return write_options;
}

leveldb::ReadOptions MakeDataReadOptions() {
  leveldb::ReadOptions read_options;
  // Attachment content is typically large and only read once. Don't cache it on
  // db level.
  read_options.fill_cache = false;
  read_options.verify_checksums = true;
  return read_options;
}

leveldb::ReadOptions MakeMetadataReadOptions() {
  leveldb::ReadOptions read_options;
  read_options.fill_cache = true;
  read_options.verify_checksums = true;
  return read_options;
}

leveldb::Status ReadStoreMetadata(
    leveldb::DB* db,
    attachment_store_pb::StoreMetadata* metadata) {
  std::string data_str;

  leveldb::Status status =
      db->Get(MakeMetadataReadOptions(), kDatabaseMetadataKey, &data_str);
  if (!status.ok())
    return status;
  if (!metadata->ParseFromString(data_str))
    return leveldb::Status::Corruption("Metadata record corruption");
  return leveldb::Status::OK();
}

leveldb::Status WriteStoreMetadata(
    leveldb::DB* db,
    const attachment_store_pb::StoreMetadata& metadata) {
  std::string data_str;

  metadata.SerializeToString(&data_str);
  return db->Put(MakeWriteOptions(), kDatabaseMetadataKey, data_str);
}

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

  AttachmentIdList::const_iterator iter = ids.begin();
  const AttachmentIdList::const_iterator end = ids.end();
  for (; iter != end; ++iter) {
    scoped_ptr<Attachment> attachment = ReadSingleAttachment(*iter);
    if (attachment) {
      result_map->insert(std::make_pair(*iter, *attachment));
    } else {
      unavailable_attachments->push_back(*iter);
    }
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

  AttachmentList::const_iterator iter = attachments.begin();
  const AttachmentList::const_iterator end = attachments.end();
  for (; iter != end; ++iter) {
    if (!WriteSingleAttachment(*iter))
      result_code = UNSPECIFIED_ERROR;
  }
  callback_task_runner_->PostTask(FROM_HERE, base::Bind(callback, result_code));
}

void OnDiskAttachmentStore::Drop(const AttachmentIdList& ids,
                                 const DropCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_);
  Result result_code = SUCCESS;
  leveldb::WriteOptions write_options = MakeWriteOptions();
  AttachmentIdList::const_iterator iter = ids.begin();
  const AttachmentIdList::const_iterator end = ids.end();
  for (; iter != end; ++iter) {
    leveldb::WriteBatch write_batch;
    write_batch.Delete(MakeDataKeyFromAttachmentId(*iter));
    write_batch.Delete(MakeMetadataKeyFromAttachmentId(*iter));

    leveldb::Status status = db_->Write(write_options, &write_batch);
    if (!status.ok()) {
      // DB::Delete doesn't check if record exists, it returns ok just like
      // AttachmentStore::Drop should.
      DVLOG(1) << "DB::Write failed: status=" << status.ToString();
      result_code = UNSPECIFIED_ERROR;
    }
  }
  callback_task_runner_->PostTask(FROM_HERE, base::Bind(callback, result_code));
}

AttachmentStore::Result OnDiskAttachmentStore::OpenOrCreate(
    const base::FilePath& path) {
  DCHECK(CalledOnValidThread());
  DCHECK(!db_);
  base::FilePath leveldb_path = path.Append(kLeveldbDirectory);

  leveldb::DB* db_raw;
  scoped_ptr<leveldb::DB> db;
  leveldb::Options options;
  options.create_if_missing = true;
  // TODO(pavely): crbug/424287 Consider adding info_log, block_cache and
  // filter_policy to options.
  leveldb::Status status =
      leveldb::DB::Open(options, leveldb_path.AsUTF8Unsafe(), &db_raw);
  if (!status.ok()) {
    DVLOG(1) << "DB::Open failed: status=" << status.ToString()
             << ", path=" << path.AsUTF8Unsafe();
    return UNSPECIFIED_ERROR;
  }

  db.reset(db_raw);

  attachment_store_pb::StoreMetadata metadata;
  status = ReadStoreMetadata(db.get(), &metadata);
  if (!status.ok() && !status.IsNotFound()) {
    DVLOG(1) << "ReadStoreMetadata failed: status=" << status.ToString();
    return UNSPECIFIED_ERROR;
  }
  if (status.IsNotFound()) {
    // Brand new database.
    metadata.set_schema_version(kCurrentSchemaVersion);
    status = WriteStoreMetadata(db.get(), metadata);
    if (!status.ok()) {
      DVLOG(1) << "WriteStoreMetadata failed: status=" << status.ToString();
      return UNSPECIFIED_ERROR;
    }
  }
  DCHECK(status.ok());

  // Upgrade code goes here.

  if (metadata.schema_version() != kCurrentSchemaVersion) {
    DVLOG(1) << "Unknown schema version: " << metadata.schema_version();
    return UNSPECIFIED_ERROR;
  }

  db_ = db.Pass();
  return SUCCESS;
}

scoped_ptr<Attachment> OnDiskAttachmentStore::ReadSingleAttachment(
    const AttachmentId& attachment_id) {
  scoped_ptr<Attachment> attachment;

  const std::string key = MakeDataKeyFromAttachmentId(attachment_id);
  const std::string metadata_key =
      MakeMetadataKeyFromAttachmentId(attachment_id);
  leveldb::Status status;
  std::string metadata_str;
  status = db_->Get(MakeMetadataReadOptions(), metadata_key, &metadata_str);
  if (!status.ok()) {
    DVLOG(1) << "DB::Get for metadata failed: status=" << status.ToString();
    return attachment.Pass();
  }
  attachment_store_pb::RecordMetadata record_metadata;
  if (!record_metadata.ParseFromString(metadata_str)) {
    DVLOG(1) << "RecordMetadata::ParseFromString failed";
    return attachment.Pass();
  }
  std::string data_str;
  status = db_->Get(MakeDataReadOptions(), key, &data_str);
  if (!status.ok()) {
    DVLOG(1) << "DB::Get for data failed: status=" << status.ToString();
    return attachment.Pass();
  }
  scoped_refptr<base::RefCountedMemory> data =
      base::RefCountedString::TakeString(&data_str);
  uint32_t crc32c = ComputeCrc32c(data);
  if (record_metadata.has_crc32c() && record_metadata.crc32c() != crc32c) {
    DVLOG(1) << "Attachment crc does not match";
    return attachment.Pass();
  }
  attachment.reset(
      new Attachment(Attachment::CreateFromParts(attachment_id, data, crc32c)));
  return attachment.Pass();
}

bool OnDiskAttachmentStore::WriteSingleAttachment(
    const Attachment& attachment) {
  const std::string metadata_key =
      MakeMetadataKeyFromAttachmentId(attachment.GetId());
  const std::string data_key = MakeDataKeyFromAttachmentId(attachment.GetId());

  std::string metadata_str;
  leveldb::Status status =
      db_->Get(MakeMetadataReadOptions(), metadata_key, &metadata_str);
  if (status.ok()) {
    // Entry exists, don't overwrite.
    return true;
  } else if (!status.IsNotFound()) {
    // Entry exists but failed to read.
    DVLOG(1) << "DB::Get failed: status=" << status.ToString();
    return false;
  }
  DCHECK(status.IsNotFound());

  leveldb::WriteBatch write_batch;
  // Write metadata.
  attachment_store_pb::RecordMetadata metadata;
  metadata.set_attachment_size(attachment.GetData()->size());
  metadata.set_crc32c(attachment.GetCrc32c());
  metadata_str = metadata.SerializeAsString();
  write_batch.Put(metadata_key, metadata_str);
  // Write data.
  scoped_refptr<base::RefCountedMemory> data = attachment.GetData();
  leveldb::Slice data_slice(data->front_as<char>(), data->size());
  write_batch.Put(data_key, data_slice);

  status = db_->Write(MakeWriteOptions(), &write_batch);
  if (!status.ok()) {
    // Failed to write.
    DVLOG(1) << "DB::Write failed: status=" << status.ToString();
    return false;
  }
  return true;
}

std::string OnDiskAttachmentStore::MakeDataKeyFromAttachmentId(
    const AttachmentId& attachment_id) {
  std::string key = kDataPrefix + attachment_id.GetProto().unique_id();
  return key;
}

std::string OnDiskAttachmentStore::MakeMetadataKeyFromAttachmentId(
    const AttachmentId& attachment_id) {
  std::string key = kMetadataPrefix + attachment_id.GetProto().unique_id();
  return key;
}

}  // namespace syncer
