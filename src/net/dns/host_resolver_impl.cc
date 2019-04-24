// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_impl.h"

#if defined(OS_WIN)
#include <Winsock2.h>
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <netdb.h>
#include <netinet/in.h>
#if !defined(OS_NACL)
#include <net/if.h>
#if !defined(OS_ANDROID)
#include <ifaddrs.h>
#endif  // !defined(OS_ANDROID)
#endif  // !defined(OS_NACL)
#endif  // defined(OS_WIN)

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/containers/linked_list.h"
#include "base/debug/debugger.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/numerics/checked_math.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/base/url_util.h"
#include "net/dns/address_sorter.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_reloader.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_resolver_mdns_listener_impl.h"
#include "net/dns/host_resolver_mdns_task.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/mdns_client.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/record_parsed.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_parameters_callback.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"
#include "url/url_canon_ip.h"

#if BUILDFLAG(ENABLE_MDNS)
#include "net/dns/mdns_client_impl.h"
#endif

#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "net/android/network_library.h"
#endif

namespace net {

namespace {

// Limit the size of hostnames that will be resolved to combat issues in
// some platform's resolvers.
const size_t kMaxHostLength = 4096;

// Default TTL for successful resolutions with ProcTask.
const unsigned kCacheEntryTTLSeconds = 60;

// Default TTL for unsuccessful resolutions with ProcTask.
const unsigned kNegativeCacheEntryTTLSeconds = 0;

// Minimum TTL for successful resolutions with DnsTask.
const unsigned kMinimumTTLSeconds = kCacheEntryTTLSeconds;

// Time between IPv6 probes, i.e. for how long results of each IPv6 probe are
// cached.
const int kIPv6ProbePeriodMs = 1000;

// Google DNS address used for IPv6 probes.
const uint8_t kIPv6ProbeAddress[] =
    { 0x20, 0x01, 0x48, 0x60, 0x48, 0x60, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x88 };

enum DnsResolveStatus {
  RESOLVE_STATUS_DNS_SUCCESS = 0,
  RESOLVE_STATUS_PROC_SUCCESS,
  RESOLVE_STATUS_FAIL,
  RESOLVE_STATUS_SUSPECT_NETBIOS,
  RESOLVE_STATUS_MAX
};

// ICANN uses this localhost address to indicate a name collision.
//
// The policy in Chromium is to fail host resolving if it resolves to
// this special address.
//
// Not however that IP literals are exempt from this policy, so it is still
// possible to navigate to http://127.0.53.53/ directly.
//
// For more details: https://www.icann.org/news/announcement-2-2014-08-01-en
const uint8_t kIcanNameCollisionIp[] = {127, 0, 53, 53};

bool ContainsIcannNameCollisionIp(const AddressList& addr_list) {
  for (const auto& endpoint : addr_list) {
    const IPAddress& addr = endpoint.address();
    if (addr.IsIPv4() && IPAddressStartsWith(addr, kIcanNameCollisionIp)) {
      return true;
    }
  }
  return false;
}

// True if |hostname| ends with either ".local" or ".local.".
bool ResemblesMulticastDNSName(const std::string& hostname) {
  DCHECK(!hostname.empty());
  const char kSuffix[] = ".local.";
  const size_t kSuffixLen = sizeof(kSuffix) - 1;
  const size_t kSuffixLenTrimmed = kSuffixLen - 1;
  if (hostname.back() == '.') {
    return hostname.size() > kSuffixLen &&
        !hostname.compare(hostname.size() - kSuffixLen, kSuffixLen, kSuffix);
  }
  return hostname.size() > kSuffixLenTrimmed &&
      !hostname.compare(hostname.size() - kSuffixLenTrimmed, kSuffixLenTrimmed,
                        kSuffix, kSuffixLenTrimmed);
}

bool ConfigureAsyncDnsNoFallbackFieldTrial() {
  const bool kDefault = false;

  // Configure the AsyncDns field trial as follows:
  // groups AsyncDnsNoFallbackA and AsyncDnsNoFallbackB: return true,
  // groups AsyncDnsA and AsyncDnsB: return false,
  // groups SystemDnsA and SystemDnsB: return false,
  // otherwise (trial absent): return default.
  std::string group_name = base::FieldTrialList::FindFullName("AsyncDns");
  if (!group_name.empty()) {
    return base::StartsWith(group_name, "AsyncDnsNoFallback",
                            base::CompareCase::INSENSITIVE_ASCII);
  }
  return kDefault;
}

const base::FeatureParam<base::TaskPriority>::Option prio_modes[] = {
    {base::TaskPriority::USER_VISIBLE, "default"},
    {base::TaskPriority::USER_BLOCKING, "user_blocking"}};
const base::Feature kSystemResolverPriorityExperiment = {
    "SystemResolverPriorityExperiment", base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<base::TaskPriority> priority_mode{
    &kSystemResolverPriorityExperiment, "mode",
    base::TaskPriority::USER_VISIBLE, &prio_modes};

//-----------------------------------------------------------------------------

// Returns true if |addresses| contains only IPv4 loopback addresses.
bool IsAllIPv4Loopback(const AddressList& addresses) {
  for (unsigned i = 0; i < addresses.size(); ++i) {
    const IPAddress& address = addresses[i].address();
    switch (addresses[i].GetFamily()) {
      case ADDRESS_FAMILY_IPV4:
        if (address.bytes()[0] != 127)
          return false;
        break;
      case ADDRESS_FAMILY_IPV6:
        return false;
      default:
        NOTREACHED();
        return false;
    }
  }
  return true;
}

// Returns true if it can determine that only loopback addresses are configured.
// i.e. if only 127.0.0.1 and ::1 are routable.
// Also returns false if it cannot determine this.
bool HaveOnlyLoopbackAddresses() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
#if defined(OS_WIN)
  // TODO(wtc): implement with the GetAdaptersAddresses function.
  NOTIMPLEMENTED();
  return false;
#elif defined(OS_ANDROID)
  return android::HaveOnlyLoopbackAddresses();
#elif defined(OS_NACL)
  NOTIMPLEMENTED();
  return false;
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  struct ifaddrs* interface_addr = NULL;
  int rv = getifaddrs(&interface_addr);
  if (rv != 0) {
    DVPLOG(1) << "getifaddrs() failed";
    return false;
  }

  bool result = true;
  for (struct ifaddrs* interface = interface_addr;
       interface != NULL;
       interface = interface->ifa_next) {
    if (!(IFF_UP & interface->ifa_flags))
      continue;
    if (IFF_LOOPBACK & interface->ifa_flags)
      continue;
    const struct sockaddr* addr = interface->ifa_addr;
    if (!addr)
      continue;
    if (addr->sa_family == AF_INET6) {
      // Safe cast since this is AF_INET6.
      const struct sockaddr_in6* addr_in6 =
          reinterpret_cast<const struct sockaddr_in6*>(addr);
      const struct in6_addr* sin6_addr = &addr_in6->sin6_addr;
      if (IN6_IS_ADDR_LOOPBACK(sin6_addr) || IN6_IS_ADDR_LINKLOCAL(sin6_addr))
        continue;
    }
    if (addr->sa_family != AF_INET6 && addr->sa_family != AF_INET)
      continue;

    result = false;
    break;
  }
  freeifaddrs(interface_addr);
  return result;
#endif  // defined(various platforms)
}

// Creates NetLog parameters when the resolve failed.
std::unique_ptr<base::Value> NetLogProcTaskFailedCallback(
    uint32_t attempt_number,
    int net_error,
    int os_error,
    NetLogCaptureMode /* capture_mode */) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  if (attempt_number)
    dict->SetInteger("attempt_number", attempt_number);

  dict->SetInteger("net_error", net_error);

  if (os_error) {
    dict->SetInteger("os_error", os_error);
#if defined(OS_WIN)
    // Map the error code to a human-readable string.
    LPWSTR error_string = nullptr;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                  0,  // Use the internal message table.
                  os_error,
                  0,  // Use default language.
                  (LPWSTR)&error_string,
                  0,  // Buffer size.
                  0);  // Arguments (unused).
    dict->SetString("os_error_string", base::WideToUTF8(error_string));
    LocalFree(error_string);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
    dict->SetString("os_error_string", gai_strerror(os_error));
#endif
  }

  return std::move(dict);
}

// Creates NetLog parameters when the DnsTask failed.
std::unique_ptr<base::Value> NetLogDnsTaskFailedCallback(
    int net_error,
    int dns_error,
    NetLogParametersCallback results_callback,
    NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetInteger("net_error", net_error);
  if (dns_error)
    dict->SetInteger("dns_error", dns_error);
  if (results_callback)
    dict->Set("resolve_results", results_callback.Run(capture_mode));
  return std::move(dict);
}

// Creates NetLog parameters containing the information of the request. Use
// NetLogRequestInfoCallback if the request is specified via RequestInfo.
std::unique_ptr<base::Value> NetLogRequestCallback(
    const HostPortPair& host,
    NetLogCaptureMode /* capture_mode */) {
  auto dict = std::make_unique<base::DictionaryValue>();

  dict->SetString("host", host.ToString());
  dict->SetInteger("address_family",
                   static_cast<int>(ADDRESS_FAMILY_UNSPECIFIED));
  dict->SetBoolean("allow_cached_response", true);
  dict->SetBoolean("is_speculative", false);
  return std::move(dict);
}

// Creates NetLog parameters for the creation of a HostResolverImpl::Job.
std::unique_ptr<base::Value> NetLogJobCreationCallback(
    const NetLogSource& source,
    const std::string* host,
    NetLogCaptureMode /* capture_mode */) {
  auto dict = std::make_unique<base::DictionaryValue>();
  source.AddToEventParameters(dict.get());
  dict->SetString("host", *host);
  return std::move(dict);
}

// Creates NetLog parameters for HOST_RESOLVER_IMPL_JOB_ATTACH/DETACH events.
std::unique_ptr<base::Value> NetLogJobAttachCallback(
    const NetLogSource& source,
    RequestPriority priority,
    NetLogCaptureMode /* capture_mode */) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  source.AddToEventParameters(dict.get());
  dict->SetString("priority", RequestPriorityToString(priority));
  return std::move(dict);
}

// Creates NetLog parameters for the DNS_CONFIG_CHANGED event.
std::unique_ptr<base::Value> NetLogDnsConfigCallback(
    const DnsConfig* config,
    NetLogCaptureMode /* capture_mode */) {
  return config->ToValue();
}

std::unique_ptr<base::Value> NetLogIPv6AvailableCallback(
    bool ipv6_available,
    bool cached,
    NetLogCaptureMode /* capture_mode */) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetBoolean("ipv6_available", ipv6_available);
  dict->SetBoolean("cached", cached);
  return std::move(dict);
}

// The logging routines are defined here because some requests are resolved
// without a Request object.

// Logs when a request has just been started. Overloads for whether or not the
// request information is specified via a RequestInfo object.
void LogStartRequest(const NetLogWithSource& source_net_log,
                     const HostPortPair& host) {
  source_net_log.BeginEvent(NetLogEventType::HOST_RESOLVER_IMPL_REQUEST,
                            base::BindRepeating(&NetLogRequestCallback, host));
}

// Logs when a request has just completed (before its callback is run).
void LogFinishRequest(const NetLogWithSource& source_net_log, int net_error) {
  source_net_log.EndEventWithNetErrorCode(
      NetLogEventType::HOST_RESOLVER_IMPL_REQUEST, net_error);
}

// Logs when a request has been cancelled.
void LogCancelRequest(const NetLogWithSource& source_net_log) {
  source_net_log.AddEvent(NetLogEventType::CANCELLED);
  source_net_log.EndEvent(NetLogEventType::HOST_RESOLVER_IMPL_REQUEST);
}

//-----------------------------------------------------------------------------

// Keeps track of the highest priority.
class PriorityTracker {
 public:
  explicit PriorityTracker(RequestPriority initial_priority)
      : highest_priority_(initial_priority), total_count_(0) {
    memset(counts_, 0, sizeof(counts_));
  }

  RequestPriority highest_priority() const {
    return highest_priority_;
  }

  size_t total_count() const {
    return total_count_;
  }

  void Add(RequestPriority req_priority) {
    ++total_count_;
    ++counts_[req_priority];
    if (highest_priority_ < req_priority)
      highest_priority_ = req_priority;
  }

  void Remove(RequestPriority req_priority) {
    DCHECK_GT(total_count_, 0u);
    DCHECK_GT(counts_[req_priority], 0u);
    --total_count_;
    --counts_[req_priority];
    size_t i;
    for (i = highest_priority_; i > MINIMUM_PRIORITY && !counts_[i]; --i) {
    }
    highest_priority_ = static_cast<RequestPriority>(i);

    // In absence of requests, default to MINIMUM_PRIORITY.
    if (total_count_ == 0)
      DCHECK_EQ(MINIMUM_PRIORITY, highest_priority_);
  }

 private:
  RequestPriority highest_priority_;
  size_t total_count_;
  size_t counts_[NUM_PRIORITIES];
};

// Is |dns_server| within the list of known DNS servers that also support
// DNS-over-HTTPS?
bool DnsServerSupportsDoh(const IPAddress& dns_server) {
  static const base::NoDestructor<std::unordered_set<std::string>>
      upgradable_servers(std::initializer_list<std::string>({
          // Google Public DNS
          "8.8.8.8",
          "8.8.4.4",
          "2001:4860:4860::8888",
          "2001:4860:4860::8844",
          // Cloudflare DNS
          "1.1.1.1",
          "1.0.0.1",
          "2606:4700:4700::1111",
          "2606:4700:4700::1001",
          // Quad9 DNS
          "9.9.9.9",
          "149.112.112.112",
          "2620:fe::fe",
          "2620:fe::9",
      }));
  return upgradable_servers->find(dns_server.ToString()) !=
         upgradable_servers->end();
}

}  // namespace

