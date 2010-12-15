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
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/origin.h"

namespace plugin {

namespace {

bool ElfHeaderLooksValid(const char* e_ident_bytes,
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

}  // namespace

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

bool BrowserInterface::MightBeElfExecutable(nacl::DescWrapper* wrapper,
                                            nacl::string* error) {
  if (wrapper == NULL) {
    *error = "Load failed: bad descriptor for reading.";
    return false;
  }
  if (wrapper->type_tag() == NACL_DESC_SHM) {
    void* buf;
    size_t size;
    if (0 != wrapper->Map(&buf, &size)) {
      *error = "Load failed: map should not fail.";
      return false;
    }
    char* header = reinterpret_cast<char*>(buf);
    bool might_be_elf = ElfHeaderLooksValid(header, size, error);
    if (0 != wrapper->Unmap(buf, size)) {
      *error = "Load failed: unmap should not fail.";
      return false;
    }
    return might_be_elf;
  } else if (wrapper->type_tag() == NACL_DESC_HOST_IO) {
    static int const kAbiHeaderSize = sizeof(Elf_Ehdr);
    char elf_hdr[kAbiHeaderSize];
    if (kAbiHeaderSize > wrapper->Read(elf_hdr, sizeof elf_hdr)) {
      *error = "Load failed: read should not fail.";
      return false;
    }
    return ElfHeaderLooksValid(elf_hdr, kAbiHeaderSize, error);
  } else {
    NACL_NOTREACHED();
  }
}

}  // namespace plugin
