// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/rdp_client.h"

#include <windows.h>
#include <iphlpapi.h>
#include <winsock2.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"
#include "remoting/base/typed_buffer.h"
#include "remoting/host/win/rdp_client_window.h"

namespace remoting {

namespace {

// Default width and hight of the RDP client window.
const long kDefaultWidth = 1024;
const long kDefaultHeight = 768;

// The range of addresses RdpClient may use to distinguish client connections:
// 127.0.0.2 - 127.255.255.254. 127.0.0.1 is explicitly blocked by the RDP
// ActiveX control.
const int kMinLoopbackAddress = 0x7f000002;
const int kMaxLoopbackAddress = 0x7ffffffe;

const int kRdpPort = 3389;

}  // namespace

// The core of RdpClient is ref-counted since it services calls and notifies
// events on the caller task runner, but runs the ActiveX control on the UI
// task runner.
class RdpClient::Core
    : public base::RefCountedThreadSafe<Core>,
      public RdpClientWindow::EventHandler {
 public:
  Core(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      RdpClient::EventHandler* event_handler);

  // Initiates a loopback RDP connection.
  void Connect();

  // Initiates a graceful shutdown of the RDP connection.
  void Disconnect();

  // RdpClientWindow::EventHandler interface.
  virtual void OnConnected() OVERRIDE;
  virtual void OnDisconnected() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<Core>;
  virtual ~Core();

  // Returns the local address that is connected to |remote_endpoint|.
  // The address is returned in |*local_endpoint|. The method fails if
  // the connection could not be found in the table of TCP connections or there
  // are more than one matching connection.
  bool MatchRemoteEndpoint(const sockaddr_in& remote_endpoint,
                           sockaddr_in* local_endpoint);

  // Same as MatchRemoteEndpoint() but also uses the PID of the process bound
  // to |local_endpoint| to match the connection.
  bool MatchRemoteEndpointWithPid(const sockaddr_in& remote_endpoint,
                                  DWORD local_pid,
                                  sockaddr_in* local_endpoint);

  // Helpers for the event handler's methods that make sure that OnRdpClosed()
  // is the last notification delivered and is delevered only once.
  void NotifyConnected(const net::IPEndPoint& client_endpoint);
  void NotifyClosed();

  // Task runner on which the caller expects |event_handler_| to be notified.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Task runner on which |rdp_client_window_| is running.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Event handler receiving notification about connection state. The pointer is
  // cleared when Disconnect() methods is called, stopping any further updates.
  RdpClient::EventHandler* event_handler_;

  // Points to GetExtendedTcpTable().
  typedef DWORD (WINAPI * GetExtendedTcpTableFn)(
      PVOID, PDWORD, BOOL, ULONG, TCP_TABLE_CLASS, ULONG);
  GetExtendedTcpTableFn get_extended_tcp_table_;

  // Hosts the RDP ActiveX control.
  scoped_ptr<RdpClientWindow> rdp_client_window_;

  // The endpoint that |rdp_client_window_| connects to.
  sockaddr_in server_address_;

  // Same as |server_address_| but represented as net::IPEndPoint.
  net::IPEndPoint server_endpoint_;

  // A self-reference to keep the object alive during connection shutdown.
  scoped_refptr<Core> self_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

RdpClient::RdpClient(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    EventHandler* event_handler) {
  DCHECK(caller_task_runner->BelongsToCurrentThread());

  core_ = new Core(caller_task_runner, ui_task_runner, event_handler);
  core_->Connect();
}

RdpClient::~RdpClient() {
  DCHECK(CalledOnValidThread());

  core_->Disconnect();
}

RdpClient::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    RdpClient::EventHandler* event_handler)
    : caller_task_runner_(caller_task_runner),
      ui_task_runner_(ui_task_runner),
      event_handler_(event_handler),
      get_extended_tcp_table_(NULL) {
}

void RdpClient::Core::Connect() {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(FROM_HERE, base::Bind(&Core::Connect, this));
    return;
  }

  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);
  DCHECK(!rdp_client_window_);
  DCHECK(!self_);

  // This code link statically agains iphlpapi.dll, so it must be loaded
  // already.
  HMODULE iphlpapi_handle = GetModuleHandle(L"iphlpapi.dll");
  CHECK(iphlpapi_handle != NULL);

  // Get a pointer to GetExtendedTcpTable() which is available starting from
  // XP SP2 / W2K3 SP1.
  get_extended_tcp_table_ = reinterpret_cast<GetExtendedTcpTableFn>(
      GetProcAddress(iphlpapi_handle, "GetExtendedTcpTable"));
  CHECK(get_extended_tcp_table_);

  // Generate a random loopback address to connect to.
  memset(&server_address_, 0, sizeof(server_address_));
  server_address_.sin_family = AF_INET;
  server_address_.sin_port = htons(kRdpPort);
  server_address_.sin_addr.S_un.S_addr = htonl(
      base::RandInt(kMinLoopbackAddress, kMaxLoopbackAddress));

  CHECK(server_endpoint_.FromSockAddr(
      reinterpret_cast<struct sockaddr*>(&server_address_),
      sizeof(server_address_)));

  // Create the ActiveX control window.
  rdp_client_window_.reset(new RdpClientWindow(server_endpoint_, this));
  if (!rdp_client_window_->Connect(SkISize::Make(kDefaultWidth,
                                                 kDefaultHeight))) {
    rdp_client_window_.reset();

    // Notify the caller that connection attempt failed.
    NotifyClosed();
  }
}

