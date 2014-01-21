/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/desc_cacheability/desc_cacheability.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/public/desc_metadata_types.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/fault_injection/fault_injection.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/validator/nacl_file_info.h"
#include "native_client/src/trusted/validator/rich_file_info.h"
#include "native_client/src/trusted/validator/validation_cache.h"

int NaClDescSetFileToken(struct NaClDesc *desc,
                         struct NaClFileToken const *token) {
  int error;
  error = (*NACL_VTBL(NaClDesc, desc)->
           SetMetadata)(desc,
                        NACL_DESC_METADATA_FILE_TOKEN_TYPE,
                        sizeof *token, (uint8_t const *) token);
  if (0 != error) {
    NaClLog(4, "NaClDescSetFileToken: failed, errno %d\n", -error);
    return 0;
  }
  return 1;
}

int NaClDescGetFileToken(struct NaClDesc *desc,
                         struct NaClFileToken *out_token) {
  int32_t metadata_type;
  uint32_t metadata_bytes;

  metadata_bytes = (uint32_t) sizeof *out_token;
  metadata_type = (*NACL_VTBL(NaClDesc, desc)->
                   GetMetadata)(desc, &metadata_bytes,
                                (uint8_t *) out_token);
  if (NACL_DESC_METADATA_NONE_TYPE == metadata_type) {
    NaClLog(4, "NaClDescGetFileToken: no meta data, cannot map\n");
    return 0;
  } else if (NACL_DESC_METADATA_FILE_TOKEN_TYPE != metadata_type) {
    return 0;
  } else if (metadata_bytes != (uint32_t) sizeof *out_token) {
    /* there is supposed to be a file token, but wrong size? */
    NaClLog(LOG_WARNING,
            "NaClDescGetFileToken: purported file token present,"
            " but token size is incorrect.\n");
    return 0;
  }
  NaClLog(4,
          "NaClDescGetFileToken: got token 0x%"NACL_PRIx64":%"NACL_PRIx64"\n",
          out_token->hi, out_token->lo);
  return 1;
}

struct NaClDesc *NaClExchangeFileTokenForMappableDesc(
    struct NaClFileToken *file_token,
    struct NaClValidationCache *validation_cache) {
  int32_t new_fd;
  char *file_path;
  uint32_t file_path_length;
  struct NaClDesc *desc = NULL;

  /*
   * Not all file loading paths will produce a valid token.  Zero is
   * an invalid token value that indicates there is nothing to
   * resolve.  In this case, assume nothing about the providence of
   * the file.
   */
  if (!(file_token->lo == 0 && file_token->hi == 0) &&
      validation_cache->ResolveFileToken != NULL &&
      validation_cache->ResolveFileToken(validation_cache->handle,
                                         file_token,
                                         &new_fd,
                                         &file_path,
                                         &file_path_length)) {
    struct NaClRichFileInfo info;

    desc = NaClDescIoDescFromHandleAllocCtor((NaClHandle) new_fd,
                                             NACL_ABI_O_RDONLY);

    /* Mark the desc as OK for mmapping. */
    NaClDescMarkSafeForMmap(desc);

    /* Provide metadata for validation. */
    NaClRichFileInfoCtor(&info);
    info.known_file = 1;
    info.file_path = file_path;  /* Takes ownership. */
    info.file_path_length = file_path_length;
    NaClSetFileOriginInfo(desc, &info);
    NaClRichFileInfoDtor(&info);
  }
  return desc;
}

void NaClReplaceDescIfValidationCacheAssertsMappable(
    struct NaClDesc **desc_in_out,
    struct NaClValidationCache *validation_cache) {
  struct NaClDesc *desc = *desc_in_out;
  struct NaClDesc *replacement;
  struct NaClFileToken file_token;

  if (NACL_FI("validation_cache_replacement_bypass", 0, 1)) {
    NaClDescMarkSafeForMmap(desc);
  } else if (!NaClDescGetFileToken(desc, &file_token)) {
    NaClLog(4,
            "NaClReplaceDescIfValidationCacheAssertsMappable: no valid"
            " file token\n");
  } else {
    replacement = NaClExchangeFileTokenForMappableDesc(&file_token,
                                                       validation_cache);
    if (NULL != replacement) {
      NaClDescUnref(desc);
      *desc_in_out = replacement;
    }
  }
}
