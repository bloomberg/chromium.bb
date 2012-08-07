// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_COMPOSITE_HOST_CONFIG_H_
#define REMOTING_HOST_COMPOSITE_HOST_CONFIG_H_

#include "remoting/host/host_config.h"

#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"

class FilePath;

namespace remoting {

class JsonHostConfig;

// CompositeConfig reads multiple configuration files and merges them together.
class CompositeHostConfig : public HostConfig {
 public:
  CompositeHostConfig();
  virtual ~CompositeHostConfig();

  // Add configuration file specified stored at |path|. When the same parameter
  // is present in more than one file priority is given to those that are added
  // first.
  void AddConfigPath(const FilePath& path);

  // Reads all configuration files. Returns false if it fails to load any of the
  // files.
  bool Read();

  // HostConfig interface.
  virtual bool GetString(const std::string& path,
                         std::string* out_value) const OVERRIDE;
  virtual bool GetBoolean(const std::string& path,
                          bool* out_value) const OVERRIDE;

 private:
  ScopedVector<JsonHostConfig> configs_;

  DISALLOW_COPY_AND_ASSIGN(CompositeHostConfig);
};

}  // namespace remoting

#endif  // REMOTING_HOST_COMPOSITE_HOST_CONFIG_H_