//-----------------------------------------------------------------------------

bool ResolveLocalHostname(base::StringPiece host, AddressList* address_list) {
  address_list->clear();

  bool is_local6;
  if (!IsLocalHostname(host, &is_local6))
    return false;

  address_list->push_back(IPEndPoint(IPAddress::IPv6Localhost(), 0));
  if (!is_local6) {
    address_list->push_back(IPEndPoint(IPAddress::IPv4Localhost(), 0));
  }

  return true;
}

const unsigned HostResolverImpl::kMaximumDnsFailures = 16;

// Holds the callback and request parameters for an outstanding request.
//
// The RequestImpl is owned by the end user of host resolution. Deletion prior
// to the request having completed means the request was cancelled by the
// caller.
//
// Both the RequestImpl and its associated Job hold non-owning pointers to each
// other. Care must be taken to clear the corresponding pointer when
// cancellation is initiated by the Job (OnJobCancelled) vs by the end user
// (~RequestImpl).
class HostResolverImpl::RequestImpl
    : public HostResolver::ResolveHostRequest,
      public base::LinkNode<HostResolverImpl::RequestImpl> {
 public:
  RequestImpl(const NetLogWithSource& source_net_log,
              const HostPortPair& request_host,
              const base::Optional<ResolveHostParameters>& optional_parameters,
              base::WeakPtr<HostResolverImpl> resolver)
      : source_net_log_(source_net_log),
        request_host_(request_host),
        parameters_(optional_parameters ? optional_parameters.value()
                                        : ResolveHostParameters()),
        host_resolver_flags_(
            HostResolver::ParametersToHostResolverFlags(parameters_)),
        priority_(parameters_.initial_priority),
        job_(nullptr),
        resolver_(resolver),
        complete_(false) {}

  ~RequestImpl() override;

  int Start(CompletionOnceCallback callback) override {
    DCHECK(callback);
    // Start() may only be called once per request.
    DCHECK(!job_);
    DCHECK(!complete_);
    DCHECK(!callback_);
    // Parent HostResolver must still be alive to call Start().
    DCHECK(resolver_);

    int rv = resolver_->Resolve(this);
    DCHECK(!complete_);
    if (rv == ERR_IO_PENDING) {
      DCHECK(job_);
      callback_ = std::move(callback);
    } else {
      DCHECK(!job_);
      complete_ = true;
    }
    resolver_ = nullptr;

    return rv;
  }

  const base::Optional<AddressList>& GetAddressResults() const override {
    DCHECK(complete_);
    static const base::NoDestructor<base::Optional<AddressList>> nullopt_result;
    return results_ ? results_.value().addresses() : *nullopt_result;
  }

  const base::Optional<std::vector<std::string>>& GetTextResults()
      const override {
    DCHECK(complete_);
    static const base::NoDestructor<base::Optional<std::vector<std::string>>>
        nullopt_result;
    return results_ ? results_.value().text_records() : *nullopt_result;
  }

  const base::Optional<std::vector<HostPortPair>>& GetHostnameResults()
      const override {
    DCHECK(complete_);
    static const base::NoDestructor<base::Optional<std::vector<HostPortPair>>>
        nullopt_result;
    return results_ ? results_.value().hostnames() : *nullopt_result;
  }

  const base::Optional<HostCache::EntryStaleness>& GetStaleInfo()
      const override {
    DCHECK(complete_);
    return stale_info_;
  }

  void ChangeRequestPriority(RequestPriority priority) override;

  void set_results(HostCache::Entry results) {
    // Should only be called at most once and before request is marked
    // completed.
    DCHECK(!complete_);
    DCHECK(!results_);
    DCHECK(!parameters_.is_speculative);

    results_ = std::move(results);
  }

  void set_stale_info(HostCache::EntryStaleness stale_info) {
    // Should only be called at most once and before request is marked
    // completed.
    DCHECK(!complete_);
    DCHECK(!stale_info_);
    DCHECK(!parameters_.is_speculative);

    stale_info_ = std::move(stale_info);
  }

  void AssignJob(Job* job) {
    DCHECK(job);
    DCHECK(!job_);

    job_ = job;
  }

  // Unassigns the Job without calling completion callback.
  void OnJobCancelled(Job* job) {
    DCHECK_EQ(job_, job);
    job_ = nullptr;
    DCHECK(!complete_);
    DCHECK(callback_);
    callback_.Reset();

    // No results should be set.
    DCHECK(!results_);
  }

  // Cleans up Job assignment, marks request completed, and calls the completion
  // callback.
  void OnJobCompleted(Job* job, int error) {
    DCHECK_EQ(job_, job);
    job_ = nullptr;

    DCHECK(!complete_);
    complete_ = true;

    DCHECK(callback_);
    std::move(callback_).Run(error);
  }

  Job* job() const { return job_; }

  // NetLog for the source, passed in HostResolver::Resolve.
  const NetLogWithSource& source_net_log() { return source_net_log_; }

  const HostPortPair& request_host() const { return request_host_; }

  const ResolveHostParameters& parameters() const { return parameters_; }

  HostResolverFlags host_resolver_flags() const { return host_resolver_flags_; }

  RequestPriority priority() const { return priority_; }
  void set_priority(RequestPriority priority) { priority_ = priority; }

  bool complete() const { return complete_; }

  base::TimeTicks request_time() const {
    DCHECK(!request_time_.is_null());
    return request_time_;
  }
  void set_request_time(base::TimeTicks request_time) {
    DCHECK(request_time_.is_null());
    DCHECK(!request_time.is_null());
    request_time_ = request_time;
  }

 private:
  const NetLogWithSource source_net_log_;

  const HostPortPair request_host_;
  const ResolveHostParameters parameters_;
  const HostResolverFlags host_resolver_flags_;

  RequestPriority priority_;

  // The resolve job that this request is dependent on.
  Job* job_;
  base::WeakPtr<HostResolverImpl> resolver_;

  // The user's callback to invoke when the request completes.
  CompletionOnceCallback callback_;

  bool complete_;
  base::Optional<HostCache::Entry> results_;
  base::Optional<HostCache::EntryStaleness> stale_info_;

  base::TimeTicks request_time_;

  DISALLOW_COPY_AND_ASSIGN(RequestImpl);
};

//------------------------------------------------------------------------------

// Calls HostResolverProc in TaskScheduler. Performs retries if necessary.
//
// In non-test code, the HostResolverProc is always SystemHostResolverProc,
// which calls a platform API that implements host resolution.
//
// Whenever we try to resolve the host, we post a delayed task to check if host
// resolution (OnLookupComplete) is completed or not. If the original attempt
// hasn't completed, then we start another attempt for host resolution. We take
// the results from the first attempt that finishes and ignore the results from
// all other attempts.
//
// TODO(szym): Move to separate source file for testing and mocking.
//
class HostResolverImpl::ProcTask {
 public:
  typedef base::OnceCallback<void(int net_error, const AddressList& addr_list)>
      Callback;

  ProcTask(const Key& key,
           const ProcTaskParams& params,
           Callback callback,
           scoped_refptr<base::TaskRunner> proc_task_runner,
           const NetLogWithSource& job_net_log,
           const base::TickClock* tick_clock)
      : key_(key),
        params_(params),
        callback_(std::move(callback)),
        network_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        proc_task_runner_(std::move(proc_task_runner)),
        attempt_number_(0),
        net_log_(job_net_log),
        tick_clock_(tick_clock),
        weak_ptr_factory_(this) {
    // ProcTask only supports resolving addresses.
    DCHECK(IsAddressType(key_.dns_query_type));

    DCHECK(callback_);
    if (!params_.resolver_proc.get())
      params_.resolver_proc = HostResolverProc::GetDefault();
    // If default is unset, use the system proc.
    if (!params_.resolver_proc.get())
      params_.resolver_proc = new SystemHostResolverProc();
  }

  // Cancels this ProcTask. Any outstanding resolve attempts running on worker
  // thread will continue running, but they will post back to the network thread
  // before checking their WeakPtrs to find that this task is cancelled.
  ~ProcTask() {
    DCHECK(network_task_runner_->BelongsToCurrentThread());

    // If this is cancellation, log the EndEvent (otherwise this was logged in
    // OnLookupComplete()).
    if (!was_completed())
      net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_IMPL_PROC_TASK);
  }

  void Start() {
    DCHECK(network_task_runner_->BelongsToCurrentThread());
    DCHECK(!was_completed());
    net_log_.BeginEvent(NetLogEventType::HOST_RESOLVER_IMPL_PROC_TASK);
    StartLookupAttempt();
  }

  bool was_completed() const {
    DCHECK(network_task_runner_->BelongsToCurrentThread());
    return callback_.is_null();
  }

 private:
  using AttemptCompletionCallback = base::OnceCallback<
      void(const AddressList& results, int error, const int os_error)>;

  void StartLookupAttempt() {
    DCHECK(network_task_runner_->BelongsToCurrentThread());
    DCHECK(!was_completed());
    base::TimeTicks start_time = tick_clock_->NowTicks();
    ++attempt_number_;
    // Dispatch the lookup attempt to a worker thread.
    AttemptCompletionCallback completion_callback = base::BindOnce(
        &ProcTask::OnLookupAttemptComplete, weak_ptr_factory_.GetWeakPtr(),
        start_time, attempt_number_, tick_clock_);
    proc_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ProcTask::DoLookup, key_, params_.resolver_proc,
                       network_task_runner_, std::move(completion_callback)));

    net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_ATTEMPT_STARTED,
                      NetLog::IntCallback("attempt_number", attempt_number_));

    // If the results aren't received within a given time, RetryIfNotComplete
    // will start a new attempt if none of the outstanding attempts have
    // completed yet.
    // Use a WeakPtr to avoid keeping the ProcTask alive after completion or
    // cancellation.
    if (attempt_number_ <= params_.max_retry_attempts) {
      network_task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ProcTask::StartLookupAttempt,
                         weak_ptr_factory_.GetWeakPtr()),
          params_.unresponsive_delay *
              std::pow(params_.retry_factor, attempt_number_ - 1));
    }
  }

  // WARNING: This code runs in TaskScheduler with CONTINUE_ON_SHUTDOWN. The
  // shutdown code cannot wait for it to finish, so this code must be very
  // careful about using other objects (like MessageLoops, Singletons, etc).
  // During shutdown these objects may no longer exist.
  static void DoLookup(
      Key key,
      scoped_refptr<HostResolverProc> resolver_proc,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      AttemptCompletionCallback completion_callback) {
    AddressList results;
    int os_error = 0;
    int error = resolver_proc->Resolve(
        key.hostname,
        HostResolver::DnsQueryTypeToAddressFamily(key.dns_query_type),
        key.host_resolver_flags, &results, &os_error);

    network_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(completion_callback), results,
                                  error, os_error));
  }

  // Callback for when DoLookup() completes (runs on task runner thread). Now
  // that we're back in the network thread, checks that |proc_task| is still
  // valid, and if so, passes back to the object.
  static void OnLookupAttemptComplete(base::WeakPtr<ProcTask> proc_task,
                                      const base::TimeTicks& start_time,
                                      const uint32_t attempt_number,
                                      const base::TickClock* tick_clock,
                                      const AddressList& results,
                                      int error,
                                      const int os_error) {
    TRACE_EVENT0(NetTracingCategory(), "ProcTask::OnLookupComplete");

    // If results are empty, we should return an error.
    bool empty_list_on_ok = (error == OK && results.empty());
    if (empty_list_on_ok)
      error = ERR_NAME_NOT_RESOLVED;

    // Ideally the following code would be part of host_resolver_proc.cc,
    // however it isn't safe to call NetworkChangeNotifier from worker threads.
    // So do it here on the IO thread instead.
    if (error != OK && NetworkChangeNotifier::IsOffline())
      error = ERR_INTERNET_DISCONNECTED;

    if (!proc_task)
      return;

    proc_task->OnLookupComplete(results, start_time, attempt_number, error,
                                os_error);
  }

  void OnLookupComplete(const AddressList& results,
                        const base::TimeTicks& start_time,
                        const uint32_t attempt_number,
                        int error,
                        const int os_error) {
    DCHECK(network_task_runner_->BelongsToCurrentThread());
    DCHECK(!was_completed());

    // Invalidate WeakPtrs to cancel handling of all outstanding lookup attempts
    // and retries.
    weak_ptr_factory_.InvalidateWeakPtrs();

    NetLogParametersCallback net_log_callback;
    NetLogParametersCallback attempt_net_log_callback;
    if (error != OK) {
      net_log_callback = base::BindRepeating(&NetLogProcTaskFailedCallback, 0,
                                             error, os_error);
      attempt_net_log_callback = base::BindRepeating(
          &NetLogProcTaskFailedCallback, attempt_number, error, os_error);
    } else {
      net_log_callback = results.CreateNetLogCallback();
      attempt_net_log_callback =
          NetLog::IntCallback("attempt_number", attempt_number);
    }
    net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_IMPL_PROC_TASK,
                      net_log_callback);
    net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_ATTEMPT_FINISHED,
                      attempt_net_log_callback);

    std::move(callback_).Run(error, results);
  }

  Key key_;

  // Holds an owning reference to the HostResolverProc that we are going to use.
  // This may not be the current resolver procedure by the time we call
  // ResolveAddrInfo, but that's OK... we'll use it anyways, and the owning
  // reference ensures that it remains valid until we are done.
  ProcTaskParams params_;

  // The listener to the results of this ProcTask.
  Callback callback_;

  // Used to post events onto the network thread.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  // Used to post blocking HostResolverProc tasks.
  scoped_refptr<base::TaskRunner> proc_task_runner_;

  // Keeps track of the number of attempts we have made so far to resolve the
  // host. Whenever we start an attempt to resolve the host, we increase this
  // number.
  uint32_t attempt_number_;

  NetLogWithSource net_log_;

  const base::TickClock* tick_clock_;

  // Used to loop back from the blocking lookup attempt tasks as well as from
  // delayed retry tasks. Invalidate WeakPtrs on completion and cancellation to
  // cancel handling of such posted tasks.
  base::WeakPtrFactory<ProcTask> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProcTask);
};

