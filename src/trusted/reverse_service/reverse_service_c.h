/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_REVERSE_SERVICE_REVERSE_SERVICE_C_H_
#define NATIVE_CLIENT_SRC_TRUSTED_REVERSE_SERVICE_REVERSE_SERVICE_C_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"

#include "native_client/src/trusted/nacl_base/nacl_refcount.h"
#include "native_client/src/trusted/threading/nacl_thread_interface.h"

#include "native_client/src/trusted/simple_service/nacl_simple_rservice.h"

EXTERN_C_BEGIN

struct NaClFileInfo;
struct NaClReverseInterface;
struct NaClReverseInterfaceVtbl;

struct NaClReverseService {
  struct NaClSimpleRevService base NACL_IS_REFCOUNT_SUBCLASS;

  struct NaClReverseInterface *iface;

  struct NaClMutex            mu;
  struct NaClCondVar          cv;
  /*
   * |mu| protects the thread count access.
   */
  uint32_t                    thread_count;
};

int NaClReverseServiceCtor(struct NaClReverseService   *self,
                           struct NaClReverseInterface *iface,
                           struct NaClDesc             *conn_cap);

void NaClReverseServiceDtor(struct NaClRefCount *vself);

struct NaClReverseServiceVtbl {
  struct NaClSimpleRevServiceVtbl vbase;

  int                             (*Start)(
      struct NaClReverseService   *self,
      int                         crash_report);

  void                            (*WaitForServiceThreadsToExit)(
      struct NaClReverseService   *self);

  void                            (*ThreadCountIncr)(
      struct NaClReverseService   *self);

  void                            (*ThreadCountDecr)(
      struct NaClReverseService   *self);
};

int NaClReverseServiceStart(
    struct NaClReverseService *self,
    int                       crash_report);

void NaClReverseServiceWaitForServiceThreadsToExit(
    struct NaClReverseService *self);

void NaClReverseServiceThreadCountIncr(
    struct NaClReverseService *self);

void NaClReverseServiceThreadCountDecr(
    struct NaClReverseService *self);

extern struct NaClReverseServiceVtbl const kNaClReverseServiceVtbl;

struct NaClReverseInterface {
  struct NaClRefCount base NACL_IS_REFCOUNT_SUBCLASS;
};

struct NaClReverseInterfaceVtbl {
  struct NaClRefCountVtbl       vbase;

  /* Startup handshake */
  void                          (*StartupInitializationComplete)(
      struct NaClReverseInterface   *self);

  /*
   * Name service use.
   *
   * Some of these functions require that the actual operation be done
   * in a different thread, so that the implementation of the
   * interface will have to block the requesting thread.  However, on
   * surf away, the thread switch may get cancelled, and the
   * implementation will have to reply with a failure indication.
   *
   * The bool functions returns false if the service thread unblocked
   * because of surf-away, shutdown, or other issues.  The plugin,
   * when it tells sel_ldr to shut down, will also signal all threads
   * that are waiting for main thread callbacks to wake up and abandon
   * their vigil after the callbacks are all cancelled (by abandoning
   * the WeakRefAnchor or by bombing their CompletionCallbackFactory).
   * Since shutdown/surfaway is the only admissible error, we use bool
   * as the return type.
   */

  /*
   * Opens manifest entry specified by |url_key|. Returns 1 if
   * successful and stores the file descriptor in |out_desc|, otherwise
   * returns 0.
   */
  int                           (*OpenManifestEntry)(
      struct NaClReverseInterface   *self,
      char const                    *url_key,
      struct NaClFileInfo           *info);

  /*
   * Reports the client crash.
   */
  void                          (*ReportCrash)(
      struct NaClReverseInterface   *self);

  /*
   * The low-order 8 bits of the |exit_status| should be reported to
   * any interested parties.
   */
  void                          (*ReportExitStatus)(
      struct NaClReverseInterface   *self,
      int                           exit_status);

