/*
 * Copyright (c) 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl testing shell
 */

#include <assert.h>
#include <string.h>
#include <sstream>

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_invalid.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/sel_universal/parsing.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"

#define kMaxCommandLineLength 4096

using std::stringstream;

static NaClSrpcImcDescType DescFromPlatformDesc(int fd, int mode) {
  return
      (NaClSrpcImcDescType) NaClDescIoDescMake(NaClHostDescPosixMake(fd, mode));
}


void NaClCommandLoop::AddDesc(NaClSrpcImcDescType desc, string name) {
  NaClLog(1, "adding descriptor %p -> %s\n",
          static_cast<void*>(desc), name.c_str());
  descs_[name] = desc;
}


NaClSrpcImcDescType NaClCommandLoop::FindDescByName(string name) const {
  if (descs_.find(name) == descs_.end()) {
    NaClLog(LOG_ERROR, "could not find desc: %s\n", name.c_str());
    return kNaClSrpcInvalidImcDesc;
  } else {
    // NOTE: descs_[name] is not const
    return descs_.find(name)->second;
  }
}


string NaClCommandLoop::AddDescUniquify(NaClSrpcImcDescType desc,
                                        string prefix) {
  // check whether we have given this descriptor a name already
  for (map<string, NaClSrpcImcDescType>::iterator it = descs_.begin();
       it != descs_.end();
       ++it) {
    if (it->second == desc) return it->first;
  }

  while (1) {
    stringstream sstr;
    sstr << prefix << "_" << desc_count_;
    ++desc_count_;
    string unique = sstr.str();
    if (descs_.find(unique) == descs_.end()) {
      AddDesc(desc, unique);
      return unique;
    }
  }
}


void NaClCommandLoop::AddHandler(string name, CommandHandler handler) {
  if (handlers_.find(name) != handlers_.end()) {
    NaClLog(LOG_ERROR, "handler %s already exists\n", name.c_str());
  }
  handlers_[name] = handler;
}


void NaClCommandLoop::SetVariable(string name, string value) {
  vars_[name] = value;
}


string NaClCommandLoop::GetVariable(string name) const {
  if (vars_.find(name) == vars_.end()) {
    NaClLog(LOG_ERROR, "unknown variable %s\n", name.c_str());
    return "";
  }
  // NOTE: vars_[name] is not const
  return vars_.find(name)->second;
}


void NaClCommandLoop::AddUpcallRpc(string name, NaClSrpcMethod rpc) {
  if (upcall_installed_) {
    NaClLog(LOG_ERROR, "upcall service is already install - ignoring\n");
    return;
  }

  if (upcall_rpcs_.find(name) != upcall_rpcs_.end()) {
    NaClLog(LOG_ERROR, "rpc %s already exists\n", name.c_str());
  }
  upcall_rpcs_[name] = rpc;
}


static string BuildSignature(string name, NaClSrpcArg** in, NaClSrpcArg** out) {
  string result = name;

  result.push_back(':');
  for (int i = 0; i < NACL_SRPC_MAX_ARGS; ++i) {
    if (NULL == in[i]) {
      break;
    }
    result.push_back(in[i]->tag);
  }

  result.push_back(':');
  for (int i = 0; i < NACL_SRPC_MAX_ARGS; ++i) {
    if (NULL == out[i]) {
      break;
    }
    result.push_back(out[i]->tag);
  }
  return result;
}


bool NaClCommandLoop::HandleShowDescriptors(NaClCommandLoop* ncl,
                                            const vector<string>& args) {
  if (args.size() != 1) {
    NaClLog(LOG_ERROR, "not the right number of args for this command\n");
    return false;
  }
  printf("Descriptors:\n");
  typedef map<string, NaClSrpcImcDescType>::const_iterator Ci;
  for (Ci it = ncl->descs_.begin(); it != ncl->descs_.end(); ++it) {
    printf("  %p: %s\n", static_cast<void*>(it->second), it->first.c_str());
  }
  return true;
}


bool NaClCommandLoop::HandleShowVariables(NaClCommandLoop* ncl,
                                          const vector<string>& args) {
  if (args.size() != 1) {
    NaClLog(LOG_ERROR, "not the right number of args for this command\n");
    return false;
  }
  printf("Variables:\n");
  typedef map<string, string>::const_iterator Ci;
  for (Ci it = ncl->vars_.begin(); it != ncl->vars_.end(); ++it) {
    printf("  %s: [%s]\n", it->first.c_str(), it->second.c_str());
  }
  return true;
}


