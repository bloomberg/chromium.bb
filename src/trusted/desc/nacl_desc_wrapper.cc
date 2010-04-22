// Copyright (c) 2009 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <new>
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_conn_cap.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_invalid.h"
#include "native_client/src/trusted/desc/nacl_desc_sync_socket.h"
#if defined(NACL_LINUX)
#include "native_client/src/trusted/desc/linux/nacl_desc_sysv_shm.h"
#endif  // defined(NACL_LINUX)
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"
#include "native_client/src/trusted/desc/nrd_xfer_effector.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_imc_api.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"

namespace {
static const size_t kSizeTMax = std::numeric_limits<size_t>::max();
}  // namespace

namespace nacl {

// Descriptor creation and manipulation sometimes requires additional state
// (for instance, Effectors).  Therefore, we create an object that encapsulates
// that state.
class DescWrapperCommon {
  friend class DescWrapperFactory;

 public:
  typedef uint32_t RefCountType;

  // Get a pointer to the effector.
  struct NaClDescEffector* effp() {
    return reinterpret_cast<struct NaClDescEffector*>(&eff_);
  }

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
    sockets_[0] = NULL;
    sockets_[1] = NULL;
    NaClMutexCtor(&ref_count_mu_);
  }
  ~DescWrapperCommon() {
    if (is_initialized_) {
      effp()->vtbl->Dtor(effp());
      // effector took ownership of sockets_, so no need to Unref directly.
    } else {
      NaClDescSafeUnref(sockets_[0]);
      sockets_[0] = NULL;
      NaClDescSafeUnref(sockets_[1]);
      sockets_[1] = NULL;
    }
    NaClMutexDtor(&ref_count_mu_);
  }

  // Set up the state.  Returns true on success.
  bool Init();

  // Boolean to indicate the object was successfully initialized.
  bool is_initialized_;
  // Effector for transferring descriptors.
  struct NaClNrdXferEffector eff_;
  // Bound socket, socket address pair (used for effectors).
  struct NaClDesc* sockets_[2];
  // The reference count and the mutex to protect it.
  RefCountType ref_count_;
  struct NaClMutex ref_count_mu_;

  DISALLOW_COPY_AND_ASSIGN(DescWrapperCommon);
};

bool DescWrapperCommon::Init() {
  // Create a bound socket for use by the transfer effector.
  if (0 != NaClCommonDescMakeBoundSock(sockets_)) {
    return false;
  }
  // Set up the transfer effector.
  if (!NaClNrdXferEffectorCtor(&eff_, sockets_[0])) {
    return false;
  }
  NaClDescUnref(sockets_[0]);
  sockets_[0] = NULL;  // eff_ took ownership.
  NaClDescUnref(sockets_[1]);
  sockets_[1] = NULL;  // NaClDescUnref freed this already.
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
  // Return an error if the factory wasn't properly initialized.
  if (!common_data_->is_initialized()) {
    return -1;
  }
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
  // Return an error if the factory wasn't properly initialized.
  if (!common_data_->is_initialized()) {
    return NULL;
  }
  struct NaClDescImcDesc* imc_desc = NULL;
  DescWrapper* wrapper = NULL;

  imc_desc = reinterpret_cast<NaClDescImcDesc*>(
      calloc(1, sizeof(*imc_desc)));
  if (NULL == imc_desc) {
    goto cleanup;
  }
  if (!NaClDescImcDescCtor(imc_desc, handle)) {
    free(imc_desc);
    imc_desc = NULL;
    goto cleanup;
  }
  wrapper = new(std::nothrow)
      DescWrapper(common_data_, reinterpret_cast<struct NaClDesc*>(imc_desc));
  if (NULL == wrapper) {
    goto cleanup;
  }
  imc_desc = NULL;  // DescWrapper takes ownership of imc_desc.
  return wrapper;

 cleanup:
  NaClDescSafeUnref(reinterpret_cast<struct NaClDesc*>(imc_desc));
  return NULL;
}

