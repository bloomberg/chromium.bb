// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/stats_counters.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

using net::URLFetcher;
using net::URLFetcherDelegate;
using net::URLRequestContextGetter;

void usage(const char* program_name) {
  printf("usage: %s --url=<url> [--n=<clients>] [--stats] [--use_cache] "
         "[--v]\n",
         program_name);
  exit(1);
}

// Test Driver
class Driver {
 public:
  Driver()
      : clients_(0) {}

  void ClientStarted() { clients_++; }
  void ClientStopped() {
    if (!--clients_) {
      base::MessageLoop::current()->Quit();
    }
  }

 private:
  int clients_;
};

scoped_ptr<net::URLRequestContext>
BuildURLRequestContext(bool use_cache) {
  net::URLRequestContextBuilder builder;
  builder.set_file_enabled(true);
  builder.set_data_enabled(true);
  builder.set_ftp_enabled(true);
  if (!use_cache)
    builder.DisableHttpCache();
  scoped_ptr<net::URLRequestContext> context(builder.Build());
  return context.Pass();
}

// Builds a URLRequestContext assuming there's only a single message loop.
class SingleThreadRequestContextGetter : public net::URLRequestContextGetter {
 public:
  // Since there's only a single thread, there's no need to worry
  // about when |context_| gets created.
  SingleThreadRequestContextGetter(
      bool use_cache,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner)
      : context_(BuildURLRequestContext(use_cache)),
        main_task_runner_(main_task_runner) {}

  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE {
    return context_.get();
  }

  virtual scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const OVERRIDE {
    return main_task_runner_;
  }

 private:
  virtual ~SingleThreadRequestContextGetter() {}

  const scoped_ptr<net::URLRequestContext> context_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
};

static base::LazyInstance<Driver> g_driver = LAZY_INSTANCE_INITIALIZER;

// A network client
class Client : public URLFetcherDelegate {
 public:
  Client(const std::string& url,
         URLRequestContextGetter* getter) :
      url_(url), getter_(getter), last_current_(0) {
  }

  void Start() {
    fetcher_.reset(net::URLFetcher::Create(
        url_, net::URLFetcher::GET, this));
    if (!fetcher_.get())
      return;
    g_driver.Get().ClientStarted();
    fetcher_->SetRequestContext(getter_);
    fetcher_->Start();
  };

 private:

  // URLFetcherDelegate overrides.
  virtual void OnURLFetchComplete(const URLFetcher* source) OVERRIDE {
    base::StatsCounter requests("FetchClient.requests");
    requests.Increment();
    g_driver.Get().ClientStopped();
    printf(".");
  }

  virtual void OnURLFetchDownloadProgress(const URLFetcher* source,
                                          int64 current, int64 total) OVERRIDE {
    base::StatsCounter bytes_read("FetchClient.bytes_read");
    DCHECK(current >= last_current_);
    int64 delta = current - last_current_;
    bytes_read.Add(delta);
    last_current_ = current;
  }

  virtual void OnURLFetchUploadProgress(const URLFetcher* source,
                                        int64 current, int64 total) OVERRIDE {
    CHECK(false);
  }

  GURL url_;
  scoped_ptr<URLFetcher> fetcher_;
  URLRequestContextGetter* getter_;
  int64 last_current_;
};

int main(int argc, char** argv) {
  base::AtExitManager exit;
  base::StatsTable table("fetchclient", 50, 1000);
  table.set_current(&table);

  CommandLine::Init(argc, argv);
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  std::string url = parsed_command_line.GetSwitchValueASCII("url");
  if (!url.length())
    usage(argv[0]);
  int client_limit = 1;
  if (parsed_command_line.HasSwitch("n")) {
    base::StringToInt(parsed_command_line.GetSwitchValueASCII("n"),
                      &client_limit);
  }
  bool use_cache = parsed_command_line.HasSwitch("use-cache");
  if (parsed_command_line.HasSwitch("v")) {
    logging::SetMinLogLevel(-10);
  }

  // Do work here.
  base::MessageLoopForIO loop;

  scoped_refptr<SingleThreadRequestContextGetter> context_getter(
      new SingleThreadRequestContextGetter(use_cache,
                                           loop.message_loop_proxy()));

  {
    base::StatsCounterTimer driver_time("FetchClient.total_time");
    base::StatsScope<base::StatsCounterTimer> scope(driver_time);

    Client** clients = new Client*[client_limit];
    for (int i = 0; i < client_limit; i++) {
      clients[i] = new Client(url, context_getter);
      clients[i]->Start();
    }

    base::MessageLoop::current()->Run();
  }

  // Print Statistics here.
  int num_clients = table.GetCounterValue("c:FetchClient.requests");
  int test_time = table.GetCounterValue("t:FetchClient.total_time");
  int bytes_read = table.GetCounterValue("c:FetchClient.bytes_read");

  printf("\n");
  printf("Clients     : %d\n", num_clients);
  printf("Time        : %dms\n", test_time);
  printf("Bytes Read  : %d\n", bytes_read);
  if (test_time > 0) {
    const char *units = "bps";
    double bps = static_cast<float>(bytes_read * 8) /
        (static_cast<float>(test_time) / 1000.0);

    if (bps > (1024*1024)) {
      bps /= (1024*1024);
      units = "Mbps";
    } else if (bps > 1024) {
      bps /= 1024;
      units = "Kbps";
    }
    printf("Bandwidth   : %.2f%s\n", bps, units);
  }

  if (parsed_command_line.HasSwitch("stats")) {
    // Dump the stats table.
    printf("<stats>\n");
    int counter_max = table.GetMaxCounters();
    for (int index = 0; index < counter_max; index++) {
      std::string name(table.GetRowName(index));
      if (name.length() > 0) {
        int value = table.GetRowValue(index);
        printf("%s:\t%d\n", name.c_str(), value);
      }
    }
    printf("</stats>\n");
  }
  return 0;
}