//-----------------------------------------------------------------------------

// Resolves the hostname using DnsTransaction, which is a full implementation of
// a DNS stub resolver. One DnsTransaction is created for each resolution
// needed, which for AF_UNSPEC resolutions includes both A and AAAA. The
// transactions are scheduled separately and started separately.
//
// TODO(szym): This could be moved to separate source file as well.
class HostResolverImpl::DnsTask : public base::SupportsWeakPtr<DnsTask> {
 public:
  class Delegate {
   public:
    virtual void OnDnsTaskComplete(base::TimeTicks start_time,
                                   const HostCache::Entry& results,
                                   bool secure) = 0;

    // Called when the first of two jobs succeeds.  If the first completed
    // transaction fails, this is not called.  Also not called when the DnsTask
    // only needs to run one transaction.
    virtual void OnFirstDnsTransactionComplete() = 0;

    virtual URLRequestContext* url_request_context() = 0;
    virtual RequestPriority priority() const = 0;

   protected:
    Delegate() = default;
    virtual ~Delegate() = default;
  };

  DnsTask(DnsClient* client,
          const Key& key,
          bool allow_fallback_resolution,
          Delegate* delegate,
          const NetLogWithSource& job_net_log,
          const base::TickClock* tick_clock)
      : client_(client),
        key_(key),
        allow_fallback_resolution_(allow_fallback_resolution),
        delegate_(delegate),
        net_log_(job_net_log),
        num_completed_transactions_(0),
        tick_clock_(tick_clock),
        task_start_time_(tick_clock_->NowTicks()) {
    DCHECK(client);
    DCHECK(delegate_);
  }

  bool allow_fallback_resolution() const { return allow_fallback_resolution_; }

  bool needs_two_transactions() const {
    return key_.dns_query_type == DnsQueryType::UNSPECIFIED;
  }

  bool needs_another_transaction() const {
    return needs_two_transactions() && !transaction2_;
  }

  void StartFirstTransaction() {
    DCHECK_EQ(0u, num_completed_transactions_);
    DCHECK(!transaction1_);

    net_log_.BeginEvent(NetLogEventType::HOST_RESOLVER_IMPL_DNS_TASK);
    if (key_.dns_query_type == DnsQueryType::UNSPECIFIED) {
      transaction1_ = CreateTransaction(DnsQueryType::A);
    } else {
      transaction1_ = CreateTransaction(key_.dns_query_type);
    }
    transaction1_->Start();
  }

  void StartSecondTransaction() {
    DCHECK(needs_another_transaction());
    transaction2_ = CreateTransaction(DnsQueryType::AAAA);
    transaction2_->Start();
  }

 private:
  static const HostCache::Entry& GetMalformedResponseResult() {
    static const base::NoDestructor<HostCache::Entry> kMalformedResponseResult(
        ERR_DNS_MALFORMED_RESPONSE, HostCache::Entry::SOURCE_DNS);
    return *kMalformedResponseResult;
  }

  std::unique_ptr<DnsTransaction> CreateTransaction(
      DnsQueryType dns_query_type) {
    DCHECK_NE(DnsQueryType::UNSPECIFIED, dns_query_type);
    std::unique_ptr<DnsTransaction> trans =
        client_->GetTransactionFactory()->CreateTransaction(
            key_.hostname, DnsQueryTypeToQtype(dns_query_type),
            base::BindOnce(&DnsTask::OnTransactionComplete,
                           base::Unretained(this), tick_clock_->NowTicks(),
                           dns_query_type),
            net_log_, SecureDnsMode::AUTOMATIC);
    trans->SetRequestContext(delegate_->url_request_context());
    trans->SetRequestPriority(delegate_->priority());
    return trans;
  }

  void OnTransactionComplete(const base::TimeTicks& start_time,
                             DnsQueryType dns_query_type,
                             DnsTransaction* transaction,
                             int net_error,
                             const DnsResponse* response,
                             bool secure) {
    DCHECK(transaction);
    if (net_error != OK && !(net_error == ERR_NAME_NOT_RESOLVED && response &&
                             response->IsValid())) {
      OnFailure(net_error, DnsResponse::DNS_PARSE_OK, base::nullopt, secure);
      return;
    }

    DnsResponse::Result parse_result = DnsResponse::DNS_PARSE_RESULT_MAX;
    HostCache::Entry results(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
    switch (dns_query_type) {
      case DnsQueryType::UNSPECIFIED:
        // Should create two separate transactions with specified type.
        NOTREACHED();
        break;
      case DnsQueryType::A:
      case DnsQueryType::AAAA:
        parse_result = ParseAddressDnsResponse(response, &results);
        break;
      case DnsQueryType::TXT:
        parse_result = ParseTxtDnsResponse(response, &results);
        break;
      case DnsQueryType::PTR:
        parse_result = ParsePointerDnsResponse(response, &results);
        break;
      case DnsQueryType::SRV:
        parse_result = ParseServiceDnsResponse(response, &results);
        break;
    }
    DCHECK_LT(parse_result, DnsResponse::DNS_PARSE_RESULT_MAX);

    if (results.error() != OK && results.error() != ERR_NAME_NOT_RESOLVED) {
      OnFailure(results.error(), parse_result, results.GetOptionalTtl(),
                secure);
      return;
    }

    // Merge results with saved results from previous transactions.
    DCHECK_EQ(saved_results_.has_value(), saved_secure_.has_value());
    if (saved_results_) {
      DCHECK(needs_two_transactions());
      DCHECK_GE(1u, num_completed_transactions_);

      switch (dns_query_type) {
        case DnsQueryType::A:
          // A results in |results| go after other results in |saved_results_|,
          // so merge |saved_results_| to the front.
          results = HostCache::Entry::MergeEntries(
              std::move(saved_results_).value(), std::move(results));
          break;
        case DnsQueryType::AAAA:
          // AAAA results in |results| go before other results in
          // |saved_results_|, so merge |saved_results_| to the back.
          results = HostCache::Entry::MergeEntries(
              std::move(results), std::move(saved_results_).value());
          break;
        default:
          // Only expect address query types with multiple transactions.
          NOTREACHED();
      }
      // If the earlier result was retrieved insecurely, the merged result
      // should be stored accordingly.
      if (!saved_secure_.value())
        secure = false;
    }

    // If not all transactions are complete, the task cannot yet be completed
    // and the results so far must be saved to merge with additional results.
    ++num_completed_transactions_;
    if (needs_two_transactions() && num_completed_transactions_ == 1) {
      saved_results_ = std::move(results);
      saved_secure_ = secure;
      // No need to repeat the suffix search.
      key_.hostname = transaction->GetHostname();
      delegate_->OnFirstDnsTransactionComplete();
      return;
    }

    // If there are multiple addresses, and at least one is IPv6, need to sort
    // them.  Note that IPv6 addresses are always put before IPv4 ones, so it's
    // sufficient to just check the family of the first address.
    if (results.addresses() && results.addresses().value().size() > 1 &&
        results.addresses().value()[0].GetFamily() == ADDRESS_FAMILY_IPV6) {
      // Sort addresses if needed.  Sort could complete synchronously.
      client_->GetAddressSorter()->Sort(
          results.addresses().value(),
          base::BindOnce(&DnsTask::OnSortComplete, AsWeakPtr(),
                         tick_clock_->NowTicks(), std::move(results), secure));
      return;
    }

    OnSuccess(results, secure);
  }

  DnsResponse::Result ParseAddressDnsResponse(const DnsResponse* response,
                                              HostCache::Entry* out_results) {
    AddressList addresses;
    base::TimeDelta ttl;
    DnsResponse::Result parse_result =
        response->ParseToAddressList(&addresses, &ttl);

    if (parse_result != DnsResponse::DNS_PARSE_OK) {
      *out_results = GetMalformedResponseResult();
    } else if (addresses.empty()) {
      *out_results = HostCache::Entry(ERR_NAME_NOT_RESOLVED, AddressList(),
                                      HostCache::Entry::SOURCE_DNS, ttl);
    } else {
      *out_results = HostCache::Entry(OK, std::move(addresses),
                                      HostCache::Entry::SOURCE_DNS, ttl);
    }
    return parse_result;
  }

  DnsResponse::Result ParseTxtDnsResponse(const DnsResponse* response,
                                          HostCache::Entry* out_results) {
    std::vector<std::unique_ptr<const RecordParsed>> records;
    base::Optional<base::TimeDelta> response_ttl;
    DnsResponse::Result parse_result = ParseAndFilterResponseRecords(
        response, dns_protocol::kTypeTXT, &records, &response_ttl);

    if (parse_result != DnsResponse::DNS_PARSE_OK) {
      *out_results = GetMalformedResponseResult();
      return parse_result;
    }

    std::vector<std::string> text_records;
    for (const auto& record : records) {
      const TxtRecordRdata* rdata = record->rdata<net::TxtRecordRdata>();
      text_records.insert(text_records.end(), rdata->texts().begin(),
                          rdata->texts().end());
    }

    *out_results = HostCache::Entry(
        text_records.empty() ? ERR_NAME_NOT_RESOLVED : OK,
        std::move(text_records), HostCache::Entry::SOURCE_DNS, response_ttl);
    return DnsResponse::DNS_PARSE_OK;
  }

  DnsResponse::Result ParsePointerDnsResponse(const DnsResponse* response,
                                              HostCache::Entry* out_results) {
    std::vector<std::unique_ptr<const RecordParsed>> records;
    base::Optional<base::TimeDelta> response_ttl;
    DnsResponse::Result parse_result = ParseAndFilterResponseRecords(
        response, dns_protocol::kTypePTR, &records, &response_ttl);

    if (parse_result != DnsResponse::DNS_PARSE_OK) {
      *out_results = GetMalformedResponseResult();
      return parse_result;
    }

    std::vector<HostPortPair> pointers;
    for (const auto& record : records) {
      const PtrRecordRdata* rdata = record->rdata<net::PtrRecordRdata>();
      std::string pointer = rdata->ptrdomain();

      // Skip pointers to the root domain.
      if (!pointer.empty())
        pointers.emplace_back(std::move(pointer), 0);
    }

    *out_results = HostCache::Entry(
        pointers.empty() ? ERR_NAME_NOT_RESOLVED : OK, std::move(pointers),
        HostCache::Entry::SOURCE_DNS, response_ttl);
    return DnsResponse::DNS_PARSE_OK;
  }

  DnsResponse::Result ParseServiceDnsResponse(const DnsResponse* response,
                                              HostCache::Entry* out_results) {
    std::vector<std::unique_ptr<const RecordParsed>> records;
    base::Optional<base::TimeDelta> response_ttl;
    DnsResponse::Result parse_result = ParseAndFilterResponseRecords(
        response, dns_protocol::kTypeSRV, &records, &response_ttl);

    if (parse_result != DnsResponse::DNS_PARSE_OK) {
      *out_results = GetMalformedResponseResult();
      return parse_result;
    }

    std::vector<const SrvRecordRdata*> fitered_rdatas;
    for (const auto& record : records) {
      const SrvRecordRdata* rdata = record->rdata<net::SrvRecordRdata>();

      // Skip pointers to the root domain.
      if (!rdata->target().empty())
        fitered_rdatas.push_back(rdata);
    }

    std::vector<HostPortPair> ordered_service_targets =
        SortServiceTargets(fitered_rdatas);

    *out_results = HostCache::Entry(
        ordered_service_targets.empty() ? ERR_NAME_NOT_RESOLVED : OK,
        std::move(ordered_service_targets), HostCache::Entry::SOURCE_DNS,
        response_ttl);
    return DnsResponse::DNS_PARSE_OK;
  }

  // Sort service targets per RFC2782.  In summary, sort first by |priority|,
  // lowest first.  For targets with the same priority, secondary sort randomly
  // using |weight| with higher weighted objects more likely to go first.
  std::vector<HostPortPair> SortServiceTargets(
      const std::vector<const SrvRecordRdata*>& rdatas) {
    std::map<uint16_t, std::unordered_set<const SrvRecordRdata*>>
        ordered_by_priority;
    for (const SrvRecordRdata* rdata : rdatas)
      ordered_by_priority[rdata->priority()].insert(rdata);

    std::vector<HostPortPair> sorted_targets;
    for (auto& priority : ordered_by_priority) {
      // With (num results) <= UINT16_MAX (and in practice, much less) and
      // (weight per result) <= UINT16_MAX, then it should be the case that
      // (total weight) <= UINT32_MAX, but use CheckedNumeric for extra safety.
      auto total_weight = base::MakeCheckedNum<uint32_t>(0);
      for (const SrvRecordRdata* rdata : priority.second)
        total_weight += rdata->weight();

      // Add 1 to total weight because, to deal with 0-weight targets, we want
      // our random selection to be inclusive [0, total].
      total_weight++;

      // Order by weighted random. Make such random selections, removing from
      // |priority.second| until |priority.second| only contains 1 rdata.
      while (priority.second.size() >= 2) {
        uint32_t random_selection =
            base::RandGenerator(total_weight.ValueOrDie());
        const SrvRecordRdata* selected_rdata = nullptr;
        for (const SrvRecordRdata* rdata : priority.second) {
          // >= to always select the first target on |random_selection| == 0,
          // even if its weight is 0.
          if (rdata->weight() >= random_selection) {
            selected_rdata = rdata;
            break;
          }
          random_selection -= rdata->weight();
        }

        DCHECK(selected_rdata);
        sorted_targets.emplace_back(selected_rdata->target(),
                                    selected_rdata->port());
        total_weight -= selected_rdata->weight();
        size_t removed = priority.second.erase(selected_rdata);
        DCHECK_EQ(1u, removed);
      }

      DCHECK_EQ(1u, priority.second.size());
      DCHECK_EQ((total_weight - 1).ValueOrDie(),
                (*priority.second.begin())->weight());
      const SrvRecordRdata* rdata = *priority.second.begin();
      sorted_targets.emplace_back(rdata->target(), rdata->port());
    }

    return sorted_targets;
  }

  DnsResponse::Result ParseAndFilterResponseRecords(
      const DnsResponse* response,
      uint16_t filter_dns_type,
      std::vector<std::unique_ptr<const RecordParsed>>* out_records,
      base::Optional<base::TimeDelta>* out_response_ttl) {
    out_records->clear();
    out_response_ttl->reset();

    DnsRecordParser parser = response->Parser();

    // Expected to be validated by DnsTransaction.
    DCHECK_EQ(filter_dns_type, response->qtype());

    for (unsigned i = 0; i < response->answer_count(); ++i) {
      std::unique_ptr<const RecordParsed> record =
          RecordParsed::CreateFrom(&parser, base::Time::Now());

      if (!record)
        return DnsResponse::DNS_MALFORMED_RESPONSE;
      if (!base::EqualsCaseInsensitiveASCII(record->name(),
                                            response->GetDottedName())) {
        return DnsResponse::DNS_NAME_MISMATCH;
      }

      // Ignore any records that are not class Internet and type
      // |filter_dns_type|.
      if (record->klass() == dns_protocol::kClassIN &&
          record->type() == filter_dns_type) {
        base::TimeDelta ttl = base::TimeDelta::FromSeconds(record->ttl());
        *out_response_ttl =
            std::min(out_response_ttl->value_or(base::TimeDelta::Max()), ttl);

        out_records->push_back(std::move(record));
      }
    }

    return DnsResponse::DNS_PARSE_OK;
  }

  void OnSortComplete(base::TimeTicks sort_start_time,
                      HostCache::Entry results,
                      bool secure,
                      bool success,
                      const AddressList& addr_list) {
    results.set_addresses(addr_list);

    if (!success) {
      OnFailure(ERR_DNS_SORT_ERROR, DnsResponse::DNS_PARSE_OK,
                results.GetOptionalTtl(), secure);
      return;
    }

    // AddressSorter prunes unusable destinations.
    if (addr_list.empty() &&
        results.text_records().value_or(std::vector<std::string>()).empty() &&
        results.hostnames().value_or(std::vector<HostPortPair>()).empty()) {
      LOG(WARNING) << "Address list empty after RFC3484 sort";
      OnFailure(ERR_NAME_NOT_RESOLVED, DnsResponse::DNS_PARSE_OK,
                results.GetOptionalTtl(), secure);
      return;
    }

    OnSuccess(results, secure);
  }

  void OnFailure(int net_error,
                 DnsResponse::Result parse_result,
                 base::Optional<base::TimeDelta> ttl,
                 bool secure) {
    DCHECK_NE(OK, net_error);

    HostCache::Entry results(net_error, HostCache::Entry::SOURCE_UNKNOWN);

    net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_IMPL_DNS_TASK,
                      base::Bind(&NetLogDnsTaskFailedCallback, results.error(),
                                 parse_result, results.CreateNetLogCallback()));

    // If we have a TTL from a previously completed transaction, use it.
    DCHECK_EQ(saved_results_.has_value(), saved_secure_.has_value());
    base::TimeDelta previous_transaction_ttl;
    if (saved_results_ && saved_results_.value().has_ttl() &&
        saved_results_.value().ttl() <
            base::TimeDelta::FromSeconds(
                std::numeric_limits<uint32_t>::max())) {
      previous_transaction_ttl = saved_results_.value().ttl();
      if (ttl)
        results.set_ttl(std::min(ttl.value(), previous_transaction_ttl));
      else
        results.set_ttl(previous_transaction_ttl);
    } else if (ttl) {
      results.set_ttl(ttl.value());
    }
    // If the earlier result was retrieved insecurely, any entry stored in the
    // cache for this transaction should be stored with an insecure key.
    if (saved_secure_ && !saved_secure_.value())
      secure = false;

    delegate_->OnDnsTaskComplete(task_start_time_, results, secure);
  }

