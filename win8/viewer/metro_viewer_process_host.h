// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_VIEWER_METRO_VIEWER_PROCESS_HOST_H_
#define WIN8_VIEWER_METRO_VIEWER_PROCESS_HOST_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/threading/non_thread_safe.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class SingleThreadTaskRunner;
class WaitableEvent;
}

namespace IPC {
class Message;
}

namespace win8 {

// Abstract base class for various Metro viewer process host implementations.
class MetroViewerProcessHost : public IPC::Listener,
                               public IPC::Sender,
                               public base::NonThreadSafe {
 public:
  // Initializes a viewer process host to connect to the Metro viewer process
  // over IPC. The given task runner correspond to a thread on which
  // IPC::Channel is created and used (e.g. IO thread). Instantly connects to
  // the viewer process if one is already connected to |ipc_channel_name|; a
  // viewer can otherwise be launched synchronously via
  // LaunchViewerAndWaitForConnection().
  explicit MetroViewerProcessHost(
      base::SingleThreadTaskRunner* ipc_task_runner);
  virtual ~MetroViewerProcessHost();

  // Returns the process id of the viewer process if one is connected to this
  // host, returns base::kNullProcessId otherwise.
  base::ProcessId GetViewerProcessId();

  // Launches the viewer process associated with the given |app_user_model_id|
  // and blocks until that viewer process connects or until a timeout is
  // reached. Returns true if the viewer process connects before the timeout is
  // reached. NOTE: this assumes that the app referred to by |app_user_model_id|
  // is registered as the default browser.
  bool LaunchViewerAndWaitForConnection(
      const base::string16& app_user_model_id);

 private:
  // IPC::Sender implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE = 0;

  // Called over IPC by the viewer process to tell this host that it should be
  // drawing to |target_surface|.
  virtual void OnSetTargetSurface(gfx::NativeViewId target_surface) = 0;

  // Called over IPC by the viewer process to request that the url passed in be
  // opened.
  virtual void OnOpenURL(const string16& url) = 0;

  // Called over IPC by the viewer process to request that the search string
  // passed in is passed to the default search provider and a URL navigation be
  // performed.
  virtual void OnHandleSearchRequest(const string16& search_string) = 0;

  void NotifyChannelConnected();

  // Inner message filter used to handle connection event on the IPC channel
  // proxy's background thread. This prevents consumers of
  // MetroViewerProcessHost from having to pump messages on their own message
  // loop.
  class InternalMessageFilter : public IPC::ChannelProxy::MessageFilter {
   public:
    InternalMessageFilter(MetroViewerProcessHost* owner);

    // IPC::ChannelProxy::MessageFilter implementation.
    virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;

   private:
    MetroViewerProcessHost* owner_;
    DISALLOW_COPY_AND_ASSIGN(InternalMessageFilter);
  };

  scoped_ptr<IPC::ChannelProxy> channel_;
  scoped_ptr<base::WaitableEvent> channel_connected_event_;

  DISALLOW_COPY_AND_ASSIGN(MetroViewerProcessHost);
};

}  // namespace win8

#endif  // WIN8_VIEWER_METRO_VIEWER_PROCESS_HOST_H_
