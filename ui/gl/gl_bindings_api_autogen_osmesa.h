// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// ui/gl/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// The following line silences a presubmit warning that would otherwise be
// triggered by this:
// no-include-guard-because-multiply-included

void OSMesaColorClampFn(GLboolean enable) override;
OSMesaContext OSMesaCreateContextFn(GLenum format,
                                    OSMesaContext sharelist) override;
OSMesaContext OSMesaCreateContextExtFn(GLenum format,
                                       GLint depthBits,
                                       GLint stencilBits,
                                       GLint accumBits,
                                       OSMesaContext sharelist) override;
void OSMesaDestroyContextFn(OSMesaContext ctx) override;
GLboolean OSMesaGetColorBufferFn(OSMesaContext c,
                                 GLint* width,
                                 GLint* height,
                                 GLint* format,
                                 void** buffer) override;
OSMesaContext OSMesaGetCurrentContextFn(void) override;
GLboolean OSMesaGetDepthBufferFn(OSMesaContext c,
                                 GLint* width,
                                 GLint* height,
                                 GLint* bytesPerValue,
                                 void** buffer) override;
void OSMesaGetIntegervFn(GLint pname, GLint* value) override;
OSMESAproc OSMesaGetProcAddressFn(const char* funcName) override;
GLboolean OSMesaMakeCurrentFn(OSMesaContext ctx,
                              void* buffer,
                              GLenum type,
                              GLsizei width,
                              GLsizei height) override;
void OSMesaPixelStoreFn(GLint pname, GLint value) override;
