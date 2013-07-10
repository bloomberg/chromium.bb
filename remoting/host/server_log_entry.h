// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SERVER_LOG_ENTRY_H_
#define REMOTING_HOST_SERVER_LOG_ENTRY_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/transport.h"

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace remoting {

class ServerLogEntry {
 public:
  // The mode of a connection.
  enum Mode {
    IT2ME,
    ME2ME
  };

  // Constructs a log stanza. The caller should add one or more log entry
  // stanzas as children of this stanza, before sending the log stanza to
  // the remoting bot.
  static scoped_ptr<buzz::XmlElement> MakeStanza();

  // Constructs a log entry for a session state change.
  // Currently this is either connection or disconnection.
  static scoped_ptr<ServerLogEntry> MakeForSessionStateChange(bool connection);

  // Constructs a log entry for a heartbeat.
  static scoped_ptr<ServerLogEntry> MakeForHeartbeat();

  ~ServerLogEntry();

  // Adds fields describing the host to this log entry.
  void AddHostFields();

  // Adds a field describing the mode of a connection to this log entry.
  void AddModeField(Mode mode);

  // Adds a field describing connection type (direct/stun/relay).
  void AddConnectionTypeField(protocol::TransportRoute::RouteType type);

  // Converts this object to an XML stanza.
  scoped_ptr<buzz::XmlElement> ToStanza() const;

 private:
  typedef std::map<std::string, std::string> ValuesMap;

  ServerLogEntry();
  void Set(const std::string& key, const std::string& value);

  static const char* GetValueSessionState(bool connected);
  static const char* GetValueMode(Mode mode);

  ValuesMap values_map_;
};

}  // namespace remoting

#endif
