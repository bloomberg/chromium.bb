// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebBlobInfo_h
#define WebBlobInfo_h

#include "WebCommon.h"
#include "WebString.h"

namespace blink {

class WebBlobInfo {
 public:
  WebBlobInfo() : is_file_(false), size_(-1), last_modified_(0) {}
  WebBlobInfo(const WebString& uuid, const WebString& type, long long size)
      : is_file_(false),
        uuid_(uuid),
        type_(type),
        size_(size),
        last_modified_(0) {}
  WebBlobInfo(const WebString& uuid,
              const WebString& file_path,
              const WebString& file_name,
              const WebString& type)
      : is_file_(true),
        uuid_(uuid),
        type_(type),
        size_(-1),
        file_path_(file_path),
        file_name_(file_name),
        last_modified_(0) {}
  WebBlobInfo(const WebString& uuid,
              const WebString& file_path,
              const WebString& file_name,
              const WebString& type,
              double last_modified,
              long long size)
      : is_file_(true),
        uuid_(uuid),
        type_(type),
        size_(size),
        file_path_(file_path),
        file_name_(file_name),
        last_modified_(last_modified) {}
  bool IsFile() const { return is_file_; }
  const WebString& Uuid() const { return uuid_; }
  const WebString& GetType() const { return type_; }
  long long size() const { return size_; }
  const WebString& FilePath() const { return file_path_; }
  const WebString& FileName() const { return file_name_; }
  double LastModified() const { return last_modified_; }

 private:
  bool is_file_;
  WebString uuid_;
  WebString type_;  // MIME type
  long long size_;
  WebString file_path_;   // Only for File
  WebString file_name_;   // Only for File
  double last_modified_;  // Only for File
};

}  // namespace blink

#endif
