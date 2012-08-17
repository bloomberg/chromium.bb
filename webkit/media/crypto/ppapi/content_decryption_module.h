// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_CRYPTO_CONTENT_DECRYPTION_MODULE_H_
#define WEBKIT_MEDIA_CRYPTO_CONTENT_DECRYPTION_MODULE_H_

#if defined(_MSC_VER)
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef __int64 int64_t;
#else
#include <stdint.h>
#endif

namespace cdm {
class ContentDecryptionModule;
}

extern "C" {
cdm::ContentDecryptionModule* CreateCdmInstance();
void DestroyCdmInstance(cdm::ContentDecryptionModule* instance);
const char* GetCdmVersion();
}

namespace cdm {

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
  uint32_t clear_bytes;
  uint32_t cipher_bytes;
};

struct InputBuffer {
  uint8_t* data;  // Pointer to the beginning of the input data.
  uint32_t data_size;  // Size (in bytes) of |data|.

  uint32_t data_offset;  // Number of bytes to be discarded before decryption.

  uint8_t* key_id;  // Key ID to identify the decryption key.
  uint32_t key_id_size;  // Size (in bytes) of |key_id|.

  uint8_t* iv;  // Initialization vector.
  uint32_t iv_size;  // Size (in bytes) of |iv|.

  uint8_t* checksum;
  uint32_t checksum_size;  // Size (in bytes) of the |checksum|.

  struct SubsampleEntry* subsamples;
  uint32_t num_subsamples;  // Number of subsamples in |subsamples|.

  int64_t timestamp;  // Presentation timestamp in microseconds.
};

struct OutputBuffer {
  uint8_t* data;  // Pointer to the beginning of the output data.
  uint32_t data_size;  // Size (in bytes) of |data|.

  int64_t timestamp;  // Presentation timestamp in microseconds.
};

class ContentDecryptionModule {
 public:
  enum Status {
    kSuccess = 0,
    kErrorUnknown,
    kErrorNoKey
  };

  // Generates a |key_request| as well as a |session_id| given the |init_data|.
  // The CDM may also extract a |default_url|.
  // Returns kSuccess if the key request was successfully generated,
  // in which case the callee should have allocated memory for the output
  // parameters (e.g |session_id|) and passed the ownership to the caller.
  // Returns kErrorUnknown otherwise, in which case the output
  // parameters should not be used by the caller.
  //
  // TODO(xhwang): It's not safe to pass the ownership of the dynamically
  // allocated memory over library boundaries. Fix it after related PPAPI change
  // and sample CDM are landed.
  virtual Status GenerateKeyRequest(const uint8_t* init_data,
                                    int init_data_size,
                                    char** session_id,
                                    int* session_id_size,
                                    uint8_t** key_request,
                                    int* key_request_size,
                                    char** default_url,
                                    int* default_url_size) = 0;

  // Adds the |key| to the CDM to be associated with |key_id|.
  // Returns kSuccess if the key was successfully added.
  // Returns kErrorUnknown otherwise.
  virtual Status AddKey(const char* session_id,
                        int session_id_size,
                        const uint8_t* key,
                        int key_size,
                        const uint8_t* key_id,
                        int key_id_size) = 0;

  // Cancels any pending key request made to the CDM for |session_id|.
  // Returns kSuccess if all pending key requests for |session_id| were
  // successfully canceled or there was no key request to be canceled.
  // Returns kErrorUnknown otherwise.
  virtual Status CancelKeyRequest(const char* session_id,
                                  int session_id_size) = 0;

  // Decrypts the |encrypted_buffer|.
  // Returns kSuccess if decryption succeeded, in which case the callee
  // should have filled the |decrypted_buffer| and passed the ownership of
  // |data| in |decrypted_buffer| to the caller.
  // Returns kErrorNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kErrorUnknown if any other error happened.
  // In these two cases, |decrypted_buffer| should not be used by the caller.
  //
  // TODO(xhwang): It's not safe to pass the ownership of the dynamically
  // allocated memory over library boundaries. Fix it after related PPAPI change
  // and sample CDM are landed.
  virtual Status Decrypt(const char* session_id,
                         int session_id_size,
                         const InputBuffer& encrypted_buffer,
                         OutputBuffer* decrypted_buffer) = 0;

  virtual ~ContentDecryptionModule() {};
};

}  // namespace cdm

#endif  // WEBKIT_MEDIA_CRYPTO_CONTENT_DECRYPTION_MODULE_H_