bool NaClCommandLoop::HandleSetVariable(NaClCommandLoop* ncl,
                                        const vector<string>& args) {
  if (args.size() != 3) {
    NaClLog(LOG_ERROR, "not the right number of args for this command\n");
    return false;
  }
  ncl->SetVariable(args[1], args[2]);
  return true;
}


bool NaClCommandLoop::HandleEcho(NaClCommandLoop* ncl,
                                 const vector<string>& args) {
  for (size_t i = 1; i < args.size(); ++i) {
    printf("%s", SubstituteVars(args[i], ncl).c_str());
  }
  printf("\n");
  return true;
}


bool NaClCommandLoop::HandleInstallUpcalls(NaClCommandLoop* ncl,
                                           const vector<string>& args) {
  if (args.size() != 2) {
    NaClLog(LOG_ERROR, "not the right number of args for this command\n");
    return false;
  }

  NaClLog(1, "installing %d upcall rpcs", (int)ncl->upcall_rpcs_.size());
  // Note, we do not care about this leak
  NaClSrpcHandlerDesc* handlers =
    new NaClSrpcHandlerDesc[ncl->upcall_rpcs_.size() + 1];

  NaClSrpcHandlerDesc* curr = handlers;
  typedef map<string, NaClSrpcMethod>::const_iterator Ci;
  for (Ci it = ncl->upcall_rpcs_.begin(); it != ncl->upcall_rpcs_.end(); ++it) {
    curr->entry_fmt = it->first.c_str();
    curr->handler = it->second;
    ++curr;
    NaClLog(1, "adding upcall handler: %s\n",  it->first.c_str());
  }
  // terminate the handlers with a zero sentinel
  curr->entry_fmt = 0;
  curr->handler = 0;

  // Note, we do not care about this leak
  NaClSrpcService* service = new NaClSrpcService;
  if (!NaClSrpcServiceHandlerCtor(service, handlers)) {
    NaClLog(LOG_ERROR, "could not create upcall service");
  }
  ncl->channel_->server = service;
  ncl->SetVariable(args[1], service->service_string);
  ncl->upcall_installed_ = true;
  return true;
}


static bool HandleHelp(NaClCommandLoop* ncl, const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  UNREFERENCED_PARAMETER(args);
  // TODO(sehr,robertm): we should have a syntax description arg
  //                     in the handler install call.
  printf("Commands:\n");
  printf("  # <anything>\n");
  printf("    comment\n");
  printf("  show_descriptors\n");
  printf("    print the table of known descriptors (handles)\n");
  printf("  show_variables\n");
  printf("    print the table of variables and their values\n");
  printf("  set_variable <name> <value>\n");
  printf("    set variable to the given value\n");
  printf("  sysv\n");
  printf("    create a descriptor for an SysV shared memory (Linux only)\n");
  printf("  rpc method_name <in_args> * <out_args>\n");
  printf("    Invoke method_name.\n");
  printf("    Each in_arg is of form 'type(value)', e.g. i(42), s(\"foo\").\n");
  printf("    Each out_arg is of form 'type', e.g. i, s.\n");
  printf("  service\n");
  printf("    print the methods found by service_discovery\n");
  printf("  install_upcalls <name>\n");
  printf("    install upcalls and set variable name to the service_string\n");
  printf("  echo <args>*\n");
  printf("    print rest of line - performs variable substitution\n");
  printf("  quit\n");
  printf("    quit the program\n");
  printf("  help\n");
  printf("    print this menu\n");
  printf("  ?\n");
  printf("    print this menu\n");
  return true;
}

