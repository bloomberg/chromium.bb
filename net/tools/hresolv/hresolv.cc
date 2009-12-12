// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// hrseolv is a command line utility which runs the HostResolver in the
// Chromium network stack.
//
// The user specifies the hosts to lookup and when to look them up.
// The hosts must be specified in order.
// The hosts can be contained in a file or on the command line. If no
// time is specified, the resolv is assumed to be the same time as the
// previous host - which is an offset of 0 for the very first host.
//
// The user can also control whether the lookups happen asynchronously
// or synchronously by specifying --async on the command line.
//
// Future ideas:
//   Specify whether the lookup is speculative.
//   Interleave synchronous and asynchronous lookups.
//   Specify the address family.

#include <stdio.h>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/condition_variable.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/host_resolver_impl.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/sys_addrinfo.h"

struct FlagName {
  int flag;
  const char* name;
};

static const FlagName kAddrinfoFlagNames[] = {
  {AI_PASSIVE, "AI_PASSIVE"},
  {AI_CANONNAME, "AI_CANONNAME"},
  {AI_NUMERICHOST, "AI_NUMERICHOST"},
  {AI_V4MAPPED, "AI_V4MAPPED"},
  {AI_ALL, "AI_ALL"},
  {AI_ADDRCONFIG, "AI_ADDRCONFIG"},
#if defined(OS_LINUX) || defined(OS_WIN)
  {AI_NUMERICSERV, "AI_NUMERICSERV"},
#endif
};

std::string FormatAddrinfoFlags(int ai_flags) {
  std::string flag_names;
  for (unsigned int i = 0; i < arraysize(kAddrinfoFlagNames); ++i) {
    const FlagName& flag_name = kAddrinfoFlagNames[i];
    if (ai_flags & flag_name.flag) {
      ai_flags &= ~flag_name.flag;
      if (!flag_names.empty()) {
        flag_names += "|";
      }
      flag_names += flag_name.name;
    }
  }
  if (ai_flags) {
   if (!flag_names.empty()) {
     flag_names += "|";
   }
   flag_names += StringPrintf("0x%x", ai_flags);
  }
  return flag_names;
}

const char* GetNameOfFlag(const FlagName* flag_names,
                          unsigned int num_flag_names,
                          int flag) {
  for (unsigned int i = 0; i < num_flag_names; ++i) {
    const FlagName& flag_name = flag_names[i];
    if (flag_name.flag == flag) {
      return flag_name.name;
    }
  }
  return "UNKNOWN";
}

static const FlagName kFamilyFlagNames[] = {
  {AF_UNSPEC, "AF_UNSPEC"},
  {AF_INET, "AF_INET"},
  {AF_INET6, "AF_INET6"},
};

const char* FormatAddrinfoFamily(int ai_family) {
  return GetNameOfFlag(kFamilyFlagNames,
                       arraysize(kFamilyFlagNames),
                       ai_family);
}

static const FlagName kSocktypeFlagNames[] = {
  {SOCK_STREAM, "SOCK_STREAM"},
  {SOCK_DGRAM, "SOCK_DGRAM"},
  {SOCK_RAW, "SOCK_RAW"},
};

const char* FormatAddrinfoSocktype(int ai_socktype) {
  return GetNameOfFlag(kSocktypeFlagNames,
                       arraysize(kSocktypeFlagNames),
                       ai_socktype);
}

static const FlagName kProtocolFlagNames[] = {
  {IPPROTO_TCP, "IPPROTO_TCP"},
  {IPPROTO_UDP, "IPPROTO_UDP"},
};

const char* FormatAddrinfoProtocol(int ai_protocol) {
  return GetNameOfFlag(kProtocolFlagNames,
                       arraysize(kProtocolFlagNames),
                       ai_protocol);
}

std::string FormatAddrinfoDetails(const struct addrinfo& ai,
                                  const char* indent) {
  std::string ai_flags = FormatAddrinfoFlags(ai.ai_flags);
  const char* ai_family = FormatAddrinfoFamily(ai.ai_family);
  const char* ai_socktype = FormatAddrinfoSocktype(ai.ai_socktype);
  const char* ai_protocol = FormatAddrinfoProtocol(ai.ai_protocol);
  std::string ai_addr = net::NetAddressToString(&ai);
  std::string ai_canonname;
  if (ai.ai_canonname) {
    ai_canonname = StringPrintf("%s  ai_canonname: %s\n",
                                indent,
                                ai.ai_canonname);
  }
  return StringPrintf("%saddrinfo {\n"
                      "%s  ai_flags: %s\n"
                      "%s  ai_family: %s\n"
                      "%s  ai_socktype: %s\n"
                      "%s  ai_protocol: %s\n"
                      "%s  ai_addrlen: %d\n"
                      "%s  ai_addr: %s\n"
                      "%s"
                      "%s}\n",
                      indent,
                      indent, ai_flags.c_str(),
                      indent, ai_family,
                      indent, ai_socktype,
                      indent, ai_protocol,
                      indent, ai.ai_addrlen,
                      indent, ai_addr.c_str(),
                      ai_canonname.c_str(),
                      indent);
}

