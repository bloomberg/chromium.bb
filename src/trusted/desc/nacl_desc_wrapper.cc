// Copyright (c) 2012 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <new>
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/public/imc_types.h"
#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_conn_cap.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_invalid.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/desc/nacl_desc_sync_socket.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"
#include "native_client/src/trusted/nacl_base/nacl_refcount.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"

// TODO(polina): follow the style guide and replace "nhdp" and "ndiodp" with
//               "host_desc" and "io_desc"

namespace {

struct NaClDesc* OpenHostFileCommon(const char* fname, int flags, int mode) {
  struct NaClHostDesc* nhdp =
    reinterpret_cast<struct NaClHostDesc*>(calloc(1, sizeof(*nhdp)));
  if (NULL == nhdp) {
    return NULL;
  }
  if (0 != NaClHostDescOpen(nhdp, fname, flags, mode)) {
    free(nhdp);
    return NULL;
  }
  struct NaClDescIoDesc* ndiodp = NaClDescIoDescMake(nhdp);
  if (NULL == ndiodp) {
    if (0 != NaClHostDescClose(nhdp)) {
      NaClLog(LOG_FATAL, "OpenHostFileCommon: NaClHostDescClose failed\n");
    }
    free(nhdp);
    return NULL;
  }
  return reinterpret_cast<struct NaClDesc*>(ndiodp);
}

}  // namespace

namespace nacl {

// Descriptor creation and manipulation sometimes requires additional
// state.  Therefore, we create an object that encapsulates that
// state.
class DescWrapperCommon {
  friend class DescWrapperFactory;

 public:
  typedef uint32_t RefCountType;

  // Inform clients that the object was successfully initialized.
  bool is_initialized() const { return is_initialized_; }

  // Manipulate reference count
  DescWrapperCommon* Ref() {
    // TODO(sehr): replace with a reference count class when we have one.
    NaClXMutexLock(&ref_count_mu_);
    if (std::numeric_limits<RefCountType>::max() == ref_count_) {
      NaClLog(LOG_FATAL, "DescWrapperCommon ref count overflow\n");
    }
    ++ref_count_;
    NaClXMutexUnlock(&ref_count_mu_);
    return this;
  }

  void Unref() {
    NaClXMutexLock(&ref_count_mu_);
    if (0 == ref_count_) {
      NaClLog(LOG_FATAL, "DescWrapperCommon ref count already zero\n");
    }
    --ref_count_;
    bool destroy = (0 == ref_count_);
    NaClXMutexUnlock(&ref_count_mu_);
    if (destroy) {
      delete this;
    }
  }

 private:
  DescWrapperCommon() : is_initialized_(false), ref_count_(1) {
    NaClXMutexCtor(&ref_count_mu_);
  }
  ~DescWrapperCommon() {
    NaClMutexDtor(&ref_count_mu_);
  }

  // Set up the state.  Returns true on success.
  bool Init();

  // Boolean to indicate the object was successfully initialized.
  bool is_initialized_;
  // The reference count and the mutex to protect it.
  RefCountType ref_count_;
  struct NaClMutex ref_count_mu_;

