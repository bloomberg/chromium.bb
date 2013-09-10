/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <cstring>
#include <string>
#include <vector>

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_time.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

#include "native_client/src/trusted/sel_universal/parsing.h"
#include "native_client/src/trusted/sel_universal/pnacl_emu_handler.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"

using std::string;
using std::vector;

namespace {

int
SendDataChunk(NaClCommandLoop* ncl, nacl_abi_size_t size, const char *data) {
  const string signature = string("StreamChunk:C:");
  NaClSrpcArg in[1];
  NaClSrpcArg* inv[2];
  BuildArgVec(inv, in, 1);
  in[0].tag = NACL_SRPC_ARG_TYPE_CHAR_ARRAY;
  in[0].arrays.carr = static_cast<char*>(malloc(size));
  if (0 == in[0].arrays.carr) {
    NaClLog(LOG_ERROR, "allocation failed\n");
    FreeArrayArgs(inv);
    return -1;
  }
  in[0].u.count = size;
  memcpy(in[0].arrays.carr, data, size);

  NaClSrpcArg* outv[1];
  outv[0] = NULL;
  if (!ncl->InvokeNexeRpc(signature, inv, outv)) {
    NaClLog(LOG_ERROR, "StreamChunk failed\n");
    FreeArrayArgs(inv);
    return -1;
  }
  return 0;
}

// Stream the file to the client with a series of StreamChunk RPCs.
// Rate-limit the sending to bits_per_sec to emulate a download
bool PnaclStreamFile(NaClCommandLoop* ncl, FILE* input_file,
                            int chunk_size, int bits_per_sec) {
  nacl::scoped_array<char> data(new char[chunk_size]);
  NaClLog(LOG_INFO, "Streaming file at %d bps\n", bits_per_sec);
  uint64_t start_time = NaClGetTimeOfDayMicroseconds();
  size_t data_sent = 0;
  while (!feof(input_file) && !ferror(input_file)) {
    size_t data_read = fread(data.get(), 1, chunk_size, input_file);
    uint64_t cur_elapsed_us = NaClGetTimeOfDayMicroseconds() - start_time;
    uint64_t target_elapsed_us = static_cast<uint64_t>(data_sent + data_read)
        * 8 * 1000000 / bits_per_sec;
    if (cur_elapsed_us < target_elapsed_us) {
      uint64_t sleep_us = target_elapsed_us - cur_elapsed_us;
      struct nacl_abi_timespec req;
      struct nacl_abi_timespec rem;
      req.tv_sec = sleep_us / 1000000;
      req.tv_nsec = sleep_us % 1000000 * 1000;
      int ret = NaClNanosleep(&req, &rem);
      if (ret == -1) {
        NaClLog(LOG_ERROR, "NaClNanosleep failed");
        return false;
      }
    }
    if (SendDataChunk(ncl, static_cast<nacl_abi_size_t>(data_read),
                      data.get())) {
      // If SendDataChunkFails, just return immediately, but don't fail.
      // This will cause the final RPC to be run by the script to get the
      // error string.
      NaClLog(LOG_ERROR, "stream_file: SendDataChunk failed, but returning"
                         " without failing. Expect call to StreamEnd.");
      return true;
    }
    data_sent += data_read;
  }
  return true;
}

}  // namespace

bool HandlerPnaclFileStream(NaClCommandLoop* ncl,
                            const vector<string>& args) {
  if (args.size() != 4) {
    NaClLog(LOG_ERROR, "not enough args to file_stream\n");
    return false;
  }

  FILE* input_file = fopen(args[1].c_str(), "rb");
  if (NULL == input_file) {
    NaClLog(LOG_ERROR, "could not open input file %s\n", args[1].c_str());
    return false;
  }
  return PnaclStreamFile(ncl, input_file, strtol(args[2].c_str(), 0, 0),
                         strtol(args[3].c_str(), 0, 0));
}