std::string FormatAddressList(const net::AddressList& address_list,
                              const std::string& host) {
  std::string ret_string;
  StringAppendF(&ret_string, "AddressList {\n");
  StringAppendF(&ret_string, "  Host: %s\n", host.c_str());
  for (const struct addrinfo* it = address_list.head();
       it != NULL;
       it = it->ai_next) {
    StringAppendF(&ret_string, "%s", FormatAddrinfoDetails(*it, "  ").c_str());
  }
  StringAppendF(&ret_string, "}\n");
  return ret_string;
}

class ResolverInvoker;

// DelayedResolve contains state for a DNS resolution to be performed later.
class DelayedResolve : public base::RefCounted<DelayedResolve> {
 public:
  DelayedResolve(const std::string& host, bool is_async,
                 net::HostResolver* resolver,
                 ResolverInvoker* invoker)
      : host_(host),
        address_list_(),
        is_async_(is_async),
        resolver_(resolver),
        invoker_(invoker),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            io_callback_(this, &DelayedResolve::OnResolveComplete)) {
   }

  void Start() {
    net::CompletionCallback* callback = (is_async_) ? &io_callback_ : NULL;
    net::HostResolver::RequestInfo request_info(host_, 80);
    int rv = resolver_->Resolve(request_info,
                                &address_list_,
                                callback,
                                NULL,
                                NULL);
    if (rv != net::ERR_IO_PENDING) {
      OnResolveComplete(rv);
    }
  }

 private:

  // Without this, VC++ complains about the private destructor below.
  friend class base::RefCounted<DelayedResolve>;

  // The destructor is called by Release.
  ~DelayedResolve() {}

  void OnResolveComplete(int result);

  std::string host_;
  net::AddressList address_list_;
  bool is_async_;
  scoped_refptr<net::HostResolver> resolver_;
  ResolverInvoker* invoker_;
  net::CompletionCallbackImpl<DelayedResolve> io_callback_;
};


struct HostAndTime {
  // The host to resolve, i.e. www.google.com
  std::string host;
  // Time since the start of this program to actually kick off the resolution.
  int delta_in_milliseconds;
};

// Invokes a sequence of host resolutions at specified times.
class ResolverInvoker {
 public:
  explicit ResolverInvoker(net::HostResolver* resolver)
      : message_loop_(MessageLoop::TYPE_DEFAULT),
        resolver_(resolver),
        remaining_requests_(0) {
  }

  ~ResolverInvoker() {
  }

  // Resolves all specified hosts in the order provided. hosts_and_times is
  // assumed to be ordered by the delta_in_milliseconds field. There is no
  // guarantee that the resolutions will complete in the order specified when
  // async is true. There is no guarantee that the DNS queries will be issued
  // at exactly the time specified by delta_in_milliseconds, but they are
  // guaranteed to be issued at a time >= delta_in_milliseconds.
  //
  // When async is true, HostResolver::Resolve will issue the DNS lookups
  // asynchronously - this can be used to have multiple requests in flight at
  // the same time.
  //
  // ResolveAll will block until all resolutions are complete.
  void ResolveAll(const std::vector<HostAndTime>& hosts_and_times,
                  bool async) {
    // Schedule all tasks on our message loop, and then run.
    int num_requests = hosts_and_times.size();
    remaining_requests_ = num_requests;
    for (int i = 0; i < num_requests; ++i) {
      const HostAndTime& host_and_time = hosts_and_times[i];
      const std::string& host = host_and_time.host;
      DelayedResolve* resolve_request = new DelayedResolve(host,
        async, resolver_, this);
      resolve_request->AddRef();
      message_loop_.PostDelayedTask(
          FROM_HERE,
          NewRunnableMethod(resolve_request, &DelayedResolve::Start),
          host_and_time.delta_in_milliseconds);
    }
    message_loop_.Run();
  }

 private:
  friend class DelayedResolve;

  void OnResolved(int err, net::AddressList* address_list,
                  const std::string& host) {
    if (err == net::OK) {
      printf("%s", FormatAddressList(*address_list, host).c_str());
    } else {
      printf("Error resolving %s\n", host.c_str());
    }
    DCHECK(remaining_requests_ > 0);
    --remaining_requests_;
    if (remaining_requests_ == 0) {
      message_loop_.Quit();
    }
  }

  MessageLoop message_loop_;
  scoped_refptr<net::HostResolver> resolver_;
  int remaining_requests_;
};

void DelayedResolve::OnResolveComplete(int result) {
  invoker_->OnResolved(result, &address_list_, host_);
  this->Release();
}

struct CommandLineOptions {
  CommandLineOptions()
      : verbose(false),
        async(false),
        cache_size(100),
        cache_ttl(50),
        input_path() {
  }

