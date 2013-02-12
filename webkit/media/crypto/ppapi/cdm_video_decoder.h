// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_CRYPTO_PPAPI_CDM_VIDEO_DECODER_H_
#define WEBKIT_MEDIA_CRYPTO_PPAPI_CDM_VIDEO_DECODER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/media/crypto/ppapi/cdm/content_decryption_module.h"

namespace webkit_media {

class CdmVideoDecoder {
 public:
  virtual ~CdmVideoDecoder() {}
  virtual bool Initialize(const cdm::VideoDecoderConfig& config) = 0;
  virtual void Deinitialize() = 0;
  virtual void Reset() = 0;
  virtual bool is_initialized() const = 0;

  // Decodes |compressed_frame|. Stores output frame in |decoded_frame| and
  // returns |cdm::kSuccess| when an output frame is available. Returns
  // |cdm::kNeedMoreData| when |compressed_frame| does not produce an output
  // frame. Returns |cdm::kDecodeError| when decoding fails.
  //
  // A null |compressed_frame| will attempt to flush the decoder of any
  // remaining frames. |compressed_frame_size| and |timestamp| are ignored.
  virtual cdm::Status DecodeFrame(const uint8_t* compressed_frame,
                                  int32_t compressed_frame_size,
                                  int64_t timestamp,
                                  cdm::VideoFrame* decoded_frame) = 0;
};

// Initializes appropriate video decoder based on GYP flags and the value of
// |config.codec|. Returns a scoped_ptr containing a non-null initialized
// CdmVideoDecoder* upon success.
scoped_ptr<CdmVideoDecoder> CreateVideoDecoder(
    cdm::Host* host, const cdm::VideoDecoderConfig& config);

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_CRYPTO_PPAPI_CDM_VIDEO_DECODER_H_
