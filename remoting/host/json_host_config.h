// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_JSON_HOST_CONFIG_H_
#define REMOTING_HOST_JSON_HOST_CONFIG_H_

#include <string>

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "remoting/host/in_memory_host_config.h"

class DictionaryValue;
class Task;

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace remoting {

// JsonHostConfig implements MutableHostConfig for JSON file.
class JsonHostConfig : public InMemoryHostConfig {
 public:
  JsonHostConfig(const FilePath& pref_filename,
                 base::MessageLoopProxy* file_message_loop_proxy);
  virtual ~JsonHostConfig();

  virtual bool Read();

  // MutableHostConfig interface.
  virtual void Save();

 private:
  void DoWrite();

  FilePath filename_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(JsonHostConfig);
};

}  // namespace remoting

#endif  // REMOTING_HOST_JSON_HOST_CONFIG_H_