  DISALLOW_COPY_AND_ASSIGN(DescWrapperCommon);
};

bool DescWrapperCommon::Init() {
  // Successfully initialized.
  is_initialized_ = true;
  return true;
}

DescWrapperFactory::DescWrapperFactory() {
  common_data_ = new(std::nothrow) DescWrapperCommon();
  if (NULL == common_data_) {
    return;
  }
  if (!common_data_->Init()) {
    delete common_data_;
    common_data_ = NULL;
  }
}

DescWrapperFactory::~DescWrapperFactory() {
  if (NULL != common_data_) {
    common_data_->Unref();
  }
}

int DescWrapperFactory::MakeBoundSock(DescWrapper* pair[2]) {
  CHECK(common_data_->is_initialized());

  struct NaClDesc* descs[2] = { NULL, NULL };
  DescWrapper* tmp_pair[2] = { NULL, NULL };

  int ret = NaClCommonDescMakeBoundSock(descs);
  if (0 != ret) {
    return ret;
  }
  tmp_pair[0] = new(std::nothrow) DescWrapper(common_data_, descs[0]);
  if (NULL == tmp_pair[0]) {
    goto cleanup;
  }
  descs[0] = NULL;  // DescWrapper took ownership of descs[0].
  tmp_pair[1] = new(std::nothrow) DescWrapper(common_data_, descs[1]);
  if (NULL == tmp_pair[1]) {
    goto cleanup;
  }
  descs[1] = NULL;  // DescWrapper took ownership of descs[1].
  pair[0] = tmp_pair[0];
  pair[1] = tmp_pair[1];
  return 0;

 cleanup:
  NaClDescSafeUnref(descs[0]);
  NaClDescSafeUnref(descs[1]);
  delete tmp_pair[0];
  delete tmp_pair[1];
  return -1;
}

DescWrapper* DescWrapperFactory::MakeImcSock(NaClHandle handle) {
  struct NaClDescImcDesc* desc =
    reinterpret_cast<NaClDescImcDesc*>(calloc(1, sizeof *desc));
  if (NULL == desc) {
    return NULL;
  }
  if (!NaClDescImcDescCtor(desc, handle)) {
    free(desc);
    return NULL;
  }

  return MakeGenericCleanup(reinterpret_cast<struct NaClDesc*>(desc));
}

DescWrapper* DescWrapperFactory::ImportShmHandle(NaClHandle handle,
                                                 size_t size) {
  struct NaClDesc *desc = NaClDescImcShmMake(handle, size);
  if (desc == NULL) {
    return NULL;
  }
  return MakeGenericCleanup(desc);
}

DescWrapper* DescWrapperFactory::ImportSyncSocketHandle(NaClHandle handle) {
  struct NaClDesc *desc = NaClDescSyncSocketMake(handle);
  if (desc == NULL) {
    return NULL;
  }
  return MakeGenericCleanup(desc);
}

DescWrapper* DescWrapperFactory::MakeGeneric(struct NaClDesc* desc) {
  CHECK(common_data_->is_initialized());
  return new(std::nothrow) DescWrapper(common_data_, desc);
}


DescWrapper* DescWrapperFactory::MakeGenericCleanup(struct NaClDesc* desc) {
  CHECK(common_data_->is_initialized());
  DescWrapper* wrapper = new(std::nothrow) DescWrapper(common_data_, desc);
  if (NULL != wrapper) {
      return wrapper;
  }
  NaClDescSafeUnref(desc);
  return NULL;
}

int DescWrapperFactory::MakeSocketPair(DescWrapper* pair[2]) {
  CHECK(common_data_->is_initialized());
  struct NaClDesc* descs[2] = { NULL, NULL };
  DescWrapper* tmp_pair[2] = { NULL, NULL };

  int ret = NaClCommonDescSocketPair(descs);
  if (0 != ret) {
    return ret;
  }
  tmp_pair[0] = new(std::nothrow) DescWrapper(common_data_, descs[0]);
  if (NULL == tmp_pair[0]) {
    goto cleanup;
  }
  descs[0] = NULL;  // DescWrapper took ownership of descs[0].
  tmp_pair[1] = new(std::nothrow) DescWrapper(common_data_, descs[1]);
  if (NULL == tmp_pair[1]) {
    goto cleanup;
  }
  descs[1] = NULL;  // DescWrapper took ownership of descs[1].
  pair[0] = tmp_pair[0];
  pair[1] = tmp_pair[1];
  return 0;

 cleanup:
  NaClDescSafeUnref(descs[0]);
  NaClDescSafeUnref(descs[1]);
  delete tmp_pair[0];
  delete tmp_pair[1];
  return -1;
}

DescWrapper* DescWrapperFactory::MakeFileDesc(int host_os_desc, int mode) {
  struct NaClDesc* desc = NaClDescIoDescFromDescAllocCtor(host_os_desc, mode);
  if (NULL == desc) {
    return NULL;
  }
  return MakeGenericCleanup(desc);
}

DescWrapper* DescWrapperFactory::OpenHostFile(const char* fname,
                                              int flags,
                                              int mode) {
  struct NaClDesc* desc = OpenHostFileCommon(fname, flags, mode);
  if (NULL == desc) {
    return NULL;
  }
  return MakeGenericCleanup(desc);
}

DescWrapper* DescWrapperFactory::MakeInvalid() {
  struct NaClDescInvalid *desc =
      const_cast<NaClDescInvalid*>(NaClDescInvalidMake());
  if (NULL == desc) {
    return NULL;
  }

  return MakeGenericCleanup(reinterpret_cast<struct NaClDesc*>(desc));
}

DescWrapper::DescWrapper(DescWrapperCommon* common_data,
                         struct NaClDesc* desc)
      : common_data_(common_data), desc_(desc) {
  // DescWrapper takes ownership of desc from caller, so no Ref call here.
  if (NULL != common_data_) {
    common_data_->Ref();
  }
}

DescWrapper::~DescWrapper() {
  if (NULL != common_data_) {
    common_data_->Unref();
  }
  NaClDescSafeUnref(desc_);
  desc_ = NULL;
}

ssize_t DescWrapper::Read(void* buf, size_t len) {
  return reinterpret_cast<struct NaClDescVtbl const *>(desc_->base.vtbl)->
      Read(desc_, buf, len);
}

ssize_t DescWrapper::Write(const void* buf, size_t len) {
  return reinterpret_cast<struct NaClDescVtbl const *>(desc_->base.vtbl)->
      Write(desc_, buf, len);
}

nacl_off64_t DescWrapper::Seek(nacl_off64_t offset, int whence) {
  return reinterpret_cast<struct NaClDescVtbl const *>(desc_->base.vtbl)->
      Seek(desc_,
           offset,
           whence);
}

int DescWrapper::Fstat(struct nacl_abi_stat* statbuf) {
  return reinterpret_cast<struct NaClDescVtbl const *>(desc_->base.vtbl)->
      Fstat(desc_, statbuf);
}

int DescWrapper::Close() {
  NaClRefCountUnref(&desc_->base);
  return 0;
}

ssize_t DescWrapper::Getdents(void* dirp, size_t count) {
  return reinterpret_cast<struct NaClDescVtbl const *>(desc_->base.vtbl)->
      Getdents(desc_,
               dirp,
               count);
}

int DescWrapper::Lock() {
  return reinterpret_cast<struct NaClDescVtbl const *>(desc_->base.vtbl)->
      Lock(desc_);
}

int DescWrapper::TryLock() {
  return reinterpret_cast<struct NaClDescVtbl const *>(desc_->base.vtbl)->
      TryLock(desc_);
}

int DescWrapper::Unlock() {
  return reinterpret_cast<struct NaClDescVtbl const *>(desc_->base.vtbl)->
      Unlock(desc_);
}

int DescWrapper::Wait(DescWrapper* mutex) {
  return reinterpret_cast<struct NaClDescVtbl const *>(desc_->base.vtbl)->
      Wait(desc_, mutex->desc_);
}

int DescWrapper::TimedWaitAbs(DescWrapper* mutex,
                              struct nacl_abi_timespec* ts) {
  return reinterpret_cast<struct NaClDescVtbl const *>(desc_->base.vtbl)->
      TimedWaitAbs(desc_,
                   mutex->desc_,
                   ts);
}

int DescWrapper::Signal() {
  return reinterpret_cast<struct NaClDescVtbl const *>(desc_->base.vtbl)->
      Signal(desc_);
}

int DescWrapper::Broadcast() {
  return reinterpret_cast<struct NaClDescVtbl const *>(desc_->base.vtbl)->
      Broadcast(desc_);
}

ssize_t DescWrapper::SendMsg(const MsgHeader* dgram, int flags) {
  struct NaClImcTypedMsgHdr header;
  ssize_t ret = -NACL_ABI_ENOMEM;
  nacl_abi_size_t diov_length = dgram->iov_length;
  nacl_abi_size_t ddescv_length = dgram->ndescv_length;
  nacl_abi_size_t i;

  // Initialize to allow simple cleanups.
  header.ndescv = NULL;
  // Allocate and copy IOV.
  if (NACL_ABI_SIZE_T_MAX / sizeof(NaClImcMsgIoVec) <= diov_length) {
    goto cleanup;
  }
  header.iov = reinterpret_cast<NaClImcMsgIoVec*>(
      calloc(diov_length, sizeof(*(header.iov))));
  if (NULL == header.iov) {
    goto cleanup;
  }
  header.iov_length = diov_length;
  for (i = 0; i < dgram->iov_length; ++i) {
    header.iov[i].base = dgram->iov[i].base;
    header.iov[i].length = dgram->iov[i].length;
  }
  // Allocate and copy the descriptor vector, removing DescWrappers.
  if (NACL_HANDLE_COUNT_MAX < dgram->ndescv_length) {
    goto cleanup;
  }
  if (NACL_ABI_SIZE_T_MAX / sizeof(header.ndescv[0]) <= ddescv_length) {
    goto cleanup;
  }
  header.ndescv = reinterpret_cast<NaClDesc**>(
      calloc(ddescv_length, sizeof(*(header.ndescv))));
  if (NULL == header.iov) {
    goto cleanup;
  }
  header.ndesc_length = ddescv_length;
  for (i = 0; i < dgram->ndescv_length; ++i) {
    header.ndescv[i] = dgram->ndescv[i]->desc_;
  }
  // Send the message.
  ret = NACL_VTBL(NaClDesc, desc_)->SendMsg(desc_, &header, flags);

 cleanup:
  free(header.ndescv);
  free(header.iov);
  return ret;
}

ssize_t DescWrapper::RecvMsg(MsgHeader* dgram, int flags,
                             struct NaClDescQuotaInterface *quota_interface) {
  struct NaClImcTypedMsgHdr header;
  ssize_t ret = -NACL_ABI_ENOMEM;
  nacl_abi_size_t diov_length = dgram->iov_length;
  nacl_abi_size_t ddescv_length = dgram->ndescv_length;
  nacl_abi_size_t i;

