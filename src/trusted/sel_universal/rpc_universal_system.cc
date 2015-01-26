/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fcntl.h>
#include <string.h>

#include <map>
#include <string>
#include <sstream>

using std::stringstream;
#include "native_client/src/include/build_config.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_sync_socket.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/sel_universal/parsing.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"


bool HandlerSyncSocketCreate(NaClCommandLoop* ncl,
                             const vector<string>& args) {
  if (args.size() < 3) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }

  NaClHandle handles[2] = {NACL_INVALID_HANDLE, NACL_INVALID_HANDLE};
  if (NaClSocketPair(handles) != 0) {
    return false;
  }

  nacl::DescWrapperFactory factory;
  nacl::DescWrapper* desc1 = factory.ImportSyncSocketHandle(handles[0]);
  ncl->AddDesc(desc1->desc(), args[1]);

  nacl::DescWrapper* desc2 = factory.ImportSyncSocketHandle(handles[1]);
  ncl->AddDesc(desc2->desc(), args[2]);
  return true;
}

bool HandlerSyncSocketWrite(NaClCommandLoop* ncl,
                            const vector<string>& args) {
  if (args.size() < 3) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }

  NaClDesc* raw_desc = ExtractDesc(args[1], ncl);
  if (raw_desc == NULL) {
    NaClLog(LOG_ERROR, "cannot find desciptor %s\n", args[1].c_str());
    return false;
  }

  const int value = ExtractInt32(args[2]);
  nacl::DescWrapperFactory factory;
  // TODO(robertm): eliminate use of NaClDesc in sel_universal and standardize
  //       on DescWrapper to eliminate the memory leak here
  factory.MakeGeneric(raw_desc)->Write(&value, sizeof value);
  return true;
}


// create a descriptor representing a readonly file
bool HandlerReadonlyFile(NaClCommandLoop* ncl, const vector<string>& args) {
  if (args.size() < 3) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }

  nacl::DescWrapperFactory factory;
  nacl::DescWrapper* desc = factory.OpenHostFile(args[2].c_str(),
                                                 NACL_ABI_O_RDONLY, 0);
  if (NULL == desc) {
    NaClLog(LOG_ERROR, "cound not create file desc for %s\n", args[2].c_str());
    return false;
  }
  ncl->AddDesc(desc->desc(), args[1]);
  return true;
}

// create a descriptor representing a read-write file.
// Used for temporary files created by the pnacl translator.
bool HandlerReadwriteFile(NaClCommandLoop* ncl, const vector<string>& args) {
  if (args.size() < 3) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }

  nacl::DescWrapperFactory factory;
  nacl::DescWrapper* desc =
      factory.OpenHostFile(args[2].c_str(),
                           NACL_ABI_O_RDWR | NACL_ABI_O_CREAT, 0666);
  if (NULL == desc) {
    NaClLog(LOG_ERROR, "cound not create file desc for %s\n", args[2].c_str());
    return false;
  }
  ncl->AddDesc(desc->desc(), args[1]);
  return true;
}

// sleep for a given number of seconds
bool HandlerSleep(NaClCommandLoop* ncl, const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  if (args.size() < 2) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }
  const int secs = ExtractInt32(args[1]);
#if (NACL_LINUX || NACL_OSX)
  sleep(secs);
#elif NACL_WINDOWS
  Sleep(secs * 1000);
#else
#error "Please specify platform as NACL_LINUX, NACL_OSX or NACL_WINDOWS"
#endif
  return true;
}

// save a memory region to a file
bool HandlerSaveToFile(NaClCommandLoop* ncl, const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  if (args.size() < 5) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }

  const char* filename = args[1].c_str();
  const char* start = reinterpret_cast<char*>(ExtractInt64(args[2]));
  const int offset = ExtractInt32(args[3]);
  const int size = ExtractInt32(args[4]);

  NaClLog(1, "opening %s\n", filename);
  FILE* fp = fopen(filename, "wb");
  if (fp == NULL) {
     NaClLog(LOG_ERROR, "cannot open %s\n", filename);
     return false;
  }


  NaClLog(1, "writing %d bytes from %p\n", (int) size, start + offset);
  const size_t n = fwrite(start + offset, 1, size, fp);
  if (static_cast<int>(n) != size) {
    NaClLog(LOG_ERROR, "wrote %d bytes, expected %d\n",
            static_cast<int>(n), size);
    fclose(fp);
    return false;
  }
  fclose(fp);
  return true;
}

// load file into memory region
bool HandlerLoadFromFile(NaClCommandLoop* ncl, const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  if (args.size() < 5) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }

  const char* filename = args[1].c_str();
  char* start = reinterpret_cast<char*>(ExtractInt64(args[2]));
  const int offset = ExtractInt32(args[3]);
  const int size = ExtractInt32(args[4]);

  NaClLog(1, "opening %s\n", filename);
  FILE* fp = fopen(filename, "rb");
  if (fp == NULL) {
     NaClLog(LOG_ERROR, "cannot open %s\n", filename);
     return false;
  }

  NaClLog(1, "loading %d bytes to %p\n", (int) size, start + offset);
  const size_t n = fread(start + offset, 1, size, fp);
  if (static_cast<int>(n) != size) {
    NaClLog(LOG_ERROR, "read %d bytes, expected %d\n",
            static_cast<int>(n), size);
    fclose(fp);
    return false;
  }
  fclose(fp);
  return true;
}

// Determine filesize and write it into a variable
bool HandlerFileSize(NaClCommandLoop* ncl, const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  if (args.size() < 3) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }

  const char* filename = args[1].c_str();
  FILE* fp = fopen(filename, "rb");
  if (fp == NULL) {
    NaClLog(LOG_ERROR, "cannot open %s\n", filename);
    return false;
  }
  fseek(fp, 0, SEEK_END);
  int size = static_cast<int>(ftell(fp));
  fclose(fp);

  NaClLog(1, "filesize is %d\n", size);

  stringstream str;
  str << size;
  ncl->SetVariable(args[2], str.str());
  return true;
}
