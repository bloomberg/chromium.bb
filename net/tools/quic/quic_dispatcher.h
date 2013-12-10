// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A server side dispatcher which dispatches a given client's data to their
// stream.

#ifndef NET_TOOLS_QUIC_QUIC_DISPATCHER_H_
#define NET_TOOLS_QUIC_QUIC_DISPATCHER_H_

#include <list>

#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/ip_endpoint.h"
#include "net/base/linked_hash_map.h"
#include "net/quic/quic_blocked_writer_interface.h"
#include "net/quic/quic_packet_writer.h"
#include "net/quic/quic_protocol.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/quic_server_session.h"
#include "net/tools/quic/quic_time_wait_list_manager.h"

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template<>
struct hash<net::QuicBlockedWriterInterface*> {
  std::size_t operator()(
      const net::QuicBlockedWriterInterface* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
}
#endif

namespace net {

class EpollServer;
class QuicConfig;
class QuicCryptoServerConfig;
class QuicSession;

namespace tools {

namespace test {
class QuicDispatcherPeer;
}  // namespace test

class DeleteSessionsAlarm;
class QuicEpollConnectionHelper;

class QuicDispatcher : public QuicPacketWriter, public QuicSessionOwner {
 public:
  // Ideally we'd have a linked_hash_set: the  boolean is unused.
  typedef linked_hash_map<QuicBlockedWriterInterface*, bool> WriteBlockedList;

  // Due to the way delete_sessions_closure_ is registered, the Dispatcher
  // must live until epoll_server Shutdown. |supported_versions| specifies the
  // list of supported QUIC versions.
  QuicDispatcher(const QuicConfig& config,
                 const QuicCryptoServerConfig& crypto_config,
                 const QuicVersionVector& supported_versions,
                 int fd,
                 EpollServer* epoll_server);
  virtual ~QuicDispatcher();

  // QuicPacketWriter
  virtual WriteResult WritePacket(
      const char* buffer, size_t buf_len,
      const IPAddressNumber& self_address,
      const IPEndPoint& peer_address,
      QuicBlockedWriterInterface* writer) OVERRIDE;
  virtual bool IsWriteBlockedDataBuffered() const OVERRIDE;

  // Process the incoming packet by creating a new session, passing it to
  // an existing session, or passing it to the TimeWaitListManager.
  virtual void ProcessPacket(const IPEndPoint& server_address,
                             const IPEndPoint& client_address,
                             QuicGuid guid,
                             bool has_version_flag,
                             const QuicEncryptedPacket& packet);

  // Called when the underyling connection becomes writable to allow
  // queued writes to happen.
  //
  // Returns true if more writes are possible, false otherwise.
  virtual bool OnCanWrite();

  // Sends ConnectionClose frames to all connected clients.
  void Shutdown();

  // Ensure that the closed connection is cleaned up asynchronously.
  virtual void OnConnectionClosed(QuicGuid guid, QuicErrorCode error) OVERRIDE;

  // Sets the fd and creates a default packet writer with that fd.
  void set_fd(int fd);

  typedef base::hash_map<QuicGuid, QuicSession*> SessionMap;

  virtual QuicSession* CreateQuicSession(
      QuicGuid guid,
      const IPEndPoint& server_address,
      const IPEndPoint& client_address);

  // Deletes all sessions on the closed session list and clears the list.
  void DeleteSessions();

  const SessionMap& session_map() const { return session_map_; }

  // Uses the specified |writer| instead of QuicSocketUtils and takes ownership
  // of writer.
  void UseWriter(QuicPacketWriter* writer);

  WriteBlockedList* write_blocked_list() { return &write_blocked_list_; }

 protected:
  const QuicConfig& config_;
  const QuicCryptoServerConfig& crypto_config_;

  QuicTimeWaitListManager* time_wait_list_manager() {
    return time_wait_list_manager_.get();
  }

  QuicEpollConnectionHelper* helper() { return helper_.get(); }
  EpollServer* epoll_server() { return epoll_server_; }

  const QuicVersionVector& supported_versions() const {
    return supported_versions_;
  }

 private:
  friend class net::tools::test::QuicDispatcherPeer;

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

  // The helper used for all connections.
  scoped_ptr<QuicEpollConnectionHelper> helper_;

  // The writer to write to the socket with.
  scoped_ptr<QuicPacketWriter> writer_;

  // This vector contains QUIC versions which we currently support.
  // This should be ordered such that the highest supported version is the first
  // element, with subsequent elements in descending order (versions can be
  // skipped as necessary).
  const QuicVersionVector supported_versions_;

  DISALLOW_COPY_AND_ASSIGN(QuicDispatcher);
};

}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_DISPATCHER_H_