  void OnSuccess(const HostCache::Entry& results, bool secure) {
    net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_IMPL_DNS_TASK,
                      results.CreateNetLogCallback());
    delegate_->OnDnsTaskComplete(task_start_time_, results, secure);
  }

  DnsClient* client_;
  Key key_;

  // Whether resolution may fallback to other task types (e.g. ProcTask) on
  // failure of this task.
  bool allow_fallback_resolution_;

  // The listener to the results of this DnsTask.
  Delegate* delegate_;
  const NetLogWithSource net_log_;

  std::unique_ptr<DnsTransaction> transaction1_;
  std::unique_ptr<DnsTransaction> transaction2_;

  unsigned num_completed_transactions_;

  // Result from previously completed transactions. Only set if a transaction
  // has completed while others are still in progress.
  base::Optional<HostCache::Entry> saved_results_;
  // Whether the result from the previously completed transaction was retrieved
  // securely. Only set if a transaction has completed while others are still
  // in progress.
  base::Optional<bool> saved_secure_;

  const base::TickClock* tick_clock_;
  base::TimeTicks task_start_time_;

  DISALLOW_COPY_AND_ASSIGN(DnsTask);
};

//-----------------------------------------------------------------------------

// Aggregates all Requests for the same Key. Dispatched via PriorityDispatch.
class HostResolverImpl::Job : public PrioritizedDispatcher::Job,
                              public HostResolverImpl::DnsTask::Delegate {
 public:
  // Creates new job for |key| where |request_net_log| is bound to the
  // request that spawned it.
  Job(const base::WeakPtr<HostResolverImpl>& resolver,
      const Key& key,
      RequestPriority priority,
      scoped_refptr<base::TaskRunner> proc_task_runner,
      const NetLogWithSource& source_net_log,
      const base::TickClock* tick_clock)
      : resolver_(resolver),
        key_(key),
        priority_tracker_(priority),
        proc_task_runner_(std::move(proc_task_runner)),
        had_non_speculative_request_(false),
        num_occupied_job_slots_(0),
        dns_task_error_(OK),
        tick_clock_(tick_clock),
        net_log_(
            NetLogWithSource::Make(source_net_log.net_log(),
                                   NetLogSourceType::HOST_RESOLVER_IMPL_JOB)),
        weak_ptr_factory_(this) {
    source_net_log.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_CREATE_JOB);

    net_log_.BeginEvent(NetLogEventType::HOST_RESOLVER_IMPL_JOB,
                        base::Bind(&NetLogJobCreationCallback,
                                   source_net_log.source(), &key_.hostname));
  }

  ~Job() override {
    if (is_running()) {
      // |resolver_| was destroyed with this Job still in flight.
      // Clean-up, record in the log, but don't run any callbacks.
      proc_task_ = nullptr;
      // Clean up now for nice NetLog.
      KillDnsTask();
      net_log_.EndEventWithNetErrorCode(NetLogEventType::HOST_RESOLVER_IMPL_JOB,
                                        ERR_ABORTED);
    } else if (is_queued()) {
      // |resolver_| was destroyed without running this Job.
      // TODO(szym): is there any benefit in having this distinction?
      net_log_.AddEvent(NetLogEventType::CANCELLED);
      net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_IMPL_JOB);
    }
    // else CompleteRequests logged EndEvent.
    while (!requests_.empty()) {
      // Log any remaining Requests as cancelled.
      RequestImpl* req = requests_.head()->value();
      req->RemoveFromList();
      DCHECK_EQ(this, req->job());
      LogCancelRequest(req->source_net_log());
      req->OnJobCancelled(this);
    }
  }

  // Add this job to the dispatcher.  If "at_head" is true, adds at the front
  // of the queue.
  void Schedule(bool at_head) {
    DCHECK(!is_queued());
    PrioritizedDispatcher::Handle handle;
    if (!at_head) {
      handle = resolver_->dispatcher_->Add(this, priority());
    } else {
      handle = resolver_->dispatcher_->AddAtHead(this, priority());
    }
    // The dispatcher could have started |this| in the above call to Add, which
    // could have called Schedule again. In that case |handle| will be null,
    // but |handle_| may have been set by the other nested call to Schedule.
    if (!handle.is_null()) {
      DCHECK(handle_.is_null());
      handle_ = handle;
    }
  }

  void AddRequest(RequestImpl* request) {
    DCHECK_EQ(key_.hostname, request->request_host().host());

    request->AssignJob(this);

    priority_tracker_.Add(request->priority());

    request->source_net_log().AddEvent(
        NetLogEventType::HOST_RESOLVER_IMPL_JOB_ATTACH,
        net_log_.source().ToEventParametersCallback());

    net_log_.AddEvent(
        NetLogEventType::HOST_RESOLVER_IMPL_JOB_REQUEST_ATTACH,
        base::Bind(&NetLogJobAttachCallback, request->source_net_log().source(),
                   priority()));

    if (!request->parameters().is_speculative)
      had_non_speculative_request_ = true;

    requests_.Append(request);

    UpdatePriority();
  }

  void ChangeRequestPriority(RequestImpl* req, RequestPriority priority) {
    DCHECK_EQ(key_.hostname, req->request_host().host());

    priority_tracker_.Remove(req->priority());
    req->set_priority(priority);
    priority_tracker_.Add(req->priority());
    UpdatePriority();
  }

  // Detach cancelled request. If it was the last active Request, also finishes
  // this Job.
  void CancelRequest(RequestImpl* request) {
    DCHECK_EQ(key_.hostname, request->request_host().host());
    DCHECK(!requests_.empty());

    LogCancelRequest(request->source_net_log());

    priority_tracker_.Remove(request->priority());
    net_log_.AddEvent(
        NetLogEventType::HOST_RESOLVER_IMPL_JOB_REQUEST_DETACH,
        base::Bind(&NetLogJobAttachCallback, request->source_net_log().source(),
                   priority()));

    if (num_active_requests() > 0) {
      UpdatePriority();
      request->RemoveFromList();
    } else {
      // If we were called from a Request's callback within CompleteRequests,
      // that Request could not have been cancelled, so num_active_requests()
      // could not be 0. Therefore, we are not in CompleteRequests().
      CompleteRequestsWithError(ERR_FAILED /* cancelled */);
    }
  }

  // Called from AbortAllInProgressJobs. Completes all requests and destroys
  // the job. This currently assumes the abort is due to a network change.
  // TODO This should not delete |this|.
  void Abort() {
    DCHECK(is_running());
    CompleteRequestsWithError(ERR_NETWORK_CHANGED);
  }

  // Gets a closure that will abort a DnsTask (see AbortDnsTask()) iff |this| is
  // still valid. Useful if aborting a list of Jobs as some may be cancelled
  // while aborting others.
  base::OnceClosure GetAbortDnsTaskClosure(int error, bool fallback_only) {
    return base::BindOnce(&Job::AbortDnsTask, weak_ptr_factory_.GetWeakPtr(),
                          error, fallback_only);
  }

  // If DnsTask present, abort it. Depending on task settings, either fall back
  // to ProcTask or abort the job entirely. Warning, aborting a job may cause
  // other jobs to be aborted, thus |jobs_| may be unpredictably changed by
  // calling this method.
  //
  // |error| is the net error that will be returned to requests if this method
  // results in completely aborting the job.
  void AbortDnsTask(int error, bool fallback_only) {
    if (dns_task_) {
      if (dns_task_->allow_fallback_resolution()) {
        KillDnsTask();
        dns_task_error_ = OK;
        StartProcTask();
      } else if (!fallback_only) {
        CompleteRequestsWithError(error);
      }
    }
  }

  // Called by HostResolverImpl when this job is evicted due to queue overflow.
  // Completes all requests and destroys the job.
  void OnEvicted() {
    DCHECK(!is_running());
    DCHECK(is_queued());
    handle_.Reset();

    net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_JOB_EVICTED);

    // This signals to CompleteRequests that this job never ran.
    CompleteRequestsWithError(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE);
  }

  // Attempts to serve the job from HOSTS. Returns true if succeeded and
  // this Job was destroyed.
  bool ServeFromHosts() {
    DCHECK_GT(num_active_requests(), 0u);
    base::Optional<HostCache::Entry> results = resolver_->ServeFromHosts(key());
    if (results) {
      // This will destroy the Job.
      CompleteRequests(results.value(), base::TimeDelta(),
                       true /* allow_cache */, true /* secure */);
      return true;
    }
    return false;
  }

  const Key& key() const { return key_; }

  bool is_queued() const {
    return !handle_.is_null();
  }

  bool is_running() const {
    return is_dns_running() || is_mdns_running() || is_proc_running();
  }

 private:
  void KillDnsTask() {
    if (dns_task_) {
      ReduceToOneJobSlot();
      dns_task_.reset();
    }
  }

  // Reduce the number of job slots occupied and queued in the dispatcher
  // to one. If the second Job slot is queued in the dispatcher, cancels the
  // queued job. Otherwise, the second Job has been started by the
  // PrioritizedDispatcher, so signals it is complete.
  void ReduceToOneJobSlot() {
    DCHECK_GE(num_occupied_job_slots_, 1u);
    if (is_queued()) {
      resolver_->dispatcher_->Cancel(handle_);
      handle_.Reset();
    } else if (num_occupied_job_slots_ > 1) {
      resolver_->dispatcher_->OnJobFinished();
      --num_occupied_job_slots_;
    }
    DCHECK_EQ(1u, num_occupied_job_slots_);
  }

  void UpdatePriority() {
    if (is_queued())
      handle_ = resolver_->dispatcher_->ChangePriority(handle_, priority());
  }

  // PriorityDispatch::Job:
  void Start() override {
    DCHECK_LE(num_occupied_job_slots_, 1u);

    handle_.Reset();
    ++num_occupied_job_slots_;

    if (num_occupied_job_slots_ == 2) {
      StartSecondDnsTransaction();
      return;
    }

    DCHECK(!is_running());

    net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_JOB_STARTED);

    start_time_ = tick_clock_->NowTicks();

    switch (key_.host_resolver_source) {
      case HostResolverSource::ANY:
        // Force address queries with canonname to use ProcTask to counter poor
        // CNAME support in DnsTask. See https://crbug.com/872665
        //
        // Otherwise, default to DnsTask (with allowed fallback to ProcTask for
        // address queries). But if hostname appears to be an MDNS name (ends in
        // *.local), go with ProcTask for address queries and MdnsTask for non-
        // address queries.
        if ((key_.host_resolver_flags & HOST_RESOLVER_CANONNAME) &&
            IsAddressType(key_.dns_query_type)) {
          StartProcTask();
        } else if (!ResemblesMulticastDNSName(key_.hostname)) {
          StartDnsTask(IsAddressType(
              key_.dns_query_type) /* allow_fallback_resolution */);
        } else if (IsAddressType(key_.dns_query_type)) {
          StartProcTask();
        } else {
          StartMdnsTask();
        }
        break;
      case HostResolverSource::SYSTEM:
        StartProcTask();
        break;
      case HostResolverSource::DNS:
        StartDnsTask(false /* allow_fallback_resolution */);
        break;
      case HostResolverSource::MULTICAST_DNS:
        StartMdnsTask();
        break;
      case HostResolverSource::LOCAL_ONLY:
        // If no external source allowed, a job should not be created or started
        NOTREACHED();
        break;
    }

    // Caution: Job::Start must not complete synchronously.
  }

  // TODO(szym): Since DnsTransaction does not consume threads, we can increase
  // the limits on |dispatcher_|. But in order to keep the number of
  // TaskScheduler threads low, we will need to use an "inner"
  // PrioritizedDispatcher with tighter limits.
  void StartProcTask() {
    DCHECK(!is_running());
    DCHECK(IsAddressType(key_.dns_query_type));

    proc_task_ = std::make_unique<ProcTask>(
        key_, resolver_->proc_params_,
        base::BindOnce(&Job::OnProcTaskComplete, base::Unretained(this),
                       tick_clock_->NowTicks()),
        proc_task_runner_, net_log_, tick_clock_);

    // Start() could be called from within Resolve(), hence it must NOT directly
    // call OnProcTaskComplete, for example, on synchronous failure.
    proc_task_->Start();
  }

  // Called by ProcTask when it completes.
  void OnProcTaskComplete(base::TimeTicks start_time,
                          int net_error,
                          const AddressList& addr_list) {
    DCHECK(is_proc_running());

    if (dns_task_error_ != OK) {
      // This ProcTask was a fallback resolution after a failed DnsTask.
      if (net_error == OK) {
        resolver_->OnFallbackResolve(dns_task_error_);
      }
    }

    if (ContainsIcannNameCollisionIp(addr_list))
      net_error = ERR_ICANN_NAME_COLLISION;

    base::TimeDelta ttl =
        base::TimeDelta::FromSeconds(kNegativeCacheEntryTTLSeconds);
    if (net_error == OK)
      ttl = base::TimeDelta::FromSeconds(kCacheEntryTTLSeconds);

    // Source unknown because the system resolver could have gotten it from a
    // hosts file, its own cache, a DNS lookup or somewhere else.
    // Don't store the |ttl| in cache since it's not obtained from the server.
    CompleteRequests(
        HostCache::Entry(net_error,
                         net_error == OK
                             ? AddressList::CopyWithPort(addr_list, 0)
                             : AddressList(),
                         HostCache::Entry::SOURCE_UNKNOWN),
        ttl, true /* allow_cache */, false /* secure */);
  }

  void StartDnsTask(bool allow_fallback_resolution) {
    if ((!resolver_->HaveDnsConfig() || resolver_->use_proctask_by_default_) &&
        allow_fallback_resolution) {
      // DnsClient or config is not available, but we're allowed to switch to
      // ProcTask instead.
      StartProcTask();
      return;
    }

    // Need to create the task even if we're going to post a failure instead of
    // running it, as a "started" job needs a task to be properly cleaned up.
    dns_task_.reset(new DnsTask(resolver_->dns_client_.get(), key_,
                                allow_fallback_resolution, this, net_log_,
                                tick_clock_));

    if (resolver_->HaveDnsConfig()) {
      dns_task_->StartFirstTransaction();
      // Schedule a second transaction, if needed.
      if (dns_task_->needs_two_transactions())
        Schedule(true);
    } else {
      // Cannot start a DNS task when DnsClient or config is not available.
      // Since we cannot complete synchronously from here, post a failure.
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &Job::OnDnsTaskFailure, weak_ptr_factory_.GetWeakPtr(),
              dns_task_->AsWeakPtr(), base::TimeDelta(),
              HostCache::Entry(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN),
              false /* secure */));
    }
  }

  void StartSecondDnsTransaction() {
    DCHECK(dns_task_->needs_two_transactions());
    dns_task_->StartSecondTransaction();
  }

  // Called if DnsTask fails. It is posted from StartDnsTask, so Job may be
  // deleted before this callback. In this case dns_task is deleted as well,
  // so we use it as indicator whether Job is still valid.
  void OnDnsTaskFailure(const base::WeakPtr<DnsTask>& dns_task,
                        base::TimeDelta duration,
                        const HostCache::Entry& failure_results,
                        bool secure) {
    DCHECK_NE(OK, failure_results.error());

    UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.DnsTask.FailureTime", duration);

    if (!dns_task)
      return;

    if (duration < base::TimeDelta::FromMilliseconds(10)) {
      base::UmaHistogramSparse("Net.DNS.DnsTask.ErrorBeforeFallback.Fast",
                               std::abs(failure_results.error()));
    } else {
      base::UmaHistogramSparse("Net.DNS.DnsTask.ErrorBeforeFallback.Slow",
                               std::abs(failure_results.error()));
    }
    dns_task_error_ = failure_results.error();

    // TODO(szym): Run ServeFromHosts now if nsswitch.conf says so.
    // http://crbug.com/117655

    // TODO(szym): Some net errors indicate lack of connectivity. Starting
    // ProcTask in that case is a waste of time.
    if (resolver_->allow_fallback_to_proctask_ &&
        dns_task->allow_fallback_resolution()) {
      KillDnsTask();
      StartProcTask();
    } else {
      base::TimeDelta ttl = failure_results.has_ttl()
                                ? failure_results.ttl()
                                : base::TimeDelta::FromSeconds(0);
      CompleteRequests(failure_results, ttl, true /* allow_cache */, secure);
    }
  }

  // HostResolverImpl::DnsTask::Delegate implementation:

  void OnDnsTaskComplete(base::TimeTicks start_time,
                         const HostCache::Entry& results,
                         bool secure) override {
    DCHECK(is_dns_running());

    base::TimeDelta duration = tick_clock_->NowTicks() - start_time;
    if (results.error() != OK) {
      OnDnsTaskFailure(dns_task_->AsWeakPtr(), duration, results, secure);
      return;
    }

    UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.DnsTask.SuccessTime", duration);

    resolver_->OnDnsTaskResolve();

    base::TimeDelta bounded_ttl = std::max(
        results.ttl(), base::TimeDelta::FromSeconds(kMinimumTTLSeconds));

    if (results.addresses() &&
        ContainsIcannNameCollisionIp(results.addresses().value())) {
      CompleteRequestsWithError(ERR_ICANN_NAME_COLLISION);
      return;
    }

    CompleteRequests(results, bounded_ttl, true /* allow_cache */, secure);
  }

  void OnFirstDnsTransactionComplete() override {
    DCHECK(dns_task_->needs_two_transactions());
    DCHECK_EQ(dns_task_->needs_another_transaction(), is_queued());
    // No longer need to occupy two dispatcher slots.
    ReduceToOneJobSlot();

    // We already have a job slot at the dispatcher, so if the second
    // transaction hasn't started, reuse it now instead of waiting in the queue
    // for the second slot.
    if (dns_task_->needs_another_transaction())
      dns_task_->StartSecondTransaction();
  }

  void StartMdnsTask() {
    DCHECK(!is_running());

    // No flags are supported for MDNS except
    // HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6 (which is not actually an
    // input flag).
    DCHECK_EQ(0, key_.host_resolver_flags &
                     ~HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6);

    std::vector<DnsQueryType> query_types;
    if (key_.dns_query_type == DnsQueryType::UNSPECIFIED) {
      query_types.push_back(DnsQueryType::A);
      query_types.push_back(DnsQueryType::AAAA);
    } else {
      query_types.push_back(key_.dns_query_type);
    }

    mdns_task_ = std::make_unique<HostResolverMdnsTask>(
        resolver_->GetOrCreateMdnsClient(), key_.hostname, query_types);
    mdns_task_->Start(
        base::BindOnce(&Job::OnMdnsTaskComplete, base::Unretained(this)));
  }

  void OnMdnsTaskComplete() {
    DCHECK(is_mdns_running());
    // TODO(crbug.com/846423): Consider adding MDNS-specific logging.

    HostCache::Entry results = mdns_task_->GetResults();
    if (results.addresses() &&
        ContainsIcannNameCollisionIp(results.addresses().value())) {
      CompleteRequestsWithError(ERR_ICANN_NAME_COLLISION);
    } else {
      // MDNS uses a separate cache, so skip saving result to cache.
      // TODO(crbug.com/926300): Consider merging caches.
      CompleteRequestsWithoutCache(results);
    }
  }

  URLRequestContext* url_request_context() override {
    return resolver_->url_request_context_;
  }

  void RecordJobHistograms(int error) {
    // Used in UMA_HISTOGRAM_ENUMERATION. Do not renumber entries or reuse
    // deprecated values.
    enum Category {
      RESOLVE_SUCCESS = 0,
      RESOLVE_FAIL = 1,
      RESOLVE_SPECULATIVE_SUCCESS = 2,
      RESOLVE_SPECULATIVE_FAIL = 3,
      RESOLVE_ABORT = 4,
      RESOLVE_SPECULATIVE_ABORT = 5,
      RESOLVE_MAX,  // Bounding value.
    };
    Category category = RESOLVE_MAX;  // Illegal value for later DCHECK only.

    base::TimeDelta duration = tick_clock_->NowTicks() - start_time_;
    if (error == OK) {
      if (had_non_speculative_request_) {
        category = RESOLVE_SUCCESS;
        UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.ResolveSuccessTime", duration);
        switch (key_.dns_query_type) {
          case DnsQueryType::A:
            UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.ResolveSuccessTime.IPV4",
                                         duration);
            break;
          case DnsQueryType::AAAA:
            UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.ResolveSuccessTime.IPV6",
                                         duration);
            break;
          case DnsQueryType::UNSPECIFIED:
            UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.ResolveSuccessTime.UNSPEC",
                                         duration);
            break;
          default:
            // No histogram for other query types.
            break;
        }
      } else {
        category = RESOLVE_SPECULATIVE_SUCCESS;
      }
    } else if (error == ERR_NETWORK_CHANGED ||
               error == ERR_HOST_RESOLVER_QUEUE_TOO_LARGE) {
      category = had_non_speculative_request_ ? RESOLVE_ABORT
                                              : RESOLVE_SPECULATIVE_ABORT;
    } else {
      if (had_non_speculative_request_) {
        category = RESOLVE_FAIL;
        UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.ResolveFailureTime", duration);
        switch (key_.dns_query_type) {
          case DnsQueryType::A:
            UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.ResolveFailureTime.IPV4",
                                         duration);
            break;
          case DnsQueryType::AAAA:
            UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.ResolveFailureTime.IPV6",
                                         duration);
            break;
          case DnsQueryType::UNSPECIFIED:
            UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.ResolveFailureTime.UNSPEC",
                                         duration);
            break;
          default:
            // No histogram for other query types.
            break;
        }
      } else {
        category = RESOLVE_SPECULATIVE_FAIL;
      }
    }
    DCHECK_LT(static_cast<int>(category),
              static_cast<int>(RESOLVE_MAX));  // Be sure it was set.
    UMA_HISTOGRAM_ENUMERATION("Net.DNS.ResolveCategory", category, RESOLVE_MAX);

    if (category == RESOLVE_FAIL || category == RESOLVE_ABORT) {
      if (duration < base::TimeDelta::FromMilliseconds(10))
        base::UmaHistogramSparse("Net.DNS.ResolveError.Fast", std::abs(error));
      else
        base::UmaHistogramSparse("Net.DNS.ResolveError.Slow", std::abs(error));
    }
  }

  // Performs Job's last rites. Completes all Requests. Deletes this.
  //
  // If not |allow_cache|, result will not be stored in the host cache, even if
  // result would otherwise allow doing so. Update the key to reflect |secure|,
  // which indicates whether or not the result was obtained securely.
  void CompleteRequests(const HostCache::Entry& results,
                        base::TimeDelta ttl,
                        bool allow_cache,
                        bool secure) {
    CHECK(resolver_.get());

    // This job must be removed from resolver's |jobs_| now to make room for a
    // new job with the same key in case one of the OnComplete callbacks decides
    // to spawn one. Consequently, if the job was owned by |jobs_|, the job
    // deletes itself when CompleteRequests is done.
    std::unique_ptr<Job> self_deleter = resolver_->RemoveJob(this);

    if (is_running()) {
      proc_task_ = nullptr;
      KillDnsTask();
      mdns_task_ = nullptr;

      // Signal dispatcher that a slot has opened.
      resolver_->dispatcher_->OnJobFinished();
    } else if (is_queued()) {
      resolver_->dispatcher_->Cancel(handle_);
      handle_.Reset();
    }

    if (num_active_requests() == 0) {
      net_log_.AddEvent(NetLogEventType::CANCELLED);
      net_log_.EndEventWithNetErrorCode(NetLogEventType::HOST_RESOLVER_IMPL_JOB,
                                        OK);
      return;
    }

    net_log_.EndEventWithNetErrorCode(NetLogEventType::HOST_RESOLVER_IMPL_JOB,
                                      results.error());

    DCHECK(!requests_.empty());

    bool did_complete = (results.error() != ERR_NETWORK_CHANGED) &&
                        (results.error() != ERR_HOST_RESOLVER_QUEUE_TOO_LARGE);
    if (did_complete && allow_cache) {
      Key effective_key = key_;
      effective_key.secure = secure;
      resolver_->CacheResult(effective_key, results, ttl);
    }

    RecordJobHistograms(results.error());

    // Complete all of the requests that were attached to the job and
    // detach them.
    while (!requests_.empty()) {
      RequestImpl* req = requests_.head()->value();
      req->RemoveFromList();
      DCHECK_EQ(this, req->job());
      // Update the net log and notify registered observers.
      LogFinishRequest(req->source_net_log(), results.error());
      if (did_complete) {
        // Record effective total time from creation to completion.
        resolver_->RecordTotalTime(
            req->parameters().is_speculative, false /* from_cache */,
            tick_clock_->NowTicks() - req->request_time());
      }
      if (results.error() == OK && !req->parameters().is_speculative) {
        req->set_results(
            results.CopyWithDefaultPort(req->request_host().port()));
      }
      req->OnJobCompleted(this, results.error());

      // Check if the resolver was destroyed as a result of running the
      // callback. If it was, we could continue, but we choose to bail.
      if (!resolver_.get())
        return;
    }
  }

  void CompleteRequestsWithoutCache(const HostCache::Entry& results) {
    CompleteRequests(results, base::TimeDelta(), false /* allow_cache */,
                     false /* secure */);
  }

  // Convenience wrapper for CompleteRequests in case of failure.
  void CompleteRequestsWithError(int net_error) {
    DCHECK_NE(OK, net_error);
    CompleteRequests(
        HostCache::Entry(net_error, HostCache::Entry::SOURCE_UNKNOWN),
        base::TimeDelta(), true /* allow_cache */, false /* secure */);
  }

  RequestPriority priority() const override {
    return priority_tracker_.highest_priority();
  }

  // Number of non-canceled requests in |requests_|.
  size_t num_active_requests() const {
    return priority_tracker_.total_count();
  }

  bool is_dns_running() const { return !!dns_task_; }

  bool is_mdns_running() const { return !!mdns_task_; }

  bool is_proc_running() const { return !!proc_task_; }

  base::WeakPtr<HostResolverImpl> resolver_;

  Key key_;

  // Tracks the highest priority across |requests_|.
  PriorityTracker priority_tracker_;

  // Task runner used for HostResolverProc.
  scoped_refptr<base::TaskRunner> proc_task_runner_;

  bool had_non_speculative_request_;

  // Number of slots occupied by this Job in resolver's PrioritizedDispatcher.
  unsigned num_occupied_job_slots_;

  // Result of DnsTask.
  int dns_task_error_;

  const base::TickClock* tick_clock_;
  base::TimeTicks start_time_;

  NetLogWithSource net_log_;

  // Resolves the host using a HostResolverProc.
  std::unique_ptr<ProcTask> proc_task_;

  // Resolves the host using a DnsTransaction.
  std::unique_ptr<DnsTask> dns_task_;

  // Resolves the host using MDnsClient.
  std::unique_ptr<HostResolverMdnsTask> mdns_task_;

  // All Requests waiting for the result of this Job. Some can be canceled.
  base::LinkedList<RequestImpl> requests_;

  // A handle used in |HostResolverImpl::dispatcher_|.
  PrioritizedDispatcher::Handle handle_;

  base::WeakPtrFactory<Job> weak_ptr_factory_;
};

