/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl testing shell
 */

#include <assert.h>
#include <string.h>
#include <sstream>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"

#include "native_client/src/include/nacl_base.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_invalid.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/sel_universal/parsing.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"

#define kMaxCommandLineLength 4096

using std::stringstream;

using nacl::DescWrapperFactory;
using nacl::DescWrapper;

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


void NaClCommandLoop::RegisterNonDeterministicOutput(string content,
                                                     string name) {
  NaClSrpcArg arg;
  // we parse and then unparse to canonicalize the string representation
  // of content
  if (!ParseArg(&arg, content, true, this)) {
    NaClLog(LOG_ERROR, "malformed argument to RegisterNonDeterministicOutput");
  }
  string data = DumpArg(&arg, this);
  nondeterministic_[data] = name;
  FreeArrayArg(&arg);
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


void NaClCommandLoop::AddUpcallRpcSecondary(string name, NaClSrpcMethod rpc) {
  if (upcall_installed_) {
    NaClLog(LOG_ERROR, "upcall service is already install - ignoring\n");
    return;
  }

  if (upcall_rpcs_secondary_.find(name) != upcall_rpcs_secondary_.end()) {
    NaClLog(LOG_ERROR, "rpc (secondary) %s already exists\n", name.c_str());
  }
  upcall_rpcs_secondary_[name] = rpc;
}


void NaClCommandLoop::DumpArgsAndResults(NaClSrpcArg* inv[],
                                         NaClSrpcArg* outv[]) {
  for (size_t j = 0; inv[j] != NULL; ++j) {
    string value = DumpArg(inv[j], this);
    if (nondeterministic_.find(value) != nondeterministic_.end()) {
      value = nondeterministic_.find(value)->second;
    }
    printf("input %d:  %s\n", (int) j, value.c_str());
  }

  for (size_t j = 0; outv[j] != NULL; ++j) {
    string value = DumpArg(outv[j], this);
    if (nondeterministic_.find(value) != nondeterministic_.end()) {
      value = nondeterministic_.find(value)->second;
    }
    printf("output %d:  %s\n", (int) j, value.c_str());
  }
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


bool NaClCommandLoop::HandleNondeterministic(NaClCommandLoop* ncl,
                                             const vector<string>& args) {
  if (args.size() != 3) {
    NaClLog(LOG_ERROR, "not the right number of args for this command\n");
    return false;
  }
  ncl->RegisterNonDeterministicOutput(args[1], args[2]);
  return true;
}


bool NaClCommandLoop::HandleEcho(NaClCommandLoop* ncl,
                                 const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  const char* delim = "";
  for (size_t i = 1; i < args.size(); ++i) {
    printf("%s%s", delim, args[i].c_str());
    delim = " ";
  }
  printf("\n");
  return true;
}

static NaClSrpcHandlerDesc* MakeDescriptorArray(
  const map<string, NaClSrpcMethod>& method_map) {
  NaClSrpcHandlerDesc* handlers =
    new NaClSrpcHandlerDesc[method_map.size() + 1];

  NaClSrpcHandlerDesc* curr = handlers;
  typedef map<string, NaClSrpcMethod>::const_iterator Ci;
  for (Ci it = method_map.begin(); it != method_map.end(); ++it) {
    curr->entry_fmt = it->first.c_str();
    curr->handler = it->second;
    ++curr;
    NaClLog(1, "adding upcall handler: %s\n",  it->first.c_str());
  }
  // terminate the handlers with a zero sentinel
  curr->entry_fmt = 0;
  curr->handler = 0;
  return handlers;
}


static void Unimplemented(NaClSrpcRpc* rpc,
                          NaClSrpcArg** inputs,
                          NaClSrpcArg** outputs,
                          NaClSrpcClosure* done) {
  UNREFERENCED_PARAMETER(inputs);
  UNREFERENCED_PARAMETER(outputs);
  UNREFERENCED_PARAMETER(done);

  const char* rpc_name;
  const char* arg_types;
  const char* ret_types;

  if (NaClSrpcServiceMethodNameAndTypes(rpc->channel->server,
                                        rpc->rpc_number,
                                        &rpc_name,
                                        &arg_types,
                                        &ret_types)) {
    NaClLog(LOG_ERROR, "cannot find signature for rpc %d\n", rpc->rpc_number);
  }

  // TODO(robertm): add full argument printing
  printf("invoking: %s (%s) -> %s\n", rpc_name, arg_types, ret_types);
}


// Makes data available to SecondaryHandlerThread.
static NaClSrpcHandlerDesc* SecondaryHandlers = 0;

void WINAPI SecondaryHandlerThread(void* desc_void) {
  DescWrapper* desc = reinterpret_cast<DescWrapper*>(desc_void);

  NaClLog(1, "secondary service thread started %p\n", desc_void);
  NaClSrpcServerLoop(desc->desc(), SecondaryHandlers, NULL);
  NaClLog(1, "secondary service thread stopped\n");
  NaClThreadExit();
}

bool NaClCommandLoop::HandleInstallUpcalls(NaClCommandLoop* ncl,
                                           const vector<string>& args) {
  if (args.size() != 3) {
    NaClLog(LOG_ERROR, "not the right number of args for this command\n");
    return false;
  }

  NaClLog(1, "installing %d primary upcall rpcs",
          (int)ncl->upcall_rpcs_.size());
  // Note, we do not care about this leak
  NaClSrpcHandlerDesc* handlers = MakeDescriptorArray(ncl->upcall_rpcs_);

  // Note, we do not care about this leak
  NaClSrpcService* service =
      reinterpret_cast<NaClSrpcService*>(malloc(sizeof *service));
  if (service == NULL || !NaClSrpcServiceHandlerCtor(service, handlers)) {
    NaClLog(LOG_ERROR, "could not create upcall service");
  }
  ncl->channel_->server = service;
  ncl->SetVariable(args[1], service->service_string);

  // NOTE: Make sure there is at least one entry.
  // weird things happen otherwise
  ncl->AddUpcallRpcSecondary("Dummy::", &Unimplemented);
  NaClLog(1, "installing %d secondary upcall rpcs",
          (int)ncl->upcall_rpcs_secondary_.size());

  SecondaryHandlers = MakeDescriptorArray(ncl->upcall_rpcs_secondary_);
  DescWrapperFactory* factory = new DescWrapperFactory();
  // NOTE: these are really NaClDescXferableDataDesc. Code was mimicked after
  //       the exisiting plugin code
  NaClLog(1, "create socket pair so that client can make pepper upcalls\n");

  DescWrapper* descs[2] = { NULL, NULL };
  if (0 != factory->MakeSocketPair(descs)) {
    NaClLog(LOG_FATAL, "cannot create socket pair\n");
  }

  NaClLog(1, "spawning secondary service thread\n");

  NaClThread thread;
  if (!NaClThreadCtor(
        &thread,
        SecondaryHandlerThread,
        descs[0],
        128 << 10)) {
    NaClLog(LOG_FATAL, "cannot create service handler thread\n");
  }

  ncl->AddDesc(descs[1]->desc(),  args[2]);

  ncl->upcall_installed_ = true;
  return true;
}


static bool HandleStrcpy(NaClCommandLoop* ncl, const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  if (args.size() < 4) {
    NaClLog(LOG_ERROR, "Insufficient arguments to 'rpc' command.\n");
    return false;
  }

  const uintptr_t start = (uintptr_t) ExtractInt64(args[1]);
  const uintptr_t offset = (uintptr_t) ExtractInt64(args[2]);
  // skip leading and trailing double quote
  string payload = args[3].substr(1, args[3].size() - 2);
  char* dst = reinterpret_cast<char*>(start + offset);
  NaClLog(1, "copy [%s] to %p\n", args[3].c_str(), dst);
  // prevent sanity warning
  strncpy(dst, payload.c_str(), payload.size() + 1);
  return true;
}


static bool HandleMemset(NaClCommandLoop* ncl, const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  if (args.size() < 5) {
    NaClLog(LOG_ERROR, "Insufficient arguments to 'rpc' command.\n");
    return false;
  }

  const uintptr_t start = (uintptr_t) ExtractInt64(args[1]);
  const uintptr_t offset = (uintptr_t) ExtractInt64(args[2]);
  const uintptr_t length = (uintptr_t) ExtractInt64(args[3]);
  const int pattern = (int)  ExtractInt32(args[4]);

  char* dst = reinterpret_cast<char*>(start + offset);
  NaClLog(1, "setting [%p, %p] to 0x%02x\n", dst, dst + length, pattern);
  memset(dst, pattern, length);
  return true;
}


static bool HandleChecksum(NaClCommandLoop* ncl, const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  if (args.size() < 4) {
    NaClLog(LOG_ERROR, "Insufficient arguments to 'rpc' command.\n");
    return false;
  }

  const uintptr_t start = (uintptr_t) ExtractInt64(args[1]);
  const uintptr_t offset = (uintptr_t) ExtractInt64(args[2]);
  const size_t length = (size_t) ExtractInt64(args[3]);
  unsigned int sum = 0;
  unsigned char* src = reinterpret_cast<unsigned char*>(start + offset);
  for (size_t i = 0; i < length; ++i) {
    sum += sum << 1;
    sum ^= src[i];
  }

  printf("CHECKSUM: 0x%08x\n", sum);
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

bool NaClCommandLoop::InvokeNexeRpc(string signature,
                                    NaClSrpcArg** in,
                                    NaClSrpcArg** out) {
  const uint32_t rpc_num = NaClSrpcServiceMethodIndex(getService(),
                                                      signature.c_str());

  if (kNaClSrpcInvalidMethodIndex == rpc_num) {
    NaClLog(LOG_ERROR, "No RPC found of signature: [%s]\n", signature.c_str());
    return false;
  }

  NaClLog(1, "Calling RPC %s (%u)\n", signature.c_str(), (unsigned) rpc_num);

  const  NaClSrpcError result = NaClSrpcInvokeV(
    getChannel(), rpc_num, in, out);
  NaClLog(1, "Result %d\n", result);

  if (NACL_SRPC_RESULT_OK != result) {
    NaClLog(LOG_ERROR, "RPC call failed: %s.\n", NaClSrpcErrorString(result));
  }
  return NACL_SRPC_RESULT_OK == result;
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

  bool show_results = true;
  size_t arg_start = 2;
  if (args[2] == "hide-results") {
    show_results = false;
    arg_start = 3;
  }
  // Build the input parameter values.
  const size_t n_in = in_out_sep - arg_start;
  NaClSrpcArg  in[NACL_SRPC_MAX_ARGS];
  NaClSrpcArg* inv[NACL_SRPC_MAX_ARGS + 1];
  NaClLog(2, "SRPC: Parsing %d input args.\n", (int)n_in);
  BuildArgVec(inv, in, n_in);
  if (!ParseArgs(inv, args, arg_start, true, ncl)) {
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

  NaClSrpcArg* empty[] = {NULL};
  printf("rpc call initiated %s\n", signature.c_str());
  ncl->DumpArgsAndResults(inv, empty);
  fflush(stdout);
  fflush(stderr);

  const bool rpc_result = ncl->InvokeNexeRpc(signature, inv, outv);

  printf("rpc call complete %s\n", signature.c_str());
  if (show_results) {
    ncl->DumpArgsAndResults(empty, outv);
  }

  if (!rpc_result)
    return false;

  // save output into variables
  for (size_t i = 0; outv[i] != 0; ++i) {
    string value = DumpArg(outv[i], ncl);
    stringstream tag;
    tag << "result" << i;
    ncl->SetVariable(tag.str(), value);
  }
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
  AddDesc(DescFromPlatformDesc(DUP(0), NACL_ABI_O_RDONLY), "stdin");
  AddDesc(DescFromPlatformDesc(DUP(1), NACL_ABI_O_WRONLY), "stdout");
  AddDesc(DescFromPlatformDesc(DUP(2), NACL_ABI_O_WRONLY), "stderr");
  if (kNaClSrpcInvalidImcDesc != default_socket_address) {
    AddDesc(default_socket_address, "module_socket_address");
  }

  // populate command handlers
  // TODO(robertm): for backward compatibility
  AddHandler("show_descriptors", HandleShowDescriptors);
  AddHandler("show_variables", HandleShowVariables);
  AddHandler("set_variable", HandleSetVariable);
  AddHandler("nondeterministic", HandleNondeterministic);
  AddHandler("install_upcalls", HandleInstallUpcalls);
  AddHandler("strcpy", HandleStrcpy);
  AddHandler("memset", HandleMemset);
  AddHandler("checksum", HandleChecksum);
  AddHandler("rpc", HandleRpc);
  AddHandler("echo", HandleEcho);
  AddHandler("help", HandleHelp);
  AddHandler("?", HandleHelp);
  AddHandler("service", HandleService);
}

NaClCommandLoop::~NaClCommandLoop() {
  NaClDescUnref(descs_["stdin"]);
  NaClDescUnref(descs_["stdout"]);
  NaClDescUnref(descs_["stderr"]);
}

// return codes:
// 0  - ok
// -1 - failure
// 1  - stop processing
int NaClCommandLoop::ProcessOneCommand(const string command) {
  vector<string> tokens;
  Tokenize(command, &tokens);

  if (tokens.size() == 0) {
    return 0;
  }

  if (tokens[0][0] == '#') {
    return 0;
  }

  if (tokens[0] == "quit") {
    return 1;
  }

  for (size_t i = 0; i < tokens.size(); ++i) {
    tokens[i] = SubstituteVars(tokens[i], this);
  }

  if (handlers_.find(tokens[0]) == handlers_.end()) {
      NaClLog(LOG_ERROR, "Unknown command [%s].\n", tokens[0].c_str());
      return -1;
  }

  if (!handlers_[tokens[0]](this, tokens)) {
    NaClLog(LOG_ERROR, "Command [%s] failed.\n", tokens[0].c_str());
    return -1;
  }

  return 0;
}

bool NaClCommandLoop::ProcessCommands(const vector<string>& commands) {
  NaClLog(1, "entering processing commands\n");
  for (size_t i = 0; i < commands.size(); ++i) {
    fflush(stdout);
    fflush(stderr);

    int result = ProcessOneCommand(commands[i]);
    if (result == 1) return true;
    if (result == -1) return false;
  }

  return true;
}


bool NaClCommandLoop::StartInteractiveLoop(bool abort_on_error) {
  int command_count = 0;

  NaClLog(1, "entering print eval loop\n");
  for (;;) {
    fflush(stdout);
    fflush(stderr);

    char buffer[kMaxCommandLineLength];

    fprintf(stderr, "%d> ", command_count);
    ++command_count;

    if (!fgets(buffer, sizeof(buffer), stdin))
      break;

    int result = ProcessOneCommand(buffer);
    if (result == 1) break;
    if (result == -1 && abort_on_error) return false;
  }
  NaClLog(1, "exiting print eval loop\n");
  return true;
}