DescWrapper* DescWrapperFactory::MakeShm(size_t size) {
  // Return an error if the factory wasn't properly initialized.
  if (!common_data_->is_initialized()) {
    return NULL;
  }
  // HACK: there's an inlining issue with this.
  // size_t rounded_size = NaClRoundAllocPage(size);
  size_t rounded_size =
      (size + NACL_MAP_PAGESIZE - 1) &
      ~static_cast<size_t>(NACL_MAP_PAGESIZE - 1);
  // TODO(sehr): fix the inlining issue.
  NaClHandle handle = CreateMemoryObject(rounded_size);
  if (kInvalidHandle == handle) {
    return NULL;
  }
  return ImportShmHandle(handle, size);
}

DescWrapper* DescWrapperFactory::ImportShmHandle(NaClHandle handle,
                                                 size_t size) {
  struct NaClDescImcShm* imc_desc = NULL;
  DescWrapper* wrapper = NULL;

  imc_desc = reinterpret_cast<NaClDescImcShm*>(
      calloc(1, sizeof(*imc_desc)));
  if (NULL == imc_desc) {
    goto cleanup;
  }
  if (!NaClDescImcShmCtor(imc_desc, handle, size)) {
    free(imc_desc);
    imc_desc = NULL;
    goto cleanup;
  }
  wrapper = new(std::nothrow)
      DescWrapper(common_data_, reinterpret_cast<struct NaClDesc*>(imc_desc));
  if (NULL == wrapper) {
    goto cleanup;
  }
  imc_desc = NULL;  // DescWrapper takes ownership of imc_desc.
  return wrapper;

 cleanup:
  NaClDescSafeUnref(reinterpret_cast<struct NaClDesc*>(imc_desc));
  return NULL;
}

#if NACL_LINUX
DescWrapper* DescWrapperFactory::ImportSysvShm(int key, size_t size) {
  struct NaClDescSysvShm* sysv_desc = NULL;
  DescWrapper* wrapper = NULL;
  size_t rounded_size = 0;

  if (kSizeTMax - NACL_PAGESIZE + 1 <= size) {
    // Avoid overflow when rounding to the nearest 4K and casting to
    // nacl_off64_t by preventing negative size.
    goto cleanup;
  }
  // HACK: there's an inlining issue with using NaClRoundPage. (See above.)
  // rounded_size = NaClRoundPage(size);
  rounded_size =
      (size + NACL_PAGESIZE - 1) & ~static_cast<size_t>(NACL_PAGESIZE - 1);
  sysv_desc = reinterpret_cast<NaClDescSysvShm*>(
      calloc(1, sizeof(*sysv_desc)));
  if (NULL == sysv_desc) {
    goto cleanup;
  }
  if (!NaClDescSysvShmImportCtor(sysv_desc,
                                 key,
                                 static_cast<nacl_off64_t>(rounded_size))) {
    // If rounded_size is negative due to overflow from rounding, it will be
    // rejected here by NaClDescSysvShmImportCtor.
    free(sysv_desc);
    sysv_desc = NULL;
    goto cleanup;
  }
  wrapper = new(std::nothrow)
      DescWrapper(common_data_, reinterpret_cast<struct NaClDesc*>(sysv_desc));
  if (NULL == wrapper) {
    goto cleanup;
  }
  return wrapper;

 cleanup:
  NaClDescSafeUnref(reinterpret_cast<struct NaClDesc*>(sysv_desc));
  return NULL;
}
#endif  // NACL_LINUX

#if defined(NACL_STANDALONE)
DescWrapper* DescWrapperFactory::ImportPepperSharedMemory(intptr_t shm_int,
                                                          size_t size) {
  // PepperSharedMemory is only present in the Chrome hookup.
  UNREFERENCED_PARAMETER(shm_int);
  UNREFERENCED_PARAMETER(size);
  return NULL;
}

