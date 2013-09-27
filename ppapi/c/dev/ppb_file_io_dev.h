/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_file_io_dev.idl modified Thu Sep 19 10:07:03 2013. */

#ifndef PPAPI_C_DEV_PPB_FILE_IO_DEV_H_
#define PPAPI_C_DEV_PPB_FILE_IO_DEV_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_FILEIO_DEV_INTERFACE_0_1 "PPB_FileIO(Dev);0.1"
#define PPB_FILEIO_DEV_INTERFACE PPB_FILEIO_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines methods for use with a PPB_FileIO resource that may become
 * stable in the future. For now, they can be used only in plugins with DEV
 * permissions.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * The PP_FileMapProtection values indicate the permissions requested for the
 * file mapping. These should be used in a uint32_t bitfield.
 */
typedef enum {
  /** Requests read access to the mapped address. */
  PP_FILEMAPPROTECTION_READ = 1u << 0,
  /** Requests write access to the mapped address. */
  PP_FILEMAPPROTECTION_WRITE = 1u << 1
} PP_FileMapProtection;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_FileMapProtection, 4);

/**
 * The PP_FileMapFlags contain flag values for use with Map().
 */
typedef enum {
  /**
   * Requests a shared mapping. If this flag is set, changes written to the
   * memory region will be reflected in the underlying file and will thus
   * eventually be visible to other processes which have opened the file. The
   * file may not actually be updated until Unmap() is called. This is only
   * valid if the PPB_FileIO resource was opened with write permission.
   */
  PP_FILEMAPFLAG_SHARED = 1u << 0,
  /**
   * Requests a copy-on-write mapping. If this flag is set, changes are not
   * written to the underlying file, but only in the memory of the process
   * (copy-on-write).
   */
  PP_FILEMAPFLAG_PRIVATE = 1u << 1,
  /**
   * Forces Map() to map the file contents at the provided |address|. If Map()
   * can not comply, Map() will fail.
   */
  PP_FILEMAPFLAG_FIXED = 1u << 2
} PP_FileMapFlags;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_FileMapFlags, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 *  PPB_FileIO_Dev contains functions that are usable with PPB_FileIO resources
 *  but aren't yet considered stable yet and thus are not supported for general
 *  NaCl or PNaCl apps yet. Features here are being tested and refined for
 *  possible future inclusion in (stable) PPB_FileIO.
 */
struct PPB_FileIO_Dev_0_1 {
  /**
   * Map() maps the contents from an offset of the file into memory.
   *
   * @param[in] file_io A PP_Resource corresponding to a file.
   * @param[in] length The number of bytes to map.
   * @param[in] map_protection A bitfield containing values from
   * PP_FileMapProtection, indicating what memory operations should be permitted
   * on the mapped region.
   * @param[in] map_flags A bitfield containing values from
   * PP_FileMapFlags, providing options for the behavior of Map. If the region
   * is to be writeable, then exactly one of PP_FILEMAPFLAG_SHARED or
   * PP_FILEMAPFLAG_PRIVATE must be set.
   * @param[in] offset The offset into the file. Must be a multiple of the
   * Map page size as returned by GetMapPageSize.
   * @param[inout] address The value of |*address|, if non-NULL, will be used as
   * a hint to determine where in memory the file should be mapped. If the value
   * is NULL, the host operating system will choose |address|. Upon
   * Map() completing, |*address| will contain the actual memory location at
   * which the file was mapped. If the plugin provides a non-NULL |*address|, it
   * must be a multiple of the map page size as returned by GetMapPageSize().
   * @param[in] callback A PP_CompletionCallback to be called upon
   * completion of Map().
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   */
  int32_t (*Map)(PP_Resource file_io,
                 int64_t length,
                 uint32_t map_protection,
                 uint32_t map_flags,
                 int64_t offset,
                 void** address,
                 struct PP_CompletionCallback callback);
  /**
   * Unmap() deletes the mapping of the specified address address to a
   * file io.  The specified address must have been retrieved with
   * Map().
   * @param[in] file_io A PP_Resource corresponding to a file.
   * @param[in] address The starting address of the address in memory to
   * be unmapped.
   * @param[in] length The length of the region to unmap.
   */
  void (*Unmap)(PP_Resource file_io, void* address, int64_t length);
  /**
   * GetMapPageSize() returns the size of pages that Map() uses. Returns 0 on
   * failure.
   */
  int64_t (*GetMapPageSize)(PP_Resource file_io);
};

typedef struct PPB_FileIO_Dev_0_1 PPB_FileIO_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_FILE_IO_DEV_H_ */

