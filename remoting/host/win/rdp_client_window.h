// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_RDP_HOST_WINDOW_H_
#define REMOTING_HOST_WIN_RDP_HOST_WINDOW_H_

#include <atlbase.h>
#include <atlcom.h>
#include <atlcrack.h>
#include <atlctl.h>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/win/scoped_comptr.h"
#include "net/base/ip_endpoint.h"
#include "third_party/skia/include/core/SkSize.h"

#import "PROGID:MsTscAx.MsTscAx" \
    exclude("wireHWND", "_RemotableHandle", "__MIDL_IWinTypes_0009"), \
    rename_namespace("mstsc") raw_interfaces_only no_implementation

namespace remoting {

// RdpClientWindow is used to establish a connection to the given RDP endpoint.
// It is a GUI window class that hosts Microsoft RDP ActiveX control, which
// takes care of handling RDP properly. RdpClientWindow must be used only on
// a UI thread.
class RdpClientWindow
    : public CWindowImpl<RdpClientWindow, CWindow, CFrameWinTraits>,
      public IDispEventImpl<1, RdpClientWindow,
                            &__uuidof(mstsc::IMsTscAxEvents),
                            &__uuidof(mstsc::__MSTSCLib), 1, 0> {
 public:
  // Receives connect/disconnect notifications. The notifications can be
  // delivered after RdpClientWindow::Connect() returned success.
  //
  // RdpClientWindow guarantees that OnDisconnected() is the last notification
  // the event handler receives. OnDisconnected() is guaranteed to be called
  // only once.
  class EventHandler {
   public:
    virtual ~EventHandler() {}

    // Invoked when the RDP control has established a connection.
    virtual void OnConnected() = 0;

    // Invoked when the RDP control has been disconnected from the RDP server.
    // This includes both graceful shutdown and any fatal error condition.
    //
    // Once RdpClientWindow::Connect() returns success the owner of the
    // |RdpClientWindow| object must keep it alive until OnDisconnected() is
    // called.
    //
    // OnDisconnected() should not delete |RdpClientWindow| object directly.
    // Instead it should post a task to delete the object. The ActiveX code
    // expects the window be alive until the currently handled window message is
    // completely processed.
    virtual void OnDisconnected() = 0;
  };

  DECLARE_WND_CLASS(L"RdpClientWindow")

  // Specifies the endpoint to connect to and passes the event handler pointer
  // to be notified about connection events.
  RdpClientWindow(const net::IPEndPoint& server_endpoint,
                  EventHandler* event_handler);
  ~RdpClientWindow();

  // Creates the window along with the ActiveX control and initiates the
  // connection. |screen_size| specifies resolution of the screen. Returns false
  // if an error occurs.
  bool Connect(const SkISize& screen_size);

  // Initiates shutdown of the connection. The caller must not delete |this|
  // until it receives OnDisconnected() notification.
  void Disconnect();

 private:
  typedef IDispEventImpl<1, RdpClientWindow,
                         &__uuidof(mstsc::IMsTscAxEvents),
                         &__uuidof(mstsc::__MSTSCLib), 1, 0> RdpEventsSink;

  // Handled window messages.
  BEGIN_MSG_MAP_EX(RdpClientWindow)
    MSG_WM_CLOSE(OnClose)
    MSG_WM_CREATE(OnCreate)
    MSG_WM_DESTROY(OnDestroy)
  END_MSG_MAP()

  // Requests the RDP ActiveX control to close the connection gracefully.
  void OnClose();

  // Creates the RDP ActiveX control, configures it, and initiates an RDP
  // connection to |server_endpoint_|.
  LRESULT OnCreate(CREATESTRUCT* create_struct);

  // Releases the RDP ActiveX control interfaces.
  void OnDestroy();

  BEGIN_SINK_MAP(RdpClientWindow)
    SINK_ENTRY_EX(1, __uuidof(mstsc::IMsTscAxEvents), 2, OnConnected)
    SINK_ENTRY_EX(1, __uuidof(mstsc::IMsTscAxEvents), 4, OnDisconnected)
    SINK_ENTRY_EX(1, __uuidof(mstsc::IMsTscAxEvents), 10, OnFatalError)
    SINK_ENTRY_EX(1, __uuidof(mstsc::IMsTscAxEvents), 15, OnConfirmClose)
  END_SINK_MAP()

  // mstsc::IMsTscAxEvents notifications.
  STDMETHOD(OnConnected)();
  STDMETHOD(OnDisconnected)(long reason);
  STDMETHOD(OnFatalError)(long error_code);
  STDMETHOD(OnConfirmClose)(VARIANT_BOOL* allow_close);

  // Wrappers for the event handler's methods that make sure that
  // OnDisconnected() is the last notification delivered and is delevered
  // only once.
  void NotifyConnected();
  void NotifyDisconnected();

  // Invoked to report connect/disconnect events.
  EventHandler* event_handler_;

  // The endpoint to connect to.
  net::IPEndPoint server_endpoint_;

  // Interfaces exposed by the RDP ActiveX control.
  base::win::ScopedComPtr<mstsc::IMsRdpClient> client_;
  base::win::ScopedComPtr<mstsc::IMsRdpClientAdvancedSettings> client_settings_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_RDP_HOST_WINDOW_H_