DescWrapper* DescWrapperFactory::ImportPepper2DSharedMemory(intptr_t shm_int) {
  // Pepper2DSharedMemory is only present in the Chrome hookup.
  UNREFERENCED_PARAMETER(shm_int);
  return NULL;
}

DescWrapper* DescWrapperFactory::ImportPepperSync(intptr_t sync_int) {
  // PepperSync is only present in the Chrome hookup.
  UNREFERENCED_PARAMETER(sync_int);
  return NULL;
}
#endif  // NACL_STANDALONE

DescWrapper* DescWrapperFactory::MakeGeneric(struct NaClDesc* desc) {
  // Return an error if the factory wasn't properly initialized.
  if (!common_data_->is_initialized()) {
    return NULL;
  }
  return new(std::nothrow) DescWrapper(common_data_, desc);
}

DescWrapper* DescWrapperFactory::MakeSocketAddress(const char* str) {
  struct NaClDescConnCap* conn_cap = NULL;
  DescWrapper* wrapper = NULL;

  // Ensure argument is a valid string short enough to be a socket address.
  if (NULL == str) {
    return NULL;
  }
  size_t len = strnlen(str, NACL_PATH_MAX);
  // strnlen ensures NACL_PATH_MAX >= len.  If NACL_PATH_MAX == len, then
  // there is not enough room to hold the address.
  if (NACL_PATH_MAX == len) {
    return NULL;
  }
  // Create a NaClSocketAddress from the string.
  struct NaClSocketAddress sock_addr;
  // We need len + 1 to guarantee the zero byte is written.  This is safe,
  // since NACL_PATH_MAX >= len + 1 from above.
  strncpy(sock_addr.path, str, len + 1);
  // Create a NaClDescConnCap from the socket address.
  conn_cap = reinterpret_cast<NaClDescConnCap*>(
      calloc(1, sizeof(*conn_cap)));
  if (NULL == conn_cap) {
    goto cleanup;
  }
  if (!NaClDescConnCapCtor(conn_cap, &sock_addr)) {
    free(conn_cap);
    conn_cap = NULL;
    goto cleanup;
  }
  wrapper = MakeGeneric(reinterpret_cast<struct NaClDesc*>(conn_cap));
  if (NULL == wrapper) {
    goto cleanup;
  }
  // If wrapper was created, it took ownership.  If not, NaClDescUnref freed it.
  conn_cap = NULL;
  return wrapper;

 cleanup:
  NaClDescSafeUnref(reinterpret_cast<struct NaClDesc*>(conn_cap));
  return NULL;
}

