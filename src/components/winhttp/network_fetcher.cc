// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/winhttp/network_fetcher.h"

#include <limits>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_math.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/windows_version.h"
#include "components/winhttp/net_util.h"
#include "components/winhttp/proxy_info.h"
#include "components/winhttp/scoped_hinternet.h"
#include "components/winhttp/scoped_winttp_proxy_info.h"
#include "url/url_constants.h"

namespace winhttp {

namespace {

constexpr base::TaskTraits kTaskTraits = {
    base::MayBlock(), base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

void CrackUrl(const GURL& url,
              bool* is_https,
              std::string* host,
              int* port,
              std::string* path_for_request) {
  if (is_https)
    *is_https = url.SchemeIs(url::kHttpsScheme);
  if (host)
    *host = url.host();
  if (port)
    *port = url.EffectiveIntPort();
  if (path_for_request)
    *path_for_request = url.PathForRequest();
}

}  // namespace

NetworkFetcher::NetworkFetcher(
    const HINTERNET& session_handle,
    scoped_refptr<ProxyConfiguration> proxy_configuration)
    : main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      session_handle_(session_handle),
      proxy_configuration_(proxy_configuration) {}

NetworkFetcher::~NetworkFetcher() {
  DVLOG(3) << "~NetworkFetcher";
}

void NetworkFetcher::Close() {
  // |write_data_callback_| maintains an outstanding reference to this object
  // and the reference must be released to avoid leaking the object.
  write_data_callback_.Reset();
  request_handle_.reset();
}

void NetworkFetcher::CompleteFetch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!file_.IsValid()) {
    std::move(fetch_complete_callback_).Run(response_code_);
    return;
  }
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, kTaskTraits,
      base::BindOnce([](base::File& file) { file.Close(); }, std::ref(file_)),
      base::BindOnce(&NetworkFetcher::CompleteFetch, this));
}

HRESULT NetworkFetcher::QueryHeaderString(const std::wstring& name,
                                          std::wstring* value) const {
  return QueryHeadersString(request_handle_.get(), WINHTTP_QUERY_CUSTOM,
                            name.c_str(), value);
}

HRESULT NetworkFetcher::QueryHeaderInt(const std::wstring& name,
                                       int* value) const {
  return QueryHeadersInt(request_handle_.get(), WINHTTP_QUERY_CUSTOM,
                         name.c_str(), value);
}

std::string NetworkFetcher::GetResponseBody() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return post_response_body_;
}

HRESULT NetworkFetcher::GetNetError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return net_error_;
}

base::FilePath NetworkFetcher::GetFilePath() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return file_path_;
}

int64_t NetworkFetcher::GetContentSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return content_size_;
}

void NetworkFetcher::PostRequest(
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    FetchStartedCallback fetch_started_callback,
    FetchProgressCallback fetch_progress_callback,
    FetchCompleteCallback fetch_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  url_ = url;
  fetch_started_callback_ = std::move(fetch_started_callback);
  fetch_progress_callback_ = std::move(fetch_progress_callback);
  fetch_complete_callback_ = std::move(fetch_complete_callback);

  CrackUrl(url_, &is_https_, &host_, &port_, &path_for_request_);

  verb_ = L"POST";
  content_type_ = content_type;
  write_data_callback_ =
      base::BindRepeating(&NetworkFetcher::WriteDataToMemory, this);

  net_error_ = BeginFetch(post_data, post_additional_headers);

  if (FAILED(net_error_))
    CompleteFetch();
}

void NetworkFetcher::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    FetchStartedCallback fetch_started_callback,
    FetchProgressCallback fetch_progress_callback,
    FetchCompleteCallback fetch_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  url_ = url;
  file_path_ = file_path;
  fetch_started_callback_ = std::move(fetch_started_callback);
  fetch_progress_callback_ = std::move(fetch_progress_callback);
  fetch_complete_callback_ = std::move(fetch_complete_callback);

  CrackUrl(url, &is_https_, &host_, &port_, &path_for_request_);

  verb_ = L"GET";
  write_data_callback_ =
      base::BindRepeating(&NetworkFetcher::WriteDataToFile, this);

  net_error_ = BeginFetch({}, {});

  if (FAILED(net_error_))
    CompleteFetch();
}

