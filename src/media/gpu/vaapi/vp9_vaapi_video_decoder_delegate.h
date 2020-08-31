// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VP9_VAAPI_VIDEO_DECODER_DELEGATE_H_
#define MEDIA_GPU_VAAPI_VP9_VAAPI_VIDEO_DECODER_DELEGATE_H_

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/vaapi/vaapi_video_decoder_delegate.h"
#include "media/gpu/vp9_decoder.h"

namespace media {

class VP9Picture;

class VP9VaapiVideoDecoderDelegate : public VP9Decoder::VP9Accelerator,
                                     public VaapiVideoDecoderDelegate {
 public:
  VP9VaapiVideoDecoderDelegate(DecodeSurfaceHandler<VASurface>* const vaapi_dec,
                               scoped_refptr<VaapiWrapper> vaapi_wrapper);
  ~VP9VaapiVideoDecoderDelegate() override;

  // VP9Decoder::VP9Accelerator implementation.
  scoped_refptr<VP9Picture> CreateVP9Picture() override;
  bool SubmitDecode(scoped_refptr<VP9Picture> pic,
                    const Vp9SegmentationParams& seg,
                    const Vp9LoopFilterParams& lf,
                    const Vp9ReferenceFrameVector& reference_frames,
                    base::OnceClosure done_cb) override;

  bool OutputPicture(scoped_refptr<VP9Picture> pic) override;
  bool IsFrameContextRequired() const override;
  bool GetFrameContext(scoped_refptr<VP9Picture> pic,
                       Vp9FrameContext* frame_ctx) override;

  DISALLOW_COPY_AND_ASSIGN(VP9VaapiVideoDecoderDelegate);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VP9_VAAPI_VIDEO_DECODER_DELEGATE_H_