  /*
   * Standard output and standard error redirection, via setting
   * NACL_EXE_STDOUT to the string "DEBUG_ONLY:dev://postmessage" (see
   * native_client/src/trusted/nacl_resource.* and sel_ldr).  NB: the
   * contents of |message| is arbitrary bytes and not an Unicode
   * string, so the implementation should take care to handle this
   * appropriately.
   */
  void                          (*DoPostMessage)(
      struct NaClReverseInterface   *self,
      char const                    *message,
      size_t                        message_bytes);

  /*
   * Create new service runtime process and return secure command
   * channel and untrusted application channel socket addresses. Returns
   * 0 if successful or negative ABI error value otherwise (see
   * service_runtime/include/sys/errno.h).  DEPRECATED.
   */
  int                           (*CreateProcess)(
      struct NaClReverseInterface  *self,
      struct NaClDesc              **out_sock_addr,
      struct NaClDesc              **out_app_addr);

  /*
   * Create new service runtime process and return secure command
   * channel, untrusted application channel socket addresses, and pid
   * via a result delivery functor.  Returns negated ABI errno value
   * in pid if there were errors (see the header file
   * service_runtime/include/sys/errno.h).  The |out_pid_or_errno|
   * functor argument is also used to allow the "init" process to
   * inform the embedding environment, via FinalizeProcess, that it is
   * okay to finalize any resources associated with the identified
   * subprocess when the subprocess has exited.
   */
  void                          (*CreateProcessFunctorResult)(
      struct NaClReverseInterface *self,
      void (*result_functor)(void *functor_state,
                             struct NaClDesc *out_sock_addr,
                             struct NaClDesc *out_app_addr,
                             int32_t out_pid_or_errno),
      void *functor_state);

  void                          (*FinalizeProcess)(
      struct NaClReverseInterface *self,
      int32_t pid);

  /*
   * Quota checking for files that were sent to the untrusted module.
   * TODO(sehr): remove the stub interface once the plugin provides one.
   */
  int64_t                       (*RequestQuotaForWrite)(
      struct NaClReverseInterface   *self,
      char const                    *file_id,
      int64_t                       offset,
      int64_t                       length);
};

/*
 * The protected Ctor is intended for use by subclasses of
 * NaClReverseInterface.
 */
int NaClReverseInterfaceCtor_protected(
    struct NaClReverseInterface   *self);

void NaClReverseInterfaceDtor(struct NaClRefCount *vself);

void NaClReverseInterfaceLog(
    struct NaClReverseInterface   *self,
    char const                    *message);

void NaClReverseInterfaceStartupInitializationComplete(
    struct NaClReverseInterface   *self);

int NaClReverseInterfaceOpenManifestEntry(
    struct NaClReverseInterface   *self,
    char const                    *url_key,
    struct NaClFileInfo           *info);

int NaClReverseInterfaceCloseManifestEntry(
    struct NaClReverseInterface   *self,
    int32_t                       desc);

void NaClReverseInterfaceReportCrash(
    struct NaClReverseInterface   *self);

void NaClReverseInterfaceReportExitStatus(
    struct NaClReverseInterface   *self,
    int                           exit_status);

void NaClReverseInterfaceDoPostMessage(
    struct NaClReverseInterface   *self,
    char const                    *message,
    size_t                        message_bytes);

void NaClReverseInterfaceCreateProcessFunctorResult(
    struct NaClReverseInterface *self,
    void (*result_functor)(void *functor_state,
                           struct NaClDesc *out_sock_addr,
                           struct NaClDesc *out_app_addr,
                           int32_t out_pid_or_errno),
    void *functor_state);

void NaClReverseInterfaceFinalizeProcess(struct NaClReverseInterface *self,
                                         int32_t pid);

int NaClReverseInterfaceCreateProcess(
    struct NaClReverseInterface   *self,
    struct NaClDesc               **out_sock_addr,
    struct NaClDesc               **out_app_addr);

int64_t NaClReverseInterfaceRequestQuotaForWrite(
    struct NaClReverseInterface   *self,
    char const                    *file_id,
    int64_t                       offset,
    int64_t                       length);

extern struct NaClReverseInterfaceVtbl const kNaClReverseInterfaceVtbl;

EXTERN_C_END

#endif /* NATIVE_CLIENT_SRC_TRUSTED_REVERSE_SERVICE_REVERSE_SERVICE_C_H_ */