int DescWrapperFactory::MakeSocketPair(DescWrapper* pair[2]) {
  // Return an error if the factory wasn't properly initialized.
  if (!common_data_->is_initialized()) {
    return -1;
  }
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

DescWrapper* DescWrapperFactory::OpenHostFile(const char* fname,
                                              int flags,
                                              int mode) {
  struct NaClHostDesc *nhdp = NULL;
  struct NaClDescIoDesc *ndiodp = NULL;
  DescWrapper* wrapper = NULL;

  nhdp = reinterpret_cast<struct NaClHostDesc*>(
      calloc(1, sizeof(*nhdp)));
  if (NULL == nhdp) {
    goto cleanup;
  }
  if (0 != NaClHostDescOpen(nhdp, fname, flags, mode)) {
    goto cleanup;
  }
  ndiodp = NaClDescIoDescMake(nhdp);
  if (NULL == ndiodp) {
    NaClHostDescClose(nhdp);
    free(nhdp);
    nhdp = NULL;
    goto cleanup;
  }
  nhdp = NULL;  // IoDesc took ownership of nhd
  wrapper = MakeGeneric(reinterpret_cast<struct NaClDesc*>(ndiodp));
  if (NULL == wrapper) {
    goto cleanup;
  }
  return wrapper;

 cleanup:
  NaClDescSafeUnref(reinterpret_cast<struct NaClDesc*>(ndiodp));
  free(nhdp);
  return NULL;
}

DescWrapper* DescWrapperFactory::MakeInvalid() {
  DescWrapper* wrapper = NULL;
  struct NaClDescInvalid *desc =
      const_cast<NaClDescInvalid*>(NaClDescInvalidMake());
  if (NULL == desc) {
    goto cleanup;
  }
  wrapper = MakeGeneric(reinterpret_cast<struct NaClDesc*>(desc));
  if (NULL == wrapper) {
    goto cleanup;
  }
  return wrapper;

 cleanup:
  NaClDescSafeUnref(reinterpret_cast<struct NaClDesc*>(desc));
  return NULL;
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

const char* DescWrapper::conn_cap_path() const {
  if (NULL == desc_ || NACL_DESC_CONN_CAP != type_tag()) {
    return NULL;
  }
  struct NaClDescConnCap* conn_cap =
      reinterpret_cast<struct NaClDescConnCap*>(desc_);
  return conn_cap->cap.path;
}

int DescWrapper::Map(void** addr, size_t* size) {
  return NaClDescMapDescriptor(desc_, common_data_->effp(), addr, size);
}

int DescWrapper::Unmap(void* start_addr, size_t len) {
  return desc_->vtbl->Unmap(desc_, common_data_->effp(), start_addr, len);
}

ssize_t DescWrapper::Read(void* buf, size_t len) {
  return desc_->vtbl->Read(desc_, common_data_->effp(), buf, len);
}

ssize_t DescWrapper::Write(const void* buf, size_t len) {
  return desc_->vtbl->Write(desc_, common_data_->effp(), buf, len);
}

nacl_off64_t DescWrapper::Seek(nacl_off64_t offset, int whence) {
  return desc_->vtbl->Seek(desc_, common_data_->effp(), offset, whence);
}

int DescWrapper::Ioctl(int request, void* arg) {
  return desc_->vtbl->Ioctl(desc_, common_data_->effp(), request, arg);
}

int DescWrapper::Fstat(struct nacl_abi_stat* statbuf) {
  return desc_->vtbl->Fstat(desc_, common_data_->effp(), statbuf);
}

int DescWrapper::Close() {
  return desc_->vtbl->Close(desc_, common_data_->effp());
}

ssize_t DescWrapper::Getdents(void* dirp, size_t count) {
  return desc_->vtbl->Getdents(desc_, common_data_->effp(), dirp, count);
}

int DescWrapper::Lock() {
  return desc_->vtbl->Lock(desc_, common_data_->effp());
}

int DescWrapper::TryLock() {
  return desc_->vtbl->TryLock(desc_, common_data_->effp());
}

int DescWrapper::Unlock() {
  return desc_->vtbl->Unlock(desc_, common_data_->effp());
}

int DescWrapper::Wait(DescWrapper* mutex) {
  return desc_->vtbl->Wait(desc_, common_data_->effp(), mutex->desc_);
}

int DescWrapper::TimedWaitAbs(DescWrapper* mutex,
                              struct nacl_abi_timespec* ts) {
  return desc_->vtbl->TimedWaitAbs(desc_,
                                   common_data_->effp(),
                                   mutex->desc_,
                                   ts);
}

int DescWrapper::Signal() {
  return desc_->vtbl->Signal(desc_, common_data_->effp());
}

int DescWrapper::Broadcast() {
  return desc_->vtbl->Broadcast(desc_, common_data_->effp());
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
  if (kSizeTMax / sizeof(NaClImcMsgIoVec) <= diov_length) {
    goto cleanup;
  }
  header.iov = reinterpret_cast<NaClImcMsgIoVec*>(
      calloc(dgram->iov_length, sizeof(*(header.iov))));
  if (NULL == header.iov) {
    goto cleanup;
  }
  header.iov_length = diov_length;
  for (i = 0; i < dgram->iov_length; ++i) {
    header.iov[i].base = dgram->iov[i].base;
    header.iov[i].length = dgram->iov[i].length;
  }
  // Allocate and copy the descriptor vector, removing DescWrappers.
  if (kHandleCountMax < dgram->ndescv_length) {
    goto cleanup;
  }
  if (kSizeTMax / sizeof(header.ndescv[0]) <= ddescv_length) {
    goto cleanup;
  }
  header.ndescv = reinterpret_cast<NaClDesc**>(
      calloc(dgram->ndescv_length, sizeof(*(header.ndescv))));
  if (NULL == header.iov) {
    goto cleanup;
  }
  header.ndesc_length = ddescv_length;
  for (i = 0; i < dgram->ndescv_length; ++i) {
    header.ndescv[i] = dgram->ndescv[i]->desc_;
  }
  // Send the message.
  ret = NaClImcSendTypedMessage(desc_, common_data_->effp(), &header, flags);

 cleanup:
  free(header.ndescv);
  free(header.iov);
  return ret;
}

ssize_t DescWrapper::RecvMsg(MsgHeader* dgram, int flags) {
  struct NaClImcTypedMsgHdr header;
  ssize_t ret = -NACL_ABI_ENOMEM;
  nacl_abi_size_t diov_length = dgram->iov_length;
  nacl_abi_size_t ddescv_length = dgram->ndescv_length;
  nacl_abi_size_t i;

  // Initialize to allow simple cleanups.
  header.ndescv = NULL;
  for (i = 0; i < dgram->ndescv_length; ++i) {
    dgram->ndescv[i] = NULL;
  }

  // Allocate and copy the IOV.
  if (kSizeTMax / sizeof(NaClImcMsgIoVec) <= diov_length) {
    goto cleanup;
  }
  header.iov = reinterpret_cast<NaClImcMsgIoVec*>(
      calloc(dgram->iov_length, sizeof(*(header.iov))));
  if (NULL == header.iov) {
    goto cleanup;
  }
  header.iov_length = diov_length;
  for (i = 0; i < dgram->iov_length; ++i) {
    header.iov[i].base = dgram->iov[i].base;
    header.iov[i].length = dgram->iov[i].length;
  }
  // Allocate and copy the descriptor vector.
  if (kHandleCountMax < dgram->ndescv_length) {
    goto cleanup;
  }
  if (kSizeTMax / sizeof(header.ndescv[0]) <= ddescv_length) {
    goto cleanup;
  }
  header.ndescv = reinterpret_cast<NaClDesc**>(
      calloc(dgram->ndescv_length, sizeof(*(header.ndescv))));
  if (NULL == header.ndescv) {
    goto cleanup;
  }
  header.ndesc_length = diov_length;
  // Receive the message.
  ret = NaClImcRecvTypedMessage(desc_, common_data_->effp(), &header, flags);
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
  int rv = desc_->vtbl->ConnectAddr(desc_, common_data_->effp());
  if (0 != rv) {
    // Connect failed.
    return NULL;
  }
  struct NaClNrdXferEffector* nrd_effector =
      reinterpret_cast<struct NaClNrdXferEffector*>(common_data_->effp());
  struct NaClDesc* connected_desc = NaClNrdXferEffectorTakeDesc(nrd_effector);
  if (NULL == connected_desc) {
    // Take from effector failed.
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
  int rv = desc_->vtbl->AcceptConn(desc_, common_data_->effp());
  if (0 != rv) {
    // Accept failed.
    return NULL;
  }

  struct NaClNrdXferEffector* nrd_effector =
      reinterpret_cast<struct NaClNrdXferEffector*>(common_data_->effp());
  struct NaClDesc* connected_desc = NaClNrdXferEffectorTakeDesc(nrd_effector);
  if (NULL == connected_desc) {
    // Take from effector failed.
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
  return desc_->vtbl->Post(desc_, common_data_->effp());
}

int DescWrapper::SemWait() {
  return desc_->vtbl->SemWait(desc_, common_data_->effp());
}

int DescWrapper::GetValue() {
  return desc_->vtbl->GetValue(desc_, common_data_->effp());
}

}  // namespace nacl
