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
  ~NaClCommandLoop();

  void SetVariable(string name, string value);
  string GetVariable(string name) const;
  void RegisterNonDeterministicOutput(string content, string name);
  void AddHandler(string name, CommandHandler handler);
  void AddDesc(NaClDesc* desc, string name);
  void AddUpcallRpc(string signature, NaClSrpcMethod rpc);
  void AddUpcallRpcSecondary(string signature, NaClSrpcMethod rpc);
  string AddDescUniquify(NaClDesc* desc, string prefix);
  NaClSrpcImcDescType FindDescByName(string name) const;
  NaClSrpcService* getService() const { return service_; }
  NaClSrpcChannel* getChannel() const { return channel_; }
  void DumpArgsAndResults(NaClSrpcArg* inv[], NaClSrpcArg* outv[]);
  bool InvokeNexeRpc(string signature, NaClSrpcArg** in, NaClSrpcArg** out);

  int ProcessOneCommand(const string command);
  bool StartInteractiveLoop(bool abort_on_error);
  bool ProcessCommands(const vector<string>& commands);

 private:
  // Note, most handlers are not even class members, the ones below
  // need to access to certain private class members
  static bool HandleShowDescriptors(NaClCommandLoop* ncl,
                                    const vector<string>& args);
  static bool HandleShowVariables(NaClCommandLoop* ncl,
                                  const vector<string>& args);
  static bool HandleSetVariable(NaClCommandLoop* ncl,
                                  const vector<string>& args);
  static bool HandleInstallUpcalls(NaClCommandLoop* ncl,
                                   const vector<string>& args);
  static bool HandleNondeterministic(NaClCommandLoop* ncl,
                                     const vector<string>& args);
  static bool HandleEcho(NaClCommandLoop* ncl,
                         const vector<string>& args);
  static bool HandleShutdown(NaClCommandLoop* nacl,
                             const vector<string>& args);

  int desc_count_;
  NaClSrpcService* service_;
  NaClSrpcChannel* channel_;

  // map names to content
  map<string, string> vars_;
  // map non-determinisic content into deterministic names
  map<string, string> nondeterministic_;
  // map names to descriptors
  map<string, NaClSrpcImcDescType> descs_;
  map<string, CommandHandler> handlers_;
  map<string, NaClSrpcMethod> upcall_rpcs_;
  map<string, NaClSrpcMethod> upcall_rpcs_secondary_;
  bool upcall_installed_;
};

// possibly platform dependent stuff (c.f. rpc_universal_system.cc)
bool HandlerSleep(NaClCommandLoop* ncl, const vector<string>& args);
bool HandlerReadonlyFile(NaClCommandLoop* ncl, const vector<string>& args);
bool HandlerReadwriteFile(NaClCommandLoop* ncl, const vector<string>& args);
bool HandlerReadwriteFileQuota(NaClCommandLoop* ncl,
                               const vector<string>& args);
bool HandlerShmem(NaClCommandLoop* ncl, const vector<string>& args);
bool HandlerMap(NaClCommandLoop* ncl, const vector<string>& args);
bool HandlerSaveToFile(NaClCommandLoop* ncl, const vector<string>& args);
bool HandlerLoadFromFile(NaClCommandLoop* ncl, const vector<string>& args);
bool HandlerFileSize(NaClCommandLoop* ncl, const vector<string>& args);
bool HandlerSyncSocketCreate(NaClCommandLoop* ncl,
                             const vector<string>& args);
bool HandlerSyncSocketWrite(NaClCommandLoop* ncl,
                            const vector<string>& args);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_RPC_UNIVERSAL_H_ */
