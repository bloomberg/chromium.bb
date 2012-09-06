// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_CRYPTO_PPAPI_CONTENT_DECRYPTION_MODULE_H_
#define WEBKIT_MEDIA_CRYPTO_PPAPI_CONTENT_DECRYPTION_MODULE_H_

#if defined(_MSC_VER)
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef __int64 int64_t;
#else
#include <stdint.h>
#endif

#include "webkit/media/crypto/ppapi/cdm_export.h"

namespace cdm {
class ContentDecryptionModule;
}

extern "C" {
CDM_EXPORT cdm::ContentDecryptionModule* CreateCdmInstance();
CDM_EXPORT void DestroyCdmInstance(cdm::ContentDecryptionModule* instance);
CDM_EXPORT const char* GetCdmVersion();
}

namespace cdm {

enum Status {
  kSuccess = 0,
  kNeedMoreData,  // Decoder needs more data to produce a decoded frame/sample.
  kNoKey,  // The required decryption key is not available.
  kError
};

// Represents a key message sent by the CDM. It does not own any pointers in
// this struct.
// TODO(xhwang): Use int32_t instead of uint32_t for sizes here and below and
// update checks to include <0.
struct KeyMessage {
  KeyMessage()
      : session_id(NULL),
        session_id_size(0),
        message(NULL),
        message_size(0),
        default_url(NULL),
        default_url_size(0) {}

  char* session_id;
  uint32_t session_id_size;
  uint8_t* message;
  uint32_t message_size;
  char* default_url;
  uint32_t default_url_size;
};

// An input buffer can be split into several continuous subsamples.
// A SubsampleEntry specifies the number of clear and cipher bytes in each
// subsample. For example, the following buffer has three subsamples:
//
// |<----- subsample1 ----->|<----- subsample2 ----->|<----- subsample3 ----->|
// |   clear1   |  cipher1  |  clear2  |   cipher2   | clear3 |    cipher3    |
//
// For decryption, all of the cipher bytes in a buffer should be concatenated
// (in the subsample order) into a single logical stream. The clear bytes should
// not be considered as part of decryption.
//
// Stream to decrypt:   |  cipher1  |   cipher2   |    cipher3    |
// Decrypted stream:    | decrypted1|  decrypted2 |   decrypted3  |
//
// After decryption, the decrypted bytes should be copied over the position
// of the corresponding cipher bytes in the original buffer to form the output
// buffer. Following the above example, the decrypted buffer should be:
//
// |<----- subsample1 ----->|<----- subsample2 ----->|<----- subsample3 ----->|
// |   clear1   | decrypted1|  clear2  |  decrypted2 | clear3 |   decrypted3  |
//
// TODO(xhwang): Add checks to make sure these structs have fixed layout.
struct SubsampleEntry {
  SubsampleEntry(uint32_t clear_bytes, uint32_t cipher_bytes)
      : clear_bytes(clear_bytes), cipher_bytes(cipher_bytes) {}

  uint32_t clear_bytes;
  uint32_t cipher_bytes;
};

// Represents an input buffer to be decrypted (and possibly decoded). It does
// own any pointers in this struct.
struct InputBuffer {
  InputBuffer()
      : data(NULL),
        data_size(0),
        data_offset(0),
        key_id(NULL),
        key_id_size(0),
        iv(NULL),
        iv_size(0),
        checksum(NULL),
        checksum_size(0),
        subsamples(NULL),
        num_subsamples(0),
        timestamp(0) {}

  const uint8_t* data;  // Pointer to the beginning of the input data.
  uint32_t data_size;  // Size (in bytes) of |data|.

  uint32_t data_offset;  // Number of bytes to be discarded before decryption.

  const uint8_t* key_id;  // Key ID to identify the decryption key.
  uint32_t key_id_size;  // Size (in bytes) of |key_id|.

  const uint8_t* iv;  // Initialization vector.
  uint32_t iv_size;  // Size (in bytes) of |iv|.

  const uint8_t* checksum;
  uint32_t checksum_size;  // Size (in bytes) of the |checksum|.

  const struct SubsampleEntry* subsamples;
  uint32_t num_subsamples;  // Number of subsamples in |subsamples|.

  int64_t timestamp;  // Presentation timestamp in microseconds.
};

// Represents an output decrypted buffer. It does not own |data|.
struct OutputBuffer {
  OutputBuffer()
      : data(NULL),
        data_size(0),
        timestamp(0) {}

  const uint8_t* data;  // Pointer to the beginning of the output data.
  uint32_t data_size;  // Size (in bytes) of |data|.

  int64_t timestamp;  // Presentation timestamp in microseconds.
};

// Surface formats based on FOURCC labels, see:
// http://www.fourcc.org/yuv.php
enum VideoFormat {
  kUnknownVideoFormat = 0,  // Unknown format value.  Used for error reporting.
  kEmptyVideoFrame,  // An empty frame.
  kYv12,  // 12bpp YVU planar 1x1 Y, 2x2 VU samples.
  kI420  // 12bpp YVU planar 1x1 Y, 2x2 UV samples.
};

struct Size {
  Size() : width(0), height(0) {}
  Size(int32_t width, int32_t height) : width(width), height(height) {}

  int32_t width;
  int32_t height;
};

struct VideoFrame {
  static const int32_t kMaxPlanes = 3;

  VideoFrame()
      : timestamp(0) {
    for (int i = 0; i < kMaxPlanes; ++i) {
      strides[i] = 0;
      data[i] = NULL;
    }
  }

