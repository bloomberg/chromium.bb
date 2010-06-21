/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/trusted/plugin/npapi/npinstance.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace nacl {

void OpenMacFile(NPStream* stream,
                 const char* filename,
                 NPInstance* module) {
  // This ugliness is necessary due to the fact that Safari on Mac returns
  // a pathname in "Mac" format, rather than a unix pathname.  To use the
  // resulting name requires conversion, which is done by a couple of Mac
  // library routines.
  if (filename && filename[0] != '/') {
    // The filename we were given is a "classic" pathname, which needs
    // to be converted to a posix pathname.
    Boolean got_posix_name = FALSE;
    CFStringRef cf_hfs_filename =
        CFStringCreateWithCString(NULL, filename, kCFStringEncodingMacRoman);
    if (cf_hfs_filename) {
      CFURLRef cf_url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                                      cf_hfs_filename,
                                                      kCFURLHFSPathStyle,
                                                      false);
      if (cf_url) {
        CFStringRef cf_posix_filename =
            CFURLCopyFileSystemPath(cf_url, kCFURLPOSIXPathStyle);
        if (cf_posix_filename) {
          CFIndex len =
              CFStringGetMaximumSizeOfFileSystemRepresentation(
                  cf_posix_filename);
          if (len > 0) {
            char* posix_filename =
                static_cast<char*>(malloc(sizeof(posix_filename[0]) * len));
            if (posix_filename) {
              got_posix_name =
                  CFStringGetFileSystemRepresentation(cf_posix_filename,
                                                      posix_filename,
                                                      len);
              if (got_posix_name) {
                module->StreamAsFile(stream, posix_filename);
                // Safari on OS X apparently wants the NPP_StreamAsFile
                // call to delete the file object after processing.
                // This was discovered in investigations by Shiki.
                FSRef ref;
                Boolean is_dir;
                if (FSPathMakeRef(reinterpret_cast<UInt8*>(posix_filename),
                                  &ref,
                                  &is_dir) == noErr) {
                  FSDeleteObject(&ref);
                }
              }
              free(posix_filename);
            }
          }
          CFRelease(cf_posix_filename);
        }
        CFRelease(cf_url);
      }
      CFRelease(cf_hfs_filename);
    }
    if (got_posix_name) {
      // If got_posix_name was true than we succesfully created
      // our posix path and called StreamAsFile above, so we
      // can exit without falling through to the case below.
      return;
    }
    filename = NULL;
  }
  module->StreamAsFile(stream, filename);
}

}  // namespace nacl
