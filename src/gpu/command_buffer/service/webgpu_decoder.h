// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_WEBGPU_DECODER_H_
#define GPU_COMMAND_BUFFER_SERVICE_WEBGPU_DECODER_H_

#include "base/macros.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

class DecoderClient;

namespace gles2 {
class Outputter;
}  // namespace gles2

namespace webgpu {

class GPU_GLES2_EXPORT WebGPUDecoder : public DecoderContext,
                                       public CommonDecoder {
 public:
  static WebGPUDecoder* Create(DecoderClient* client,
                               CommandBufferServiceBase* command_buffer_service,
                               gles2::Outputter* outputter);

  ~WebGPUDecoder() override;

 protected:
  WebGPUDecoder(DecoderClient* client,
                CommandBufferServiceBase* command_buffer_service,
                gles2::Outputter* outputter);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebGPUDecoder);
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_WEBGPU_DECODER_H_
