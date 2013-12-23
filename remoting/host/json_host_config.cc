// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/json_host_config.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace remoting {

JsonHostConfig::JsonHostConfig(const base::FilePath& filename)
    : filename_(filename) {
}

JsonHostConfig::~JsonHostConfig() {}

bool JsonHostConfig::Read() {
  DCHECK(CalledOnValidThread());

  // TODO(sergeyu): Implement better error handling here.
  std::string file_content;
  if (!base::ReadFileToString(filename_, &file_content)) {
    LOG(WARNING) << "Failed to read " << filename_.value();
    return false;
  }

  return SetSerializedData(file_content);
}

bool JsonHostConfig::Save() {
  DCHECK(CalledOnValidThread());

  return base::ImportantFileWriter::WriteFileAtomically(filename_,
                                                        GetSerializedData());
}

std::string JsonHostConfig::GetSerializedData() {
  std::string data;
  base::JSONWriter::Write(values_.get(), &data);
  return data;
}

bool JsonHostConfig::SetSerializedData(const std::string& config) {
  scoped_ptr<base::Value> value(
      base::JSONReader::Read(config, base::JSON_ALLOW_TRAILING_COMMAS));
  if (value.get() == NULL || !value->IsType(base::Value::TYPE_DICTIONARY)) {
    LOG(WARNING) << "Failed to parse " << filename_.value();
    return false;
  }

  base::DictionaryValue* dictionary =
      static_cast<base::DictionaryValue*>(value.release());
  values_.reset(dictionary);
  return true;
}

}  // namespace remoting
