// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_TESTS_PEPPER_PLUGIN_COMMAND_BUFFER_PEPPER_H_
#define NATIVE_CLIENT_TESTS_PEPPER_PLUGIN_COMMAND_BUFFER_PEPPER_H_

// Temporary workarounds for missing NaCl definitions of base macros.
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&); \
    void operator=(const TypeName&);
#define CHECK(Expression)
#define NOTREACHED()

#include <nacl/npapi.h>
#include <nacl/npapi_extensions.h>
#include <nacl/npruntime.h>
#include <nacl/npupp.h>
#include <nacl/command_buffer.h>

// A CommandBuffer proxy implementation that uses the Pepper API to access
// the command buffer.
// TODO(apatrick): move this into a library that can be used by any pepper
// plugin.

class CommandBufferPepper : public gpu::CommandBuffer {
 public:
  CommandBufferPepper(NPP npp, NPNetscapeFuncs* browser);
  virtual ~CommandBufferPepper();

  // CommandBuffer implementation.
  virtual bool Initialize(int32 size);
  virtual gpu::Buffer GetRingBuffer();
  virtual int32 GetSize();
  virtual int32 SyncOffsets(int32 put_offset);
  virtual int32 GetGetOffset();
  virtual void SetGetOffset(int32 get_offset);
  virtual int32 GetPutOffset();
  virtual int32 CreateTransferBuffer(size_t size);
  virtual void DestroyTransferBuffer(int32 id);
  virtual gpu::Buffer GetTransferBuffer(int32 handle);
  virtual int32 GetToken();
  virtual void SetToken(int32 token);
  virtual int32 ResetParseError();
  virtual void SetParseError(int32 parse_error);
  virtual bool GetErrorStatus();
  virtual void RaiseErrorStatus();

 private:
  NPP npp_;
  NPNetscapeFuncs* browser_;
  NPExtensions* extensions_;
  NPDevice* device_;
  NPDeviceContext3D context_;
};

#endif  // NATIVE_CLIENT_TESTS_PEPPER_PLUGIN_COMMAND_BUFFER_PEPPER_H_