  bool verbose;
  bool async;
  int cache_size;
  int cache_ttl;
  FilePath input_path;
};

const char* kAsync = "async";
const char* kCacheSize = "cache-size";
const char* kCacheTtl = "cache-ttl";
const char* kInputPath = "input-path";

// Parses the command line values. Returns false if there is a problem,
// options otherwise.
bool ParseCommandLine(CommandLine* command_line, CommandLineOptions* options) {
  options->async = command_line->HasSwitch(kAsync);
  if (command_line->HasSwitch(kCacheSize)) {
    std::string cache_size = command_line->GetSwitchValueASCII(kCacheSize);
    bool valid_size = StringToInt(cache_size, &options->cache_size);
    if (valid_size) {
      valid_size = options->cache_size >= 0;
    }
    if (!valid_size) {
      printf("Invalid --cachesize value: %s\n", cache_size.c_str());
      return false;
    }
  }

  if (command_line->HasSwitch(kCacheTtl)) {
    std::string cache_ttl = command_line->GetSwitchValueASCII(kCacheTtl);
    bool valid_ttl = StringToInt(cache_ttl, &options->cache_ttl);
    if (valid_ttl) {
      valid_ttl = options->cache_ttl >= 0;
    }
    if (!valid_ttl) {
      printf("Invalid --cachettl value: %s\n", cache_ttl.c_str());
      return false;
    }
  }

  if (command_line->HasSwitch(kInputPath)) {
    options->input_path = command_line->GetSwitchValuePath(kInputPath);
  }

  return true;
}

bool ReadHostsAndTimesFromLooseValues(
    const std::vector<std::wstring>& loose_args,
    std::vector<HostAndTime>* hosts_and_times) {
  std::vector<std::wstring>::const_iterator loose_args_end = loose_args.end();
  for (std::vector<std::wstring>::const_iterator it = loose_args.begin();
       it != loose_args_end;
       ++it) {
    // TODO(cbentzel): Read time offset.
    HostAndTime host_and_time = {WideToASCII(*it), 0};
    hosts_and_times->push_back(host_and_time);
  }
  return true;
}

bool ReadHostsAndTimesFromFile(const FilePath& path,
                               std::vector<HostAndTime>* hosts_and_times) {
  // TODO(cbentzel): There are smarter and safer ways to do this.
  std::string file_contents;
  if (!file_util::ReadFileToString(path, &file_contents)) {
    return false;
  }
  std::vector<std::string> lines;
  // TODO(cbentzel): This should probably handle CRLF-style separators as well.
  // Maybe it's worth adding functionality like this to base tools.
  SplitString(file_contents, '\n', &lines);
  std::vector<std::string>::const_iterator line_end = lines.end();
  int previous_timestamp = 0;
  for (std::vector<std::string>::const_iterator it = lines.begin();
       it != line_end;
       ++it) {
    std::vector<std::string> tokens;
    SplitStringAlongWhitespace(*it, &tokens);
    switch (tokens.size()) {
      case 0:
        // Unexpected, but keep going.
        break;
      case 1: {
        HostAndTime host_and_time = {tokens[0], previous_timestamp};
        hosts_and_times->push_back(host_and_time);
        break;
      }
      case 2: {
        int timestamp;
        if (!StringToInt(tokens[1], &timestamp)) {
          // Unexpected value - keep going.
        }
        if (timestamp < previous_timestamp) {
          // Unexpected value - ignore.
        }
        previous_timestamp = timestamp;
        HostAndTime host_and_time = {tokens[0], timestamp};
        hosts_and_times->push_back(host_and_time);
        break;
      }
      default:
        DCHECK(false);
        break;
    }
  }
  return true;
}

int main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;
  CommandLine::Init(argc, argv);
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  CommandLineOptions options;
  if (!ParseCommandLine(command_line, &options)) {
    exit(1);
  }

  // Get the hosts and times - either from a file or command line options.
  // TODO(cbentzel): Add stdin support to POSIX versions - not sure if
  // there's an equivalent in Windows.
  // TODO(cbentzel): If really large, may not want to spool the whole
  // file into memory.
  std::vector<HostAndTime> hosts_and_times;
  if (options.input_path.empty()) {
    if (!ReadHostsAndTimesFromLooseValues(command_line->GetLooseValues(),
                                          &hosts_and_times)) {
      exit(1);
    }
  } else {
    if (!ReadHostsAndTimesFromFile(options.input_path, &hosts_and_times)) {
      exit(1);
    }
  }

  net::HostCache* cache = new net::HostCache(
      options.cache_size,
      base::TimeDelta::FromMilliseconds(options.cache_ttl),
      base::TimeDelta::FromSeconds(0));

  scoped_refptr<net::HostResolver> host_resolver(
      new net::HostResolverImpl(NULL, cache));
  ResolverInvoker invoker(host_resolver.get());
  invoker.ResolveAll(hosts_and_times, options.async);

  CommandLine::Reset();
  return 0;
}
