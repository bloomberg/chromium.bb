// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A server side dispatcher which dispatches a given client's data to their
// stream.

#ifndef NET_TOOLS_QUIC_QUIC_DISPATCHER_H_
#define NET_TOOLS_QUIC_QUIC_DISPATCHER_H_

#include <list>

#include "base/hash_tables.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/blocked_list.h"
#include "net/quic/quic_blocked_writer_interface.h"
#include "net/quic/quic_protocol.h"
#include "net/tools/flip_server/epoll_server.h"
#include "net/tools/quic/quic_packet_writer.h"
#include "net/tools/quic/quic_server_session.h"
#include "net/tools/quic/quic_time_wait_list_manager.h"

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template<>
struct hash<net::QuicBlockedWriterInterface*> {
  std::size_t operator()(const net::QuicBlockedWriterInterface* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
}
#endif

namespace gfe2 {
  class EpollServer;
}

class QuicSession;

namespace net {

class DeleteSessionsAlarm;

class QuicDispatcher : public QuicPacketWriter, public QuicSessionOwner {
 public:
  typedef BlockedList<QuicBlockedWriterInterface*> WriteBlockedList;

  // Due to the way delete_sessions_closure_ is registered, the Dispatcher
  // must live until epoll_server Shutdown.
  QuicDispatcher(int fd, EpollServer* epoll_server);
  virtual ~QuicDispatcher();

  // QuicPacketWriter
  virtual int WritePacket(const char* buffer, size_t buf_len,
                          const IPAddressNumber& self_address,
                          const IPEndPoint& peer_address,
                          QuicBlockedWriterInterface* writer,
                          int* error) OVERRIDE;

  virtual void ProcessPacket(const IPEndPoint& server_address,
                             const IPEndPoint& client_address,
                             QuicGuid guid,
                             const QuicEncryptedPacket& packet);

  // Called when the underyling connection becomes writable to allow
  // queued writes to happen.
  //
  // Returns true if more writes are possible, false otherwise.
  virtual bool OnCanWrite();

  // Sends ConnectionClose frames to all connected clients.
  void Shutdown();

  // Ensure that the closed connection is cleaned up asynchronously.
  virtual void OnConnectionClose(QuicGuid guid, QuicErrorCode error) OVERRIDE;

  int fd() { return fd_; }
  void set_fd(int fd) { fd_ = fd; }

  typedef base::hash_map<QuicGuid, QuicSession*> SessionMap;

  virtual QuicSession* CreateQuicSession(
      QuicGuid guid,
      const IPEndPoint& client_address,
      int fd,
      EpollServer* epoll_server);

  // Deletes all sessions on the closed session list and clears the list.
  void DeleteSessions();

  const SessionMap& session_map() const { return session_map_; }

  WriteBlockedList* write_blocked_list() { return &write_blocked_list_; }

 private:
  // Removes the session from the session map and write blocked list, and
  // adds the GUID to the time-wait list.
  void CleanUpSession(SessionMap::iterator it);

  // The list of connections waiting to write.
  WriteBlockedList write_blocked_list_;

  SessionMap session_map_;

  // Entity that manages guids in time wait state.
  scoped_ptr<QuicTimeWaitListManager> time_wait_list_manager_;

  // An alarm which deletes closed sessions.
  scoped_ptr<DeleteSessionsAlarm> delete_sessions_alarm_;

  // The list of closed but not-yet-deleted sessions.
  std::list<QuicSession*> closed_session_list_;

  EpollServer* epoll_server_;  // Owned by the server.

  // The connection for client-server communication
  int fd_;

  // True if the session is write blocked due to the socket returning EAGAIN.
  // False if we have gotten a call to OnCanWrite after the last failed write.
  bool write_blocked_;

  DISALLOW_COPY_AND_ASSIGN(QuicDispatcher);
};

} // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_DISPATCHER_H_