//-----------------------------------------------------------------------------

HostResolverImpl::HostResolverImpl(const Options& options, NetLog* net_log)
    : max_queued_jobs_(0),
      proc_params_(NULL, options.max_retry_attempts),
      net_log_(net_log),
      received_dns_config_(false),
      num_dns_failures_(0),
      assume_ipv6_failure_on_wifi_(false),
      use_local_ipv6_(false),
      last_ipv6_probe_result_(true),
      additional_resolver_flags_(0),
      use_proctask_by_default_(false),
      allow_fallback_to_proctask_(true),
      url_request_context_(nullptr),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      weak_ptr_factory_(this),
      probe_weak_ptr_factory_(this) {
  if (options.enable_caching)
    cache_ = HostCache::CreateDefaultCache();

  PrioritizedDispatcher::Limits job_limits = options.GetDispatcherLimits();
  dispatcher_.reset(new PrioritizedDispatcher(job_limits));
  max_queued_jobs_ = job_limits.total_jobs * 100u;

  DCHECK_GE(dispatcher_->num_priorities(), static_cast<size_t>(NUM_PRIORITIES));

  proc_task_runner_ = base::CreateTaskRunnerWithTraits(
      {base::MayBlock(), priority_mode.Get(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

#if defined(OS_WIN)
  EnsureWinsockInit();
#endif
#if (defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)) || \
    defined(OS_FUCHSIA)
  RunLoopbackProbeJob();
#endif
  NetworkChangeNotifier::AddIPAddressObserver(this);
  NetworkChangeNotifier::AddConnectionTypeObserver(this);
  NetworkChangeNotifier::AddDNSObserver(this);
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD) && \
    !defined(OS_ANDROID)
  EnsureDnsReloaderInit();
#endif

  OnConnectionTypeChanged(NetworkChangeNotifier::GetConnectionType());

  {
    DnsConfig dns_config = GetBaseDnsConfig(false);
    // Conservatively assume local IPv6 is needed when DnsConfig is not valid.
    use_local_ipv6_ = !dns_config.IsValid() || dns_config.use_local_ipv6;
    UpdateModeForHistogram(dns_config);
  }

  allow_fallback_to_proctask_ = !ConfigureAsyncDnsNoFallbackFieldTrial();
}

HostResolverImpl::~HostResolverImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Prevent the dispatcher from starting new jobs.
  dispatcher_->SetLimitsToZero();
  // It's now safe for Jobs to call KillDnsTask on destruction, because
  // OnJobComplete will not start any new jobs.
  jobs_.clear();

  NetworkChangeNotifier::RemoveIPAddressObserver(this);
  NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  NetworkChangeNotifier::RemoveDNSObserver(this);
}

void HostResolverImpl::SetDnsClient(std::unique_ptr<DnsClient> dns_client) {
  // DnsClient and config must be updated before aborting DnsTasks, since doing
  // so may start new jobs.
  dns_client_ = std::move(dns_client);
  if (dns_client_ && !dns_client_->GetConfig() &&
      num_dns_failures_ < kMaximumDnsFailures) {
    dns_client_->SetConfig(GetBaseDnsConfig(false));
    num_dns_failures_ = 0;
  }

  AbortDnsTasks(ERR_NETWORK_CHANGED, false /* fallback_only */);
  DnsConfig dns_config;
  if (!HaveDnsConfig())
    // UpdateModeForHistogram() needs to know the DnsConfig when
    // !HaveDnsConfig()
    dns_config = GetBaseDnsConfig(false);
  UpdateModeForHistogram(dns_config);
}

std::unique_ptr<HostResolver::ResolveHostRequest>
HostResolverImpl::CreateRequest(
    const HostPortPair& host,
    const NetLogWithSource& net_log,
    const base::Optional<ResolveHostParameters>& optional_parameters) {
  return std::make_unique<RequestImpl>(net_log, host, optional_parameters,
                                       weak_ptr_factory_.GetWeakPtr());
}

std::unique_ptr<HostResolver::MdnsListener>
HostResolverImpl::CreateMdnsListener(const HostPortPair& host,
                                     DnsQueryType query_type) {
  DCHECK_NE(DnsQueryType::UNSPECIFIED, query_type);

  auto listener =
      std::make_unique<HostResolverMdnsListenerImpl>(host, query_type);

  MDnsClient* client = GetOrCreateMdnsClient();
  std::unique_ptr<net::MDnsListener> inner_listener = client->CreateListener(
      DnsQueryTypeToQtype(query_type), host.host(), listener.get());

  listener->set_inner_listener(std::move(inner_listener));
  return listener;
}

void HostResolverImpl::SetDnsClientEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if defined(ENABLE_BUILT_IN_DNS)
  if (enabled && !dns_client_) {
    SetDnsClient(DnsClient::CreateClient(net_log_));
  } else if (!enabled && dns_client_) {
    SetDnsClient(std::unique_ptr<DnsClient>());
  }
#endif
}

