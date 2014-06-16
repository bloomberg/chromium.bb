// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Library functions related to the Financial Server ping.

#include "rlz/lib/financial_ping.h"

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "rlz/lib/assert.h"
#include "rlz/lib/lib_values.h"
#include "rlz/lib/machine_id.h"
#include "rlz/lib/rlz_lib.h"
#include "rlz/lib/rlz_value_store.h"
#include "rlz/lib/string_utils.h"

#if !defined(OS_WIN)
#include "base/time/time.h"
#endif

#if defined(RLZ_NETWORK_IMPLEMENTATION_WIN_INET)

#include <windows.h>
#include <wininet.h>

namespace {

class InternetHandle {
 public:
  InternetHandle(HINTERNET handle) { handle_ = handle; }
  ~InternetHandle() { if (handle_) InternetCloseHandle(handle_); }
  operator HINTERNET() const { return handle_; }
  bool operator!() const { return (handle_ == NULL); }

 private:
  HINTERNET handle_;
};

}  // namespace

#else

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

#endif

namespace {

// Returns the time relative to a fixed point in the past in multiples of
// 100 ns stepts. The point in the past is arbitrary but can't change, as the
// result of this value is stored on disk.
int64 GetSystemTimeAsInt64() {
#if defined(OS_WIN)
  FILETIME now_as_file_time;
  // Relative to Jan 1, 1601 (UTC).
  GetSystemTimeAsFileTime(&now_as_file_time);

  LARGE_INTEGER integer;
  integer.HighPart = now_as_file_time.dwHighDateTime;
  integer.LowPart = now_as_file_time.dwLowDateTime;
  return integer.QuadPart;
#else
  // Seconds since epoch (Jan 1, 1970).
  double now_seconds = base::Time::Now().ToDoubleT();
  return static_cast<int64>(now_seconds * 1000 * 1000 * 10);
#endif
}

}  // namespace


