// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_JSON_HOST_CONFIG_H_
#define REMOTING_HOST_JSON_HOST_CONFIG_H_

#include <string>

#include "base/file_path.h"
#include "base/lock.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "remoting/host/host_config.h"

class DictionaryValue;
class Task;

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace remoting {

// JsonHostConfig implements MutableHostConfig for JSON file.
class JsonHostConfig : public MutableHostConfig {
 public:
  JsonHostConfig(const FilePath& pref_filename,
                 base::MessageLoopProxy* file_message_loop_proxy);

  virtual bool Read();

  // MutableHostConfig interface.
  virtual bool GetString(const std::string& path, std::string* out_value);

  virtual void Update(Task* task);

  virtual void SetString(const std::string& path, const std::string& in_value);

 private:
  void DoWrite();

  // |lock_| must be locked whenever we access values_;
  Lock lock_;
  FilePath filename_;
  scoped_ptr<DictionaryValue> values_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(JsonHostConfig);
};

}  // namespace remoting

#endif  // REMOTING_HOST_JSON_HOST_CONFIG_H_