HostCache* HostResolverImpl::GetHostCache() {
  return cache_.get();
}

bool HostResolverImpl::HasCached(base::StringPiece hostname,
                                 HostCache::Entry::Source* source_out,
                                 HostCache::EntryStaleness* stale_out,
                                 bool* secure_out) const {
  if (!cache_)
    return false;

  const HostCache::Key* key =
      cache_->GetMatchingKey(hostname, source_out, stale_out);
  if (key && secure_out != nullptr)
    *secure_out = key->secure;
  return !!key;
}

std::unique_ptr<base::Value> HostResolverImpl::GetDnsConfigAsValue() const {
  // Check if async DNS is disabled.
  if (!dns_client_.get())
    return nullptr;

  // Check if async DNS is enabled, but we currently have no configuration
  // for it.
  const DnsConfig* dns_config = dns_client_->GetConfig();
  if (!dns_config)
    return std::make_unique<base::DictionaryValue>();

  return dns_config->ToValue();
}

size_t HostResolverImpl::LastRestoredCacheSize() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return cache_ ? cache_->last_restore_size() : 0;
}

size_t HostResolverImpl::CacheSize() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return cache_ ? cache_->size() : 0;
}

void HostResolverImpl::SetNoIPv6OnWifi(bool no_ipv6_on_wifi) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  assume_ipv6_failure_on_wifi_ = no_ipv6_on_wifi;
}

bool HostResolverImpl::GetNoIPv6OnWifi() {
  return assume_ipv6_failure_on_wifi_;
}

void HostResolverImpl::SetDnsConfigOverrides(
    const DnsConfigOverrides& overrides) {
  if (dns_config_overrides_ == overrides)
    return;

  dns_config_overrides_ = overrides;
  if (dns_client_.get())
    UpdateDNSConfig(true);
}

void HostResolverImpl::SetRequestContext(URLRequestContext* context) {
  if (context != url_request_context_) {
    url_request_context_ = context;
  }
}

const std::vector<DnsConfig::DnsOverHttpsServerConfig>*
HostResolverImpl::GetDnsOverHttpsServersForTesting() const {
  if (!dns_config_overrides_.dns_over_https_servers ||
      dns_config_overrides_.dns_over_https_servers.value().empty()) {
    return nullptr;
  }
  return &dns_config_overrides_.dns_over_https_servers.value();
}

void HostResolverImpl::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
  cache_->set_tick_clock_for_testing(tick_clock);
}

void HostResolverImpl::SetMaxQueuedJobsForTesting(size_t value) {
  DCHECK_EQ(0u, dispatcher_->num_queued_jobs());
  DCHECK_GE(value, 0u);
  max_queued_jobs_ = value;
}

void HostResolverImpl::SetHaveOnlyLoopbackAddresses(bool result) {
  if (result) {
    additional_resolver_flags_ |= HOST_RESOLVER_LOOPBACK_ONLY;
  } else {
    additional_resolver_flags_ &= ~HOST_RESOLVER_LOOPBACK_ONLY;
  }
}

void HostResolverImpl::SetMdnsSocketFactoryForTesting(
    std::unique_ptr<MDnsSocketFactory> socket_factory) {
  DCHECK(!mdns_client_);
  mdns_socket_factory_ = std::move(socket_factory);
}

void HostResolverImpl::SetMdnsClientForTesting(
    std::unique_ptr<MDnsClient> client) {
  mdns_client_ = std::move(client);
}

void HostResolverImpl::SetBaseDnsConfigForTesting(
    const DnsConfig& base_config) {
  test_base_config_ = base_config;
  UpdateDNSConfig(true);
}

void HostResolverImpl::SetTaskRunnerForTesting(
    scoped_refptr<base::TaskRunner> task_runner) {
  proc_task_runner_ = std::move(task_runner);
}

