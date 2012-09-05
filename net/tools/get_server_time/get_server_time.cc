// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a small utility that snarfs the server time from the
// response headers of an http/https HEAD request and compares it to
// the local time.
//
// TODO(akalin): Also snarf the server time from the TLS handshake, if
// any (http://crbug.com/146090).

#include <cstdio>
#include <cstdlib>
#include <string>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#elif defined(OS_LINUX)
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service_fixed.h"
#endif

namespace {

// base::TimeTicks::Now() is documented to have a resolution of
// ~1-15ms.
const int64 kTicksResolutionMs = 15;

// For the sources that are supported (HTTP date headers, TLS
// handshake), the resolution of the server time is 1 second.
const int64 kServerTimeResolutionMs = 1000;

// Assume base::Time::Now() has the same resolution as
// base::TimeTicks::Now().
//
// TODO(akalin): Figure out the real resolution.
const int64 kTimeResolutionMs = kTicksResolutionMs;

// Simply quits the current message loop when finished.  Used to make
// URLFetcher synchronous.
class QuitDelegate : public net::URLFetcherDelegate {
 public:
  QuitDelegate() {}

  virtual ~QuitDelegate() {}

  // net::URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
    MessageLoop::current()->Quit();
  }

  virtual void OnURLFetchDownloadProgress(
      const net::URLFetcher* source,
      int64 current, int64 total) OVERRIDE {
    NOTREACHED();
  }

  virtual void OnURLFetchDownloadData(
      const net::URLFetcher* source,
      scoped_ptr<std::string> download_data) OVERRIDE{
    NOTREACHED();
  }

  virtual bool ShouldSendDownloadData() OVERRIDE {
    NOTREACHED();
    return false;
  }

  virtual void OnURLFetchUploadProgress(const net::URLFetcher* source,
                                        int64 current, int64 total) OVERRIDE {
    NOTREACHED();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QuitDelegate);
};

// NetLog implementation that simply prints events to the logs.
class PrintingLog : public net::NetLog {
 public:
  PrintingLog() : next_id_(1) {}

  virtual ~PrintingLog() {}

  // NetLog implementation:
  virtual void OnAddEntry(const net::NetLog::Entry& entry) OVERRIDE {
    // The log level of the entry is unknown, so just assume it maps
    // to VLOG(1).
    if (!VLOG_IS_ON(1))
      return;

    const char* const source_type =
        net::NetLog::SourceTypeToString(entry.source().type);
    const char* const event_type =
        net::NetLog::EventTypeToString(entry.type());
    const char* const event_phase =
        net::NetLog::EventPhaseToString(entry.phase());
    scoped_ptr<base::Value> params(entry.ParametersToValue());
    std::string params_str;
    if (params.get()) {
      base::JSONWriter::Write(params.get(), &params_str);
      params_str.insert(0, ": ");
    }

    VLOG(1) << source_type << "(" << entry.source().id << "): "
            << event_type << ": " << event_phase << params_str;
  }

  virtual uint32 NextID() OVERRIDE {
    return next_id_++;
  }

  virtual LogLevel GetLogLevel() const OVERRIDE {
    const int vlog_level = logging::GetVlogLevel(__FILE__);
    if (vlog_level <= 0) {
      return LOG_BASIC;
    }
    if (vlog_level == 1) {
      return LOG_ALL_BUT_BYTES;
    }
    return LOG_ALL;
  }

  virtual void AddThreadSafeObserver(ThreadSafeObserver* observer,
                                     LogLevel log_level) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void SetObserverLogLevel(ThreadSafeObserver* observer,
                                   LogLevel log_level) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void RemoveThreadSafeObserver(ThreadSafeObserver* observer) OVERRIDE {
    NOTIMPLEMENTED();
  }

 private:
  uint32 next_id_;

  DISALLOW_COPY_AND_ASSIGN(PrintingLog);
};

// Builds a URLRequestContext assuming there's only a single loop.
scoped_ptr<net::URLRequestContext> BuildURLRequestContext() {
  net::URLRequestContextBuilder builder;
#if defined(OS_LINUX)
  // On Linux, use a fixed ProxyConfigService, since the default one
  // depends on glib.
  //
  // TODO(akalin): Remove this once http://crbug.com/146421 is fixed.
  builder.set_proxy_config_service(
      new net::ProxyConfigServiceFixed(net::ProxyConfig()));
#endif
  scoped_ptr<net::URLRequestContext> context(builder.Build());
  context->set_net_log(new PrintingLog());
  return context.Pass();
}

class SingleThreadRequestContextGetter : public net::URLRequestContextGetter {
 public:
  // Since there's only a single thread, there's no need to worry
  // about when |context_| gets created.
  explicit SingleThreadRequestContextGetter(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner)
      : context_(BuildURLRequestContext()),
        main_task_runner_(main_task_runner) {}

  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE {
    return context_.get();
  }

  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetNetworkTaskRunner() const OVERRIDE {
    return main_task_runner_;
  }

 private:
  virtual ~SingleThreadRequestContextGetter() {}

  const scoped_ptr<net::URLRequestContext> context_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
};

