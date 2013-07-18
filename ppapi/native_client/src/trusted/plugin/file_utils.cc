// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Some common file utilities for plugin code.

#include "ppapi/native_client/src/trusted/plugin/file_utils.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"


namespace plugin {
namespace file_utils {

StatusCode SlurpFile(int32_t fd,
                     nacl::string& out_buf,
                     size_t max_size_to_read) {
  struct stat stat_buf;
  if (fstat(fd, &stat_buf) != 0) {
    CLOSE(fd);
    return PLUGIN_FILE_ERROR_STAT;
  }

  // Figure out how large a buffer we need to slurp the whole file (with a
  // '\0' at the end).
  size_t bytes_to_read = static_cast<size_t>(stat_buf.st_size);
  if (bytes_to_read > max_size_to_read - 1) {
    CLOSE(fd);
    return PLUGIN_FILE_ERROR_FILE_TOO_LARGE;
  }

  FILE* input_file = fdopen(fd, "rb");
  if (!input_file) {
    CLOSE(fd);
    return PLUGIN_FILE_ERROR_OPEN;
  }
  // From here on, closing input_file will automatically close fd.

  nacl::scoped_array<char> buffer(new char[bytes_to_read + 1]);
  if (buffer == NULL) {
    fclose(input_file);
    return PLUGIN_FILE_ERROR_MEM_ALLOC;
  }

  size_t total_bytes_read = 0;
  while (bytes_to_read > 0) {
    size_t bytes_this_read = fread(&buffer[total_bytes_read],
                                   sizeof(char),
                                   bytes_to_read,
                                   input_file);
    if (bytes_this_read < bytes_to_read && (feof(input_file) ||
                                            ferror(input_file))) {
      fclose(input_file);
      return PLUGIN_FILE_ERROR_READ;
    }
    total_bytes_read += bytes_this_read;
    bytes_to_read -= bytes_this_read;
  }

  fclose(input_file);
  buffer[total_bytes_read] = '\0';
  out_buf = buffer.get();
  return PLUGIN_FILE_SUCCESS;
}

}  // namespace file_utils
}  // namespace plugin