int HostResolverImpl::Resolve(RequestImpl* request) {
  // Request should not yet have a scheduled Job.
  DCHECK(!request->job());
  // Request may only be resolved once.
  DCHECK(!request->complete());
  // MDNS requests do not support skipping cache or stale lookups.
  // TODO(crbug.com/926300): Either add support for skipping the MDNS cache, or
  // merge to use the normal host cache for MDNS requests.
  DCHECK(request->parameters().source != HostResolverSource::MULTICAST_DNS ||
         request->parameters().cache_usage ==
             ResolveHostParameters::CacheUsage::ALLOWED);

  request->set_request_time(tick_clock_->NowTicks());

  LogStartRequest(request->source_net_log(), request->request_host());

  Key key;
  base::Optional<HostCache::EntryStaleness> stale_info;
  HostCache::Entry results = ResolveLocally(
      request->request_host().host(), request->parameters().dns_query_type,
      request->parameters().source, request->host_resolver_flags(),
      request->parameters().cache_usage, request->source_net_log(), &key,
      &stale_info);
  if (results.error() != ERR_DNS_CACHE_MISS ||
      request->parameters().source == HostResolverSource::LOCAL_ONLY) {
    if (results.error() == OK && !request->parameters().is_speculative) {
      request->set_results(
          results.CopyWithDefaultPort(request->request_host().port()));
    }
    if (stale_info && !request->parameters().is_speculative)
      request->set_stale_info(std::move(stale_info).value());
    LogFinishRequest(request->source_net_log(), results.error());
    RecordTotalTime(request->parameters().is_speculative, true /* from_cache */,
                    base::TimeDelta());
    return results.error();
  }

  int rv = CreateAndStartJob(key, request);
  // At this point, expect only async or errors.
  DCHECK_NE(OK, rv);

  return rv;
}

HostCache::Entry HostResolverImpl::ResolveLocally(
    const std::string& hostname,
    DnsQueryType dns_query_type,
    HostResolverSource source,
    HostResolverFlags flags,
    ResolveHostParameters::CacheUsage cache_usage,
    const NetLogWithSource& source_net_log,
    Key* out_key,
    base::Optional<HostCache::EntryStaleness>* out_stale_info) {
  DCHECK(out_stale_info);
  *out_stale_info = base::nullopt;

  IPAddress ip_address;
  IPAddress* ip_address_ptr = nullptr;
  if (ip_address.AssignFromIPLiteral(hostname)) {
    ip_address_ptr = &ip_address;
  } else {
    // Check that the caller supplied a valid hostname to resolve. For
    // MULTICAST_DNS, we are less restrictive.
    // TODO(ericorth): Control validation based on an explicit flag rather
    // than implicitly based on |source|.
    const bool is_valid_hostname = source == HostResolverSource::MULTICAST_DNS
                                       ? IsValidUnrestrictedDNSDomain(hostname)
                                       : IsValidDNSDomain(hostname);
    if (!is_valid_hostname) {
      return HostCache::Entry(ERR_NAME_NOT_RESOLVED,
                              HostCache::Entry::SOURCE_UNKNOWN);
    }
  }

  // Build a key that identifies the request in the cache and in the
  // outstanding jobs map. If this key is used in the future to store an entry
  // in the cache, it will be modified first to indicate whether the result was
  // obtained securely or not.
  *out_key = GetEffectiveKeyForRequest(hostname, dns_query_type, source, flags,
                                       ip_address_ptr, source_net_log);

  // The result of |getaddrinfo| for empty hosts is inconsistent across systems.
  // On Windows it gives the default interface's address, whereas on Linux it
  // gives an error. We will make it fail on all platforms for consistency.
  if (hostname.empty() || hostname.size() > kMaxHostLength) {
    return HostCache::Entry(ERR_NAME_NOT_RESOLVED,
                            HostCache::Entry::SOURCE_UNKNOWN);
  }

  base::Optional<HostCache::Entry> resolved =
      ResolveAsIP(*out_key, ip_address_ptr);
  if (resolved)
    return resolved.value();

  // Special-case localhost names, as per the recommendations in
  // https://tools.ietf.org/html/draft-west-let-localhost-be-localhost.
  resolved = ServeLocalhost(*out_key);
  if (resolved)
    return resolved.value();

  if (cache_usage == ResolveHostParameters::CacheUsage::ALLOWED ||
      cache_usage == ResolveHostParameters::CacheUsage::STALE_ALLOWED) {
    resolved = ServeFromCache(
        *out_key,
        cache_usage == ResolveHostParameters::CacheUsage::STALE_ALLOWED,
        out_stale_info);
    if (resolved) {
      DCHECK(out_stale_info->has_value());
      source_net_log.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_CACHE_HIT,
                              resolved.value().CreateNetLogCallback());
      // |ServeFromCache()| will update |*stale_info| as needed.
      return resolved.value();
    }
    DCHECK(!out_stale_info->has_value());
  }

  // TODO(szym): Do not do this if nsswitch.conf instructs not to.
  // http://crbug.com/117655
  resolved = ServeFromHosts(*out_key);
  if (resolved) {
    source_net_log.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_HOSTS_HIT,
                            resolved.value().CreateNetLogCallback());
    return resolved.value();
  }

  return HostCache::Entry(ERR_DNS_CACHE_MISS, HostCache::Entry::SOURCE_UNKNOWN);
}

int HostResolverImpl::CreateAndStartJob(const Key& key, RequestImpl* request) {
  auto jobit = jobs_.find(key);
  Job* job;
  if (jobit == jobs_.end()) {
    auto new_job = std::make_unique<Job>(
        weak_ptr_factory_.GetWeakPtr(), key, request->priority(),
        proc_task_runner_, request->source_net_log(), tick_clock_);
    job = new_job.get();
    new_job->Schedule(false);

    // Check for queue overflow.
    if (dispatcher_->num_queued_jobs() > max_queued_jobs_) {
      Job* evicted = static_cast<Job*>(dispatcher_->EvictOldestLowest());
      DCHECK(evicted);
      evicted->OnEvicted();
      if (evicted == new_job.get()) {
        LogFinishRequest(request->source_net_log(),
                         ERR_HOST_RESOLVER_QUEUE_TOO_LARGE);
        return ERR_HOST_RESOLVER_QUEUE_TOO_LARGE;
      }
    }
    jobs_[key] = std::move(new_job);
  } else {
    job = jobit->second.get();
  }

  // Can't complete synchronously. Attach request and job to each other.
  job->AddRequest(request);
  return ERR_IO_PENDING;
}

base::Optional<HostCache::Entry> HostResolverImpl::ResolveAsIP(
    const Key& key,
    const IPAddress* ip_address) {
  if (ip_address == nullptr || !IsAddressType(key.dns_query_type))
    return base::nullopt;

  AddressFamily family = GetAddressFamily(*ip_address);
  if (key.dns_query_type != DnsQueryType::UNSPECIFIED &&
      key.dns_query_type != AddressFamilyToDnsQueryType(family)) {
    // Don't return IPv6 addresses for IPv4 queries, and vice versa.
    return HostCache::Entry(ERR_NAME_NOT_RESOLVED,
                            HostCache::Entry::SOURCE_UNKNOWN);
  }

  AddressList addresses = AddressList::CreateFromIPAddress(*ip_address, 0);
  if (key.host_resolver_flags & HOST_RESOLVER_CANONNAME)
    addresses.SetDefaultCanonicalName();
  return HostCache::Entry(OK, std::move(addresses),
                          HostCache::Entry::SOURCE_UNKNOWN);
}

base::Optional<HostCache::Entry> HostResolverImpl::ServeFromCache(
    const Key& key,
    bool allow_stale,
    base::Optional<HostCache::EntryStaleness>* out_stale_info) {
  DCHECK(out_stale_info);
  *out_stale_info = base::nullopt;

  if (!cache_.get())
    return base::nullopt;

  // Local-only requests search the cache for non-local-only results.
  Key effective_key = key;
  if (effective_key.host_resolver_source == HostResolverSource::LOCAL_ONLY)
    effective_key.host_resolver_source = HostResolverSource::ANY;

  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result;
  HostCache::EntryStaleness staleness;
  if (allow_stale) {
    cache_result = cache_->LookupStale(effective_key, tick_clock_->NowTicks(),
                                       &staleness, true /* ignore_secure */);
  } else {
    cache_result = cache_->Lookup(effective_key, tick_clock_->NowTicks(),
                                  true /* ignore_secure */);
    staleness = HostCache::kNotStale;
  }
  if (!cache_result)
    return base::nullopt;

  *out_stale_info = std::move(staleness);
  return cache_result->second;
}

base::Optional<HostCache::Entry> HostResolverImpl::ServeFromHosts(
    const Key& key) {
  if (!HaveDnsConfig() || !IsAddressType(key.dns_query_type))
    return base::nullopt;

  // HOSTS lookups are case-insensitive.
  std::string hostname = base::ToLowerASCII(key.hostname);

  const DnsHosts& hosts = dns_client_->GetConfig()->hosts;

  // If |address_family| is ADDRESS_FAMILY_UNSPECIFIED other implementations
  // (glibc and c-ares) return the first matching line. We have more
  // flexibility, but lose implicit ordering.
  // We prefer IPv6 because "happy eyeballs" will fall back to IPv4 if
  // necessary.
  AddressList addresses;
  if (key.dns_query_type == DnsQueryType::AAAA ||
      key.dns_query_type == DnsQueryType::UNSPECIFIED) {
    auto it = hosts.find(DnsHostsKey(hostname, ADDRESS_FAMILY_IPV6));
    if (it != hosts.end())
      addresses.push_back(IPEndPoint(it->second, 0));
  }

  if (key.dns_query_type == DnsQueryType::A ||
      key.dns_query_type == DnsQueryType::UNSPECIFIED) {
    auto it = hosts.find(DnsHostsKey(hostname, ADDRESS_FAMILY_IPV4));
    if (it != hosts.end())
      addresses.push_back(IPEndPoint(it->second, 0));
  }

  // If got only loopback addresses and the family was restricted, resolve
  // again, without restrictions. See SystemHostResolverCall for rationale.
  if ((key.host_resolver_flags &
       HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6) &&
      IsAllIPv4Loopback(addresses)) {
    Key new_key(key);
    new_key.dns_query_type = DnsQueryType::UNSPECIFIED;
    new_key.host_resolver_flags &=
        ~HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
    return ServeFromHosts(new_key);
  }

  if (!addresses.empty()) {
    return HostCache::Entry(OK, std::move(addresses),
                            HostCache::Entry::SOURCE_HOSTS);
  }

  return base::nullopt;
}

base::Optional<HostCache::Entry> HostResolverImpl::ServeLocalhost(
    const Key& key) {
  AddressList resolved_addresses;
  if (!IsAddressType(key.dns_query_type) ||
      !ResolveLocalHostname(key.hostname, &resolved_addresses)) {
    return base::nullopt;
  }

  AddressList filtered_addresses;
  for (const auto& address : resolved_addresses) {
    // Include the address if:
    // - caller didn't specify an address family, or
    // - caller specifically asked for the address family of this address, or
    // - this is an IPv6 address and caller specifically asked for IPv4 due
    //   to lack of detected IPv6 support. (See SystemHostResolverCall for
    //   rationale).
    if (key.dns_query_type == DnsQueryType::UNSPECIFIED ||
        HostResolver::DnsQueryTypeToAddressFamily(key.dns_query_type) ==
            address.GetFamily() ||
        (address.GetFamily() == ADDRESS_FAMILY_IPV6 &&
         key.dns_query_type == DnsQueryType::A &&
         (key.host_resolver_flags &
          HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6))) {
      filtered_addresses.push_back(address);
    }
  }

  return HostCache::Entry(OK, std::move(filtered_addresses),
                          HostCache::Entry::SOURCE_UNKNOWN);
}

void HostResolverImpl::CacheResult(const Key& key,
                                   const HostCache::Entry& entry,
                                   base::TimeDelta ttl) {
  // Don't cache an error unless it has a positive TTL.
  if (cache_.get() && (entry.error() == OK || ttl > base::TimeDelta()))
    cache_->Set(key, entry, tick_clock_->NowTicks(), ttl);
}

// Record time from Request creation until a valid DNS response.
void HostResolverImpl::RecordTotalTime(bool speculative,
                                       bool from_cache,
                                       base::TimeDelta duration) const {
  if (!speculative) {
    UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.TotalTime", duration);

    switch (mode_for_histogram_) {
      case MODE_FOR_HISTOGRAM_SYSTEM:
        UMA_HISTOGRAM_MEDIUM_TIMES("Net.DNS.TotalTimeTyped.System", duration);
        break;
      case MODE_FOR_HISTOGRAM_SYSTEM_SUPPORTS_DOH:
        UMA_HISTOGRAM_MEDIUM_TIMES("Net.DNS.TotalTimeTyped.SystemSupportsDoh",
                                   duration);
        break;
      case MODE_FOR_HISTOGRAM_SYSTEM_PRIVATE_DNS:
        UMA_HISTOGRAM_MEDIUM_TIMES("Net.DNS.TotalTimeTyped.SystemPrivate",
                                   duration);
        break;
      case MODE_FOR_HISTOGRAM_ASYNC_DNS:
        UMA_HISTOGRAM_MEDIUM_TIMES("Net.DNS.TotalTimeTyped.Async", duration);
        break;
      case MODE_FOR_HISTOGRAM_ASYNC_DNS_PRIVATE_SUPPORTS_DOH:
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Net.DNS.TotalTimeTyped.AsyncPrivateSupportsDoh", duration);
        break;
    }

    if (!from_cache)
      UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.TotalTimeNotCached", duration);
  }
}

std::unique_ptr<HostResolverImpl::Job> HostResolverImpl::RemoveJob(Job* job) {
  DCHECK(job);
  std::unique_ptr<Job> retval;
  auto it = jobs_.find(job->key());
  if (it != jobs_.end() && it->second.get() == job) {
    it->second.swap(retval);
    jobs_.erase(it);
  }
  return retval;
}

