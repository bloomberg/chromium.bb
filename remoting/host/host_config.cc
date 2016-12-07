// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_config.h"

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace remoting {

std::unique_ptr<base::DictionaryValue> HostConfigFromJson(
    const std::string& json) {
  std::unique_ptr<base::Value> value =
      base::JSONReader::Read(json, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!value || !value->IsType(base::Value::Type::DICTIONARY)) {
    LOG(WARNING) << "Failed to parse host config from JSON";
    return nullptr;
  }

  return base::WrapUnique(static_cast<base::DictionaryValue*>(value.release()));
}

std::string HostConfigToJson(const base::DictionaryValue& host_config) {
  std::string data;
  base::JSONWriter::Write(host_config, &data);
  return data;
}

std::unique_ptr<base::DictionaryValue> HostConfigFromJsonFile(
    const base::FilePath& config_file) {
  // TODO(sergeyu): Implement better error handling here.
  std::string serialized;
  if (!base::ReadFileToString(config_file, &serialized)) {
    LOG(WARNING) << "Failed to read " << config_file.value();
    return nullptr;
  }

  return HostConfigFromJson(serialized);
}

bool HostConfigToJsonFile(const base::DictionaryValue& host_config,
                          const base::FilePath& config_file) {
  std::string serialized = HostConfigToJson(host_config);
  return base::ImportantFileWriter::WriteFileAtomically(config_file,
                                                        serialized);
}

}  // namespace remoting
