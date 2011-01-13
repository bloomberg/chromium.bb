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


bool NaClCommandLoop::HandleDesc(NaClCommandLoop* ncl,
                                 const vector<string>& args) {
  UNREFERENCED_PARAMETER(args);
  printf("Descriptors:\n");
  for (map<string, NaClSrpcImcDescType>::const_iterator it = ncl->descs_.begin();
       it != ncl->descs_.end();
       ++it) {
    printf("  %p: %s\n", static_cast<void*>(it->second), it->first.c_str());
  }
  return true;
}


static bool HandleHelp(NaClCommandLoop* ncl, const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  UNREFERENCED_PARAMETER(args);
  // TODO(sehr,robertm): we should have a syntax description option
  printf("Commands:\n");
  printf("  # <anything>\n");
  printf("    comment\n");
  printf("  descs\n");
  printf("    print the table of known descriptors (handles)\n");
  printf("  sysv\n");
  printf("    create a descriptor for an SysV shared memory (Linux only)\n");
  printf("  rpc method_name <in_args> * <out_args>\n");
  printf("    Invoke method_name.\n");
  printf("    Each in_arg is of form 'type(value)', e.g. i(42), s(\"foo\").\n");
  printf("    Each out_arg is of form 'type', e.g. i, s.\n");
  printf("  service\n");
  printf("    print the methods found by service_discovery\n");
  printf("  quit\n");
  printf("    quit the program\n");
  printf("  help\n");
  printf("    print this menu\n");
  printf("  ?\n");
  printf("    print this menu\n");
  return true;
}


static void BuildArgVec(NaClSrpcArg* argv[], NaClSrpcArg arg[], size_t count) {
  for (size_t i = 0; i < count; ++i) {
    NaClSrpcArgCtor(&arg[i]);
    argv[i] = &arg[i];
  }
  argv[count] = NULL;
}


static void DumpArgs(
  string name, const NaClSrpcArg* arg, size_t n, NaClCommandLoop *ncl) {
  printf("%s RESULTS: ", name.c_str());
  for (size_t i = 0; i < n; ++i) {
    printf("  ");
    DumpArg(&arg[i], ncl);
  }
  printf("\n");
}


// Read n arguments from the tokens array.  Returns true iff successful.
static bool ParseArgs(NaClSrpcArg* arg,
                      const vector<string>& tokens,
                      size_t start,
                      size_t n,
                      bool input,
                      NaClCommandLoop* ncl) {
  for (size_t i = 0; i < n; ++i) {
    if (!ParseArg(&arg[i], tokens[start + i], input, ncl)) {
      /* TODO(sehr): reclaim memory here on failure. */
      return false;
    }
  }
  return true;
}


