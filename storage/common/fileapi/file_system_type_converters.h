// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_FILEAPI_FILE_SYSTEM_TYPE_CONVERTERS_H_
#define STORAGE_COMMON_FILEAPI_FILE_SYSTEM_TYPE_CONVERTERS_H_

#include "storage/common/fileapi/file_system_types.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom.h"

namespace mojo {

template <>
struct TypeConverter<blink::mojom::FileSystemType, storage::FileSystemType> {
  static blink::mojom::FileSystemType Convert(
      const storage::FileSystemType& type) {
    switch (type) {
      case storage::FileSystemType::kFileSystemTypeTemporary:
        return blink::mojom::FileSystemType::kTemporary;
      case storage::FileSystemType::kFileSystemTypePersistent:
        return blink::mojom::FileSystemType::kPersistent;
      case storage::FileSystemType::kFileSystemTypeIsolated:
        return blink::mojom::FileSystemType::kIsolated;
      case storage::FileSystemType::kFileSystemTypeExternal:
        return blink::mojom::FileSystemType::kExternal;
      // Internal enum types
      case storage::FileSystemType::kFileSystemTypeUnknown:
      case storage::FileSystemType::kFileSystemInternalTypeEnumStart:
      case storage::FileSystemType::kFileSystemTypeTest:
      case storage::FileSystemType::kFileSystemTypeNativeLocal:
      case storage::FileSystemType::kFileSystemTypeRestrictedNativeLocal:
      case storage::FileSystemType::kFileSystemTypeDragged:
      case storage::FileSystemType::kFileSystemTypeNativeMedia:
      case storage::FileSystemType::kFileSystemTypeDeviceMedia:
      case storage::FileSystemType::kFileSystemTypeDrive:
      case storage::FileSystemType::kFileSystemTypeSyncable:
      case storage::FileSystemType::kFileSystemTypeSyncableForInternalSync:
      case storage::FileSystemType::kFileSystemTypeNativeForPlatformApp:
      case storage::FileSystemType::kFileSystemTypeForTransientFile:
      case storage::FileSystemType::kFileSystemTypePluginPrivate:
      case storage::FileSystemType::kFileSystemTypeCloudDevice:
      case storage::FileSystemType::kFileSystemTypeProvided:
      case storage::FileSystemType::kFileSystemTypeDeviceMediaAsFileStorage:
      case storage::FileSystemType::kFileSystemTypeArcContent:
      case storage::FileSystemType::kFileSystemTypeArcDocumentsProvider:
      case storage::FileSystemType::kFileSystemInternalTypeEnumEnd:
        NOTREACHED();
        return blink::mojom::FileSystemType::kTemporary;
    }
    NOTREACHED();
    return blink::mojom::FileSystemType::kTemporary;
  }
};

template <>
struct TypeConverter<storage::FileSystemType, blink::mojom::FileSystemType> {
  static storage::FileSystemType Convert(
      const blink::mojom::FileSystemType& type) {
    switch (type) {
      case blink::mojom::FileSystemType::kTemporary:
        return storage::FileSystemType::kFileSystemTypeTemporary;
      case blink::mojom::FileSystemType::kPersistent:
        return storage::FileSystemType::kFileSystemTypePersistent;
      case blink::mojom::FileSystemType::kIsolated:
        return storage::FileSystemType::kFileSystemTypeIsolated;
      case blink::mojom::FileSystemType::kExternal:
        return storage::FileSystemType::kFileSystemTypeExternal;
    }
    NOTREACHED();
    return storage::FileSystemType::kFileSystemTypeTemporary;
  }
};

template <>
struct TypeConverter<blink::mojom::FileSystemInfoPtr, storage::FileSystemInfo> {
  static blink::mojom::FileSystemInfoPtr Convert(
      const storage::FileSystemInfo& info) {
    return blink::mojom::FileSystemInfo::New(
        info.name, info.root_url,
        mojo::ConvertTo<blink::mojom::FileSystemType>(info.mount_type));
  }
};

template <>
struct TypeConverter<storage::FileSystemInfo, blink::mojom::FileSystemInfoPtr> {
  static storage::FileSystemInfo Convert(
      const blink::mojom::FileSystemInfoPtr& info) {
    return storage::FileSystemInfo(
        info->name, info->root_url,
        mojo::ConvertTo<storage::FileSystemType>(info->mount_type));
  }
};

}  // namespace mojo

#endif  // STORAGE_COMMON_FILEAPI_FILE_SYSTEM_TYPE_CONVERTERS_H_
