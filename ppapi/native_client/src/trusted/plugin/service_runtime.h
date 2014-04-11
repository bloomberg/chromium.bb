/* -*- c++ -*- */
/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// A class containing information regarding a socket connection to a
// service runtime instance.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SERVICE_RUNTIME_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SERVICE_RUNTIME_H_

#include <set>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/reverse_service/reverse_service.h"
#include "native_client/src/trusted/weak_ref/weak_ref.h"

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/native_client/src/trusted/plugin/utility.h"
#include "ppapi/utility/completion_callback_factory.h"

struct NaClFileInfo;

namespace nacl {
class DescWrapper;
}  // namespace

namespace pp {
class FileIO;
}  // namespace

namespace plugin {

class ErrorInfo;
class Manifest;
class Plugin;
class SrpcClient;
class ServiceRuntime;

// Struct of params used by StartSelLdr.  Use a struct so that callback
// creation templates aren't overwhelmed with too many parameters.
struct SelLdrStartParams {
  SelLdrStartParams(const nacl::string& url,
                    bool uses_irt,
                    bool uses_ppapi,
                    bool uses_nonsfi_mode,
                    bool enable_dev_interfaces,
                    bool enable_dyncode_syscalls,
                    bool enable_exception_handling,
                    bool enable_crash_throttling)
      : url(url),
        uses_irt(uses_irt),
        uses_ppapi(uses_ppapi),
        uses_nonsfi_mode(uses_nonsfi_mode),
        enable_dev_interfaces(enable_dev_interfaces),
        enable_dyncode_syscalls(enable_dyncode_syscalls),
        enable_exception_handling(enable_exception_handling),
        enable_crash_throttling(enable_crash_throttling) {
  }
  nacl::string url;
  bool uses_irt;
  bool uses_ppapi;
  bool uses_nonsfi_mode;
  bool enable_dev_interfaces;
  bool enable_dyncode_syscalls;
  bool enable_exception_handling;
  bool enable_crash_throttling;
};

// Callback resources are essentially our continuation state.
struct PostMessageResource {
 public:
  explicit PostMessageResource(std::string msg)
      : message(msg) {}
  std::string message;
};

struct OpenManifestEntryResource {
 public:
  OpenManifestEntryResource(const std::string& target_url,
                            struct NaClFileInfo* finfo,
                            bool* op_complete)
      : url(target_url),
        file_info(finfo),
        op_complete_ptr(op_complete) {}
  std::string url;
  struct NaClFileInfo* file_info;
  bool* op_complete_ptr;
};

struct CloseManifestEntryResource {
 public:
  CloseManifestEntryResource(int32_t desc_to_close,
                             bool* op_complete,
                             bool* op_result)
      : desc(desc_to_close),
        op_complete_ptr(op_complete),
        op_result_ptr(op_result) {}

  int32_t desc;
  bool* op_complete_ptr;
  bool* op_result_ptr;
};

struct QuotaRequest {
 public:
  QuotaRequest(PP_Resource pp_resource,
               int64_t start_offset,
               int64_t quota_bytes_requested,
               int64_t* quota_bytes_granted,
               bool* op_complete)
      : resource(pp_resource),
        offset(start_offset),
        bytes_requested(quota_bytes_requested),
        bytes_granted(quota_bytes_granted),
        op_complete_ptr(op_complete) { }

  PP_Resource resource;
  int64_t offset;
  int64_t bytes_requested;
  int64_t* bytes_granted;
  bool* op_complete_ptr;
};

// Do not invoke from the main thread, since the main methods will
// invoke CallOnMainThread and then wait on a condvar for the task to
// complete: if invoked from the main thread, the main method not
// returning (and thus unblocking the main thread) means that the
// main-thread continuation methods will never get called, and thus
// we'd get a deadlock.
class PluginReverseInterface: public nacl::ReverseInterface {
 public:
  PluginReverseInterface(nacl::WeakRefAnchor* anchor,
                         Plugin* plugin,
                         const Manifest* manifest,
                         ServiceRuntime* service_runtime,
                         pp::CompletionCallback init_done_cb,
                         pp::CompletionCallback crash_cb);

  virtual ~PluginReverseInterface();

  void ShutDown();

  virtual void DoPostMessage(nacl::string message);

  virtual void StartupInitializationComplete();

  virtual bool EnumerateManifestKeys(std::set<nacl::string>* out_keys);

  virtual bool OpenManifestEntry(nacl::string url_key,
                                 struct NaClFileInfo *info);

  virtual bool CloseManifestEntry(int32_t desc);

  virtual void ReportCrash();

  virtual void ReportExitStatus(int exit_status);

  virtual int64_t RequestQuotaForWrite(nacl::string file_id,
                                       int64_t offset,
                                       int64_t bytes_to_write);

