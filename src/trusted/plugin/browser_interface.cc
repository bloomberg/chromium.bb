/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// Portable interface for browser interaction - API invariant portions.

#include "native_client/src/trusted/plugin/browser_interface.h"

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/nacl_elf.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/trusted/plugin/origin.h"

namespace plugin {

bool BrowserInterface::GetOrigin(InstanceIdentifier instance_id,
                                 nacl::string* origin) {
  nacl::string full_url;
  if (GetFullURL(instance_id, &full_url)) {
    *origin = nacl::UrlToOrigin(full_url);
    return true;
  } else {
    *origin = NACL_NO_URL;
    return false;
  }
}

bool BrowserInterface::MightBeElfExecutable(const nacl::string& filename,
                                            nacl::string* error) {
  return MightBeElfExecutable(fopen(filename.c_str(), "rb"), error);
}

bool BrowserInterface::MightBeElfExecutable(int posix_file_desc,
                                            nacl::string* error) {
  // The argument is a multi-use descriptor, so it should not be closed.
  // Make a dup to prevent that.
  int dup_file_desc = DUP(posix_file_desc);
  return MightBeElfExecutable(fdopen(dup_file_desc, "rb"), error);
}

// TODO(polina,sehr): move elf checking code to service_runtime.
bool BrowserInterface::MightBeElfExecutable(FILE* file_stream,
                                            nacl::string* error) {
  if (file_stream == NULL) {
    *error = "Load failed: cannot open local file for reading.";
    return false;
  }
  char read_buffer[EI_NIDENT];
  size_t read_amount = fread(read_buffer, sizeof read_buffer, 1, file_stream);
  fclose(file_stream);  // Also closes the file descriptor associated with it.
  if (read_amount != 1) {
    *error = "Load failed: fread should not fail.";
    return false;
  }
  return MightBeElfExecutable(read_buffer, sizeof read_buffer, error);
}

bool BrowserInterface::MightBeElfExecutable(const char* e_ident_bytes,
                                            size_t size,
                                            nacl::string* error) {
  if (size < EI_NIDENT) {
    *error = "Load failed: file too short to be an ELF executable.";
    return false;
  }
  if (strncmp(e_ident_bytes, EI_MAG0123, strlen(EI_MAG0123)) != 0) {
    // This can happen if we read a 404 error page, for example.
    *error = "Load failed: bad magic number; not an ELF executable.";
    return false;
  }
  if (e_ident_bytes[EI_ABIVERSION] != EF_NACL_ABIVERSION) {
    nacl::stringstream ss;
    ss << "Load failed: ABI version mismatch: expected " << EF_NACL_ABIVERSION
       << ", found " << (unsigned) e_ident_bytes[EI_ABIVERSION] << ".";
    *error = ss.str();
    return false;
  }
  *error = NACL_NO_ERROR;
  return true;
}

}  // namespace plugin