static bool HandleRpc(NaClCommandLoop* ncl, const vector<string>& args) {
  // we need two args at start and the "*" in out separator
  if (args.size() < 3) {
    NaClLog(LOG_ERROR, "Insufficient arguments to 'rpc' command.\n");
    return false;
  }

  // TODO(robertm): use stl find
  size_t in_out_sep;
  for (in_out_sep = 2; in_out_sep < args.size(); ++in_out_sep) {
    if (args[in_out_sep] ==  "*")
      break;
  }

  if (in_out_sep == args.size()) {
    NaClLog(LOG_ERROR, "Missing input/output argument separator\n");
    return false;
  }

  // Build the input parameter values.
  const size_t n_in = in_out_sep - 2;
  NaClSrpcArg  in[NACL_SRPC_MAX_ARGS];
  NaClSrpcArg* inv[NACL_SRPC_MAX_ARGS + 1];
  NaClLog(2, "SRPC: Parsing %d input args.\n", (int)n_in);
  BuildArgVec(inv, in, n_in);
  if (!ParseArgs(inv, args, 2, true, ncl)) {
    // TODO(sehr): reclaim memory here on failure.
    NaClLog(LOG_ERROR, "Bad input args for RPC.\n");
    return false;
  }

  // Build the output (rpc return) values.
  const size_t n_out =  args.size() - in_out_sep - 1;
  NaClSrpcArg  out[NACL_SRPC_MAX_ARGS];
  NaClSrpcArg* outv[NACL_SRPC_MAX_ARGS + 1];
  NaClLog(2, "SRPC: Parsing %d output args.\n", (int)n_out);
  BuildArgVec(outv, out, n_out);
  if (!ParseArgs(outv, args, in_out_sep + 1, false, ncl)) {
    // TODO(sehr): reclaim memory here on failure.
    NaClLog(LOG_ERROR, "Bad output args for RPC.\n");
    return false;
  }

  const string signature = BuildSignature(args[1], inv, outv);

  const uint32_t rpc_num = NaClSrpcServiceMethodIndex(ncl->getService(),
                                                      signature.c_str());

  if (kNaClSrpcInvalidMethodIndex == rpc_num) {
    NaClLog(LOG_ERROR, "No RPC found of signature: [%s]\n", signature.c_str());
    return false;
  }

  NaClLog(1, "Calling RPC %s (#%"NACL_PRIu32")...\n", args[1].c_str(), rpc_num);
  const  NaClSrpcError result = NaClSrpcInvokeV(
    ncl->getChannel(), rpc_num, inv, outv);
  NaClLog(1, "Result %d\n", result);

  if (NACL_SRPC_RESULT_OK != result) {
    NaClLog(LOG_ERROR, "RPC call failed: %s.\n", NaClSrpcErrorString(result));
    return false;
  }

  printf("%s RESULTS: ", args[1].c_str());
  DumpArgs(outv, ncl);
  printf("\n");

  /* Free the storage allocated for array valued parameters and returns. */
  FreeArrayArgs(inv);
  FreeArrayArgs(outv);
  return true;
}


static bool HandleService(NaClCommandLoop* ncl, const vector<string>& args) {
  UNREFERENCED_PARAMETER(args);
  // TODO(robertm): move this library call into sel_universal
  NaClSrpcServicePrint(ncl->getService());
  return true;
}


NaClCommandLoop::NaClCommandLoop(NaClSrpcService* service,
                                 NaClSrpcChannel* channel,
                                 NaClSrpcImcDescType default_socket_address) {
  upcall_installed_ = false;
  desc_count_ = 0;
  channel_ = channel;
  service_ = service;
  // populate descriptors
  AddDesc((NaClDesc*) NaClDescInvalidMake(), "invalid");
  AddDesc(DescFromPlatformDesc(0, NACL_ABI_O_RDONLY), "stdin");
  AddDesc(DescFromPlatformDesc(1, NACL_ABI_O_WRONLY), "stdout");
  AddDesc(DescFromPlatformDesc(2, NACL_ABI_O_WRONLY), "stderr");
  if (kNaClSrpcInvalidImcDesc != default_socket_address) {
    AddDesc(default_socket_address, "module_socket_address");
  }

  // populate command handlers
  // TODO(robertm): for backward compatibility
  AddHandler("descs", HandleShowDescriptors);
  AddHandler("show_descriptors", HandleShowDescriptors);
  AddHandler("show_variables", HandleShowVariables);
  AddHandler("set_variable", HandleSetVariable);
  AddHandler("install_upcalls", HandleInstallUpcalls);
  AddHandler("rpc", HandleRpc);
  AddHandler("echo", HandleEcho);
  AddHandler("help", HandleHelp);
  AddHandler("?", HandleHelp);
  AddHandler("service", HandleService);
}

void NaClCommandLoop::StartInteractiveLoop() {
  int command_count = 0;
  vector<string> tokens;

  NaClLog(1, "entering print eval loop\n");
  for (;;) {
    char buffer[kMaxCommandLineLength];

    fprintf(stderr, "%d> ", command_count);
    ++command_count;

    if (!fgets(buffer, sizeof(buffer), stdin))
      break;

    tokens.clear();
    Tokenize(buffer, &tokens);

    if (tokens.size() == 0) {
      continue;
    }

    if (tokens[0][0] == '#') {
      continue;
    }

    if (tokens[0] == "quit") {
      break;
    }

    if (handlers_.find(tokens[0])  == handlers_.end()) {
      NaClLog(LOG_ERROR, "Unknown command `%s'.\n", tokens[0].c_str());
      continue;
    }

    handlers_[tokens[0]](this, tokens);
  }
  NaClLog(1, "exiting print eval loop\n");
}
