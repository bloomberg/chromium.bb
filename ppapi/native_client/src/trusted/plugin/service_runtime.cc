/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define NACL_LOG_MODULE_NAME "Plugin::ServiceRuntime"

#include "native_client/src/trusted/plugin/service_runtime.h"

#include <string.h>
#include <set>
#include <string>
#include <utility>

#include "base/compiler_specific.h"

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_sync_raii.h"
#include "native_client/src/shared/platform/scoped_ptr_refcount.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
// remove when we no longer need to cast the DescWrapper below.
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/plugin/manifest.h"

// This is here due to a Windows API collision; plugin.h through
// file_downloader.h transitively includes Instance.h which defines a
// PostMessage method, so this undef must appear before any of those.
#ifdef PostMessage
#undef PostMessage
#endif
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/pnacl_coordinator.h"
#include "native_client/src/trusted/plugin/pnacl_resources.h"
#include "native_client/src/trusted/plugin/sel_ldr_launcher_chrome.h"
#include "native_client/src/trusted/plugin/srpc_client.h"
#include "native_client/src/trusted/plugin/utility.h"

#include "native_client/src/trusted/weak_ref/call_on_main_thread.h"

#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_imc_api.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_file_io_trusted.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_io.h"

namespace {

// For doing crude quota enforcement on writes to temp files.
// We do not allow a temp file bigger than 512 MB for now.
const uint64_t kMaxTempQuota = 0x20000000;

}  // namespace

