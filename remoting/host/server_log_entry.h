// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SERVER_LOG_ENTRY_H_
#define REMOTING_HOST_SERVER_LOG_ENTRY_H_

#include <map>
#include <string>

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

  // Constructs a log entry for a session state change.
  // Currently this is either connection or disconnection.
  static ServerLogEntry* MakeSessionStateChange(bool connection);

  ~ServerLogEntry();

  // Adds fields describing the host to this log entry.
  void AddHostFields();

  // Adds a field describing the mode of a connection to this log entry.
  void AddModeField(Mode mode);

  // Converts this object to an XML stanza.
  // The caller takes ownership of the stanza.
  buzz::XmlElement* ToStanza() const;

 private:
  typedef std::map<std::string, std::string> ValuesMap;

  ServerLogEntry();
  void Set(const char* key, const char* value);

  static const char* GetValueSessionState(bool connected);
  static const char* GetValueMode(Mode mode);

  ValuesMap values_map_;
};

}  // namespace remoting

#endif
