// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IN_MEMORY_HOST_CONFIG_H_
#define REMOTING_HOST_IN_MEMORY_HOST_CONFIG_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/host/host_config.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace remoting {

// In-memory host config. Used by unittests.
class InMemoryHostConfig : public MutableHostConfig,
                           public base::NonThreadSafe {
 public:
  InMemoryHostConfig();
  virtual ~InMemoryHostConfig();

  // MutableHostConfig interface.
  virtual bool GetString(const std::string& path,
                         std::string* out_value) const OVERRIDE;
  virtual bool GetBoolean(const std::string& path,
                          bool* out_value) const OVERRIDE;

  virtual void SetString(const std::string& path,
                         const std::string& in_value) OVERRIDE;
  virtual void SetBoolean(const std::string& path, bool in_value) OVERRIDE;

  virtual bool CopyFrom(const base::DictionaryValue* dictionary) OVERRIDE;

  virtual bool Save() OVERRIDE;

 protected:
  scoped_ptr<base::DictionaryValue> values_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InMemoryHostConfig);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IN_MEMORY_HOST_CONFIG_H_
