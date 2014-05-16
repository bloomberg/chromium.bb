/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/desc/nacl_desc_file_info.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/public/desc_metadata_types.h"
#include "native_client/src/public/nacl_file_info.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"


int NaClDescSetFileToken(struct NaClDesc *desc,
                         struct NaClFileToken const *token) {
  int error = (*NACL_VTBL(NaClDesc, desc)->
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

struct NaClDesc *NaClDescIoFromFileInfo(struct NaClFileInfo info,
                                        int mode) {
  struct NaClDesc *desc = NaClDescIoDescFromDescAllocCtor(
      info.desc, mode);
  if (NULL == desc) {
    return NULL;
  }
  if (NaClFileTokenIsValid(&info.file_token)) {
    if (!NaClDescSetFileToken(desc, &info.file_token)) {
      NaClDescSafeUnref(desc);
      return NULL;
    }
  }
  return desc;
}
