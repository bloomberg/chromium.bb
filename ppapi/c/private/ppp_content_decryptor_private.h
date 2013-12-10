/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppp_content_decryptor_private.idl,
 *   modified Tue Dec  3 17:05:10 2013.
 */

#ifndef PPAPI_C_PRIVATE_PPP_CONTENT_DECRYPTOR_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPP_CONTENT_DECRYPTOR_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/pp_content_decryptor.h"

#define PPP_CONTENTDECRYPTOR_PRIVATE_INTERFACE_0_9 \
    "PPP_ContentDecryptor_Private;0.9"
#define PPP_CONTENTDECRYPTOR_PRIVATE_INTERFACE \
    PPP_CONTENTDECRYPTOR_PRIVATE_INTERFACE_0_9

/**
 * @file
 * This file defines the <code>PPP_ContentDecryptor_Private</code>
 * interface. Note: This is a special interface, only to be used for Content
 * Decryption Modules, not normal plugins.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * <code>PPP_ContentDecryptor_Private</code> structure contains the function
 * pointers the decryption plugin must implement to provide services needed by
 * the browser. This interface provides the plugin side support for the Content
 * Decryption Module (CDM) for Encrypted Media Extensions:
 * http://www.w3.org/TR/encrypted-media/
 */
struct PPP_ContentDecryptor_Private_0_9 {
  /**
   * Initialize for the specified key system.
   *
   * @param[in] key_system A <code>PP_Var</code> of type
   * <code>PP_VARTYPE_STRING</code> containing the name of the key system.
   */
  void (*Initialize)(PP_Instance instance, struct PP_Var key_system);
  /**
   * Creates a session. <code>type</code> contains the MIME type of
   * <code>init_data</code>. <code>init_data</code> is a data buffer
   * containing data for use in generating the request.
   *
   * Note: <code>CreateSession()</code> must create the session ID used in
   * other methods on this interface. The session ID must be provided to the
   * browser by the CDM via <code>SessionCreated()</code> on the
   * <code>PPB_ContentDecryptor_Private</code> interface.
   *
   * @param[in] session_id A reference for the session for which a session
   * should be generated.
   *
   * @param[in] type A <code>PP_Var</code> of type
   * <code>PP_VARTYPE_STRING</code> containing the MIME type for init_data.
   *
   * @param[in] init_data A <code>PP_Var</code> of type
   * <code>PP_VARTYPE_ARRAYBUFFER</code> containing container specific
   * initialization data.
   */
  void (*CreateSession)(PP_Instance instance,
                        uint32_t session_id,
                        struct PP_Var type,
                        struct PP_Var init_data);
  /**
   * Provides a license or other message to the decryptor.
   *
   * When the CDM needs more information, it must call
   * <code>SessionMessage()</code> on the
   * <code>PPB_ContentDecryptor_Private</code> interface, and the browser
   * must notify the web application. When the CDM has finished processing
   * <code>response</code> and needs no more information, it must call
   * <code>SessionReady()</code> on the
   * <code>PPB_ContentDecryptor_Private</code> interface, and the browser
   * must notify the web application.
   *
   * @param[in] session_id A reference for the session to update.
   *
   * @param[in] response A <code>PP_Var</code> of type
   * <code>PP_VARTYPE_ARRAYBUFFER</code> containing the license or other
   * message for the given session ID.
   */
  void (*UpdateSession)(PP_Instance instance,
                        uint32_t session_id,
                        struct PP_Var response);
  /**
   * Release the specified session and related resources.
   *
   * @param[in] session_id A reference for the session that should be
   * released.
   */
  void (*ReleaseSession)(PP_Instance instance, uint32_t session_id);
  /**
   * Decrypts the block and returns the unencrypted block via
   * <code>DeliverBlock()</code> on the
   * <code>PPB_ContentDecryptor_Private</code> interface. The returned block
   * contains encoded data.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a
   * <code>PPB_Buffer_Dev</code> resource that contains an encrypted data
   * block.
   *
   * @param[in] encrypted_block_info A <code>PP_EncryptedBlockInfo</code> that
   * contains all auxiliary information needed for decryption of the
   * <code>encrypted_block</code>.
   */
  void (*Decrypt)(PP_Instance instance,
                  PP_Resource encrypted_block,
                  const struct PP_EncryptedBlockInfo* encrypted_block_info);
  /**
   * Initializes the audio decoder using codec and settings in
   * <code>decoder_config</code>, and returns the result of the initialization
   * request to the browser using the <code>DecoderInitializeDone(
      )</code> method
   * on the <code>PPB_ContentDecryptor_Private</code> interface.
   *
   * @param[in] decoder_config A <code>PP_AudioDecoderConfig</code> that
   * contains audio decoder settings and a request ID. The request ID is passed
   * to the <code>DecoderInitializeDone()</code> method on the
   * <code>PPB_ContentDecryptor_Private</code> interface to allow clients to
   * associate the result with a audio decoder initialization request.
   *
   * @param[in] codec_extra_data A <code>PP_Resource</code> corresponding to a
   * <code>PPB_Buffer_Dev</code> resource containing codec setup data required
   * by some codecs. It should be set to 0 when the codec being initialized
   * does not require it.
   */
  void (*InitializeAudioDecoder)(
      PP_Instance instance,
      const struct PP_AudioDecoderConfig* decoder_config,
      PP_Resource codec_extra_data);
  /**
   * Initializes the video decoder using codec and settings in
   * <code>decoder_config</code>, and returns the result of the initialization
   * request to the browser using the <code>DecoderInitializeDone()</code>
   * method on the <code>PPB_ContentDecryptor_Private</code> interface.
   *
   * @param[in] decoder_config A <code>PP_VideoDecoderConfig</code> that
   * contains video decoder settings and a request ID. The request ID is passed
   * to the <code>DecoderInitializeDone()</code> method on the
   * <code>PPB_ContentDecryptor_Private</code> interface to allow clients to
   * associate the result with a video decoder initialization request.
   *
   * @param[in] codec_extra_data A <code>PP_Resource</code> corresponding to a
   * <code>PPB_Buffer_Dev</code> resource containing codec setup data required
   * by some codecs. It should be set to 0 when the codec being initialized
   * does not require it.
   */
  void (*InitializeVideoDecoder)(
      PP_Instance instance,
      const struct PP_VideoDecoderConfig* decoder_config,
      PP_Resource codec_extra_data);
  /**
   * De-initializes the decoder for the <code>PP_DecryptorStreamType</code>
   * specified by <code>decoder_type</code> and sets it to an uninitialized
   * state. The decoder can be re-initialized after de-initialization completes
   * by calling <code>InitializeAudioDecoder</code> or
   * <code>InitializeVideoDecoder</code>.
   *
   * De-initialization completion is reported to the browser using the
   * <code>DecoderDeinitializeDone()</code> method on the
   * <code>PPB_ContentDecryptor_Private</code> interface.
   *
   * @param[in] decoder_type A <code>PP_DecryptorStreamType</code> that
   * specifies the decoder to de-initialize.
   *
   * @param[in] request_id A request ID that allows the browser to associate a
   * request to de-initialize a decoder with the corresponding call to the
   * <code>DecoderDeinitializeDone()</code> method on the
   * <code>PPB_ContentDecryptor_Private</code> interface.
   */
  void (*DeinitializeDecoder)(PP_Instance instance,
                              PP_DecryptorStreamType decoder_type,
                              uint32_t request_id);
  /**
   * Resets the decoder for the <code>PP_DecryptorStreamType</code> specified
   * by <code>decoder_type</code> to an initialized clean state. Reset
   * completion is reported to the browser using the
   * <code>DecoderResetDone()</code> method on the
   * <code>PPB_ContentDecryptor_Private</code> interface. This method can be
   * used to signal a discontinuity in the encoded data stream, and is safe to
   * call multiple times.
   *
   * @param[in] decoder_type A <code>PP_DecryptorStreamType</code> that
   * specifies the decoder to reset.
   *
   * @param[in] request_id A request ID that allows the browser to associate a
   * request to reset the decoder with a corresponding call to the
   * <code>DecoderResetDone()</code> method on the
   * <code>PPB_ContentDecryptor_Private</code> interface.
   */
  void (*ResetDecoder)(PP_Instance instance,
                       PP_DecryptorStreamType decoder_type,
                       uint32_t request_id);
  /**
   * Decrypts encrypted_buffer, decodes it, and returns the unencrypted
   * uncompressed (decoded) data to the browser via the
   * <code>DeliverFrame()</code> or <code>DeliverSamples()</code> method on the
   * <code>PPB_ContentDecryptor_Private</code> interface.
   *
   * @param[in] decoder_type A <code>PP_DecryptorStreamType</code> that
   * specifies the decoder to use after <code>encrypted_buffer</code> is
   * decrypted.
   *
   * @param[in] encrypted_buffer A <code>PP_Resource</code> corresponding to a
   * <code>PPB_Buffer_Dev</code> resource that contains encrypted media data.
   *
   * @param[in] encrypted_block_info A <code>PP_EncryptedBlockInfo</code> that
   * contains all auxiliary information needed for decryption of the
   * <code>encrypted_block</code>.
   */
  void (*DecryptAndDecode)(
      PP_Instance instance,
      PP_DecryptorStreamType decoder_type,
      PP_Resource encrypted_buffer,
      const struct PP_EncryptedBlockInfo* encrypted_block_info);
};

typedef struct PPP_ContentDecryptor_Private_0_9 PPP_ContentDecryptor_Private;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPP_CONTENT_DECRYPTOR_PRIVATE_H_ */

