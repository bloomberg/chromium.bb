// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/viewer/metro_viewer_process_host.h"

#include <shlobj.h>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ui/aura/remote_window_tree_host_win.h"
#include "ui/metro_viewer/metro_viewer_messages.h"
#include "win8/viewer/metro_viewer_constants.h"

namespace {

const int kViewerProcessConnectionTimeoutSecs = 60;

}  // namespace

namespace win8 {

MetroViewerProcessHost::InternalMessageFilter::InternalMessageFilter(
    MetroViewerProcessHost* owner) : owner_(owner) {
}

void MetroViewerProcessHost::InternalMessageFilter::OnChannelConnected(
    int32 /* peer_pid */) {
  owner_->NotifyChannelConnected();
}

MetroViewerProcessHost::MetroViewerProcessHost(
    base::SingleThreadTaskRunner* ipc_task_runner) {

  channel_.reset(new IPC::ChannelProxy(
      kMetroViewerIPCChannelName,
      IPC::Channel::MODE_NAMED_SERVER,
      this,
      ipc_task_runner));
}

MetroViewerProcessHost::~MetroViewerProcessHost() {
  if (!channel_)
    return;

  base::ProcessId viewer_process_id = GetViewerProcessId();
  channel_->Close();
  if (message_filter_) {
    // Wait for the viewer process to go away.
    if (viewer_process_id != base::kNullProcessId) {
      base::ProcessHandle viewer_process = NULL;
      base::OpenProcessHandleWithAccess(
          viewer_process_id,
          PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
          &viewer_process);
      if (viewer_process) {
        ::WaitForSingleObject(viewer_process, INFINITE);
        ::CloseHandle(viewer_process);
      }
    }
    channel_->RemoveFilter(message_filter_);
  }
}

base::ProcessId MetroViewerProcessHost::GetViewerProcessId() {
  if (channel_)
    return channel_->peer_pid();
  return base::kNullProcessId;
}

bool MetroViewerProcessHost::LaunchViewerAndWaitForConnection(
    const base::string16& app_user_model_id) {
  DCHECK_EQ(base::kNullProcessId, channel_->peer_pid());

  channel_connected_event_.reset(new base::WaitableEvent(false, false));

  message_filter_ = new InternalMessageFilter(this);
  channel_->AddFilter(message_filter_);

  if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
    base::win::ScopedComPtr<IApplicationActivationManager> activator;
    HRESULT hr = activator.CreateInstance(CLSID_ApplicationActivationManager);
    if (SUCCEEDED(hr)) {
      DWORD pid = 0;
      // Use the "connect" verb to
      hr = activator->ActivateApplication(
          app_user_model_id.c_str(), kMetroViewerConnectVerb, AO_NONE, &pid);
    }

    LOG_IF(ERROR, FAILED(hr)) << "Tried and failed to launch Metro Chrome. "
                              << "hr=" << std::hex << hr;
  } else {
    // For Windows 7 we need to launch the viewer ourselves.
    base::FilePath chrome_path;
    if (!PathService::Get(base::DIR_EXE, &chrome_path))
      return false;
    // TODO(cpu): launch with "-ServerName:DefaultBrowserServer"
    // note that the viewer might try to launch chrome again.
    CHECK(false);
  }

  // Having launched the viewer process, now we wait for it to connect.
  bool success =
      channel_connected_event_->TimedWait(base::TimeDelta::FromSeconds(
          kViewerProcessConnectionTimeoutSecs));
  channel_connected_event_.reset();
  return success;
}

bool MetroViewerProcessHost::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}

bool MetroViewerProcessHost::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(CalledOnValidThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MetroViewerProcessHost, message)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_SetTargetSurface, OnSetTargetSurface)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_OpenURL, OnOpenURL)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_SearchRequest, OnHandleSearchRequest)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_WindowSizeChanged,
                        OnWindowSizeChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled ? true :
      aura::RemoteWindowTreeHostWin::Instance()->OnMessageReceived(message);
}

void MetroViewerProcessHost::NotifyChannelConnected() {
  if (channel_connected_event_)
    channel_connected_event_->Signal();
}

}  // namespace win8
