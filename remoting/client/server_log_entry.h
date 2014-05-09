// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_SERVER_LOG_ENTRY_H_
#define REMOTING_CLIENT_SERVER_LOG_ENTRY_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/errors.h"

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace remoting {

class ChromotingStats;

// Temporary namespace to prevent conflict with the same-named class in
// remoting/host when linking unittests.
//
// TODO(lambroslambrou): Remove this and factor out any shared code.
namespace client {

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
  static scoped_ptr<ServerLogEntry> MakeForSessionStateChange(
      remoting::protocol::ConnectionToHost::State state,
      remoting::protocol::ErrorCode error);

  // Constructs a log entry for reporting statistics.
  static scoped_ptr<ServerLogEntry> MakeForStatistics(
      remoting::ChromotingStats* statistics);

  // Constructs a log entry for reporting session ID is old.
  static scoped_ptr<ServerLogEntry> MakeForSessionIdOld(
      const std::string& session_id);

  // Constructs a log entry for reporting session ID is old.
  static scoped_ptr<ServerLogEntry> MakeForSessionIdNew(
      const std::string& session_id);

  ~ServerLogEntry();

  // Adds fields describing the client to this log entry.
  void AddClientFields();

  // Adds a field describing the mode of a connection to this log entry.
  void AddModeField(Mode mode);

  void AddEventName(const std::string& event_name);
  void AddSessionId(const std::string& session_id);
  void AddSessionDuration(base::TimeDelta duration);

  // Converts this object to an XML stanza.
  scoped_ptr<buzz::XmlElement> ToStanza() const;

 private:
  typedef std::map<std::string, std::string> ValuesMap;

  ServerLogEntry();
  void Set(const std::string& key, const std::string& value);

  static const char* GetValueSessionState(
      remoting::protocol::ConnectionToHost::State state);
  static const char* GetValueError(remoting::protocol::ErrorCode error);
  static const char* GetValueMode(Mode mode);

  ValuesMap values_map_;
};

}  // namespace client

}  // namespace remoting

#endif  // REMOTING_CLIENT_SERVER_LOG_ENTRY_H_
