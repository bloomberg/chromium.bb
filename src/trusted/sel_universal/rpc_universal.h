/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_RPC_UNIVERSAL_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_RPC_UNIVERSAL_H_

#include "native_client/src/shared/srpc/nacl_srpc.h"

#include <map>
#include <string>
#include <vector>

using std::map;
using std::string;
using std::vector;

class NaClCommandLoop;
typedef bool (*CommandHandler)(NaClCommandLoop* ncl,
                               const vector<string>& args);

class NaClCommandLoop {
 public:
  NaClCommandLoop(NaClSrpcService* service,
                  NaClSrpcChannel* channel,
                  NaClSrpcImcDescType default_socket_address);

  void SetVariable(string name, string value);
  void AddHandler(string name, CommandHandler handler);
  void AddDesc(NaClSrpcImcDescType desc, string name);
  string AddDescUniquify(NaClSrpcImcDescType desc, string prefix);
  NaClSrpcImcDescType FindDescByName(string name) const;
  NaClSrpcService* getService() const { return service_; }
  NaClSrpcChannel* getChannel() const { return channel_; }

  void StartInteractiveLoop();

 private:
  // most other handlers are not even class members, this one needs
  // to access descs_, though.
  static bool HandleDesc(NaClCommandLoop* ncl, const vector<string>& args);

  int desc_count_;
  NaClSrpcService* service_;
  NaClSrpcChannel* channel_;

  map<string, string> vars_;
  map<string, CommandHandler> handlers_;
  map<string, NaClSrpcImcDescType> descs_;
};


#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_RPC_UNIVERSAL_H_ */
