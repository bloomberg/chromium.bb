/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/pp_content_decryptor.idl modified Wed Oct  3 16:16:49 2012. */

#ifndef PPAPI_C_PRIVATE_PP_CONTENT_DECRYPTOR_H_
#define PPAPI_C_PRIVATE_PP_CONTENT_DECRYPTOR_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @file
 * The <code>PP_DecryptTrackingInfo</code> struct contains necessary information
 * that can be used to associate the decrypted block with a decrypt request
 * and/or an input block.
 */


/**
 * @addtogroup Structs
 * @{
 */
struct PP_DecryptTrackingInfo {
  /**
   * Client-specified identifier for the associated decrypt request. By using
   * this value, the client can associate the decrypted block with a decryption
   * request.
   */
  uint32_t request_id;
  /**
   * 4-byte padding to make the size of <code>PP_DecryptTrackingInfo</code>
   * a multiple of 8 bytes and make sure |timestamp| starts on 8-byte boundary.
   * The value of this field should not be used.
   */
  uint32_t padding;
  /**
   * Timestamp in microseconds of the associated block. By using this value,
   * the client can associate the decrypted (and decoded) data with an input
   * block. This is needed because buffers may be delivered out of order and
   * not in response to the <code>request_id</code> they were provided with.
   */
  int64_t timestamp;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_DecryptTrackingInfo, 16);

/**
 * The <code>PP_DecryptSubsampleDescription</code> struct contains information
 * to support subsample decryption.
 *
 * An input block can be split into several continuous subsamples.
 * A <code>PP_DecryptSubsampleEntry</code> specifies the number of clear and
 * cipher bytes in each subsample. For example, the following block has three
 * subsamples:
 *
 * |<----- subsample1 ----->|<----- subsample2 ----->|<----- subsample3 ----->|
 * |   clear1   |  cipher1  |  clear2  |   cipher2   | clear3 |    cipher3    |
 *
 * For decryption, all of the cipher bytes in a block should be treated as a
 * contiguous (in the subsample order) logical stream. The clear bytes should
 * not be considered as part of decryption.
 *
 * Logical stream to decrypt:   |  cipher1  |   cipher2   |    cipher3    |
 * Decrypted stream:            | decrypted1|  decrypted2 |   decrypted3  |
 *
 * After decryption, the decrypted bytes should be copied over the position
 * of the corresponding cipher bytes in the original block to form the output
 * block. Following the above example, the decrypted block should be:
 *
 * |<----- subsample1 ----->|<----- subsample2 ----->|<----- subsample3 ----->|
 * |   clear1   | decrypted1|  clear2  |  decrypted2 | clear3 |   decrypted3  |
 */
struct PP_DecryptSubsampleDescription {
  /**
   * Size in bytes of clear data in a subsample entry.
   */
  uint32_t clear_bytes;
  /**
   * Size in bytes of encrypted data in a subsample entry.
   */
  uint32_t cipher_bytes;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_DecryptSubsampleDescription, 8);

/**
 * The <code>PP_EncryptedBlockInfo</code> struct contains all the information
 * needed to decrypt an encrypted block.
 */
struct PP_EncryptedBlockInfo {
  /**
   * Information needed by the client to track the block to be decrypted.
   */
  struct PP_DecryptTrackingInfo tracking_info;
  /**
   * Size in bytes of data to be discarded before applying the decryption.
   */
  uint32_t data_offset;
  /**
   * Key ID of the block to be decrypted.
   *
   * TODO(xhwang): For WebM the key ID can be as large as 2048 bytes in theory.
   * But it's not used in current implementations. If we really need to support
   * it, we should move key ID out as a separate parameter, e.g.
   * as a <code>PP_Var</code>, or make the whole
   * <code>PP_EncryptedBlockInfo</code> as a <code>PP_Resource</code>.
   */
  uint8_t key_id[64];
  uint32_t key_id_size;
  /**
   * Initialization vector of the block to be decrypted.
   */
  uint8_t iv[16];
  uint32_t iv_size;
  /**
   * Subsample information of the block to be decrypted.
   */
  struct PP_DecryptSubsampleDescription subsamples[16];
  uint32_t num_subsamples;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_EncryptedBlockInfo, 240);
/**
 * @}
 */

/**
 * @addtogroup Enums
 * @{
 */
/**
 * <code>PP_DecryptedFrameFormat</code> contains video frame formats.
 */