HostResolverImpl::Key HostResolverImpl::GetEffectiveKeyForRequest(
    const std::string& hostname,
    DnsQueryType dns_query_type,
    HostResolverSource source,
    HostResolverFlags flags,
    const IPAddress* ip_address,
    const NetLogWithSource& net_log) {
  HostResolverFlags effective_flags = flags | additional_resolver_flags_;

  DnsQueryType effective_query_type = dns_query_type;

  if (effective_query_type == DnsQueryType::UNSPECIFIED &&
      // When resolving IPv4 literals, there's no need to probe for IPv6.
      // When resolving IPv6 literals, there's no benefit to artificially
      // limiting our resolution based on a probe.  Prior logic ensures
      // that this query is UNSPECIFIED (see effective_query_type check above)
      // so the code requesting the resolution should be amenable to receiving a
      // IPv6 resolution.
      !use_local_ipv6_ && ip_address == nullptr && !IsIPv6Reachable(net_log)) {
    effective_query_type = DnsQueryType::A;
    effective_flags |= HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  }

  return Key(hostname, effective_query_type, effective_flags, source);
}

bool HostResolverImpl::IsIPv6Reachable(const NetLogWithSource& net_log) {
  // Don't bother checking if the device is on WiFi and IPv6 is assumed to not
  // work on WiFi.
  if (assume_ipv6_failure_on_wifi_ &&
      NetworkChangeNotifier::GetConnectionType() ==
          NetworkChangeNotifier::CONNECTION_WIFI) {
    return false;
  }

  // Cache the result for kIPv6ProbePeriodMs (measured from after
  // IsGloballyReachable() completes).
  bool cached = true;
  if ((tick_clock_->NowTicks() - last_ipv6_probe_time_).InMilliseconds() >
      kIPv6ProbePeriodMs) {
    last_ipv6_probe_result_ =
        IsGloballyReachable(IPAddress(kIPv6ProbeAddress), net_log);
    last_ipv6_probe_time_ = tick_clock_->NowTicks();
    cached = false;
  }
  net_log.AddEvent(NetLogEventType::HOST_RESOLVER_IMPL_IPV6_REACHABILITY_CHECK,
                   base::Bind(&NetLogIPv6AvailableCallback,
                              last_ipv6_probe_result_, cached));
  return last_ipv6_probe_result_;
}

bool HostResolverImpl::IsGloballyReachable(const IPAddress& dest,
                                           const NetLogWithSource& net_log) {
  std::unique_ptr<DatagramClientSocket> socket(
      ClientSocketFactory::GetDefaultFactory()->CreateDatagramClientSocket(
          DatagramSocket::DEFAULT_BIND, net_log.net_log(), net_log.source()));
  int rv = socket->Connect(IPEndPoint(dest, 53));
  if (rv != OK)
    return false;
  IPEndPoint endpoint;
  rv = socket->GetLocalAddress(&endpoint);
  if (rv != OK)
    return false;
  DCHECK_EQ(ADDRESS_FAMILY_IPV6, endpoint.GetFamily());
  const IPAddress& address = endpoint.address();

  bool is_link_local =
      (address.bytes()[0] == 0xFE) && ((address.bytes()[1] & 0xC0) == 0x80);
  if (is_link_local)
    return false;

  const uint8_t kTeredoPrefix[] = {0x20, 0x01, 0, 0};
  if (IPAddressStartsWith(address, kTeredoPrefix))
    return false;

  return true;
}

void HostResolverImpl::RunLoopbackProbeJob() {
  // Run this asynchronously as it can take 40-100ms and should not block
  // initialization.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&HaveOnlyLoopbackAddresses),
      base::BindOnce(&HostResolverImpl::SetHaveOnlyLoopbackAddresses,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HostResolverImpl::AbortAllInProgressJobs() {
  // In Abort, a Request callback could spawn new Jobs with matching keys, so
  // first collect and remove all running jobs from |jobs_|.
  std::vector<std::unique_ptr<Job>> jobs_to_abort;
  for (auto it = jobs_.begin(); it != jobs_.end();) {
    Job* job = it->second.get();
    if (job->is_running()) {
      jobs_to_abort.push_back(std::move(it->second));
      jobs_.erase(it++);
    } else {
      DCHECK(job->is_queued());
      ++it;
    }
  }

  // Pause the dispatcher so it won't start any new dispatcher jobs while
  // aborting the old ones.  This is needed so that it won't start the second
  // DnsTransaction for a job in |jobs_to_abort| if the DnsConfig just became
  // invalid.
  PrioritizedDispatcher::Limits limits = dispatcher_->GetLimits();
  dispatcher_->SetLimits(
      PrioritizedDispatcher::Limits(limits.reserved_slots.size(), 0));

  // Life check to bail once |this| is deleted.
  base::WeakPtr<HostResolverImpl> self = weak_ptr_factory_.GetWeakPtr();

  // Then Abort them.
  for (size_t i = 0; self.get() && i < jobs_to_abort.size(); ++i) {
    jobs_to_abort[i]->Abort();
  }

  if (self)
    dispatcher_->SetLimits(limits);
}

void HostResolverImpl::AbortDnsTasks(int error, bool fallback_only) {
  // Aborting jobs potentially modifies |jobs_| and may even delete some jobs.
  // Create safe closures of all current jobs.
  std::vector<base::OnceClosure> job_abort_closures;
  for (auto& job : jobs_) {
    job_abort_closures.push_back(
        job.second->GetAbortDnsTaskClosure(error, fallback_only));
  }

  // Pause the dispatcher so it won't start any new dispatcher jobs while
  // aborting the old ones.  This is needed so that it won't start the second
  // DnsTransaction for a job if the DnsConfig just changed.
  PrioritizedDispatcher::Limits limits = dispatcher_->GetLimits();
  dispatcher_->SetLimits(
      PrioritizedDispatcher::Limits(limits.reserved_slots.size(), 0));

  for (base::OnceClosure& closure : job_abort_closures)
    std::move(closure).Run();

  dispatcher_->SetLimits(limits);
}

void HostResolverImpl::TryServingAllJobsFromHosts() {
  if (!HaveDnsConfig())
    return;

  // TODO(szym): Do not do this if nsswitch.conf instructs not to.
  // http://crbug.com/117655

  // Life check to bail once |this| is deleted.
  base::WeakPtr<HostResolverImpl> self = weak_ptr_factory_.GetWeakPtr();

  for (auto it = jobs_.begin(); self.get() && it != jobs_.end();) {
    Job* job = it->second.get();
    ++it;
    // This could remove |job| from |jobs_|, but iterator will remain valid.
    job->ServeFromHosts();
  }
}

void HostResolverImpl::OnIPAddressChanged() {
  last_ipv6_probe_time_ = base::TimeTicks();
  // Abandon all ProbeJobs.
  probe_weak_ptr_factory_.InvalidateWeakPtrs();
  if (cache_.get())
    cache_->OnNetworkChange();
#if (defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)) || \
    defined(OS_FUCHSIA)
  RunLoopbackProbeJob();
#endif
  AbortAllInProgressJobs();
  // |this| may be deleted inside AbortAllInProgressJobs().
}

void HostResolverImpl::OnConnectionTypeChanged(
    NetworkChangeNotifier::ConnectionType type) {
  proc_params_.unresponsive_delay =
      GetTimeDeltaForConnectionTypeFromFieldTrialOrDefault(
          "DnsUnresponsiveDelayMsByConnectionType",
          ProcTaskParams::kDnsDefaultUnresponsiveDelay, type);
}

void HostResolverImpl::OnInitialDNSConfigRead() {
  UpdateDNSConfig(false);
}

void HostResolverImpl::OnDNSChanged() {
  // Ignore changes if we're using a test config or if we have overriding
  // configuration that overrides everything from the base config.
  if (test_base_config_ || dns_config_overrides_.OverridesEverything())
    return;

  UpdateDNSConfig(true);
}

DnsConfig HostResolverImpl::GetBaseDnsConfig(bool log_to_net_log) {
  DnsConfig dns_config;

  // Skip retrieving the base config if all values will be overridden.
  if (!dns_config_overrides_.OverridesEverything()) {
    if (test_base_config_) {
      dns_config = test_base_config_.value();
    } else {
      NetworkChangeNotifier::GetDnsConfig(&dns_config);
    }

    if (log_to_net_log && net_log_) {
      net_log_->AddGlobalEntry(
          NetLogEventType::DNS_CONFIG_CHANGED,
          base::BindRepeating(&NetLogDnsConfigCallback, &dns_config));
    }

    // TODO(szym): Remove once http://crbug.com/137914 is resolved.
    received_dns_config_ = dns_config.IsValid();
  }

  return dns_config_overrides_.ApplyOverrides(dns_config);
}

void HostResolverImpl::UpdateDNSConfig(bool config_changed) {
  DnsConfig dns_config = GetBaseDnsConfig(true);

  // Conservatively assume local IPv6 is needed when DnsConfig is not valid.
  use_local_ipv6_ = !dns_config.IsValid() || dns_config.use_local_ipv6;

  num_dns_failures_ = 0;

  // We want a new DnsSession in place, before we Abort running Jobs, so that
  // the newly started jobs use the new config.
  if (dns_client_.get()) {
    // Make sure that if the update is an initial read, not a change, there
    // wasn't already a DnsConfig or it's the same one.
    DCHECK(config_changed || !dns_client_->GetConfig() ||
           dns_client_->GetConfig()->Equals(dns_config));
    dns_client_->SetConfig(dns_config);
  }
  use_proctask_by_default_ = false;

  if (config_changed) {
    // If the DNS server has changed, existing cached info could be wrong so we
    // have to expire our internal cache :( Note that OS level DNS caches, such
    // as NSCD's cache should be dropped automatically by the OS when
    // resolv.conf changes so we don't need to do anything to clear that cache.
    if (cache_.get())
      cache_->OnNetworkChange();

    // Life check to bail once |this| is deleted.
    base::WeakPtr<HostResolverImpl> self = weak_ptr_factory_.GetWeakPtr();

    // Existing jobs will have been sent to the original server so they need to
    // be aborted.
    AbortAllInProgressJobs();

    // |this| may be deleted inside AbortAllInProgressJobs().
    if (self.get())
      TryServingAllJobsFromHosts();
  }

  UpdateModeForHistogram(dns_config);
}

bool HostResolverImpl::HaveDnsConfig() const {
  // Use DnsClient only if it's fully configured and there is no override by
  // ScopedDefaultHostResolverProc.
  // The alternative is to use NetworkChangeNotifier to override DnsConfig,
  // but that would introduce construction order requirements for NCN and SDHRP.
  return dns_client_ && dns_client_->GetConfig() &&
         (proc_params_.resolver_proc || !HostResolverProc::GetDefault());
}

void HostResolverImpl::OnDnsTaskResolve() {
  DCHECK(dns_client_);
  num_dns_failures_ = 0;
}

void HostResolverImpl::OnFallbackResolve(int dns_task_error) {
  DCHECK(dns_client_);
  DCHECK_NE(OK, dns_task_error);

  ++num_dns_failures_;
  if (num_dns_failures_ < kMaximumDnsFailures)
    return;

  // Force fallback until the next DNS change.  Must be done before aborting
  // DnsTasks, since doing so may start new jobs.  Do not fully clear out or
  // disable the DnsClient as some requests (e.g. those specifying DNS source)
  // are not allowed to fallback and will continue using DnsTask.
  use_proctask_by_default_ = true;

  // Fallback all fallback-allowed DnsTasks to ProcTasks.
  AbortDnsTasks(ERR_FAILED, true /* fallback_only */);
}

MDnsClient* HostResolverImpl::GetOrCreateMdnsClient() {
#if BUILDFLAG(ENABLE_MDNS)
  if (!mdns_client_) {
    if (!mdns_socket_factory_)
      mdns_socket_factory_ = std::make_unique<MDnsSocketFactoryImpl>(net_log_);

    mdns_client_ = MDnsClient::CreateDefault();
    mdns_client_->StartListening(mdns_socket_factory_.get());
  }

  DCHECK(mdns_client_->IsListening());
  return mdns_client_.get();
#else
  // Should not request MDNS resoltuion unless MDNS is enabled.
  NOTREACHED();
  return nullptr;
#endif
}

void HostResolverImpl::UpdateModeForHistogram(const DnsConfig& dns_config) {
  // Resolving with Async DNS resolver?
  if (HaveDnsConfig()) {
    mode_for_histogram_ = MODE_FOR_HISTOGRAM_ASYNC_DNS;
    for (const auto& dns_server : dns_client_->GetConfig()->nameservers) {
      if (DnsServerSupportsDoh(dns_server.address())) {
        mode_for_histogram_ = MODE_FOR_HISTOGRAM_ASYNC_DNS_PRIVATE_SUPPORTS_DOH;
        break;
      }
    }
  } else {
    mode_for_histogram_ = MODE_FOR_HISTOGRAM_SYSTEM;
    for (const auto& dns_server : dns_config.nameservers) {
      if (DnsServerSupportsDoh(dns_server.address())) {
        mode_for_histogram_ = MODE_FOR_HISTOGRAM_SYSTEM_SUPPORTS_DOH;
        break;
      }
    }
#if defined(OS_ANDROID)
    if (base::android::BuildInfo::GetInstance()->sdk_int() >=
        base::android::SDK_VERSION_P) {
      std::vector<IPEndPoint> dns_servers;
      if (net::android::GetDnsServers(&dns_servers) ==
          internal::CONFIG_PARSE_POSIX_PRIVATE_DNS_ACTIVE) {
        mode_for_histogram_ = MODE_FOR_HISTOGRAM_SYSTEM_PRIVATE_DNS;
      }
    }
#endif  // defined(OS_ANDROID)
  }
}

HostResolverImpl::RequestImpl::~RequestImpl() {
  if (job_)
    job_->CancelRequest(this);
}

void HostResolverImpl::RequestImpl::ChangeRequestPriority(
    RequestPriority priority) {
  DCHECK(job_);
  job_->ChangeRequestPriority(this, priority);
}

}  // namespace net
