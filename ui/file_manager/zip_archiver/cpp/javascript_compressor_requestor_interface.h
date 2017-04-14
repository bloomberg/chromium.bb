// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JAVASCRIPT_COMPRESSOR_REQUESTOR_INTERFACE_H_
#define JAVASCRIPT_COMPRESSOR_REQUESTOR_INTERFACE_H_

#include <string>

// Makes requests from Compressor to JavaScript.
class JavaScriptCompressorRequestorInterface {
 public:
  virtual ~JavaScriptCompressorRequestorInterface() {}

  virtual void WriteChunkRequest(int64_t offset,
                                 int64_t length,
                                 const pp::VarArrayBuffer& buffer) = 0;

  virtual void ReadFileChunkRequest(int64_t length) = 0;
};

#endif  // JAVASCRIPT_COMPRESSOR_REQUESTOR_INTERFACE_H_