typedef enum {
  PP_DECRYPTEDFRAMEFORMAT_UNKNOWN = 0,
  PP_DECRYPTEDFRAMEFORMAT_EMPTY = 1,
  PP_DECRYPTEDFRAMEFORMAT_YV12 = 2,
  PP_DECRYPTEDFRAMEFORMAT_I420 = 3
} PP_DecryptedFrameFormat;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_DecryptedFrameFormat, 4);

/**
 * <code>PP_VideoCodec</code> contains video codec type constants.
 */
typedef enum {
  PP_VIDEOCODEC_UNKNOWN = 0,
  PP_VIDEOCODEC_VP8 = 1
} PP_VideoCodec;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_VideoCodec, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
/**
 * <code>PP_EncryptedVideoFrameInfo</code> contains the information required
 * to decrypt and decode a video frame.
 * TODO(tomfinegan): Revisit necessity of including format information in this
 * struct once we decide how to implement video decoder initialization.
 */
struct PP_EncryptedVideoFrameInfo {
  /**
   * The decoded video frame format.
   */
  PP_DecryptedFrameFormat format;
  /**
   * The video frame codec type.
   */
  PP_VideoCodec codec;
  /**
   * Video frame width in pixels.
   */
  int32_t width;
  /**
   * Video frame height in pixels.
   */
  int32_t height;
  /**
   * Information required to decrypt the frame.
   */
  struct PP_EncryptedBlockInfo encryption_info;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_EncryptedVideoFrameInfo, 256);
/**
 * @}
 */

/**
 * @addtogroup Enums
 * @{
 */
/**
 * The <code>PP_DecryptResult</code> enum contains decryption and decoding
 * result constants.
 */
typedef enum {
  /** The decryption (and/or decoding) operation finished successfully. */
  PP_DECRYPTRESULT_SUCCESS = 0,
  /** The decryptor did not have the necessary decryption key. */
  PP_DECRYPTRESULT_DECRYPT_NOKEY = 1,
  /** An unexpected error happened during decryption. */
  PP_DECRYPTRESULT_DECRYPT_ERROR = 2,
  /** An unexpected error happened during decoding. */
  PP_DECRYPTRESULT_DECODE_ERROR = 3
} PP_DecryptResult;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_DecryptResult, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
/**
 * <code>PP_DecryptedBlockInfo</code> struct contains the decryption result and
 * tracking info associated with the decrypted block.
 */
struct PP_DecryptedBlockInfo {
  /**
   * Result of the decryption (and/or decoding) operation.
   */
  PP_DecryptResult result;
  /**
   * 4-byte padding to make the size of <code>PP_DecryptedBlockInfo</code>
   * a multiple of 8 bytes and make sure |tracking_info| starts on an 8-byte
   * boundary. The value of this field should not be used.
   */
  uint32_t padding;
  /**
   * Information needed by the client to track the block to be decrypted.
   */
  struct PP_DecryptTrackingInfo tracking_info;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_DecryptedBlockInfo, 24);
/**
 * @}
 */

/**
 * @addtogroup Enums
 * @{
 */
/**
 * <code>PP_DecryptedFramePlanes</code> provides YUV plane index values for
 * accessing plane offsets stored in <code>PP_DecryptedFrameInfo</code>.
 */
typedef enum {
  PP_DECRYPTEDFRAMEPLANES_Y = 0,
  PP_DECRYPTEDFRAMEPLANES_U = 1,
  PP_DECRYPTEDFRAMEPLANES_V = 2
} PP_DecryptedFramePlanes;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_DecryptedFramePlanes, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
/**
 * <code>PP_DecryptedFrameInfo</code> contains the result of the
 * decrypt and decode operation on the associated frame, information required
 * to access the frame data in buffer, and tracking info.
 */
struct PP_DecryptedFrameInfo {
  /**
   * Result of the decrypt and decode operation.
   */
  PP_DecryptResult result;
  /**
   * Format of the decrypted frame.
   */
  PP_DecryptedFrameFormat format;
  /**
   * Offsets into the buffer resource for accessing video planes.
   */
  int32_t plane_offsets[3];
  /**
   * Stride of each plane.
   */
  int32_t strides[3];
  /**
   * Width of the video frame, in pixels.
   */
  int32_t width;
  /**
   * Height of the video frame, in pixels.
   */
  int32_t height;
  /**
   * Information needed by the client to track the decrypted frame.
   */
  struct PP_DecryptTrackingInfo tracking_info;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_DecryptedFrameInfo, 56);
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PP_CONTENT_DECRYPTOR_H_ */

