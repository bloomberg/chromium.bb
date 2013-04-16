/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator/validation_cache.h"

#include <string.h>
#include <sys/stat.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/trusted/validator/validation_metadata.h"

#if NACL_WINDOWS
#include <Windows.h>
#include <io.h>
#endif

#define ADD_LITERAL(cache, query, data) \
  ((cache)->AddData((query), (uint8_t*)&(data), sizeof(data)))

int CachingIsInexpensive(const struct NaClValidationMetadata *metadata) {
  return NULL != metadata && metadata->identity_type == NaClCodeIdentityFile;
}

void ConstructMetadataForFD(int file_desc,
                            const char* file_name,
                            size_t file_name_length,
                            struct NaClValidationMetadata *metadata) {
  struct NaClHostDesc wrapper;
  nacl_host_stat_t stat;
#if NACL_WINDOWS
  BY_HANDLE_FILE_INFORMATION file_info;
#endif

  memset(metadata, 0, sizeof(*metadata));
  /* If we early out, identity_type will be 0 / NaClCodeIdentityData. */

  metadata->file_name = file_name;
  metadata->file_name_length = file_name_length;

  wrapper.d = file_desc;
  if(NaClHostDescFstat(&wrapper, &stat))
    return;

#if NACL_WINDOWS
  /*
   * This will not get us the complete file ID on ReFS, but doing the correct
   * thing (calling GetFileInformationByHandleEx) causes linkage issues on
   * Windows XP.  We aren't relying on the file ID for security, just collision
   * resistance, so we don't need all of it.
   * In many cases (including on NTFS) we're also getting the 32 least
   * significant bits of a 64-bit volume serial number - but again, since it's
   * random we can live with it.
   */
  if (!GetFileInformationByHandle((HANDLE) _get_osfhandle(file_desc),
                                  &file_info))
    return;
  metadata->device_id = file_info.dwVolumeSerialNumber;
  metadata->file_id = ((((uint64_t)file_info.nFileIndexHigh) << 32) |
                       file_info.nFileIndexLow);
#else
  /* st_dev is not actually a property of the device, so skip it. */
  metadata->file_id = stat.st_ino;
#endif

  metadata->file_size = stat.st_size;
  metadata->mtime = stat.st_mtime;
  metadata->ctime = stat.st_ctime;

  /* We have all the identity information we need. */
  metadata->identity_type = NaClCodeIdentityFile;
}

void AddCodeIdentity(uint8_t *data,
                     size_t size,
                     const struct NaClValidationMetadata *metadata,
                     struct NaClValidationCache *cache,
                     void *query) {
  NaClCodeIdentityType identity_type;
  if (NULL != metadata) {
    identity_type = metadata->identity_type;
  } else {
    /* Fallback: identity unknown, treat it as anonymous data. */
    identity_type = NaClCodeIdentityData;
  }
  CHECK(identity_type < NaClCodeIdentityMax);

  /*
   * Explicitly record the type of identity being used to prevent attacks
   * that confuse the payload of different identity types.
   */
  ADD_LITERAL(cache, query, identity_type);

  if (identity_type == NaClCodeIdentityFile) {
    /* Sanity checks. */
    CHECK(metadata->file_name);
    CHECK(metadata->file_name_length);
    CHECK(metadata->code_offset + (int64_t) size <= metadata->file_size);

    /* The location of the code in the file. */
    ADD_LITERAL(cache, query, metadata->code_offset);
    ADD_LITERAL(cache, query, size);

    /* The identity of the file. */
    ADD_LITERAL(cache, query, metadata->file_name_length);
    cache->AddData(query, (uint8_t *) metadata->file_name,
                   metadata->file_name_length);
    ADD_LITERAL(cache, query, metadata->device_id);
    ADD_LITERAL(cache, query, metadata->file_id);
    ADD_LITERAL(cache, query, metadata->file_size);
    ADD_LITERAL(cache, query, metadata->mtime);
    ADD_LITERAL(cache, query, metadata->ctime);
  } else {
    /* Hash all the code. */
    cache->AddData(query, data, size);
  }
}