void RdpClient::Core::Disconnect() {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(FROM_HERE, base::Bind(&Core::Disconnect, this));
    return;
  }

  // The caller does not expect any notifications to be delivered after this
  // point.
  event_handler_ = NULL;

  // Gracefully shutdown the RDP connection.
  if (rdp_client_window_) {
    self_ = this;
    rdp_client_window_->Disconnect();
  }
}

void RdpClient::Core::OnConnected() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(rdp_client_window_);

  // Now that the connection is established, map the server endpoint to
  // the client one.
  bool result;
  sockaddr_in client_address;
  if (get_extended_tcp_table_) {
    result = MatchRemoteEndpointWithPid(server_address_, GetCurrentProcessId(),
                                        &client_address);
  } else {
    result = MatchRemoteEndpoint(server_address_, &client_address);
  }

  if (!result) {
    NotifyClosed();
    Disconnect();
    return;
  }

  net::IPEndPoint client_endpoint;
  CHECK(client_endpoint.FromSockAddr(
      reinterpret_cast<struct sockaddr*>(&client_address),
      sizeof(client_address)));

  NotifyConnected(client_endpoint);
}

void RdpClient::Core::OnDisconnected() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(rdp_client_window_);

  NotifyClosed();

  // Delay window destruction until no ActiveX control's code is on the stack.
  ui_task_runner_->DeleteSoon(FROM_HERE, rdp_client_window_.release());
  self_ = NULL;
}

RdpClient::Core::~Core() {
  DCHECK(!event_handler_);
  DCHECK(!rdp_client_window_);
}

bool RdpClient::Core::MatchRemoteEndpoint(
    const sockaddr_in& remote_endpoint,
    sockaddr_in* local_endpoint) {
  TypedBuffer<MIB_TCPTABLE> tcp_table;
  DWORD tcp_table_size = 0;

  // Retrieve the size of the buffer needed for the IPv4 TCP connection table.
  DWORD result = GetTcpTable(tcp_table.get(), &tcp_table_size, FALSE);
  for (int retries = 0; retries < 5 && result == ERROR_INSUFFICIENT_BUFFER;
       ++retries) {
    // Allocate a buffer that is large enough.
    TypedBuffer<MIB_TCPTABLE> buffer(tcp_table_size);
    tcp_table.Swap(buffer);

    // Get the list of TCP connections (IPv4 only).
    result = GetTcpTable(tcp_table.get(), &tcp_table_size, FALSE);
  }
  if (result != ERROR_SUCCESS) {
    SetLastError(result);
    LOG_GETLASTERROR(ERROR)
        << "Failed to get the list of existing IPv4 TCP endpoints";
    return false;
  }

  // Match the connection by the server endpoint.
  bool found = false;
  MIB_TCPROW* row = tcp_table->table;
  MIB_TCPROW* row_end = row + tcp_table->dwNumEntries;
  for (; row != row_end; ++row) {
    if (row->dwRemoteAddr != remote_endpoint.sin_addr.S_un.S_addr ||
        LOWORD(row->dwRemotePort) != remote_endpoint.sin_port) {
      continue;
    }

    // Check if more than one connection has been matched.
    if (found) {
      LOG(ERROR) << "More than one connections matching "
                 << server_endpoint_.ToString() << " found.";
      return false;
    }

    memset(local_endpoint, 0, sizeof(*local_endpoint));
    local_endpoint->sin_family = AF_INET;
    local_endpoint->sin_addr.S_un.S_addr = row->dwLocalAddr;
    local_endpoint->sin_port = LOWORD(row->dwLocalPort);
    found = true;
  }

  if (!found) {
    LOG(ERROR) << "No connection matching " << server_endpoint_.ToString()
               << " found.";
    return false;
  }

  return true;
}

