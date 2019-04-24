// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_CONTENT_OPERATION_H_
#define COMPONENTS_FEED_CORE_FEED_CONTENT_OPERATION_H_

#include <string>

#include "base/macros.h"

namespace feed {

// Native counterpart of ContentOperation.java.
class ContentOperation {
 public:
  enum Type {
    CONTENT_DELETE,
    CONTENT_DELETE_ALL,
    CONTENT_DELETE_BY_PREFIX,
    CONTENT_UPSERT,
  };

  static ContentOperation CreateDeleteOperation(std::string key);
  static ContentOperation CreateDeleteAllOperation();
  static ContentOperation CreateDeleteByPrefixOperation(std::string prefix);
  static ContentOperation CreateUpsertOperation(std::string key,
                                                std::string value);

  ContentOperation(ContentOperation&& operation);

  Type type();
  const std::string& key();
  const std::string& value();
  const std::string& prefix();

 private:
  ContentOperation(Type type,
                   std::string key,
                   std::string value,
                   std::string prefix);

  const Type type_;
  const std::string key_;
  const std::string value_;
  const std::string prefix_;

  DISALLOW_COPY_AND_ASSIGN(ContentOperation);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_CONTENT_OPERATION_H_