  void AddQuotaManagedFile(const nacl::string& file_id,
                           const pp::FileIO& file_io);
  void AddTempQuotaManagedFile(const nacl::string& file_id);

 protected:
  virtual void PostMessage_MainThreadContinuation(PostMessageResource* p,
                                                  int32_t err);

  virtual void OpenManifestEntry_MainThreadContinuation(
      OpenManifestEntryResource* p,
      int32_t err);

  virtual void StreamAsFile_MainThreadContinuation(
      OpenManifestEntryResource* p,
      int32_t result);

  virtual void CloseManifestEntry_MainThreadContinuation(
      CloseManifestEntryResource* cls,
      int32_t err);

 private:
  nacl::WeakRefAnchor* anchor_;  // holds a ref
  Plugin* plugin_;  // value may be copied, but should be used only in
                    // main thread in WeakRef-protected callbacks.
  const Manifest* manifest_;
  ServiceRuntime* service_runtime_;
  NaClMutex mu_;
  NaClCondVar cv_;
  std::set<int64_t> quota_files_;
  bool shutting_down_;

  pp::CompletionCallback init_done_cb_;
  pp::CompletionCallback crash_cb_;
};

//  ServiceRuntime abstracts a NativeClient sel_ldr instance.
class ServiceRuntime {
 public:
  // TODO(sehr): This class should also implement factory methods, using the
  // Start method below.
  ServiceRuntime(Plugin* plugin,
                 const Manifest* manifest,
                 bool main_service_runtime,
                 bool uses_nonsfi_mode,
                 pp::CompletionCallback init_done_cb,
                 pp::CompletionCallback crash_cb);
  // The destructor terminates the sel_ldr process.
  ~ServiceRuntime();

  // Spawn the sel_ldr instance.
  void StartSelLdr(const SelLdrStartParams& params,
                   pp::CompletionCallback callback);

  // If starting sel_ldr from a background thread, wait for sel_ldr to
  // actually start. Returns |false| if timed out waiting for the process
  // to start. Otherwise, returns |true| if StartSelLdr is complete
  // (either successfully or unsuccessfully).
  bool WaitForSelLdrStart();

  // Signal to waiting threads that StartSelLdr is complete (either
  // successfully or unsuccessfully).
  // Done externally, in case external users want to write to shared
  // memory that is yet to be fenced.
  void SignalStartSelLdrDone();

  // Establish an SrpcClient to the sel_ldr instance and load the nexe.
  // The nexe to be started is passed through |nacl_file_desc|.
  // On success, returns true. On failure, returns false.
  bool LoadNexeAndStart(nacl::DescWrapper* nacl_file_desc,
                        const pp::CompletionCallback& crash_cb);

  // Starts the application channel to the nexe.
  SrpcClient* SetupAppChannel();

  bool Log(int severity, const nacl::string& msg);
  Plugin* plugin() const { return plugin_; }
  void Shutdown();

  // exit_status is -1 when invalid; when we set it, we will ensure
  // that it is non-negative (the portion of the exit status from the
  // nexe that is transferred is the low 8 bits of the argument to the
  // exit syscall).
  int exit_status();  // const, but grabs mutex etc.
  void set_exit_status(int exit_status);

  nacl::string GetCrashLogOutput();

  // To establish quota callbacks the pnacl coordinator needs to communicate
  // with the reverse interface.
  PluginReverseInterface* rev_interface() const { return rev_interface_; }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ServiceRuntime);
  bool SetupCommandChannel(ErrorInfo* error_info);
  bool LoadModule(nacl::DescWrapper* shm, ErrorInfo* error_info);
  bool InitReverseService(ErrorInfo* error_info);
  bool StartModule(ErrorInfo* error_info);
  void StartSelLdrContinuation(int32_t pp_error,
                               pp::CompletionCallback callback);

  NaClSrpcChannel command_channel_;
  Plugin* plugin_;
  bool main_service_runtime_;
  bool uses_nonsfi_mode_;
  nacl::ReverseService* reverse_service_;
  nacl::scoped_ptr<nacl::SelLdrLauncherBase> subprocess_;

  nacl::WeakRefAnchor* anchor_;

  PluginReverseInterface* rev_interface_;

  // Mutex to protect exit_status_.
  // Also, in conjunction with cond_ it is used to signal when
  // StartSelLdr is complete with either success or error.
  NaClMutex mu_;
  NaClCondVar cond_;
  int exit_status_;
  bool start_sel_ldr_done_;

  PP_Var start_sel_ldr_error_message_;
  pp::CompletionCallbackFactory<ServiceRuntime> callback_factory_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SERVICE_RUNTIME_H_
