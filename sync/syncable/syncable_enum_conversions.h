// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_SYNCABLE_ENUM_CONVERSIONS_H_
#define SYNC_SYNCABLE_SYNCABLE_ENUM_CONVERSIONS_H_

// Keep this file in sync with entry_kernel.h.

#include "sync/base/sync_export.h"
#include "sync/syncable/entry_kernel.h"

// Utility functions to get the string equivalent for some syncable
// enums.

namespace syncer {
namespace syncable {

// The returned strings (which don't have to be freed) are in ASCII.
// The result of passing in an invalid enum value is undefined.

SYNC_EXPORT_PRIVATE const char* GetMetahandleFieldString(
    MetahandleField metahandle_field);

SYNC_EXPORT_PRIVATE const char* GetBaseVersionString(BaseVersion base_version);

SYNC_EXPORT_PRIVATE const char* GetInt64FieldString(Int64Field int64_field);

SYNC_EXPORT_PRIVATE const char* GetTimeFieldString(TimeField time_field);

SYNC_EXPORT_PRIVATE const char* GetIdFieldString(IdField id_field);

SYNC_EXPORT_PRIVATE const char* GetIndexedBitFieldString(
    IndexedBitField indexed_bit_field);

SYNC_EXPORT_PRIVATE const char* GetIsDelFieldString(IsDelField is_del_field);

SYNC_EXPORT_PRIVATE const char* GetBitFieldString(BitField bit_field);

SYNC_EXPORT_PRIVATE const char* GetStringFieldString(StringField string_field);

SYNC_EXPORT_PRIVATE const char* GetProtoFieldString(ProtoField proto_field);

SYNC_EXPORT_PRIVATE const char* GetUniquePositionFieldString(
    UniquePositionField position_field);

SYNC_EXPORT_PRIVATE const char* GetAttachmentMetadataFieldString(
    AttachmentMetadataField attachment_metadata_field);

SYNC_EXPORT_PRIVATE const char* GetBitTempString(BitTemp bit_temp);

}  // namespace syncable
}  // namespace syncer

#endif  // SYNC_SYNCABLE_SYNCABLE_ENUM_CONVERSIONS_H_