// Assuming that the time |server_time| was received from a server,
// that the request for the server was started on |start_ticks|, and
// that it ended on |end_ticks|, fills |server_now| with an estimate
// of the current time and |server_now_uncertainty| with a
// conservative estimate of the uncertainty.
void EstimateServerTimeNow(base::Time server_time,
                           base::TimeTicks start_ticks,
                           base::TimeTicks end_ticks,
                           base::Time* server_now,
                           base::TimeDelta* server_now_uncertainty) {
  const base::TimeDelta delta_ticks = end_ticks - start_ticks;
  const base::TimeTicks mid_ticks = start_ticks + delta_ticks / 2;
  const base::TimeDelta estimated_elapsed = base::TimeTicks::Now() - mid_ticks;

  *server_now = server_time + estimated_elapsed;

  *server_now_uncertainty =
      base::TimeDelta::FromMilliseconds(kServerTimeResolutionMs) +
      delta_ticks + 3 * base::TimeDelta::FromMilliseconds(kTicksResolutionMs);
}

// Assuming that the time of the server is |server_now| with
// uncertainty |server_now_uncertainty| and that the local time is
// |now|, fills |skew| with the skew of the local clock (i.e., add
// |*skew| to a client time to get a server time) and
// |skew_uncertainty| with a conservative estimate of the uncertainty.
void EstimateSkew(base::Time server_now,
                  base::TimeDelta server_now_uncertainty,
                  base::Time now,
                  base::TimeDelta now_uncertainty,
                  base::TimeDelta* skew,
                  base::TimeDelta* skew_uncertainty) {
  *skew = server_now - now;
  *skew_uncertainty = server_now_uncertainty + now_uncertainty;
}

}  // namespace

int main(int argc, char* argv[]) {
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool pool;
#endif

  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
  logging::InitLogging(
      NULL,
      logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
      logging::LOCK_LOG_FILE,
      logging::DELETE_OLD_LOG_FILE,
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  GURL url(parsed_command_line.GetSwitchValueASCII("url"));
  if (!url.is_valid() ||
      (url.scheme() != "http" && url.scheme() != "https")) {
    std::fprintf(
        stderr,
        "Usage: %s --url=[http|https]://www.example.com [--v=[1|2]]\n",
        argv[0]);
    return EXIT_FAILURE;
  }

  MessageLoopForIO main_loop;

  // NOTE: A NetworkChangeNotifier could be instantiated here, but
  // that interferes with the request that will be sent; some
  // implementations always send out an OnIPAddressChanged() message,
  // which causes the DNS resolution to abort.  It's simpler to just
  // not instantiate one, since only a single request is sent anyway.

  scoped_refptr<SingleThreadRequestContextGetter> context_getter(
      new SingleThreadRequestContextGetter(main_loop.message_loop_proxy()));

  QuitDelegate delegate;
  scoped_ptr<net::URLFetcher> fetcher(
      net::URLFetcher::Create(url, net::URLFetcher::HEAD, &delegate));
  fetcher->SetRequestContext(context_getter.get());

  const base::Time start_time = base::Time::Now();
  const base::TimeTicks start_ticks = base::TimeTicks::Now();

  fetcher->Start();
  std::printf(
      "Request started at %s (ticks = %"PRId64")\n",
      UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(start_time)).c_str(),
      start_ticks.ToInternalValue());

  // |delegate| quits |main_loop| when the request is done.
  main_loop.Run();

  const base::Time end_time = base::Time::Now();
  const base::TimeTicks end_ticks = base::TimeTicks::Now();

  std::printf(
      "Request ended at %s (ticks = %"PRId64")\n",
      UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(end_time)).c_str(),
      end_ticks.ToInternalValue());

  const int64 delta_ticks_internal =
      end_ticks.ToInternalValue() - start_ticks.ToInternalValue();
  const base::TimeDelta delta_ticks = end_ticks - start_ticks;

  std::printf(
      "Request took %"PRId64" ticks (%.2f ms)\n",
      delta_ticks_internal, delta_ticks.InMillisecondsF());

  const net::URLRequestStatus status = fetcher->GetStatus();
  if (status.status() != net::URLRequestStatus::SUCCESS) {
    LOG(ERROR) << "Request failed with error code: "
               << net::ErrorToString(status.error());
    return EXIT_FAILURE;
  }

  const net::HttpResponseHeaders* const headers =
      fetcher->GetResponseHeaders();
  if (!headers) {
    LOG(ERROR) << "Response does not have any headers";
    return EXIT_FAILURE;
  }

  void* iter = NULL;
  std::string date_header;
  while (headers->EnumerateHeader(&iter, "Date", &date_header)) {
    std::printf("Got date header: %s\n", date_header.c_str());
  }

  base::Time server_time;
  if (!headers->GetDateValue(&server_time)) {
    LOG(ERROR) << "Could not parse time from server response headers";
    return EXIT_FAILURE;
  }

  std::printf(
      "Got time %s from server\n",
      UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(server_time)).c_str());

  base::Time server_now;
  base::TimeDelta server_now_uncertainty;
  EstimateServerTimeNow(server_time, start_ticks, end_ticks,
                        &server_now, &server_now_uncertainty);
  base::Time now = base::Time::Now();

  std::printf(
      "According to the server, it is now %s with uncertainty %.2f ms\n",
      UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(server_now)).c_str(),
      server_now_uncertainty.InMillisecondsF());

  base::TimeDelta skew;
  base::TimeDelta skew_uncertainty;
  EstimateSkew(server_now, server_now_uncertainty, now,
               base::TimeDelta::FromMilliseconds(kTimeResolutionMs),
               &skew, &skew_uncertainty);

  std::printf(
      "An estimate for the local clock skew is %.2f ms with "
      "uncertainty %.2f ms\n",
      skew.InMillisecondsF(),
      skew_uncertainty.InMillisecondsF());

  return EXIT_SUCCESS;
}
