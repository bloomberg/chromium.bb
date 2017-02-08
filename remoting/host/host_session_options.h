// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_SESSION_OPTIONS_H_
#define REMOTING_HOST_HOST_SESSION_OPTIONS_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/optional.h"

namespace remoting {

// Session based host options sending from client. This class parses and stores
// session configuration from client side to control the behavior of other host
// components.
class HostSessionOptions final {
 public:
  HostSessionOptions();
  ~HostSessionOptions();

  HostSessionOptions(const std::string& parameter);

  // Appends one key-value pair into current instance.
  void Append(const std::string& key, const std::string& value);

  // Retrieves the value of |key|. Returns a true Optional if |key| has been
  // found, value of the Optional wil be set to corresponding value.
  base::Optional<std::string> Get(const std::string& key) const;

  // Returns a string to represent current instance. Consumers can rebuild an
  // exactly same instance with Import() function.
  std::string Export() const;

  // Overwrite current instance with |parameter|, which is a string returned by
  // Export() function. So a parent process can send HostSessionOptions to a
  // child process.
  void Import(const std::string& parameter);

 private:
  std::map<std::string, std::string> options_;

  HostSessionOptions(HostSessionOptions&&) = delete;
  HostSessionOptions& operator=(HostSessionOptions&&) = delete;
  DISALLOW_COPY_AND_ASSIGN(HostSessionOptions);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_SESSION_OPTIONS_H_