namespace rlz_lib {

using base::subtle::AtomicWord;

bool FinancialPing::FormRequest(Product product,
    const AccessPoint* access_points, const char* product_signature,
    const char* product_brand, const char* product_id,
    const char* product_lang, bool exclude_machine_id,
    std::string* request) {
  if (!request) {
    ASSERT_STRING("FinancialPing::FormRequest: request is NULL");
    return false;
  }

  request->clear();

  ScopedRlzValueStoreLock lock;
  RlzValueStore* store = lock.GetStore();
  if (!store || !store->HasAccess(RlzValueStore::kReadAccess))
    return false;

  if (!access_points) {
    ASSERT_STRING("FinancialPing::FormRequest: access_points is NULL");
    return false;
  }

  if (!product_signature) {
    ASSERT_STRING("FinancialPing::FormRequest: product_signature is NULL");
    return false;
  }

  if (!SupplementaryBranding::GetBrand().empty()) {
    if (SupplementaryBranding::GetBrand() != product_brand) {
      ASSERT_STRING("FinancialPing::FormRequest: supplementary branding bad");
      return false;
    }
  }

  base::StringAppendF(request, "%s?", kFinancialPingPath);

  // Add the signature, brand, product id and language.
  base::StringAppendF(request, "%s=%s", kProductSignatureCgiVariable,
                      product_signature);
  if (product_brand)
    base::StringAppendF(request, "&%s=%s", kProductBrandCgiVariable,
                        product_brand);

  if (product_id)
    base::StringAppendF(request, "&%s=%s", kProductIdCgiVariable, product_id);

  if (product_lang)
    base::StringAppendF(request, "&%s=%s", kProductLanguageCgiVariable,
                        product_lang);

  // Add the product events.
  char cgi[kMaxCgiLength + 1];
  cgi[0] = 0;
  bool has_events = GetProductEventsAsCgi(product, cgi, arraysize(cgi));
  if (has_events)
    base::StringAppendF(request, "&%s", cgi);

  // If we don't have any events, we should ping all the AP's on the system
  // that we know about and have a current RLZ value, even if they are not
  // used by this product.
  AccessPoint all_points[LAST_ACCESS_POINT];
  if (!has_events) {
    char rlz[kMaxRlzLength + 1];
    int idx = 0;
    for (int ap = NO_ACCESS_POINT + 1; ap < LAST_ACCESS_POINT; ap++) {
      rlz[0] = 0;
      AccessPoint point = static_cast<AccessPoint>(ap);
      if (GetAccessPointRlz(point, rlz, arraysize(rlz)) &&
          rlz[0] != '\0')
        all_points[idx++] = point;
    }
    all_points[idx] = NO_ACCESS_POINT;
  }

  // Add the RLZ's and the DCC if needed. This is the same as get PingParams.
  // This will also include the RLZ Exchange Protocol CGI Argument.
  cgi[0] = 0;
  if (GetPingParams(product, has_events ? access_points : all_points,
                    cgi, arraysize(cgi)))
    base::StringAppendF(request, "&%s", cgi);

  if (has_events && !exclude_machine_id) {
    std::string machine_id;
    if (GetMachineId(&machine_id)) {
      base::StringAppendF(request, "&%s=%s", kMachineIdCgiVariable,
                          machine_id.c_str());
    }
  }

  return true;
}

#if defined(RLZ_NETWORK_IMPLEMENTATION_CHROME_NET)
// The pointer to URLRequestContextGetter used by FinancialPing::PingServer().
// It is atomic pointer because it can be accessed and modified by multiple
// threads.
AtomicWord g_context;

bool FinancialPing::SetURLRequestContext(
    net::URLRequestContextGetter* context) {
  base::subtle::Release_Store(
      &g_context, reinterpret_cast<AtomicWord>(context));
  return true;
}

namespace {

class FinancialPingUrlFetcherDelegate : public net::URLFetcherDelegate {
 public:
  FinancialPingUrlFetcherDelegate(const base::Closure& callback)
      : callback_(callback) {
  }
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  base::Closure callback_;
};

void FinancialPingUrlFetcherDelegate::OnURLFetchComplete(
    const net::URLFetcher* source) {
  callback_.Run();
}

bool send_financial_ping_interrupted_for_test = false;

}  // namespace

void ShutdownCheck(base::WeakPtr<base::RunLoop> weak) {
  if (!weak.get())
    return;
  if (!base::subtle::Acquire_Load(&g_context)) {
    send_financial_ping_interrupted_for_test = true;
    weak->QuitClosure().Run();
    return;
  }
  // How frequently the financial ping thread should check
  // the shutdown condition?
  const base::TimeDelta kInterval = base::TimeDelta::FromMilliseconds(500);
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ShutdownCheck, weak),
      kInterval);
}
#endif

