// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_terminator.h"

#include <unistd.h>
#include <memory>

#include "android_webview/browser/aw_render_process_gone_delegate.h"
#include "android_webview/common/aw_descriptors.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "components/crash/content/app/crashpad.h"
#include "components/crash/content/browser/crash_metrics_reporter_android.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "jni/AwBrowserProcess_jni.h"

using content::BrowserThread;

namespace android_webview {

namespace {

void GetAwRenderProcessGoneDelegatesForRenderProcess(
    content::RenderProcessHost* rph,
    std::vector<AwRenderProcessGoneDelegate*>* delegates) {
  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    content::RenderViewHost* view = content::RenderViewHost::From(widget);
    if (view && rph == view->GetProcess()) {
      content::WebContents* wc = content::WebContents::FromRenderViewHost(view);
      if (wc) {
        AwRenderProcessGoneDelegate* delegate =
            AwRenderProcessGoneDelegate::FromWebContents(wc);
        if (delegate)
          delegates->push_back(delegate);
      }
    }
  }
}

void OnRenderProcessGone(content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<AwRenderProcessGoneDelegate*> delegates;
  GetAwRenderProcessGoneDelegatesForRenderProcess(host, &delegates);
  for (auto* delegate : delegates)
    delegate->OnRenderProcessGone(host->GetID());
}

void OnRenderProcessGoneDetail(content::RenderProcessHost* host,
                               base::ProcessId child_process_pid,
                               bool crashed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<AwRenderProcessGoneDelegate*> delegates;
  GetAwRenderProcessGoneDelegatesForRenderProcess(host, &delegates);
  for (auto* delegate : delegates) {
    if (!delegate->OnRenderProcessGoneDetail(child_process_pid, crashed)) {
      if (crashed) {
        // Keeps this log unchanged, CTS test uses it to detect crash.
        std::string message = base::StringPrintf(
            "Render process (%d)'s crash wasn't handled by all associated  "
            "webviews, triggering application crash.",
            child_process_pid);
        crash_reporter::CrashWithoutDumping(message);
      } else {
        // The render process was most likely killed for OOM or switching
        // WebView provider, to make WebView backward compatible, kills the
        // browser process instead of triggering crash.
        LOG(ERROR) << "Render process (" << child_process_pid << ") kill (OOM"
                   << " or update) wasn't handed by all associated webviews,"
                   << " killing application.";
        kill(getpid(), SIGKILL);
      }
    }
  }

  // By this point we have moved the minidump to the crash directory, so it can
  // now be copied and uploaded.
  Java_AwBrowserProcess_triggerMinidumpUploading(
      base::android::AttachCurrentThread());
}

}  // namespace

AwBrowserTerminator::AwBrowserTerminator() = default;

AwBrowserTerminator::~AwBrowserTerminator() = default;

void AwBrowserTerminator::OnChildExit(
    const crash_reporter::ChildExitObserver::TerminationInfo& info) {
  content::RenderProcessHost* rph =
      content::RenderProcessHost::FromID(info.process_host_id);
  OnRenderProcessGone(rph);

  crash_reporter::CrashMetricsReporter::GetInstance()->ChildProcessExited(info);

  if (info.normal_termination) {
    return;
  }

  LOG(ERROR) << "Renderer process (" << info.pid << ") crash detected (code "
             << info.crash_signo << ").";

  OnRenderProcessGoneDetail(rph, info.pid, info.is_crashed());
}

}  // namespace android_webview
