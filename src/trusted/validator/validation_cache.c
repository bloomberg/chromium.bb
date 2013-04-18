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

int CachingIsInexpensive(struct NaClValidationCache *cache,
                         const struct NaClValidationMetadata *metadata) {
  if (cache->CachingIsInexpensive != NULL) {
    return cache->CachingIsInexpensive(metadata);
  } else {
    return NULL != metadata && metadata->identity_type == NaClCodeIdentityFile;
  }
}

void MetadataFromFDCtor(struct NaClValidationMetadata *metadata,
                        int file_desc,
                        const char* file_name,
                        size_t file_name_length) {
  struct NaClHostDesc wrapper;
  nacl_host_stat_t stat;
#if NACL_WINDOWS
  BY_HANDLE_FILE_INFORMATION file_info;
#endif

  memset(metadata, 0, sizeof(*metadata));
  /* If we early out, identity_type will be 0 / NaClCodeIdentityData. */

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

  CHECK(0 < file_name_length);
  metadata->file_name = malloc(file_name_length);
  CHECK(NULL != metadata->file_name);
  memcpy(metadata->file_name, file_name, file_name_length);
  metadata->file_name_length = file_name_length;

  /* We have all the identity information we need. */
  metadata->identity_type = NaClCodeIdentityFile;
}

void MetadataDtor(struct NaClValidationMetadata *metadata) {
  free(metadata->file_name);
  /* Prevent use after free. */
  memset(metadata, 0, sizeof(*metadata));
}

static void Serialize(uint8_t *buffer, const void *value, size_t size,
                      uint32_t *offset) {
  if (buffer != NULL)
    memcpy(&buffer[*offset], value, size);
  *offset += (uint32_t) size;
}

static void SerializeNaClDescMetadataInternal(
    uint8_t known_file,
    const char *file_name,
    uint32_t file_name_length,
    uint8_t *buffer,
    uint32_t *offset) {
  *offset = 0;
  Serialize(buffer, &known_file, sizeof(known_file), offset);
  if (known_file) {
    Serialize(buffer, &file_name_length, sizeof(file_name_length), offset);
    Serialize(buffer, file_name, file_name_length, offset);
  }
}

int SerializeNaClDescMetadata(
    uint8_t known_file,
    const char *file_name,
    uint32_t file_name_length,
    uint8_t **buffer,
    uint32_t *buffer_length) {

  *buffer = NULL;

  /* Calculate the buffer size. */
  SerializeNaClDescMetadataInternal(known_file, file_name, file_name_length,
                                    NULL, buffer_length);

  /* Allocate the buffer. */
  *buffer = malloc(*buffer_length);
  if (NULL == buffer)
    return 1;

  /* Fill the buffer. */
  SerializeNaClDescMetadataInternal(known_file, file_name, file_name_length,
                                    *buffer, buffer_length);
  return 0;
}

static int Deserialize(uint8_t *buffer, uint32_t buffer_length, void *value,
                       size_t size, uint32_t *offset) {
  if (*offset + size > buffer_length)
    return 1;
  memcpy(value, &buffer[*offset], size);
  *offset += (uint32_t) size;
  return 0;
}

int DeserializeNaClDescMetadata(
    uint8_t *buffer,
    uint32_t buffer_length,
    uint8_t *known_file,
    char **file_name,
    uint32_t *file_name_length) {

  uint32_t offset = 0;
  *file_name = NULL;

  if (Deserialize(buffer, buffer_length, known_file, sizeof(*known_file),
                  &offset))
    goto on_error;

  if (*known_file) {
    if (Deserialize(buffer, buffer_length, file_name_length,
                    sizeof(*file_name_length), &offset))
      goto on_error;
    *file_name = malloc(*file_name_length);
    if (NULL == *file_name)
      goto on_error;
    if (Deserialize(buffer, buffer_length, *file_name, *file_name_length,
                    &offset))
      goto on_error;
  }

  /* Entire buffer consumed? */
  if (offset != buffer_length)
    goto on_error;
  return 0;

 on_error:
  *known_file = 0;
  free(*file_name);
  *file_name = NULL;
  *file_name_length = 0;
  return 1;
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
