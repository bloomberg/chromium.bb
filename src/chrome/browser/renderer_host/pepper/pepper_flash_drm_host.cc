// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/pepper_flash_drm_host.h"

#if defined(OS_WIN)
#include <Windows.h>
#endif

#include "base/bind.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/pepper_plugin_info.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/renderer_host/pepper/monitor_finder_mac.h"
#endif

using content::BrowserPpapiHost;

namespace {
const char kVoucherFilename[] = "plugin.vch";
}

#if defined(OS_WIN)
// Helper class to get the UI thread which monitor is showing the
// window associated with the instance's render view. Since we get
// called by the IO thread and we cannot block, the first answer is
// of GetMonitor() may be NULL, but eventually it will contain the
// right monitor.
class MonitorFinder : public base::RefCountedThreadSafe<MonitorFinder> {
 public:
  MonitorFinder(int process_id, int render_frame_id)
      : process_id_(process_id),
        render_frame_id_(render_frame_id),
        monitor_(NULL),
        request_sent_(0) {}

  int64_t GetMonitor() {
    // We use |request_sent_| as an atomic boolean so that we
    // never have more than one task posted at a given time. We
    // do this because we don't know how often our client is going
    // to call and we can't cache the |monitor_| value.
    if (InterlockedCompareExchange(&request_sent_, 1, 0) == 0) {
      base::PostTask(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(&MonitorFinder::FetchMonitorFromWidget, this));
    }
    return reinterpret_cast<int64_t>(monitor_);
  }

 private:
  friend class base::RefCountedThreadSafe<MonitorFinder>;
  ~MonitorFinder() {}

  void FetchMonitorFromWidget() {
    InterlockedExchange(&request_sent_, 0);
    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromID(process_id_, render_frame_id_);
    if (!rfh)
      return;
    gfx::NativeView native_view = rfh->GetNativeView();
    if (!native_view)
      return;
#if defined(USE_AURA)
    aura::WindowTreeHost* host = native_view->GetHost();
    if (!host)
      return;
    HWND window = host->GetAcceleratedWidget();
#else
    HWND window = native_view;
#endif
    HMONITOR monitor = ::MonitorFromWindow(window, MONITOR_DEFAULTTONULL);
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(&monitor_),
                               monitor);
  }

  const int process_id_;
  const int render_frame_id_;
  volatile HMONITOR monitor_;
  volatile long request_sent_;
};
#elif !defined(OS_MACOSX)
// TODO(cpu): Support Linux someday.
class MonitorFinder : public base::RefCountedThreadSafe<MonitorFinder> {
 public:
  MonitorFinder(int, int) {}
  int64_t GetMonitor() { return 0; }

 private:
  friend class base::RefCountedThreadSafe<MonitorFinder>;
  ~MonitorFinder() {}
};
#endif

PepperFlashDRMHost::PepperFlashDRMHost(BrowserPpapiHost* host,
                                       PP_Instance instance,
                                       PP_Resource resource)
    : ppapi::host::ResourceHost(host->GetPpapiHost(), instance, resource) {
  // Grant permissions to read the flash voucher file.
  int render_process_id;
  int render_frame_id;
  bool success = host->GetRenderFrameIDsForInstance(
      instance, &render_process_id, &render_frame_id);
  base::FilePath plugin_dir = host->GetPluginPath().DirName();
  DCHECK(!plugin_dir.empty() && success);
  base::FilePath voucher_file = plugin_dir.AppendASCII(kVoucherFilename);
  content::ChildProcessSecurityPolicy::GetInstance()->GrantReadFile(
      render_process_id, voucher_file);

  fetcher_ = new DeviceIDFetcher(render_process_id);
  monitor_finder_ = new MonitorFinder(render_process_id, render_frame_id);
  monitor_finder_->GetMonitor();
}

PepperFlashDRMHost::~PepperFlashDRMHost() {}

int32_t PepperFlashDRMHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperFlashDRMHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FlashDRM_GetDeviceID,
                                        OnHostMsgGetDeviceID)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FlashDRM_GetHmonitor,
                                        OnHostMsgGetHmonitor)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FlashDRM_MonitorIsExternal,
                                        OnHostMsgMonitorIsExternal)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperFlashDRMHost::OnHostMsgGetDeviceID(
    ppapi::host::HostMessageContext* context) {
  if (!fetcher_->Start(base::Bind(&PepperFlashDRMHost::GotDeviceID,
                                  weak_factory_.GetWeakPtr(),
                                  context->MakeReplyMessageContext()))) {
    return PP_ERROR_INPROGRESS;
  }
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFlashDRMHost::OnHostMsgGetHmonitor(
    ppapi::host::HostMessageContext* context) {
  int64_t monitor_id = monitor_finder_->GetMonitor();
  if (monitor_id) {
    context->reply_msg = PpapiPluginMsg_FlashDRM_GetHmonitorReply(monitor_id);
    return PP_OK;
  }
  return PP_ERROR_FAILED;
}

int32_t PepperFlashDRMHost::OnHostMsgMonitorIsExternal(
    ppapi::host::HostMessageContext* context) {
  int64_t monitor_id = monitor_finder_->GetMonitor();
  if (!monitor_id)
    return PP_ERROR_FAILED;

  PP_Bool is_external = PP_FALSE;
#if defined(OS_MACOSX)
  if (!MonitorFinder::IsMonitorBuiltIn(monitor_id))
    is_external = PP_TRUE;
#endif
  context->reply_msg =
      PpapiPluginMsg_FlashDRM_MonitorIsExternalReply(is_external);
  return PP_OK;
}

void PepperFlashDRMHost::GotDeviceID(
    ppapi::host::ReplyMessageContext reply_context,
    const std::string& id,
    int32_t result) {
  if (id.empty() && result == PP_OK) {
    NOTREACHED();
    result = PP_ERROR_FAILED;
  }
  reply_context.params.set_result(result);
  host()->SendReply(reply_context,
                    PpapiPluginMsg_FlashDRM_GetDeviceIDReply(id));
}