  // Array of strides for each plane, typically greater or equal to the width
  // of the surface divided by the horizontal sampling period.  Note that
  // strides can be negative.
  int32_t strides[kMaxPlanes];

  // Array of data pointers to each plane.
  uint8_t* data[kMaxPlanes];

  int64_t timestamp;  // Presentation timestamp in microseconds.
};

struct VideoDecoderConfig {
  enum VideoCodec {
    kUnknownVideoCodec = 0,
    kCodecVP8
  };

  enum VideoCodecProfile {
    kUnknownVideoCodecProfile = 0,
    kVp8ProfileMain
  };

  VideoDecoderConfig()
      : codec(kUnknownVideoCodec),
        profile(kUnknownVideoCodecProfile),
        format(kUnknownVideoFormat),
        extra_data(NULL),
        extra_data_size() {}

  VideoCodec codec;
  VideoCodecProfile profile;
  VideoFormat format;

  // Width and height of video frame immediately post-decode. Not all pixels
  // in this region are valid.
  Size coded_size;

  // Optional byte data required to initialize video decoders, such as H.264
  // AAVC data.
  uint8_t* extra_data;
  int32_t extra_data_size;
};

class ContentDecryptionModule {
 public:
  // Generates a |key_request| given the |init_data|.
  //
  // Returns kSuccess if the key request was successfully generated,
  // in which case the callee should have allocated memory for the output
  // parameters (e.g |session_id| in |key_request|) and passed the ownership
  // to the caller.
  // Returns kError if any error happened, in which case the |key_request|
  // should not be used by the caller.
  //
  // TODO(xhwang): It's not safe to pass the ownership of the dynamically
  // allocated memory over library boundaries. Fix it after related PPAPI change
  // and sample CDM are landed.
  virtual Status GenerateKeyRequest(const uint8_t* init_data,
                                    int init_data_size,
                                    KeyMessage* key_request) = 0;

  // Adds the |key| to the CDM to be associated with |key_id|.
  //
  // Returns kSuccess if the key was successfully added, kError otherwise.
  virtual Status AddKey(const char* session_id,
                        int session_id_size,
                        const uint8_t* key,
                        int key_size,
                        const uint8_t* key_id,
                        int key_id_size) = 0;

  // Cancels any pending key request made to the CDM for |session_id|.
  //
  // Returns kSuccess if all pending key requests for |session_id| were
  // successfully canceled or there was no key request to be canceled, kError
  // otherwise.
  virtual Status CancelKeyRequest(const char* session_id,
                                  int session_id_size) = 0;

  // Decrypts the |encrypted_buffer|.
  //
  // Returns kSuccess if decryption succeeded, in which case the callee
  // should have filled the |decrypted_buffer| and passed the ownership of
  // |data| in |decrypted_buffer| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kError if any other error happened.
  // If the return value is not kSuccess, |decrypted_buffer| should be ignored
  // by the caller.
  //
  // TODO(xhwang): It's not safe to pass the ownership of the dynamically
  // allocated memory over library boundaries. Fix it after related PPAPI change
  // and sample CDM are landed.
  virtual Status Decrypt(const InputBuffer& encrypted_buffer,
                         OutputBuffer* decrypted_buffer) = 0;

  // Initializes the CDM video decoder with |video_decoder_config|. This
  // function must be called before DecryptAndDecodeVideo() is called.
  //
  // Returns kSuccess if the |video_decoder_config| is supported and the CDM
  // video decoder is successfully initialized.
  // Returns kError if |video_decoder_config| is not supported. The CDM may
  // still be able to do Decrypt().
  //
  // TODO(xhwang): Add stream ID here and in the following video decoder
  // functions when we need to support multiple video streams in one CDM.
  virtual Status InitializeVideoDecoder(
      const VideoDecoderConfig& video_decoder_config) = 0;

  // Decrypts the |encrypted_buffer| and decodes the decrypted buffer into a
  // |video_frame|. Upon end-of-stream, the caller should call this function
  // repeatedly with empty |encrypted_buffer| (|data| == NULL) until only empty
  // |video_frame| (|format| == kEmptyVideoFrame) is produced.
  //
  // Returns kSuccess if decryption and decoding both succeeded, in which case
  // the callee should have filled the |video_frame| and passed the ownership of
  // |data| in |video_frame| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kNeedMoreData if more data was needed by the decoder to generate
  // a decoded frame (e.g. during initialization).
  // Returns kError if any other (decryption or decoding) error happened.
  // If the return value is not kSuccess, |video_frame| should be ignored by
  // the caller.
  //
  // TODO(xhwang): It's not safe to pass the ownership of the dynamically
  // allocated memory over library boundaries. Fix it after related PPAPI change
  // and sample CDM are landed.
  virtual Status DecryptAndDecodeVideo(const InputBuffer& encrypted_buffer,
                                       VideoFrame* video_frame) = 0;

  // Resets the CDM video decoder to an initialized clean state. All internal
  // buffers will be flushed.
  virtual void ResetVideoDecoder() = 0;

  // Stops the CDM video decoder and sets it to an uninitialized state. The
  // caller can call InitializeVideoDecoder() again after this call to
  // re-initialize the video decoder. This can be used to reconfigure the
  // video decoder if the config changes.
  virtual void StopVideoDecoder() = 0;

  virtual ~ContentDecryptionModule() {}
};

}  // namespace cdm

#endif  // WEBKIT_MEDIA_CRYPTO_PPAPI_CONTENT_DECRYPTION_MODULE_H_