// Best effort
static void FreeArrayArgs(NaClSrpcArg arg[], size_t count) {
  for (size_t i = 0; i < count; ++i) {
    switch (arg[i].tag) {
     case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      free(arg[i].arrays.carr);
      break;
     case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      free(arg[i].arrays.darr);
      break;
     case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      free(arg[i].arrays.iarr);
      break;
     case NACL_SRPC_ARG_TYPE_LONG_ARRAY:
      free(arg[i].arrays.larr);
      break;
     case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
      FreeArrayArgs(arg[i].arrays.varr, arg[i].u.count);
      break;
     case NACL_SRPC_ARG_TYPE_INVALID:
     case NACL_SRPC_ARG_TYPE_BOOL:
     case NACL_SRPC_ARG_TYPE_DOUBLE:
     case NACL_SRPC_ARG_TYPE_HANDLE:
     case NACL_SRPC_ARG_TYPE_INT:
     case NACL_SRPC_ARG_TYPE_LONG:
     case NACL_SRPC_ARG_TYPE_STRING:
     case NACL_SRPC_ARG_TYPE_OBJECT:
     default:
      break;
    }
  }
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
  NaClLog(1, "SRPC: Parsing %d input args.\n", (int)n_in);
  BuildArgVec(inv, in, n_in);
  if (!ParseArgs(in, args, 2, n_in, true, ncl)) {
    NaClLog(LOG_ERROR, "Bad input args for RPC.\n");
    return false;
  }

  // Build the output (rpc return) values.
  const size_t n_out =  args.size() - in_out_sep - 1;
  NaClSrpcArg  out[NACL_SRPC_MAX_ARGS];
  NaClSrpcArg* outv[NACL_SRPC_MAX_ARGS + 1];
  NaClLog(1, "SRPC: Parsing %d output args.\n", (int)n_out);
  BuildArgVec(outv, out, n_out);
  if (!ParseArgs(out, args, in_out_sep + 1, n_out, false, ncl)) {
    NaClLog(LOG_ERROR, "Bad output args for RPC.\n");
    return false;
  }

  const string signature = BuildSignature(args[1], inv, outv);

  const uint32_t rpc_num = NaClSrpcServiceMethodIndex(ncl->getService(),
                                                      signature.c_str());

  if (kNaClSrpcInvalidMethodIndex == rpc_num) {
    NaClLog(LOG_ERROR, "No RPC found of that name/signature.\n");
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

  DumpArgs(args[1], outv[0], n_out, ncl);

  /* Free the storage allocated for array valued parameters and returns. */
  FreeArrayArgs(in, n_in);
  FreeArrayArgs(out, n_out);
  return true;
}


static bool HandleService(NaClCommandLoop* ncl, const vector<string>& args) {
  UNREFERENCED_PARAMETER(args);
  // TODO(robertm): move this library call into sel_universal
  NaClSrpcServicePrint(ncl->getService());
  return true;
}


static void UpcallString(NaClSrpcRpc* rpc,
                         NaClSrpcArg** ins,
                         NaClSrpcArg** outs,
                         NaClSrpcClosure* done) {
  UNREFERENCED_PARAMETER(outs);
  printf("UpcallString: called with '%s'\n", ins[0]->arrays.str);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


NaClCommandLoop::NaClCommandLoop(NaClSrpcService* service,
                                 NaClSrpcChannel* channel,
                                 NaClSrpcImcDescType default_socket_address) {

  desc_count_ = 0;
  channel_ = channel;
  service_ = service;

  // Add a simple upcall service to the channel (if any)
  if (NULL != channel) {
    NaClLog(1, "adding upcall service\n");
    static const NaClSrpcHandlerDesc upcall_handlers[] = {
      { "upcall_string:s:", UpcallString },
      { NULL, NULL }
    };
    NaClSrpcService* s = static_cast<NaClSrpcService*>(malloc(sizeof(*service)));
    if (s == 0) {
      NaClLog(LOG_FATAL, "could not allocate service");
    }

    if (!NaClSrpcServiceHandlerCtor(s, upcall_handlers)) {
      NaClLog(LOG_FATAL, "could not create service");
      free(s);
    }
    channel_->server = s;
  }

  // populate descriptors
  AddDesc((NaClDesc*) NaClDescInvalidMake(), "invalid");
  AddDesc(DescFromPlatformDesc(0, NACL_ABI_O_RDONLY), "stdin");
  AddDesc(DescFromPlatformDesc(1, NACL_ABI_O_WRONLY), "stdout");
  AddDesc(DescFromPlatformDesc(2, NACL_ABI_O_WRONLY), "stderr");
  if (kNaClSrpcInvalidImcDesc != default_socket_address) {
    AddDesc(default_socket_address, "module_socket_address");
  }

  // populate command handlers
  AddHandler("descs", HandleDesc);
  AddHandler("rpc", HandleRpc);
  AddHandler("help", HandleHelp);
  AddHandler("?", HandleHelp);
  AddHandler("service", HandleService);
#if NACL_LINUX
  extern bool HandleSysv(NaClCommandLoop* ncl, const vector<string>& args);
  AddHandler("sysv", HandleSysv);
#endif  /* NACL_LINUX */
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
      NaClLog(LOG_ERROR, "bad line\n");
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
