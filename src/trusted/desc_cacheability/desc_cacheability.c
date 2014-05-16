/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/desc_cacheability/desc_cacheability.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/public/desc_metadata_types.h"
#include "native_client/src/public/nacl_file_info.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_file_info.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/validator/rich_file_info.h"
#include "native_client/src/trusted/validator/validation_cache.h"

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
  if (NaClFileTokenIsValid(file_token) &&
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

  if (!NaClDescGetFileToken(desc, &file_token)) {
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
