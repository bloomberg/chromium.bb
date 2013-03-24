// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/rdp_client_window.h"

#include <wtsdefs.h>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"

// RDP connection disconnect reasons codes that should not be interpreted as
// errors.
const long kDisconnectReasonNoInfo = 0;
const long kDisconnectReasonLocalNotError = 1;
const long kDisconnectReasonRemoteByUser = 2;
const long kDisconnectReasonByServer = 3;

namespace remoting {

RdpClientWindow::RdpClientWindow(const net::IPEndPoint& server_endpoint,
                                 EventHandler* event_handler)
    : event_handler_(event_handler),
      screen_size_(SkISize::Make(0, 0)),
      server_endpoint_(server_endpoint) {
}

RdpClientWindow::~RdpClientWindow() {
  if (m_hWnd)
    DestroyWindow();

  DCHECK(!client_);
  DCHECK(!client_settings_);
}

bool RdpClientWindow::Connect(const SkISize& screen_size) {
  DCHECK(!m_hWnd);

  screen_size_ = screen_size;
  RECT rect = { 0, 0, screen_size_.width(), screen_size_.height() };
  bool result = Create(NULL, rect, NULL) != NULL;

  // Hide the window since this class is about establishing a connection, not
  // about showing a UI to the user.
  if (result)
    ShowWindow(SW_HIDE);

  return result;
}

void RdpClientWindow::Disconnect() {
  DCHECK(m_hWnd);

  SendMessage(WM_CLOSE);
}

void RdpClientWindow::OnClose() {
  if (!client_) {
    NotifyDisconnected();
    return;
  }

  // Request a graceful shutdown.
  mstsc::ControlCloseStatus close_status;
  HRESULT result = client_->RequestClose(&close_status);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to request a graceful shutdown of an RDP connection"
               << ", result=0x" << std::hex << result << std::dec;
    NotifyDisconnected();
    return;
  }

  if (close_status != mstsc::controlCloseWaitForEvents) {
    NotifyDisconnected();
    return;
  }

  // Expect IMsTscAxEvents::OnConfirmClose() or IMsTscAxEvents::OnDisconnect()
  // to be called if mstsc::controlCloseWaitForEvents was returned.
}

LRESULT RdpClientWindow::OnCreate(CREATESTRUCT* create_struct) {
  CAxWindow2 activex_window;
  base::win::ScopedComPtr<IUnknown> control;
  HRESULT result = E_FAIL;
  base::win::ScopedBstr server_name(
      UTF8ToUTF16(server_endpoint_.ToStringWithoutPort()).c_str());

  // Create the child window that actually hosts the ActiveX control.
  RECT rect = { 0, 0, screen_size_.width(), screen_size_.height() };
  activex_window.Create(m_hWnd, rect, NULL, WS_CHILD | WS_VISIBLE | WS_BORDER);
  if (activex_window.m_hWnd == NULL) {
    result = HRESULT_FROM_WIN32(GetLastError());
    goto done;
  }

  // Instantiate the RDP ActiveX control.
  result = activex_window.CreateControlEx(
      OLESTR("MsTscAx.MsTscAx"),
      NULL,
      NULL,
      control.Receive(),
      __uuidof(mstsc::IMsTscAxEvents),
      reinterpret_cast<IUnknown*>(static_cast<RdpEventsSink*>(this)));
  if (FAILED(result))
    goto done;

  result = control.QueryInterface(client_.Receive());
  if (FAILED(result))
    goto done;

  // Use 32-bit color.
  result = client_->put_ColorDepth(32);
  if (FAILED(result))
    goto done;

  // Set dimensions of the remote desktop.
  result = client_->put_DesktopWidth(screen_size_.width());
  if (FAILED(result))
    goto done;
  result = client_->put_DesktopHeight(screen_size_.height());
  if (FAILED(result))
    goto done;

  // Set the server name to connect to.
  result = client_->put_Server(server_name);
  if (FAILED(result))
    goto done;

  // Fetch IMsRdpClientAdvancedSettings interface for the client.
  result = client_->get_AdvancedSettings2(client_settings_.Receive());
  if (FAILED(result))
    goto done;

  // Disable background input mode.
  result = client_settings_->put_allowBackgroundInput(0);
  if (FAILED(result))
    goto done;

  // Do not use bitmap cache.
  result = client_settings_->put_BitmapPersistence(0);
  if (SUCCEEDED(result))
    result = client_settings_->put_CachePersistenceActive(0);
  if (FAILED(result))
    goto done;

  // Do not use compression.
  result = client_settings_->put_Compress(0);
  if (FAILED(result))
    goto done;

  // Disable printer and clipboard redirection.
  result = client_settings_->put_DisableRdpdr(FALSE);
  if (FAILED(result))
    goto done;

  // Do not display the connection bar.
  result = client_settings_->put_DisplayConnectionBar(VARIANT_FALSE);
  if (FAILED(result))
    goto done;

  // Do not grab focus on connect.
  result = client_settings_->put_GrabFocusOnConnect(VARIANT_FALSE);
  if (FAILED(result))
    goto done;

  // Enable enhanced graphics, font smoothing and desktop composition.
  const LONG kDesiredFlags = WTS_PERF_ENABLE_ENHANCED_GRAPHICS |
                             WTS_PERF_ENABLE_FONT_SMOOTHING |
                             WTS_PERF_ENABLE_DESKTOP_COMPOSITION;
  result = client_settings_->put_PerformanceFlags(kDesiredFlags);
  if (FAILED(result))
    goto done;

  // Set the port to connect to.
  result = client_settings_->put_RDPPort(server_endpoint_.port());
  if (FAILED(result))
    goto done;

  result = client_->Connect();
  if (FAILED(result))
    goto done;

done:
  if (FAILED(result)) {
    LOG(ERROR) << "RDP: failed to initiate a connection to "
               << server_endpoint_.ToString() << ": error="
               << std::hex << result << std::dec;
    client_.Release();
    client_settings_.Release();
    return -1;
  }

  return 0;
}

