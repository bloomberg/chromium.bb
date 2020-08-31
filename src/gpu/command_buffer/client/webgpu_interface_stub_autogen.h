// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_webgpu_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file is included by gles2_interface_stub.h.
#ifndef GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_STUB_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_STUB_AUTOGEN_H_

void AssociateMailbox(GLuint64 device_client_id,
                      GLuint device_generation,
                      GLuint id,
                      GLuint generation,
                      GLuint usage,
                      const GLbyte* mailbox) override;
void DissociateMailbox(GLuint64 device_client_id,
                       GLuint texture_id,
                       GLuint texture_generation) override;
#endif  // GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_STUB_AUTOGEN_H_