  // Initialize to allow simple cleanups.
  header.iov = NULL;
  header.ndescv = NULL;
  for (i = 0; i < dgram->ndescv_length; ++i) {
    dgram->ndescv[i] = NULL;
  }

  // Allocate and copy the IOV.
  if (NACL_ABI_SIZE_T_MAX / sizeof(NaClImcMsgIoVec) <= diov_length) {
    goto cleanup;
  }
  header.iov = reinterpret_cast<NaClImcMsgIoVec*>(
      calloc(diov_length, sizeof(*(header.iov))));
  if (NULL == header.iov) {
    goto cleanup;
  }
  header.iov_length = diov_length;
  for (i = 0; i < dgram->iov_length; ++i) {
    header.iov[i].base = dgram->iov[i].base;
    header.iov[i].length = dgram->iov[i].length;
  }
  // Allocate and copy the descriptor vector.
  if (NACL_HANDLE_COUNT_MAX < dgram->ndescv_length) {
    goto cleanup;
  }
  if (NACL_ABI_SIZE_T_MAX / sizeof(header.ndescv[0]) <= ddescv_length) {
    goto cleanup;
  }
  header.ndescv = reinterpret_cast<NaClDesc**>(
      calloc(ddescv_length, sizeof(*(header.ndescv))));
  if (NULL == header.ndescv) {
    goto cleanup;
  }
  header.ndesc_length = ddescv_length;
  // Receive the message.
  ret = NACL_VTBL(NaClDesc, desc_)->RecvMsg(desc_, &header, flags,
                                            quota_interface);
  if (ret < 0) {
    goto cleanup;
  }
  dgram->ndescv_length = header.ndesc_length;
  dgram->flags = header.flags;
  // Copy the descriptors, creating new DescWrappers around them.
  for (i = 0; i < header.ndesc_length; ++i) {
    dgram->ndescv[i] =
        new(std::nothrow) DescWrapper(common_data_, header.ndescv[i]);
    if (NULL == dgram->ndescv[i]) {
      goto cleanup;
    }
  }
  free(header.ndescv);
  free(header.iov);
  return ret;

