/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WEBKIT_TOOLS_PEPPER_TEST_PLUGIN_PLUGIN_OBJECT_H_
#define WEBKIT_TOOLS_PEPPER_TEST_PLUGIN_PLUGIN_OBJECT_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "webkit/glue/plugins/nphostapi.h"
#if !defined(INDEPENDENT_PLUGIN)
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "webkit/tools/pepper_test_plugin/command_buffer_pepper.h"
#endif

extern NPNetscapeFuncs* browser;

class PluginObject {
 public:
  explicit PluginObject(NPP npp);
  ~PluginObject();

  static NPClass* GetPluginClass();

  NPObject* header() { return &header_; }
  NPP npp() const { return npp_; }

  void New(NPMIMEType pluginType, int16 argc, char* argn[], char* argv[]);
  void SetWindow(const NPWindow& window);
  void Draw3D();

 private:
  bool InitializeCommandBuffer();

  NPObject header_;
  NPP npp_;
  NPObject* test_object_;
  int dimensions_;

  NPDevice* device2d_;

  // TODO(apatrick): this destruction order causes the plugin to crash on
  // shutdown.
#if !defined(INDEPENDENT_PLUGIN)
  scoped_ptr<CommandBufferPepper> command_buffer_;
  scoped_ptr<gpu::gles2::GLES2Implementation> gles2_implementation_;
  scoped_ptr<gpu::gles2::GLES2CmdHelper> helper_;
#endif

  int width_;
  int height_;

  DISALLOW_COPY_AND_ASSIGN(PluginObject);
};


#endif  // WEBKIT_TOOLS_PEPPER_TEST_PLUGIN_PLUGIN_OBJECT_H_