HRESULT NetworkFetcher::BeginFetch(
    const std::string& data,
    base::flat_map<std::string, std::string> additional_headers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!url_.SchemeIsHTTPOrHTTPS())
    return E_INVALIDARG;

  connect_handle_ = Connect();
  if (!connect_handle_.get())
    return HRESULTFromLastError();

  absl::optional<ScopedWinHttpProxyInfo> winhttp_proxy_info =
      proxy_configuration_->GetProxyForUrl(session_handle_, url_);

  request_handle_ = OpenRequest();
  if (!request_handle_.get())
    return HRESULTFromLastError();

  SetProxyForRequest(request_handle_.get(), winhttp_proxy_info);

  const auto winhttp_callback = ::WinHttpSetStatusCallback(
      request_handle_.get(), &NetworkFetcher::WinHttpStatusCallback,
      WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0);
  if (winhttp_callback == WINHTTP_INVALID_STATUS_CALLBACK)
    return HRESULTFromLastError();

  auto hr =
      SetOption(request_handle_.get(), WINHTTP_OPTION_CONTEXT_VALUE, context());
  if (FAILED(hr))
    return hr;

  self_ = this;

  // Disables both saving and sending cookies.
  hr = SetOption(request_handle_.get(), WINHTTP_OPTION_DISABLE_FEATURE,
                 WINHTTP_DISABLE_COOKIES);
  if (FAILED(hr))
    return hr;

  if (!content_type_.empty())
    additional_headers.insert({"Content-Type", content_type_});

  for (const auto& header : additional_headers) {
    const auto raw_header = base::SysUTF8ToWide(
        base::StrCat({header.first, ": ", header.second, "\r\n"}));
    if (!::WinHttpAddRequestHeaders(
            request_handle_.get(), raw_header.c_str(), raw_header.size(),
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)) {
      PLOG(ERROR) << "Failed to set the request header: " << raw_header;
    }
  }

  hr = SendRequest(data);
  if (FAILED(hr))
    return hr;

  return S_OK;
}

ScopedHInternet NetworkFetcher::Connect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ScopedHInternet(::WinHttpConnect(
      session_handle_, base::SysUTF8ToWide(host_).c_str(), port_, 0));
}

ScopedHInternet NetworkFetcher::OpenRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  uint32_t flags = WINHTTP_FLAG_REFRESH;
  if (is_https_)
    flags |= WINHTTP_FLAG_SECURE;
  return ScopedHInternet(::WinHttpOpenRequest(
      connect_handle_.get(), verb_.data(),
      base::SysUTF8ToWide(path_for_request_).c_str(), nullptr,
      WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
}

HRESULT NetworkFetcher::SendRequest(const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(2) << data;

  const uint32_t bytes_to_send = base::saturated_cast<uint32_t>(data.size());
  void* request_body =
      bytes_to_send ? const_cast<char*>(data.c_str()) : WINHTTP_NO_REQUEST_DATA;
  if (!::WinHttpSendRequest(request_handle_.get(),
                            WINHTTP_NO_ADDITIONAL_HEADERS, 0, request_body,
                            bytes_to_send, bytes_to_send, context())) {
    return HRESULTFromLastError();
  }

  return S_OK;
}

void NetworkFetcher::SendRequestComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::wstring all;
  QueryHeadersString(
      request_handle_.get(),
      WINHTTP_QUERY_RAW_HEADERS_CRLF | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
      WINHTTP_HEADER_NAME_BY_INDEX, &all);
  VLOG(3) << "request headers: " << all;

  net_error_ = ReceiveResponse();
  if (FAILED(net_error_))
    CompleteFetch();
}

HRESULT NetworkFetcher::ReceiveResponse() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!::WinHttpReceiveResponse(request_handle_.get(), nullptr))
    return HRESULTFromLastError();
  return S_OK;
}

