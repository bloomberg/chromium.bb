// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/engine_file_requests_proxy.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace chrome_cleaner {

namespace {

void SaveFindFirstFileCallback(uint32_t* out_result,
                               LPWIN32_FIND_DATAW lpFindFileData,
                               FindFileHandle* out_handle,
                               base::OnceClosure quit_closure,
                               uint32_t result,
                               mojom::FindFileDataPtr data,
                               mojom::FindHandlePtr handle) {
  *out_result = result;
  // The layout for WIN32_FIND_DATAW is the same in the broker and the target
  // processes.
  memcpy(lpFindFileData, data->data.data(), sizeof(WIN32_FIND_DATAW));
  *out_handle = handle->find_handle;

  std::move(quit_closure).Run();
}

void SaveFindNextFileCallback(uint32_t* out_result,
                              LPWIN32_FIND_DATAW lpFindFileData,
                              base::OnceClosure quit_closure,
                              uint32_t result,
                              mojom::FindFileDataPtr data) {
  *out_result = result;
  // The layout for WIN32_FIND_DATAW is the same in the broker and the target
  // processes.
  memcpy(lpFindFileData, data->data.data(), sizeof(WIN32_FIND_DATAW));

  std::move(quit_closure).Run();
}

void SaveFindCloseCallback(uint32_t* out_result,
                           base::OnceClosure quit_closure,
                           uint32_t result) {
  *out_result = result;

  std::move(quit_closure).Run();
}

void SaveOpenReadOnlyFileCallback(base::win::ScopedHandle* result_holder,
                                  base::OnceClosure quit_closure,
                                  mojo::ScopedHandle handle) {
  HANDLE raw_handle = INVALID_HANDLE_VALUE;
  MojoResult mojo_result =
      mojo::UnwrapPlatformFile(std::move(handle), &raw_handle);
  LOG_IF(ERROR, mojo_result != MOJO_RESULT_OK)
      << "UnwrapPlatformFile failed " << mojo_result;
  result_holder->Set(raw_handle);
  std::move(quit_closure).Run();
}

}  // namespace

EngineFileRequestsProxy::EngineFileRequestsProxy(
    mojom::EngineFileRequestsAssociatedPtr file_requests_ptr,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : file_requests_ptr_(std::move(file_requests_ptr)),
      task_runner_(task_runner) {}

EngineFileRequestsProxy::~EngineFileRequestsProxy() = default;

uint32_t EngineFileRequestsProxy::FindFirstFile(
    const base::FilePath& path,
    LPWIN32_FIND_DATAW lpFindFileData,
    FindFileHandle* handle) {
  if (lpFindFileData == nullptr) {
    LOG(ERROR) << "FindFirstFileCallback got a null lpFindFileData";
    return SandboxErrorCode::NULL_DATA_HANDLE;
  }
  if (handle == nullptr) {
    LOG(ERROR) << "FindFirstFileCallback got a null handle";
    return SandboxErrorCode::NULL_FIND_HANDLE;
  }

  uint32_t result;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineFileRequestsProxy::SandboxFindFirstFile,
                     base::Unretained(this), path),
      base::BindOnce(&SaveFindFirstFileCallback, &result, lpFindFileData,
                     handle));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return call_status.error_code;
  }
  return result;
}

uint32_t EngineFileRequestsProxy::FindNextFile(
    FindFileHandle hFindFile,
    LPWIN32_FIND_DATAW lpFindFileData) {
  if (lpFindFileData == nullptr) {
    LOG(ERROR) << "FindNextFileCallback received a null lpFindFileData";
    return SandboxErrorCode::NULL_DATA_HANDLE;
  }

  uint32_t result;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineFileRequestsProxy::SandboxFindNextFile,
                     base::Unretained(this), hFindFile),
      base::BindOnce(&SaveFindNextFileCallback, &result, lpFindFileData));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return call_status.error_code;
  }
  return result;
}

uint32_t EngineFileRequestsProxy::FindClose(FindFileHandle hFindFile) {
  uint32_t result;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineFileRequestsProxy::SandboxFindClose,
                     base::Unretained(this), hFindFile),
      base::BindOnce(&SaveFindCloseCallback, &result));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return call_status.error_code;
  }
  return result;
}

base::win::ScopedHandle EngineFileRequestsProxy::OpenReadOnlyFile(
    const base::FilePath& path,
    uint32_t dwFlagsAndAttributes) {
  base::win::ScopedHandle handle;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineFileRequestsProxy::SandboxOpenReadOnlyFile,
                     base::Unretained(this), path, dwFlagsAndAttributes),
      base::BindOnce(&SaveOpenReadOnlyFileCallback, &handle));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR)
    return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
  return handle;
}

void EngineFileRequestsProxy::UnbindRequestsPtr() {
  file_requests_ptr_.reset();
}

MojoCallStatus EngineFileRequestsProxy::SandboxFindFirstFile(
    const base::FilePath& path,
    mojom::EngineFileRequests::SandboxFindFirstFileCallback result_callback) {
  if (!file_requests_ptr_.is_bound()) {
    LOG(ERROR) << "SandboxFindFirstFile called without bound pointer";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  file_requests_ptr_->SandboxFindFirstFile(path, std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineFileRequestsProxy::SandboxFindNextFile(
    FindFileHandle handle,
    mojom::EngineFileRequests::SandboxFindNextFileCallback result_callback) {
  if (!file_requests_ptr_.is_bound()) {
    LOG(ERROR) << "SandboxFindNextFile called without bound pointer";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  auto find_handle = mojom::FindHandle::New();
  find_handle->find_handle = handle;
  file_requests_ptr_->SandboxFindNextFile(std::move(find_handle),
                                          std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineFileRequestsProxy::SandboxFindClose(
    FindFileHandle handle,
    mojom::EngineFileRequests::SandboxFindCloseCallback result_callback) {
  if (!file_requests_ptr_.is_bound()) {
    LOG(ERROR) << "SandboxFindClose called without bound pointer";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  auto find_handle = mojom::FindHandle::New();
  find_handle->find_handle = handle;
  file_requests_ptr_->SandboxFindClose(std::move(find_handle),
                                       std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineFileRequestsProxy::SandboxOpenReadOnlyFile(
    const base::FilePath& path,
    uint32_t dwFlagsAndAttributes,
    mojom::EngineFileRequests::SandboxOpenReadOnlyFileCallback
        result_callback) {
  if (!file_requests_ptr_.is_bound()) {
    LOG(ERROR) << "SandboxOpenReadOnlyFile called without bound pointer";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  file_requests_ptr_->SandboxOpenReadOnlyFile(path, dwFlagsAndAttributes,
                                              std::move(result_callback));

  return MojoCallStatus::Success();
}

}  // namespace chrome_cleaner
