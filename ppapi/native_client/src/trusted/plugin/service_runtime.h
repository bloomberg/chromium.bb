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

#include <map>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/plugin/utility.h"
#include "native_client/src/trusted/reverse_service/reverse_service.h"
#include "native_client/src/trusted/weak_ref/weak_ref.h"

#include "ppapi/c/trusted/ppb_file_io_trusted.h"
#include "ppapi/cpp/completion_callback.h"

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
class PnaclCoordinator;
class SrpcClient;
class ServiceRuntime;

// Callback resources are essentially our continuation state.

struct LogToJavaScriptConsoleResource {
 public:
  explicit LogToJavaScriptConsoleResource(std::string msg)
      : message(msg) {}
  std::string message;
};

struct PostMessageResource {
 public:
  explicit PostMessageResource(std::string msg)
      : message(msg) {}
  std::string message;
};

struct OpenManifestEntryResource {
 public:
  OpenManifestEntryResource(const std::string& target_url,
                            int32_t* descp,
                            ErrorInfo* infop,
                            bool* op_complete)
      : url(target_url),
        out_desc(descp),
        error_info(infop),
        pnacl_translate(false),
        op_complete_ptr(op_complete) {}
  std::string url;
  int32_t* out_desc;
  ErrorInfo* error_info;
  bool pnacl_translate;
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

enum QuotaDataType {
  PepperQuotaType,
  TempQuotaType
};

struct QuotaData {
  QuotaData(QuotaDataType type_, PP_Resource resource_)
      : type(type_), resource(resource_) {}
  QuotaData()
      : type(PepperQuotaType), resource(0) {}

  QuotaDataType type;
  PP_Resource resource;
};

struct QuotaRequest {
 public:
  QuotaRequest(QuotaData quota_data,
               int64_t start_offset,
               int64_t quota_bytes_requested,
               int64_t* quota_bytes_granted,
               bool* op_complete)
      : data(quota_data),
        offset(start_offset),
        bytes_requested(quota_bytes_requested),
        bytes_granted(quota_bytes_granted),
        op_complete_ptr(op_complete) { }

  QuotaData data;
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

  virtual void Log(nacl::string message);

  virtual void DoPostMessage(nacl::string message);

  virtual void StartupInitializationComplete();

  virtual bool EnumerateManifestKeys(std::set<nacl::string>* out_keys);

  virtual bool OpenManifestEntry(nacl::string url_key, int32_t* out_desc);

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
  virtual void Log_MainThreadContinuation(LogToJavaScriptConsoleResource* p,
                                          int32_t err);

  virtual void PostMessage_MainThreadContinuation(PostMessageResource* p,
                                                  int32_t err);

  virtual void OpenManifestEntry_MainThreadContinuation(
      OpenManifestEntryResource* p,
      int32_t err);

  virtual void StreamAsFile_MainThreadContinuation(
      OpenManifestEntryResource* p,
      int32_t result);

  virtual void BitcodeTranslate_MainThreadContinuation(
      OpenManifestEntryResource* p,
      int32_t result);

  virtual void CloseManifestEntry_MainThreadContinuation(
      CloseManifestEntryResource* cls,
      int32_t err);

  virtual void QuotaRequest_MainThreadContinuation(
      QuotaRequest* request,
      int32_t err);

  virtual void QuotaRequest_MainThreadResponse(
      QuotaRequest* request,
      int32_t err);

 private:
  nacl::WeakRefAnchor* anchor_;  // holds a ref
  Plugin* plugin_;  // value may be copied, but should be used only in
                    // main thread in WeakRef-protected callbacks.
  const Manifest* manifest_;
  ServiceRuntime* service_runtime_;
  NaClMutex mu_;
  NaClCondVar cv_;
  std::map<int64_t, QuotaData> quota_map_;
  bool shutting_down_;

  nacl::scoped_ptr<PnaclCoordinator> pnacl_coordinator_;

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
                 bool should_report_uma,
                 pp::CompletionCallback init_done_cb,
                 pp::CompletionCallback crash_cb);
  // The destructor terminates the sel_ldr process.
  ~ServiceRuntime();

  // Spawn a sel_ldr instance and establish an SrpcClient to it.  The nexe
  // to be started is passed through |nacl_file_desc|.  On success, returns
  // true.  On failure, returns false and |error_string| is set to something
  // describing the error.
  bool Start(nacl::DescWrapper* nacl_file_desc,
             ErrorInfo* error_info,
             const nacl::string& url,
             bool uses_irt,
             bool uses_ppapi,
             bool enable_ppapi_dev,
             pp::CompletionCallback crash_cb);

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
  bool InitCommunication(nacl::DescWrapper* shm, ErrorInfo* error_info);

  NaClSrpcChannel command_channel_;
  Plugin* plugin_;
  bool should_report_uma_;
  nacl::ReverseService* reverse_service_;
  nacl::scoped_ptr<nacl::SelLdrLauncherBase> subprocess_;

  nacl::WeakRefAnchor* anchor_;

  PluginReverseInterface* rev_interface_;

  NaClMutex mu_;
  int exit_status_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SERVICE_RUNTIME_H_