void NetworkFetcher::HeadersAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::wstring all;
  QueryHeadersString(request_handle_.get(), WINHTTP_QUERY_RAW_HEADERS_CRLF,
                     WINHTTP_HEADER_NAME_BY_INDEX, &all);
  VLOG(3) << "response headers: " << all;

  net_error_ = QueryHeadersInt(request_handle_.get(), WINHTTP_QUERY_STATUS_CODE,
                               WINHTTP_HEADER_NAME_BY_INDEX, &response_code_);
  if (FAILED(net_error_)) {
    CompleteFetch();
    return;
  }

  int content_length = 0;
  net_error_ =
      QueryHeadersInt(request_handle_.get(), WINHTTP_QUERY_CONTENT_LENGTH,
                      WINHTTP_HEADER_NAME_BY_INDEX, &content_length);
  if (FAILED(net_error_)) {
    CompleteFetch();
    return;
  }

  std::move(fetch_started_callback_).Run(response_code_, content_length);

  // Start reading the body of response.
  net_error_ = ReadData();
  if (FAILED(net_error_))
    CompleteFetch();
}

HRESULT NetworkFetcher::ReadData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Use a fixed buffer size, larger than the internal WinHTTP buffer size (8K),
  // according to the documentation for WinHttpReadData.
  constexpr size_t kNumBytesToRead = 0x4000;  // 16KiB.
  read_buffer_.resize(kNumBytesToRead);

  if (!::WinHttpReadData(request_handle_.get(), &read_buffer_.front(),
                         read_buffer_.size(), nullptr)) {
    return HRESULTFromLastError();
  }

  DVLOG(3) << "reading data...";
  return S_OK;
}

void NetworkFetcher::ReadDataComplete(size_t num_bytes_read) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  read_buffer_.resize(num_bytes_read);
  write_data_callback_.Run();
}

void NetworkFetcher::RequestError(const WINHTTP_ASYNC_RESULT* result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  net_error_ = HRESULT_FROM_WIN32(result->dwError);
  CompleteFetch();
}

void NetworkFetcher::WriteDataToFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(&NetworkFetcher::WriteDataToFileBlocking, this),
      base::BindOnce(&NetworkFetcher::WriteDataToFileComplete, this));
}

// Returns true if EOF is reached.
bool NetworkFetcher::WriteDataToFileBlocking() {
  if (read_buffer_.empty()) {
    file_.Close();
    net_error_ = S_OK;
    return true;
  }

  if (!file_.IsValid()) {
    file_.Initialize(file_path_,
                     base::File::Flags::FLAG_CREATE_ALWAYS |
                         base::File::Flags::FLAG_WRITE |
                         base::File::Flags::FLAG_WIN_SEQUENTIAL_SCAN);
    if (!file_.IsValid()) {
      net_error_ = HRESULTFromLastError();
      return false;
    }
  }

  DCHECK(file_.IsValid());
  if (file_.WriteAtCurrentPos(&read_buffer_.front(), read_buffer_.size()) ==
      -1) {
    net_error_ = HRESULTFromLastError();
    file_.Close();
    base::DeleteFile(file_path_);
    return false;
  }

  content_size_ += read_buffer_.size();
  return false;
}

void NetworkFetcher::WriteDataToFileComplete(bool is_eof) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  fetch_progress_callback_.Run(base::saturated_cast<int64_t>(content_size_));

  if (is_eof || FAILED(net_error_)) {
    CompleteFetch();
    return;
  }

  net_error_ = ReadData();
  if (FAILED(net_error_))
    CompleteFetch();
}

void NetworkFetcher::WriteDataToMemory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (read_buffer_.empty()) {
    VLOG(2) << post_response_body_;
    net_error_ = S_OK;
    CompleteFetch();
    return;
  }

  post_response_body_.append(read_buffer_.begin(), read_buffer_.end());
  content_size_ += read_buffer_.size();
  fetch_progress_callback_.Run(base::saturated_cast<int64_t>(content_size_));

  net_error_ = ReadData();
  if (FAILED(net_error_))
    CompleteFetch();
}