 cleanup:
  for (i = 0; i < ddescv_length; ++i) {
    delete dgram->ndescv[i];
  }
  free(header.ndescv);
  free(header.iov);
  return ret;
}

DescWrapper* DescWrapper::Connect() {
  struct NaClDesc* connected_desc;
  int rv = reinterpret_cast<struct NaClDescVtbl const *>(desc_->base.vtbl)->
      ConnectAddr(desc_,
                  &connected_desc);
  if (0 != rv) {
    // Connect failed.
    return NULL;
  }
  DescWrapper* wrapper =
      new(std::nothrow) DescWrapper(common_data_, connected_desc);
  if (NULL == wrapper) {
    NaClDescUnref(connected_desc);
  }
  return wrapper;
}

DescWrapper* DescWrapper::Accept() {
  struct NaClDesc* connected_desc;
  int rv = reinterpret_cast<struct NaClDescVtbl const *>(desc_->base.vtbl)->
      AcceptConn(desc_, &connected_desc);
  if (0 != rv) {
    // Accept failed.
    return NULL;
  }
  DescWrapper* wrapper =
      new(std::nothrow) DescWrapper(common_data_, connected_desc);
  if (NULL == wrapper) {
    NaClDescUnref(connected_desc);
  }
  return wrapper;
}

int DescWrapper::Post() {
  return reinterpret_cast<struct NaClDescVtbl const *>(desc_->base.vtbl)->
      Post(desc_);
}

int DescWrapper::SemWait() {
  return reinterpret_cast<struct NaClDescVtbl const *>(desc_->base.vtbl)->
      SemWait(desc_);
}

int DescWrapper::GetValue() {
  return reinterpret_cast<struct NaClDescVtbl const *>(desc_->base.vtbl)->
      GetValue(desc_);
}

}  // namespace nacl