namespace plugin {

PluginReverseInterface::PluginReverseInterface(
    nacl::WeakRefAnchor* anchor,
    Plugin* plugin,
    const Manifest* manifest,
    ServiceRuntime* service_runtime,
    pp::CompletionCallback init_done_cb,
    pp::CompletionCallback crash_cb)
      : anchor_(anchor),
        plugin_(plugin),
        manifest_(manifest),
        service_runtime_(service_runtime),
        shutting_down_(false),
        init_done_cb_(init_done_cb),
        crash_cb_(crash_cb) {
  NaClXMutexCtor(&mu_);
  NaClXCondVarCtor(&cv_);
}

PluginReverseInterface::~PluginReverseInterface() {
  NaClCondVarDtor(&cv_);
  NaClMutexDtor(&mu_);
}

void PluginReverseInterface::ShutDown() {
  NaClLog(4, "PluginReverseInterface::Shutdown: entered\n");
  nacl::MutexLocker take(&mu_);
  shutting_down_ = true;
  NaClXCondVarBroadcast(&cv_);
  NaClLog(4, "PluginReverseInterface::Shutdown: broadcasted, exiting\n");
}

void PluginReverseInterface::Log(nacl::string message) {
  LogToJavaScriptConsoleResource* continuation =
      new LogToJavaScriptConsoleResource(message);
  CHECK(continuation != NULL);
  NaClLog(4, "PluginReverseInterface::Log(%s)\n", message.c_str());
  plugin::WeakRefCallOnMainThread(
      anchor_,
      0,  /* delay in ms */
      ALLOW_THIS_IN_INITIALIZER_LIST(this),
      &plugin::PluginReverseInterface::Log_MainThreadContinuation,
      continuation);
}

void PluginReverseInterface::DoPostMessage(nacl::string message) {
  PostMessageResource* continuation = new PostMessageResource(message);
  CHECK(continuation != NULL);
  NaClLog(4, "PluginReverseInterface::DoPostMessage(%s)\n", message.c_str());
  plugin::WeakRefCallOnMainThread(
      anchor_,
      0,  /* delay in ms */
      ALLOW_THIS_IN_INITIALIZER_LIST(this),
      &plugin::PluginReverseInterface::PostMessage_MainThreadContinuation,
      continuation);
}

void PluginReverseInterface::StartupInitializationComplete() {
  NaClLog(4, "PluginReverseInterface::StartupInitializationComplete\n");
  if (init_done_cb_.pp_completion_callback().func != NULL) {
    NaClLog(4,
            "PluginReverseInterface::StartupInitializationComplete:"
            " invoking CB\n");
    pp::Module::Get()->core()->CallOnMainThread(0, init_done_cb_, PP_OK);
  } else {
    NaClLog(1,
            "PluginReverseInterface::StartupInitializationComplete:"
            " init_done_cb_ not valid, skipping.\n");
  }
}

void PluginReverseInterface::Log_MainThreadContinuation(
    LogToJavaScriptConsoleResource* p,
    int32_t err) {
  UNREFERENCED_PARAMETER(err);
  NaClLog(4,
          "PluginReverseInterface::Log_MainThreadContinuation(%s)\n",
          p->message.c_str());
  plugin_->AddToConsole(p->message);
}
void PluginReverseInterface::PostMessage_MainThreadContinuation(
    PostMessageResource* p,
    int32_t err) {
  UNREFERENCED_PARAMETER(err);
  NaClLog(4,
          "PluginReverseInterface::PostMessage_MainThreadContinuation(%s)\n",
          p->message.c_str());
  plugin_->PostMessage(std::string("DEBUG_POSTMESSAGE:") + p->message);
}

bool PluginReverseInterface::EnumerateManifestKeys(
    std::set<nacl::string>* out_keys) {
  Manifest const* mp = manifest_;

  if (!mp->GetFileKeys(out_keys)) {
    return false;
  }

  return true;
}

// TODO(bsy): OpenManifestEntry should use the manifest to ResolveKey
// and invoke StreamAsFile with a completion callback that invokes
// GetPOSIXFileDesc.
bool PluginReverseInterface::OpenManifestEntry(nacl::string url_key,
                                               int32_t* out_desc) {
  ErrorInfo error_info;
  bool op_complete = false;  // NB: mu_ and cv_ also controls access to this!
  OpenManifestEntryResource* to_open =
      new OpenManifestEntryResource(url_key, out_desc,
                                    &error_info, &op_complete);
  CHECK(to_open != NULL);
  NaClLog(4, "PluginReverseInterface::OpenManifestEntry: %s\n",
          url_key.c_str());
  // This assumes we are not on the main thread.  If false, we deadlock.
  plugin::WeakRefCallOnMainThread(
      anchor_,
      0,
      this,
      &plugin::PluginReverseInterface::OpenManifestEntry_MainThreadContinuation,
      to_open);
  NaClLog(4,
          "PluginReverseInterface::OpenManifestEntry:"
          " waiting on main thread\n");
  bool shutting_down;
  do {
    nacl::MutexLocker take(&mu_);
    for (;;) {
      NaClLog(4,
              "PluginReverseInterface::OpenManifestEntry:"
              " got lock, checking shutdown and completion: (%s, %s)\n",
              shutting_down_ ? "yes" : "no",
              op_complete ? "yes" : "no");
      shutting_down = shutting_down_;
      if (op_complete || shutting_down) {
        NaClLog(4,
                "PluginReverseInterface::OpenManifestEntry:"
                " done!\n");
        break;
      }
      NaClXCondVarWait(&cv_, &mu_);
    }
  } while (0);
  if (shutting_down) {
    NaClLog(4,
            "PluginReverseInterface::OpenManifestEntry:"
            " plugin is shutting down\n");
    return false;
  }
  // out_desc has the returned descriptor if successful, else -1.

  // The caller is responsible for not closing *out_desc.  If it is
  // closed prematurely, then another open could re-use the OS
  // descriptor, confusing the opened_ map.  If the caller is going to
  // want to make a NaClDesc object and transfer it etc., then the
  // caller should DUP the descriptor (but remember the original
  // value) for use by the NaClDesc object, which closes when the
  // object is destroyed.
  NaClLog(4,
          "PluginReverseInterface::OpenManifestEntry:"
          " *out_desc = %d\n",
          *out_desc);
  if (*out_desc == -1) {
    // TODO(bsy,ncbray): what else should we do with the error?  This
    // is a runtime error that may simply be a programming error in
    // the untrusted code, or it may be something else wrong w/ the
    // manifest.
    NaClLog(4,
            "OpenManifestEntry: failed for key %s, code %d (%s)\n",
            url_key.c_str(),
            error_info.error_code(),
            error_info.message().c_str());
  }
  return true;
}

// Transfer point from OpenManifestEntry() which runs on the main thread
// (Some PPAPI actions -- like StreamAsFile -- can only run on the main thread).
// OpenManifestEntry() is waiting on a condvar for this continuation to
// complete.  We Broadcast and awaken OpenManifestEntry() whenever we are done
// either here, or in a later MainThreadContinuation step, if there are
// multiple steps.
void PluginReverseInterface::OpenManifestEntry_MainThreadContinuation(
    OpenManifestEntryResource* p,
    int32_t err) {
  OpenManifestEntryResource *open_cont;
  UNREFERENCED_PARAMETER(err);
  // CallOnMainThread continuations always called with err == PP_OK.

  NaClLog(4, "Entered OpenManifestEntry_MainThreadContinuation\n");

  std::string mapped_url;
  std::string cache_identity;
  if (!manifest_->ResolveKey(p->url, &mapped_url, &cache_identity,
                             p->error_info, &p->pnacl_translate)) {
    NaClLog(4, "OpenManifestEntry_MainThreadContinuation: ResolveKey failed\n");
    // Failed, and error_info has the details on what happened.  Wake
    // up requesting thread -- we are done.
    nacl::MutexLocker take(&mu_);
    *p->op_complete_ptr = true;  // done...
    *p->out_desc = -1;       // but failed.
    NaClXCondVarBroadcast(&cv_);
    return;
  }
  NaClLog(4,
          "OpenManifestEntry_MainThreadContinuation: "
          "ResolveKey: %s -> %s (pnacl_translate(%d))\n",
          p->url.c_str(), mapped_url.c_str(), p->pnacl_translate);

  open_cont = new OpenManifestEntryResource(*p);  // copy ctor!
  CHECK(open_cont != NULL);
  open_cont->url = mapped_url;
  if (!open_cont->pnacl_translate) {
    pp::CompletionCallback stream_cc = WeakRefNewCallback(
        anchor_,
        this,
        &PluginReverseInterface::StreamAsFile_MainThreadContinuation,
        open_cont);
    //
    if (!PnaclUrls::IsPnaclComponent(mapped_url)) {
      if (!plugin_->StreamAsFile(mapped_url,
                                 stream_cc.pp_completion_callback())) {
        NaClLog(4,
                "OpenManifestEntry_MainThreadContinuation: "
                "StreamAsFile failed\n");
        nacl::MutexLocker take(&mu_);
        *p->op_complete_ptr = true;  // done...
        *p->out_desc = -1;       // but failed.
        p->error_info->SetReport(ERROR_MANIFEST_OPEN,
                                 "ServiceRuntime: StreamAsFile failed");
        NaClXCondVarBroadcast(&cv_);
        return;
      }
      NaClLog(4,
              "OpenManifestEntry_MainThreadContinuation: StreamAsFile okay\n");
    } else {
      int32_t fd = PnaclResources::GetPnaclFD(
          plugin_,
          PnaclUrls::PnaclComponentURLToFilename(mapped_url).c_str());
      if (fd < 0) {
        // We should check earlier if the pnacl component wasn't installed
        // yet.  At this point, we can't do much anymore, so just continue
        // with an invalid fd.
        NaClLog(4,
                "OpenManifestEntry_MainThreadContinuation: "
                "GetReadonlyPnaclFd failed\n");
        // TODO(jvoung): Separate the error codes?
        p->error_info->SetReport(ERROR_MANIFEST_OPEN,
                                 "ServiceRuntime: GetPnaclFd failed");
      }
      nacl::MutexLocker take(&mu_);
      *p->op_complete_ptr = true;  // done!
      *p->out_desc = fd;
      NaClXCondVarBroadcast(&cv_);
      NaClLog(4,
              "OpenManifestEntry_MainThreadContinuation: GetPnaclFd okay\n");
    }
  } else {
    NaClLog(4,
            "OpenManifestEntry_MainThreadContinuation: "
            "pulling down and translating.\n");
    if (plugin_->nacl_interface()->IsPnaclEnabled()) {
      pp::CompletionCallback translate_callback =
          WeakRefNewCallback(
              anchor_,
              this,
              &PluginReverseInterface::BitcodeTranslate_MainThreadContinuation,
              open_cont);
      // Will always call the callback on success or failure.
      pnacl_coordinator_.reset(
          PnaclCoordinator::BitcodeToNative(plugin_,
                                            mapped_url,
                                            cache_identity,
                                            translate_callback));
    } else {
      nacl::MutexLocker take(&mu_);
      *p->op_complete_ptr = true;  // done...
      *p->out_desc = -1;       // but failed.
      p->error_info->SetReport(ERROR_PNACL_NOT_ENABLED,
                               "ServiceRuntime: GetPnaclFd failed -- pnacl not "
                               "enabled with --enable-pnacl.");
      NaClXCondVarBroadcast(&cv_);
      return;
    }
  }
  // p is deleted automatically
}

void PluginReverseInterface::StreamAsFile_MainThreadContinuation(
    OpenManifestEntryResource* p,
    int32_t result) {
  NaClLog(4,
          "Entered StreamAsFile_MainThreadContinuation\n");

  nacl::MutexLocker take(&mu_);
  if (result == PP_OK) {
    NaClLog(4, "StreamAsFile_MainThreadContinuation: GetPOSIXFileDesc(%s)\n",
            p->url.c_str());
    *p->out_desc = plugin_->GetPOSIXFileDesc(p->url);
    NaClLog(4,
            "StreamAsFile_MainThreadContinuation: PP_OK, desc %d\n",
            *p->out_desc);
  } else {
    NaClLog(4,
            "StreamAsFile_MainThreadContinuation: !PP_OK, setting desc -1\n");
    *p->out_desc = -1;
    p->error_info->SetReport(ERROR_MANIFEST_OPEN,
                             "Plugin StreamAsFile failed at callback");
  }
  *p->op_complete_ptr = true;
  NaClXCondVarBroadcast(&cv_);
}


void PluginReverseInterface::BitcodeTranslate_MainThreadContinuation(
    OpenManifestEntryResource* p,
    int32_t result) {
  NaClLog(4,
          "Entered BitcodeTranslate_MainThreadContinuation\n");

  nacl::MutexLocker take(&mu_);
  if (result == PP_OK) {
    // TODO(jvoung): clean this up. We are assuming that the NaClDesc is
    // a host IO desc and doing a downcast. Once the ReverseInterface
    // accepts NaClDescs we can avoid this downcast.
    NaClDesc* desc = pnacl_coordinator_->ReleaseTranslatedFD()->desc();
    struct NaClDescIoDesc* ndiodp = (struct NaClDescIoDesc*)desc;
    *p->out_desc = ndiodp->hd->d;
    pnacl_coordinator_.reset(NULL);
    NaClLog(4,
            "BitcodeTranslate_MainThreadContinuation: PP_OK, desc %d\n",
            *p->out_desc);
  } else {
    NaClLog(4,
            "BitcodeTranslate_MainThreadContinuation: !PP_OK, "
            "setting desc -1\n");
    *p->out_desc = -1;
    // Error should have been reported by pnacl coordinator.
    PLUGIN_PRINTF(("PluginReverseInterface::BitcodeTranslate error.\n"));
  }
  *p->op_complete_ptr = true;
  NaClXCondVarBroadcast(&cv_);
}


bool PluginReverseInterface::CloseManifestEntry(int32_t desc) {
  bool op_complete = false;
  bool op_result;
  CloseManifestEntryResource* to_close =
      new CloseManifestEntryResource(desc, &op_complete, &op_result);

  bool shutting_down;
  plugin::WeakRefCallOnMainThread(
      anchor_,
      0,
      this,
      &plugin::PluginReverseInterface::
        CloseManifestEntry_MainThreadContinuation,
      to_close);
  // wait for completion or surf-away.
  do {
    nacl::MutexLocker take(&mu_);
    for (;;) {
      shutting_down = shutting_down_;
      if (op_complete || shutting_down) {
        break;
      }
      NaClXCondVarWait(&cv_, &mu_);
    }
  } while (0);

  if (shutting_down) return false;
  // op_result true if close was successful; false otherwise (e.g., bad desc).
  return op_result;
}

void PluginReverseInterface::CloseManifestEntry_MainThreadContinuation(
    CloseManifestEntryResource* cls,
    int32_t err) {
  UNREFERENCED_PARAMETER(err);

  nacl::MutexLocker take(&mu_);
  // TODO(bsy): once the plugin has a reliable way to report that the
  // file usage is done -- and sel_ldr uses this RPC call -- we should
  // tell the plugin that the associated resources can be freed.
  *cls->op_result_ptr = true;
  *cls->op_complete_ptr = true;
  NaClXCondVarBroadcast(&cv_);
  // cls automatically deleted
}

void PluginReverseInterface::ReportCrash() {
  NaClLog(4, "PluginReverseInterface::ReportCrash\n");

  if (crash_cb_.pp_completion_callback().func != NULL) {
    NaClLog(4, "PluginReverseInterface::ReportCrash: invoking CB\n");
    pp::Module::Get()->core()->CallOnMainThread(0, crash_cb_, PP_OK);
  } else {
    NaClLog(1,
            "PluginReverseInterface::ReportCrash:"
            " crash_cb_ not valid, skipping\n");
  }
}

void PluginReverseInterface::ReportExitStatus(int exit_status) {
  service_runtime_->set_exit_status(exit_status);
}

void PluginReverseInterface::QuotaRequest_MainThreadContinuation(
    QuotaRequest* request,
    int32_t err) {
  if (err != PP_OK) {
    return;
  }

  switch (request->data.type) {
    case plugin::PepperQuotaType: {
      const PPB_FileIOTrusted* file_io_trusted =
          static_cast<const PPB_FileIOTrusted*>(
              pp::Module::Get()->GetBrowserInterface(
                  PPB_FILEIOTRUSTED_INTERFACE));
      // Copy the request object because this one will be deleted on return.
      // copy ctor!
      QuotaRequest* cont_for_response = new QuotaRequest(*request);
      pp::CompletionCallback quota_cc = WeakRefNewCallback(
          anchor_,
          this,
          &PluginReverseInterface::QuotaRequest_MainThreadResponse,
          cont_for_response);
      file_io_trusted->WillWrite(request->data.resource,
                                 request->offset,
                                 // TODO(sehr): remove need for cast.
                                 // Unify WillWrite interface vs Quota request.
                                 nacl::assert_cast<int32_t>(
                                     request->bytes_requested),
                                 quota_cc.pp_completion_callback());
      break;
    }
    case plugin::TempQuotaType: {
      uint64_t len = request->offset + request->bytes_requested;
      nacl::MutexLocker take(&mu_);
      // Do some crude quota enforcement.
      if (len > kMaxTempQuota) {
        *request->bytes_granted = 0;
      } else {
        *request->bytes_granted = request->bytes_requested;
      }
      *request->op_complete_ptr = true;
      NaClXCondVarBroadcast(&cv_);
      break;
    }
  }
  // request automatically deleted
}

void PluginReverseInterface::QuotaRequest_MainThreadResponse(
    QuotaRequest* request,
    int32_t err) {
  NaClLog(4,
          "PluginReverseInterface::QuotaRequest_MainThreadResponse:"
          " (resource=%"NACL_PRIx32", offset=%"NACL_PRId64", requested=%"
          NACL_PRId64", err=%"NACL_PRId32")\n",
          request->data.resource,
          request->offset, request->bytes_requested, err);
  nacl::MutexLocker take(&mu_);
  if (err >= PP_OK) {
    *request->bytes_granted = err;
  } else {
    *request->bytes_granted = 0;
  }
  *request->op_complete_ptr = true;
  NaClXCondVarBroadcast(&cv_);
  // request automatically deleted
}

int64_t PluginReverseInterface::RequestQuotaForWrite(
    nacl::string file_id, int64_t offset, int64_t bytes_to_write) {
  NaClLog(4,
          "PluginReverseInterface::RequestQuotaForWrite:"
          " (file_id='%s', offset=%"NACL_PRId64", bytes_to_write=%"
          NACL_PRId64")\n", file_id.c_str(), offset, bytes_to_write);
  QuotaData quota_data;
  {
    nacl::MutexLocker take(&mu_);
    uint64_t file_key = STRTOULL(file_id.c_str(), NULL, 10);
    if (quota_map_.find(file_key) == quota_map_.end()) {
      // Look up failed to find the requested quota managed resource.
      NaClLog(4, "PluginReverseInterface::RequestQuotaForWrite: failed...\n");
      return 0;
    }
    quota_data = quota_map_[file_key];
  }
  // Variables set by requesting quota.
  int64_t quota_granted = 0;
  bool op_complete = false;
  QuotaRequest* continuation =
      new QuotaRequest(quota_data, offset, bytes_to_write, &quota_granted,
                       &op_complete);
  // The reverse service is running on a background thread and the PPAPI quota
  // methods must be invoked only from the main thread.
  plugin::WeakRefCallOnMainThread(
      anchor_,
      0,  /* delay in ms */
      ALLOW_THIS_IN_INITIALIZER_LIST(this),
      &plugin::PluginReverseInterface::QuotaRequest_MainThreadContinuation,
      continuation);
  // Wait for the main thread to request quota and signal completion.
  // It is also possible that the main thread will signal shut down.
  bool shutting_down;
  do {
    nacl::MutexLocker take(&mu_);
    for (;;) {
      shutting_down = shutting_down_;
      if (op_complete || shutting_down) {
        break;
      }
      NaClXCondVarWait(&cv_, &mu_);
    }
  } while (0);
  if (shutting_down) return 0;
  return quota_granted;
}

void PluginReverseInterface::AddQuotaManagedFile(const nacl::string& file_id,
                                                 const pp::FileIO& file_io) {
  PP_Resource resource = file_io.pp_resource();
  NaClLog(4,
          "PluginReverseInterface::AddQuotaManagedFile: "
          "(file_id='%s', file_io_ref=%"NACL_PRIx32")\n",
          file_id.c_str(), resource);
  nacl::MutexLocker take(&mu_);
  uint64_t file_key = STRTOULL(file_id.c_str(), NULL, 10);
  QuotaData data(plugin::PepperQuotaType, resource);
  quota_map_[file_key] = data;
}

void PluginReverseInterface::AddTempQuotaManagedFile(
    const nacl::string& file_id) {
  PLUGIN_PRINTF(("PluginReverseInterface::AddTempQuotaManagedFile: "
                 "(file_id='%s')\n", file_id.c_str()));
  nacl::MutexLocker take(&mu_);
  uint64_t file_key = STRTOULL(file_id.c_str(), NULL, 10);
  QuotaData data(plugin::TempQuotaType, 0);
  quota_map_[file_key] = data;
}

ServiceRuntime::ServiceRuntime(Plugin* plugin,
                               const Manifest* manifest,
                               bool should_report_uma,
                               pp::CompletionCallback init_done_cb,
                               pp::CompletionCallback crash_cb)
    : plugin_(plugin),
      should_report_uma_(should_report_uma),
      reverse_service_(NULL),
      subprocess_(NULL),
      anchor_(new nacl::WeakRefAnchor()),
      rev_interface_(new PluginReverseInterface(anchor_, plugin,
                                                manifest,
                                                this,
                                                init_done_cb, crash_cb)),
      exit_status_(-1) {
  NaClSrpcChannelInitialize(&command_channel_);
  NaClXMutexCtor(&mu_);
}

bool ServiceRuntime::InitCommunication(nacl::DescWrapper* nacl_desc,
                                       ErrorInfo* error_info) {
  PLUGIN_PRINTF(("ServiceRuntime::InitCommunication"
                 " (this=%p, subprocess=%p)\n",
                 static_cast<void*>(this),
                 static_cast<void*>(subprocess_.get())));
  // Create the command channel to the sel_ldr and load the nexe from nacl_desc.
  if (!subprocess_->SetupCommandAndLoad(&command_channel_, nacl_desc)) {
    error_info->SetReport(ERROR_SEL_LDR_COMMUNICATION_CMD_CHANNEL,
                          "ServiceRuntime: command channel creation failed");
    return false;
  }
  // Hook up the reverse service channel.  We are the IMC client, but
  // provide SRPC service.
  NaClDesc* out_conn_cap;
  NaClSrpcResultCodes rpc_result =
      NaClSrpcInvokeBySignature(&command_channel_,
                                "reverse_setup::h",
                                &out_conn_cap);

  if (NACL_SRPC_RESULT_OK != rpc_result) {
    error_info->SetReport(ERROR_SEL_LDR_COMMUNICATION_REV_SETUP,
                          "ServiceRuntime: reverse setup rpc failed");
    return false;
  }
  //  Get connection capability to service runtime where the IMC
  //  server/SRPC client is waiting for a rendezvous.
  PLUGIN_PRINTF(("ServiceRuntime: got 0x%"NACL_PRIxPTR"\n",
                 (uintptr_t) out_conn_cap));
  nacl::DescWrapper* conn_cap = plugin_->wrapper_factory()->MakeGenericCleanup(
      out_conn_cap);
  if (conn_cap == NULL) {
    error_info->SetReport(ERROR_SEL_LDR_COMMUNICATION_WRAPPER,
                          "ServiceRuntime: wrapper allocation failure");
    return false;
  }
  out_conn_cap = NULL;  // ownership passed
  PLUGIN_PRINTF(("ServiceRuntime::InitCommunication"
                 " starting reverse service\n"));
  reverse_service_ = new nacl::ReverseService(conn_cap, rev_interface_->Ref());
  if (!reverse_service_->Start()) {
    error_info->SetReport(ERROR_SEL_LDR_COMMUNICATION_REV_SERVICE,
                          "ServiceRuntime: starting reverse services failed");
    return false;
  }

  // start the module.  otherwise we cannot connect for multimedia
  // subsystem since that is handled by user-level code (not secure!)
  // in libsrpc.
  int load_status = -1;
  rpc_result =
      NaClSrpcInvokeBySignature(&command_channel_,
                                "start_module::i",
                                &load_status);

  if (NACL_SRPC_RESULT_OK != rpc_result) {
    error_info->SetReport(ERROR_SEL_LDR_START_MODULE,
                          "ServiceRuntime: could not start nacl module");
    return false;
  }
  PLUGIN_PRINTF(("ServiceRuntime::InitCommunication (load_status=%d)\n",
                 load_status));
  if (should_report_uma_) {
    plugin_->ReportSelLdrLoadStatus(load_status);
  }
  if (LOAD_OK != load_status) {
    error_info->SetReport(
        ERROR_SEL_LDR_START_STATUS,
        NaClErrorString(static_cast<NaClErrorCode>(load_status)));
    return false;
  }
  return true;
}

bool ServiceRuntime::Start(nacl::DescWrapper* nacl_desc,
                           ErrorInfo* error_info,
                           const nacl::string& url,
                           bool uses_ppapi,
                           bool enable_ppapi_dev,
                           pp::CompletionCallback crash_cb) {
  PLUGIN_PRINTF(("ServiceRuntime::Start (nacl_desc=%p)\n",
                 reinterpret_cast<void*>(nacl_desc)));

  nacl::scoped_ptr<SelLdrLauncherChrome>
      tmp_subprocess(new SelLdrLauncherChrome());
  if (NULL == tmp_subprocess.get()) {
    PLUGIN_PRINTF(("ServiceRuntime::Start (subprocess create failed)\n"));
    error_info->SetReport(ERROR_SEL_LDR_CREATE_LAUNCHER,
                          "ServiceRuntime: failed to create sel_ldr launcher");
    return false;
  }
  bool started = tmp_subprocess->Start(plugin_->pp_instance(),
                                       url.c_str(),
                                       uses_ppapi,
                                       enable_ppapi_dev);
  if (!started) {
    PLUGIN_PRINTF(("ServiceRuntime::Start (start failed)\n"));
    error_info->SetReport(ERROR_SEL_LDR_LAUNCH,
                          "ServiceRuntime: failed to start");
    return false;
  }

  subprocess_.reset(tmp_subprocess.release());
  if (!InitCommunication(nacl_desc, error_info)) {
    // On a load failure the service runtime does not crash itself to
    // avoid a race where the no-more-senders error on the reverse
    // channel esrvice thread might cause the crash-detection logic to
    // kick in before the start_module RPC reply has been received. So
    // we induce a service runtime crash here. We do not release
    // subprocess_ since it's needed to collect crash log output after
    // the error is reported.
    Log(LOG_FATAL, "reap logs");
    if (NULL == reverse_service_) {
      // No crash detector thread.
      PLUGIN_PRINTF(("scheduling to get crash log\n"));
      pp::Module::Get()->core()->CallOnMainThread(0, crash_cb, PP_OK);
      PLUGIN_PRINTF(("should fire soon\n"));
    } else {
      PLUGIN_PRINTF(("Reverse service thread will pick up crash log\n"));
    }
    return false;
  }

  PLUGIN_PRINTF(("ServiceRuntime::Start (return 1)\n"));
  return true;
}

SrpcClient* ServiceRuntime::SetupAppChannel() {
  PLUGIN_PRINTF(("ServiceRuntime::SetupAppChannel (subprocess_=%p)\n",
                 reinterpret_cast<void*>(subprocess_.get())));
  nacl::DescWrapper* connect_desc = subprocess_->socket_addr()->Connect();
  if (NULL == connect_desc) {
    PLUGIN_PRINTF(("ServiceRuntime::SetupAppChannel (connect failed)\n"));
    return NULL;
  } else {
    PLUGIN_PRINTF(("ServiceRuntime::SetupAppChannel (conect_desc=%p)\n",
                   static_cast<void*>(connect_desc)));
    SrpcClient* srpc_client = SrpcClient::New(connect_desc);
    PLUGIN_PRINTF(("ServiceRuntime::SetupAppChannel (srpc_client=%p)\n",
                   static_cast<void*>(srpc_client)));
    delete connect_desc;
    return srpc_client;
  }
}

bool ServiceRuntime::Log(int severity, const nacl::string& msg) {
  NaClSrpcResultCodes rpc_result =
      NaClSrpcInvokeBySignature(&command_channel_,
                                "log:is:",
                                severity,
                                strdup(msg.c_str()));
  return (NACL_SRPC_RESULT_OK == rpc_result);
}

void ServiceRuntime::Shutdown() {
  rev_interface_->ShutDown();
  anchor_->Abandon();
  // Abandon callbacks, tell service threads to quit if they were
  // blocked waiting for main thread operations to finish.  Note that
  // some callbacks must still await their completion event, e.g.,
  // CallOnMainThread must still wait for the time out, or I/O events
  // must finish, so resources associated with pending events cannot
  // be deallocated.

  // Note that this does waitpid() to get rid of any zombie subprocess.
  subprocess_.reset(NULL);

  NaClSrpcDtor(&command_channel_);

  // subprocess_ has been shut down, but threads waiting on messages
  // from the service runtime may not have noticed yet.  The low-level
  // NaClSimpleRevService code takes care to refcount the data objects
  // that it needs, and reverse_service_ is also refcounted.  We wait
  // for the service threads to get their EOF indications.
  if (reverse_service_ != NULL) {
    reverse_service_->WaitForServiceThreadsToExit();
    reverse_service_->Unref();
    reverse_service_ = NULL;
  }
}

ServiceRuntime::~ServiceRuntime() {
  PLUGIN_PRINTF(("ServiceRuntime::~ServiceRuntime (this=%p)\n",
                 static_cast<void*>(this)));
  // We do this just in case Shutdown() was not called.
  subprocess_.reset(NULL);
  if (reverse_service_ != NULL) {
    reverse_service_->Unref();
  }

  rev_interface_->Unref();

  anchor_->Unref();
  NaClMutexDtor(&mu_);
}

int ServiceRuntime::exit_status() {
  nacl::MutexLocker take(&mu_);
  return exit_status_;
}

void ServiceRuntime::set_exit_status(int exit_status) {
  nacl::MutexLocker take(&mu_);
  exit_status_ = exit_status & 0xff;
}

nacl::string ServiceRuntime::GetCrashLogOutput() {
  if (NULL != subprocess_.get()) {
    return subprocess_->GetCrashLogOutput();
  } else {
    return "";
  }
}

}  // namespace plugin