void RdpClientWindow::OnDestroy() {
  client_.Release();
  client_settings_.Release();
}

HRESULT RdpClientWindow::OnConnected() {
  VLOG(1) << "RDP: successfully connected to " << server_endpoint_.ToString();

  NotifyConnected();
  return S_OK;
}

HRESULT RdpClientWindow::OnDisconnected(long reason) {
  if (reason == kDisconnectReasonNoInfo ||
      reason == kDisconnectReasonLocalNotError ||
      reason == kDisconnectReasonRemoteByUser ||
      reason == kDisconnectReasonByServer) {
    VLOG(1) << "RDP: disconnected from " << server_endpoint_.ToString()
            << ", reason=" << reason;
    NotifyDisconnected();
    return S_OK;
  }

  // Get the extended disconnect reason code.
  mstsc::ExtendedDisconnectReasonCode extended_code;
  HRESULT result = client_->get_ExtendedDisconnectReason(&extended_code);
  if (FAILED(result))
    extended_code = mstsc::exDiscReasonNoInfo;

  // Get the error message as well.
  base::win::ScopedBstr error_message;
  base::win::ScopedComPtr<mstsc::IMsRdpClient5> client5;
  result = client_.QueryInterface(client5.Receive());
  if (SUCCEEDED(result)) {
    result = client5->GetErrorDescription(reason, extended_code,
                                          error_message.Receive());
    if (FAILED(result))
      error_message.Reset();
  }

  LOG(ERROR) << "RDP: disconnected from " << server_endpoint_.ToString()
             << ": " << error_message << " (reason=" << reason
             << ", extended_code=" << extended_code << ")";

  NotifyDisconnected();
  return S_OK;
}

HRESULT RdpClientWindow::OnFatalError(long error_code) {
  LOG(ERROR) << "RDP: an error occured: error_code="
             << error_code;

  NotifyDisconnected();
  return S_OK;
}

HRESULT RdpClientWindow::OnConfirmClose(VARIANT_BOOL* allow_close) {
  *allow_close = VARIANT_TRUE;

  NotifyDisconnected();
  return S_OK;
}

void RdpClientWindow::NotifyConnected() {
  if (event_handler_)
    event_handler_->OnConnected();
}

void RdpClientWindow::NotifyDisconnected() {
  if (event_handler_) {
    EventHandler* event_handler = event_handler_;
    event_handler_ = NULL;
    event_handler->OnDisconnected();
  }
}

}  // namespace remoting
