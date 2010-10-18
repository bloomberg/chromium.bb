// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_STRING_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_STRING_H_

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"

namespace pepper {

class String : public base::RefCountedThreadSafe<String> {
 public:
  String(const char* str, uint32 len);
  virtual ~String();

  const std::string& value() const { return value_; }

 private:
  std::string value_;

  DISALLOW_COPY_AND_ASSIGN(String);
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_STRING_H_
