/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */



// This module provides interfaces for writing to and reading from a
// GDB RSP connection.  The connection uses a generic transport
// object for sending packets from one endpoint to another.  The session
// is responsible to delievery including generation of checksums and
// retransmits and handling timeouts as needed.  See:
// http:// ftp.gnu.org/old-gnu/Manuals/gdb-5.1.1/html_node/gdb_129.html#SEC134
//
// All data is read one character at a time through 'GetChar' which will
// poll on DataAvail and timeout after one second, allowing the session
// to handle blocking transports.
//
// The session object is not reentrant, and will have unpredictable
// results if two threads attempt to read from the session at the same
// time.
//
// This module may throw a std::bad_alloc if there is an error while trying
// to resize the packet.  In addition, the underlying ITransport is free
// to throw an exception if the connection is lost, which will pass out
// of any packet send or receive function.
#ifndef NATIVE_CLIENT_GDB_RSP_SESSION_H_
#define NATIVE_CLIENT_GDB_RSP_SESSION_H_ 1

#include <sstream>

#include "native_client/src/trusted/port/std_types.h"
#include "native_client/src/trusted/port/mutex.h"
#include "native_client/src/trusted/port/transport.h"

namespace gdb_rsp {

class Packet;

class Session {
 public:
  explicit Session(port::ITransport *transport);
  ~Session();

  enum DPResult {
    DPR_ERROR = -1,   // IO error on the stream, close the pipe
    DPR_NO_DATA = 0,  // No data availible
    DPR_OK = 1        // Completed OK
  };

  enum {
    IGNORE_ACK = 1,   // Do not emit or wait for '+' from RSP stream.
    USE_SEQ = 2,      // Automatically use a sequence number
    DEBUG_SEND = 4,   // Log all SENDs
    DEBUG_RECV = 8,   // Log all RECVs
    DEBUG_MASK = (DEBUG_SEND | DEBUG_RECV)
  };

 public:
  void  SetFlags(uint32_t flags);
  void  ClearFlags(uint32_t flags);
  uint32_t GetFlags();

  DPResult SendPacketOnly(Packet *packet);
  DPResult SendPacket(Packet *packet);
  DPResult GetPacket(Packet *packet);
  bool DataAvailable();
  bool Connected();

 protected:
  DPResult GetChar(char *ch);
  DPResult SendStream(const char *str);

 private:
  Session(const Session&);
  Session &operator=(const Session&);

 private:
  port::IMutex *mutex_;     // Lock to enforce correct response order.
  port::ITransport *io_;    // Transport object not owned by the Session.
  uint32_t flags_;          // Session flags for Sequence/Ack generation.
  uint8_t seq_;             // Next sequence number to use or -1.
  bool connected_;          // Is the connection still valid.
};

}  // namespace gdb_rsp

#endif  // NATIVE_CLIENT_GDB_RSP_SESSION_H_

