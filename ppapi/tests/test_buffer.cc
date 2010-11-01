// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_buffer.h"

#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Buffer);

bool TestBuffer::Init() {
  buffer_interface_ = reinterpret_cast<PPB_Buffer_Dev const*>(
      pp::Module::Get()->GetBrowserInterface(PPB_BUFFER_DEV_INTERFACE));
  return !!buffer_interface_;
}

void TestBuffer::RunTest() {
  instance_->LogTest("InvalidSize", TestInvalidSize());
  instance_->LogTest("InitToZero", TestInitToZero());
  instance_->LogTest("IsBuffer", TestIsBuffer());
}

std::string TestBuffer::TestInvalidSize() {
  pp::Buffer_Dev zero_size(0);
  if (!zero_size.is_null())
    return "Zero size accepted";

  return "";
}

std::string TestBuffer::TestInitToZero() {
  pp::Buffer_Dev buffer(100);
  if (buffer.is_null())
    return "Could not create buffer";

  if (buffer.size() != 100)
    return "Buffer size not as expected";

  // Now check that everything is 0.
  unsigned char* bytes = static_cast<unsigned char *>(buffer.data());
  for (int index = 0; index < buffer.size(); index++) {
    if (bytes[index] != 0)
      return "Buffer isn't entirely zero";
  }

  return "";
}

std::string TestBuffer::TestIsBuffer() {
  // Test that a NULL resource isn't a buffer.
  pp::Resource null_resource;
  if (buffer_interface_->IsBuffer(null_resource.pp_resource()))
    return "Null resource was reported as a valid buffer";

  // Make another resource type and test it.
  const int w = 16, h = 16;
  pp::Graphics2D device(pp::Size(w, h), true);
  if (device.is_null())
    return "Couldn't create device context";
  if (buffer_interface_->IsBuffer(device.pp_resource()))
    return "Device context was reported as a buffer";

  // Make a valid buffer.
  pp::Buffer_Dev buffer(100);
  if (buffer.is_null())
    return "Couldn't create buffer";
  if (!buffer_interface_->IsBuffer(buffer.pp_resource()))
    return "Buffer should be identified as a buffer";

  return "";
}