bool FinancialPing::PingServer(const char* request, std::string* response) {
  if (!response)
    return false;

  response->clear();

#if defined(RLZ_NETWORK_IMPLEMENTATION_WIN_INET)
  // Initialize WinInet.
  InternetHandle inet_handle = InternetOpenA(kFinancialPingUserAgent,
                                             INTERNET_OPEN_TYPE_PRECONFIG,
                                             NULL, NULL, 0);
  if (!inet_handle)
    return false;

  // Open network connection.
  InternetHandle connection_handle = InternetConnectA(inet_handle,
      kFinancialServer, kFinancialPort, "", "", INTERNET_SERVICE_HTTP,
      INTERNET_FLAG_NO_CACHE_WRITE, 0);
  if (!connection_handle)
    return false;

  // Prepare the HTTP request.
  InternetHandle http_handle = HttpOpenRequestA(connection_handle,
      "GET", request, NULL, NULL, kFinancialPingResponseObjects,
      INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_COOKIES, NULL);
  if (!http_handle)
    return false;

  // Timeouts are probably:
  // INTERNET_OPTION_SEND_TIMEOUT, INTERNET_OPTION_RECEIVE_TIMEOUT

  // Send the HTTP request. Note: Fails if user is working in off-line mode.
  if (!HttpSendRequest(http_handle, NULL, 0, NULL, 0))
    return false;

  // Check the response status.
  DWORD status;
  DWORD status_size = sizeof(status);
  if (!HttpQueryInfo(http_handle, HTTP_QUERY_STATUS_CODE |
                     HTTP_QUERY_FLAG_NUMBER, &status, &status_size, NULL) ||
      200 != status)
    return false;

  // Get the response text.
  scoped_ptr<char[]> buffer(new char[kMaxPingResponseLength]);
  if (buffer.get() == NULL)
    return false;

  DWORD bytes_read = 0;
  while (InternetReadFile(http_handle, buffer.get(), kMaxPingResponseLength,
                          &bytes_read) && bytes_read > 0) {
    response->append(buffer.get(), bytes_read);
    bytes_read = 0;
  };

  return true;
#else
  // Copy the pointer to stack because g_context may be set to NULL
  // in different thread. The instance is guaranteed to exist while
  // the method is running.
  net::URLRequestContextGetter* context =
      reinterpret_cast<net::URLRequestContextGetter*>(
          base::subtle::Acquire_Load(&g_context));

  // Browser shutdown will cause the context to be reset to NULL.
  if (!context)
    return false;

  // Run a blocking event loop to match the win inet implementation.
  scoped_ptr<base::MessageLoop> message_loop;
  // Ensure that we have a MessageLoop.
  if (!base::MessageLoop::current())
    message_loop.reset(new base::MessageLoop);
  base::RunLoop loop;
  FinancialPingUrlFetcherDelegate delegate(loop.QuitClosure());

  std::string url = base::StringPrintf("http://%s:%d%s",
                                       kFinancialServer, kFinancialPort,
                                       request);

  scoped_ptr<net::URLFetcher> fetcher(net::URLFetcher::Create(
      GURL(url), net::URLFetcher::GET, &delegate));

  fetcher->SetLoadFlags(net::LOAD_DISABLE_CACHE |
                        net::LOAD_DO_NOT_SEND_AUTH_DATA |
                        net::LOAD_DO_NOT_PROMPT_FOR_LOGIN |
                        net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES);

  // Ensure rlz_lib::SetURLRequestContext() has been called before sending
  // pings.
  fetcher->SetRequestContext(context);

  base::WeakPtrFactory<base::RunLoop> weak(&loop);

  const base::TimeDelta kTimeout = base::TimeDelta::FromMinutes(5);
  base::MessageLoop::ScopedNestableTaskAllower allow_nested(
      base::MessageLoop::current());
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ShutdownCheck, weak.GetWeakPtr()));
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&net::URLFetcher::Start, base::Unretained(fetcher.get())));
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), kTimeout);

  loop.Run();

  if (fetcher->GetResponseCode() != 200)
    return false;

  return fetcher->GetResponseAsString(response);
#endif
}

bool FinancialPing::IsPingTime(Product product, bool no_delay) {
  ScopedRlzValueStoreLock lock;
  RlzValueStore* store = lock.GetStore();
  if (!store || !store->HasAccess(RlzValueStore::kReadAccess))
    return false;

  int64 last_ping = 0;
  if (!store->ReadPingTime(product, &last_ping))
    return true;

  uint64 now = GetSystemTimeAsInt64();
  int64 interval = now - last_ping;

  // If interval is negative, clock was probably reset. So ping.
  if (interval < 0)
    return true;

  // Check if this product has any unreported events.
  char cgi[kMaxCgiLength + 1];
  cgi[0] = 0;
  bool has_events = GetProductEventsAsCgi(product, cgi, arraysize(cgi));
  if (no_delay && has_events)
    return true;

  return interval >= (has_events ? kEventsPingInterval : kNoEventsPingInterval);
}


bool FinancialPing::UpdateLastPingTime(Product product) {
  ScopedRlzValueStoreLock lock;
  RlzValueStore* store = lock.GetStore();
  if (!store || !store->HasAccess(RlzValueStore::kWriteAccess))
    return false;

  uint64 now = GetSystemTimeAsInt64();
  return store->WritePingTime(product, now);
}


bool FinancialPing::ClearLastPingTime(Product product) {
  ScopedRlzValueStoreLock lock;
  RlzValueStore* store = lock.GetStore();
  if (!store || !store->HasAccess(RlzValueStore::kWriteAccess))
    return false;
  return store->ClearPingTime(product);
}

#if defined(RLZ_NETWORK_IMPLEMENTATION_CHROME_NET)
namespace test {

void ResetSendFinancialPingInterrupted() {
  send_financial_ping_interrupted_for_test = false;
}

bool WasSendFinancialPingInterrupted() {
  return send_financial_ping_interrupted_for_test;
}

}  // namespace test
#endif

}  // namespace rlz_lib
