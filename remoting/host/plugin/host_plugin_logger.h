// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PLUGIN_HOST_PLUGIN_LOGGER_H_
#define REMOTING_HOST_PLUGIN_HOST_PLUGIN_LOGGER_H_

#include "remoting/base/logger.h"

namespace remoting {

class HostNPScriptObject;

class HostPluginLogger : public Logger {
 public:
  explicit HostPluginLogger(HostNPScriptObject* scriptable);
  virtual ~HostPluginLogger();

  virtual void LogToUI(const std::string& message);

 private:
  HostNPScriptObject* scriptable_object_;

  DISALLOW_COPY_AND_ASSIGN(HostPluginLogger);
};

}  // namespace remoting

#endif  // REMOTING_HOST_PLUGIN_HOST_PLUGIN_LOGGER_H_
