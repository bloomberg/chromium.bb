// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_file_io.h"

#include "base/logging.h"
#include "third_party/ppapi/c/pp_completion_callback.h"
#include "third_party/ppapi/c/pp_errors.h"
#include "third_party/ppapi/c/ppb_file_io.h"
#include "third_party/ppapi/c/ppb_file_io_trusted.h"
#include "webkit/glue/plugins/pepper_file_ref.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/plugins/pepper_resource_tracker.h"

namespace pepper {

namespace {

PP_Resource Create(PP_Module module_id) {
  PluginModule* module = PluginModule::FromPPModule(module_id);
  if (!module)
    return 0;

  FileIO* file_io = new FileIO(module);
  return file_io->GetReference();
}

bool IsFileIO(PP_Resource resource) {
  return !!Resource::GetAs<FileIO>(resource);
}

int32_t Open(PP_Resource file_io_id,
             PP_Resource file_ref_id,
             int32_t open_flags,
             PP_CompletionCallback callback) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<FileRef> file_ref(Resource::GetAs<FileRef>(file_ref_id));
  if (!file_ref)
    return PP_ERROR_BADRESOURCE;

  return file_io->Open(file_ref, open_flags, callback);
}

int32_t Query(PP_Resource file_io_id,
              PP_FileInfo* info,
              PP_CompletionCallback callback) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;

  return file_io->Query(info, callback);
}

int32_t Touch(PP_Resource file_io_id,
              PP_Time last_access_time,
              PP_Time last_modified_time,
              PP_CompletionCallback callback) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;

  return file_io->Touch(last_access_time, last_modified_time, callback);
}

int32_t Read(PP_Resource file_io_id,
             int64_t offset,
             char* buffer,
             int32_t bytes_to_read,
             PP_CompletionCallback callback) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;

  return file_io->Read(offset, buffer, bytes_to_read, callback);
}

int32_t Write(PP_Resource file_io_id,
              int64_t offset,
              const char* buffer,
              int32_t bytes_to_write,
              PP_CompletionCallback callback) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;

  return file_io->Write(offset, buffer, bytes_to_write, callback);
}

int32_t SetLength(PP_Resource file_io_id,
                  int64_t length,
                  PP_CompletionCallback callback) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;

  return file_io->SetLength(length, callback);
}

int32_t Flush(PP_Resource file_io_id,
              PP_CompletionCallback callback) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;

  return file_io->Flush(callback);
}

void Close(PP_Resource file_io_id) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return;

  file_io->Close();
}

const PPB_FileIO ppb_fileio = {
  &Create,
  &IsFileIO,
  &Open,
  &Query,
  &Touch,
  &Read,
  &Write,
  &SetLength,
  &Flush,
  &Close
};

int32_t GetOSFileDescriptor(PP_Resource file_io_id) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;

  return file_io->GetOSFileDescriptor();
}

int32_t WillWrite(PP_Resource file_io_id,
                  int64_t offset,
                  int32_t bytes_to_write,
                  PP_CompletionCallback callback) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;

  return file_io->WillWrite(offset, bytes_to_write, callback);
}

int32_t WillSetLength(PP_Resource file_io_id,
                      int64_t length,
                      PP_CompletionCallback callback) {
  scoped_refptr<FileIO> file_io(Resource::GetAs<FileIO>(file_io_id));
  if (!file_io)
    return PP_ERROR_BADRESOURCE;

  return file_io->WillSetLength(length, callback);
}

const PPB_FileIOTrusted ppb_fileiotrusted = {
  &GetOSFileDescriptor,
  &WillWrite,
  &WillSetLength
};

}  // namespace

FileIO::FileIO(PluginModule* module) : Resource(module) {
}

FileIO::~FileIO() {
}

// static
const PPB_FileIO* FileIO::GetInterface() {
  return &ppb_fileio;
}

// static
const PPB_FileIOTrusted* FileIO::GetTrustedInterface() {
  return &ppb_fileiotrusted;
}

int32_t FileIO::Open(FileRef* file_ref,
                     int32_t open_flags,
                     PP_CompletionCallback callback) {
  NOTIMPLEMENTED();  // TODO(darin): Implement me!
  return PP_ERROR_FAILED;
}

int32_t FileIO::Query(PP_FileInfo* info,
                      PP_CompletionCallback callback) {
  NOTIMPLEMENTED();  // TODO(darin): Implement me!
  return PP_ERROR_FAILED;
}

int32_t FileIO::Touch(PP_Time last_access_time,
                      PP_Time last_modified_time,
                      PP_CompletionCallback callback) {
  NOTIMPLEMENTED();  // TODO(darin): Implement me!
  return PP_ERROR_FAILED;
}

int32_t FileIO::Read(int64_t offset,
                     char* buffer,
                     int32_t bytes_to_read,
                     PP_CompletionCallback callback) {
  NOTIMPLEMENTED();  // TODO(darin): Implement me!
  return PP_ERROR_FAILED;
}

int32_t FileIO::Write(int64_t offset,
                      const char* buffer,
                      int32_t bytes_to_write,
                      PP_CompletionCallback callback) {
  NOTIMPLEMENTED();  // TODO(darin): Implement me!
  return PP_ERROR_FAILED;
}

int32_t FileIO::SetLength(int64_t length,
                          PP_CompletionCallback callback) {
  NOTIMPLEMENTED();  // TODO(darin): Implement me!
  return PP_ERROR_FAILED;
}

int32_t FileIO::Flush(PP_CompletionCallback callback) {
  NOTIMPLEMENTED();  // TODO(darin): Implement me!
  return PP_ERROR_FAILED;
}

void FileIO::Close() {
  NOTIMPLEMENTED();  // TODO(darin): Implement me!
}

int32_t FileIO::GetOSFileDescriptor() {
  NOTIMPLEMENTED();  // TODO(darin): Implement me!
  return PP_ERROR_FAILED;
}

int32_t FileIO::WillWrite(int64_t offset,
                          int32_t bytes_to_write,
                          PP_CompletionCallback callback) {
  NOTIMPLEMENTED();  // TODO(darin): Implement me!
  return PP_ERROR_FAILED;
}

int32_t FileIO::WillSetLength(int64_t length,
                              PP_CompletionCallback callback) {
  NOTIMPLEMENTED();  // TODO(darin): Implement me!
  return PP_ERROR_FAILED;
}

}  // namespace pepper
