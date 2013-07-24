// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/viewer/metro_viewer_process_host.h"

#include <shlobj.h>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "base/win/scoped_comptr.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ui/aura/remote_root_window_host_win.h"
#include "ui/metro_viewer/metro_viewer_messages.h"
#include "win8/viewer/metro_viewer_constants.h"

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

  scoped_refptr<InternalMessageFilter> message_filter(
      new InternalMessageFilter(this));
  channel_->AddFilter(message_filter);

  base::win::ScopedComPtr<IApplicationActivationManager> activator;
  HRESULT hr = activator.CreateInstance(CLSID_ApplicationActivationManager);
  if (SUCCEEDED(hr)) {
    DWORD pid = 0;
    hr = activator->ActivateApplication(
        app_user_model_id.c_str(), L"open", AO_NONE, &pid);
  }

  LOG_IF(ERROR, FAILED(hr)) << "Tried and failed to launch Metro Chrome. "
                            << "hr=" << std::hex << hr;

  // Having launched the viewer process, now we wait for it to connect.
  bool success =
      channel_connected_event_->TimedWait(base::TimeDelta::FromSeconds(60));
  channel_connected_event_.reset();

  // |message_filter| is only used to signal |channel_connected_event_| above
  // and can thus be removed after |channel_connected_event_| is no longer
  // waiting.
  channel_->RemoveFilter(message_filter);
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
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled ? true :
      aura::RemoteRootWindowHostWin::Instance()->OnMessageReceived(message);
}

void MetroViewerProcessHost::NotifyChannelConnected() {
  if (channel_connected_event_)
    channel_connected_event_->Signal();
}

}  // namespace win8