bool RdpClient::Core::MatchRemoteEndpointWithPid(
    const sockaddr_in& remote_endpoint,
    DWORD local_pid,
    sockaddr_in* local_endpoint) {
  TypedBuffer<MIB_TCPTABLE_OWNER_PID> tcp_table;
  DWORD tcp_table_size = 0;

  // Retrieve the size of the buffer needed for the IPv4 TCP connection table.
  DWORD result = get_extended_tcp_table_(tcp_table.get(),
                                         &tcp_table_size,
                                         FALSE,
                                         AF_INET,
                                         TCP_TABLE_OWNER_PID_CONNECTIONS,
                                         0);
  for (int retries = 0; retries < 5 && result == ERROR_INSUFFICIENT_BUFFER;
       ++retries) {
    // Allocate a buffer that is large enough.
    TypedBuffer<MIB_TCPTABLE_OWNER_PID> buffer(tcp_table_size);
    tcp_table.Swap(buffer);

    // Get the list of TCP connections (IPv4 only).
    result = get_extended_tcp_table_(tcp_table.get(),
                                     &tcp_table_size,
                                     FALSE,
                                     AF_INET,
                                     TCP_TABLE_OWNER_PID_CONNECTIONS,
                                     0);
  }
  if (result != ERROR_SUCCESS) {
    SetLastError(result);
    LOG_GETLASTERROR(ERROR)
        << "Failed to get the list of existing IPv4 TCP endpoints";
    return false;
  }

  // Match the connection by the server endpoint.
  bool found = false;
  MIB_TCPROW_OWNER_PID* row = tcp_table->table;
  MIB_TCPROW_OWNER_PID* row_end = row + tcp_table->dwNumEntries;
  for (; row != row_end; ++row) {
    if (row->dwRemoteAddr != remote_endpoint.sin_addr.S_un.S_addr ||
        LOWORD(row->dwRemotePort) != remote_endpoint.sin_port ||
        row->dwOwningPid != local_pid) {
      continue;
    }

    // Check if more than one connection has been matched.
    if (found) {
      LOG(ERROR) << "More than one connections matching "
                 << server_endpoint_.ToString() << " found.";
      return false;
    }

    memset(local_endpoint, 0, sizeof(*local_endpoint));
    local_endpoint->sin_family = AF_INET;
    local_endpoint->sin_addr.S_un.S_addr = row->dwLocalAddr;
    local_endpoint->sin_port = LOWORD(row->dwLocalPort);
    found = true;
  }

  if (!found) {
    LOG(ERROR) << "No connection matching " << server_endpoint_.ToString()
               << " and PID " << local_pid << " found.";
    return false;
  }

  return true;
}

void RdpClient::Core::NotifyConnected(const net::IPEndPoint& client_endpoint) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Core::NotifyConnected, this, client_endpoint));
    return;
  }

  if (event_handler_)
    event_handler_->OnRdpConnected(client_endpoint);
}

void RdpClient::Core::NotifyClosed() {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Core::NotifyClosed, this));
    return;
  }

  if (event_handler_) {
    RdpClient::EventHandler* event_handler = event_handler_;
    event_handler_ = NULL;
    event_handler->OnRdpClosed();
  }
}

}  // namespace remoting
