// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/scopes/leveldb_scopes_coding.h"

#include <utility>

#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"

namespace content {
namespace leveldb_scopes {

std::tuple<bool, int64_t> ParseScopeMetadataId(
    leveldb::Slice key,
    base::span<const uint8_t> scopes_prefix) {
  int64_t scope_id = 0;
  size_t prefix_size = scopes_prefix.size() + /*sizeof(kScopesMetadataByte)=*/1;

  // The key must be larger than the prefix.
  if (key.size() <= prefix_size)
    return std::make_tuple(false, 0);

  // The key must start with the prefix.
  if (!key.starts_with(
          leveldb::Slice(reinterpret_cast<const char*>(scopes_prefix.data()),
                         scopes_prefix.size())))
    return std::make_tuple(false, 0);

  // The metadata byte must be correct.
  if (key[scopes_prefix.size()] != kScopesMetadataByte)
    return std::make_tuple(false, 0);

  base::StringPiece part(key.data() + prefix_size, key.size() - prefix_size);
  bool decode_success = DecodeVarInt(&part, &scope_id);
  return std::make_tuple(decode_success, scope_id);
}

}  // namespace leveldb_scopes

leveldb::Slice ScopesEncoder::GlobalMetadataKey(
    base::span<const uint8_t> scopes_prefix) {
  key_buffer_.clear();
  key_buffer_.assign(reinterpret_cast<const char*>(scopes_prefix.data()),
                     scopes_prefix.size());
  key_buffer_.push_back(leveldb_scopes::kGlobalMetadataByte);
  return leveldb::Slice(key_buffer_);
}

leveldb::Slice ScopesEncoder::ScopeMetadataKey(
    base::span<const uint8_t> scopes_prefix,
    int64_t scope_number) {
  key_buffer_.clear();
  key_buffer_.assign(reinterpret_cast<const char*>(scopes_prefix.data()),
                     scopes_prefix.size());
  key_buffer_.push_back(leveldb_scopes::kScopesMetadataByte);
  EncodeVarInt(scope_number, &key_buffer_);
  return leveldb::Slice(key_buffer_);
}

leveldb::Slice ScopesEncoder::ScopeMetadataPrefix(
    base::span<const uint8_t> scopes_prefix) {
  key_buffer_.clear();
  key_buffer_.assign(reinterpret_cast<const char*>(scopes_prefix.data()),
                     scopes_prefix.size());
  key_buffer_.push_back(leveldb_scopes::kScopesMetadataByte);
  return leveldb::Slice(key_buffer_);
}

leveldb::Slice ScopesEncoder::TasksKeyPrefix(base::span<const uint8_t> prefix) {
  key_buffer_.clear();
  key_buffer_.assign(reinterpret_cast<const char*>(prefix.data()),
                     prefix.size());
  key_buffer_.push_back(leveldb_scopes::kLogByte);
  return leveldb::Slice(key_buffer_);
}

leveldb::Slice ScopesEncoder::TasksKeyPrefix(base::span<const uint8_t> prefix,
                                             int64_t scope_number) {
  key_buffer_.clear();
  key_buffer_.assign(reinterpret_cast<const char*>(prefix.data()),
                     prefix.size());
  key_buffer_.push_back(leveldb_scopes::kLogByte);
  EncodeVarInt(scope_number, &key_buffer_);
  return leveldb::Slice(key_buffer_);
}

leveldb::Slice ScopesEncoder::UndoTaskKeyPrefix(
    base::span<const uint8_t> prefix,
    int64_t scope_number) {
  key_buffer_.clear();
  key_buffer_.assign(reinterpret_cast<const char*>(prefix.data()),
                     prefix.size());
  key_buffer_.push_back(leveldb_scopes::kLogByte);
  EncodeVarInt(scope_number, &key_buffer_);
  key_buffer_.push_back(leveldb_scopes::kUndoTasksByte);
  return leveldb::Slice(key_buffer_);
}

leveldb::Slice ScopesEncoder::CleanupTaskKeyPrefix(
    base::span<const uint8_t> prefix,
    int64_t scope_number) {
  key_buffer_.clear();
  key_buffer_.assign(reinterpret_cast<const char*>(prefix.data()),
                     prefix.size());
  key_buffer_.push_back(leveldb_scopes::kLogByte);
  EncodeVarInt(scope_number, &key_buffer_);
  key_buffer_.push_back(leveldb_scopes::kCleanupTasksByte);
  return leveldb::Slice(key_buffer_);
}

leveldb::Slice ScopesEncoder::UndoTaskKey(
    base::span<const uint8_t> scopes_prefix,
    int64_t scope_number,
    int64_t undo_sequence_number) {
  UndoTaskKeyPrefix(scopes_prefix, scope_number);
  EncodeInt(undo_sequence_number, &key_buffer_);
  return leveldb::Slice(key_buffer_);
}

leveldb::Slice ScopesEncoder::CleanupTaskKey(
    base::span<const uint8_t> scopes_prefix,
    int64_t scope_number,
    int64_t cleanup_sequence_number) {
  CleanupTaskKeyPrefix(scopes_prefix, scope_number);
  EncodeInt(cleanup_sequence_number, &key_buffer_);
  return leveldb::Slice(key_buffer_);
}

std::tuple<bool, int64_t> ParseScopeMetadata(leveldb::Slice key,
                                             int prefix_bytes_to_skip) {
  int64_t scope_id = 0;
  DCHECK_GE(prefix_bytes_to_skip, 0);
  if (key.size() < static_cast<size_t>(prefix_bytes_to_skip + 1))
    return std::make_tuple(false, 0);
  base::StringPiece part(key.data() + prefix_bytes_to_skip,
                         key.size() - prefix_bytes_to_skip);
  bool success = DecodeVarInt(&part, &scope_id);
  return std::make_tuple(success, scope_id);
}

}  // namespace content