void __stdcall NetworkFetcher::WinHttpStatusCallback(HINTERNET handle,
                                                     DWORD_PTR context,
                                                     DWORD status,
                                                     void* info,
                                                     DWORD info_len) {
  DCHECK(handle);
  DCHECK(context);
  NetworkFetcher* network_fetcher = reinterpret_cast<NetworkFetcher*>(context);
  network_fetcher->main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NetworkFetcher::StatusCallback, network_fetcher, handle,
                     status, info, info_len));
}

void NetworkFetcher::StatusCallback(HINTERNET handle,
                                    uint32_t status,
                                    void* info,
                                    uint32_t info_len) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::StringPiece status_string;
  std::wstring info_string;
  switch (status) {
    case WINHTTP_CALLBACK_STATUS_HANDLE_CREATED:
      status_string = "handle created";
      break;
    case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
      status_string = "handle closing";
      break;
    case WINHTTP_CALLBACK_STATUS_RESOLVING_NAME:
      status_string = "resolving";
      info_string.assign(static_cast<wchar_t*>(info), info_len);  // host.
      break;
    case WINHTTP_CALLBACK_STATUS_NAME_RESOLVED:
      status_string = "resolved";
      break;
    case WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER:
      status_string = "connecting";
      info_string.assign(static_cast<wchar_t*>(info), info_len);  // IP.
      break;
    case WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER:
      status_string = "connected";
      break;
    case WINHTTP_CALLBACK_STATUS_SENDING_REQUEST:
      status_string = "sending";
      break;
    case WINHTTP_CALLBACK_STATUS_REQUEST_SENT:
      status_string = "sent";
      break;
    case WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE:
      status_string = "receiving response";
      break;
    case WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED:
      status_string = "response received";
      break;
    case WINHTTP_CALLBACK_STATUS_CLOSING_CONNECTION:
      status_string = "connection closing";
      break;
    case WINHTTP_CALLBACK_STATUS_CONNECTION_CLOSED:
      status_string = "connection closed";
      break;
    case WINHTTP_CALLBACK_STATUS_REDIRECT:
      // |info| may contain invalid URL data and not safe to reference always.
      status_string = "redirect";
      break;
    case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
      status_string = "data available";
      DCHECK_EQ(info_len, sizeof(uint32_t));
      info_string = base::StringPrintf(L"%lu", *static_cast<uint32_t*>(info));
      break;
    case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
      status_string = "headers available";
      break;
    case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
      status_string = "read complete";
      info_string = base::StringPrintf(L"%lu", info_len);
      break;
    case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
      status_string = "send request complete";
      break;
    case WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE:
      status_string = "write complete";
      break;
    case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
      status_string = "request error";
      break;
    case WINHTTP_CALLBACK_STATUS_SECURE_FAILURE:
      status_string = "https failure";
      DCHECK(info);
      DCHECK_EQ(info_len, sizeof(uint32_t));
      info_string = base::StringPrintf(L"%#x", *static_cast<uint32_t*>(info));
      break;
    default:
      status_string = "unknown callback";
      break;
  }

  std::string msg;
  if (!status_string.empty())
    base::StringAppendF(&msg, "status=%s", status_string.data());
  else
    base::StringAppendF(&msg, "status=%#x", status);
  if (!info_string.empty())
    base::StringAppendF(&msg, ", info=%s",
                        base::SysWideToUTF8(info_string).c_str());

  VLOG(3) << "WinHttp status callback:"
          << " handle=" << handle << ", " << msg;

  switch (status) {
    case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
      self_ = nullptr;
      break;
    case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
      SendRequestComplete();
      break;
    case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
      HeadersAvailable();
      break;
    case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
      DCHECK_EQ(info, &read_buffer_.front());
      ReadDataComplete(info_len);
      break;
    case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
      RequestError(static_cast<const WINHTTP_ASYNC_RESULT*>(info));
      break;
  }
}

}  // namespace winhttp
