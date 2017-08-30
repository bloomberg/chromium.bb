// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/spawned_test_server/spawner_communicator.h"

#include <inttypes.h>

#include <limits>
#include <utility>

#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/supports_user_data.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/port_util.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_test_util.h"
#include "url/gurl.h"

namespace net {

namespace {

int kBufferSize = 2048;

// A class to hold all data needed to send a command to spawner server.
class SpawnerRequestData : public base::SupportsUserData::Data {
 public:
  SpawnerRequestData(int* result_code, std::string* data_received)
      : buf_(new IOBuffer(kBufferSize)),
        result_code_(result_code),
        data_received_(data_received) {
    DCHECK(result_code);
    *result_code_ = OK;
    DCHECK(data_received);
    data_received_->clear();
  }

  ~SpawnerRequestData() override {}

  IOBuffer* buf() const { return buf_.get(); }

  bool IsResultOK() const { return *result_code_ == OK; }

  const std::string& data_received() const { return *data_received_; }
  void ClearReceivedData() { data_received_->clear(); }

  void SetResultCode(int result_code) { *result_code_ = result_code; }

  void IncreaseResponseStartedCount() { response_started_count_++; }

  int response_started_count() const { return response_started_count_; }

  // Write data read from URLRequest::Read() to |data_received_|. Returns true
  // if |num_bytes| is great than 0. |num_bytes| is 0 for EOF, < 0 on errors.
  bool ConsumeBytesRead(int num_bytes) {
    // Error while reading, or EOF.
    if (num_bytes <= 0)
      return false;

    data_received_->append(buf_->data(), num_bytes);
    return true;
  }

 private:
  // Buffer that URLRequest writes into.
  scoped_refptr<IOBuffer> buf_;

  // Holds the error condition that was hit on the current request, or OK.
  int* result_code_;

  // Data received from server;
  std::string* data_received_;

  // Used to track how many times the OnResponseStarted get called after
  // sending a command to spawner server.
  int response_started_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SpawnerRequestData);
};

}  // namespace

SpawnerCommunicator::SpawnerCommunicator(const RemoteTestServerConfig& config)
    : config_(config),
      io_thread_("spawner_communicator"),
      event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
             base::WaitableEvent::InitialState::NOT_SIGNALED) {}

SpawnerCommunicator::~SpawnerCommunicator() {
  DCHECK(!io_thread_.IsRunning());
}

void SpawnerCommunicator::WaitForResponse() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  event_.Wait();
  event_.Reset();
}

void SpawnerCommunicator::StartIOThread() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (io_thread_.IsRunning())
    return;

  bool thread_started = io_thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  DCHECK(thread_started);
}

void SpawnerCommunicator::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(io_thread_.IsRunning());
  // The request and its context should be created and destroyed only on the
  // IO thread.
  DCHECK(!cur_request_.get());
  DCHECK(!context_.get());
  io_thread_.Stop();
  allowed_port_.reset();
}

int SpawnerCommunicator::SendCommandAndWaitForResult(
    const std::string& command,
    const std::string& post_data,
    std::string* data_received) {
  DCHECK(data_received);

  // Start the communicator thread to talk to test server spawner.
  StartIOThread();
  DCHECK(io_thread_.message_loop());

  // Since the method will be blocked until SpawnerCommunicator gets result
  // from the spawner server or timed-out. It's safe to use base::Unretained
  // when using base::Bind.
  int result_code;
  io_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SpawnerCommunicator::SendCommandAndWaitForResultOnIOThread,
                 base::Unretained(this), command, post_data, &result_code,
                 data_received));
  WaitForResponse();

  return result_code;
}

void SpawnerCommunicator::SendCommandAndWaitForResultOnIOThread(
    const std::string& command,
    const std::string& post_data,
    int* result_code,
    std::string* data_received) {
  DCHECK(io_thread_.task_runner()->BelongsToCurrentThread());

  // Prepare the URLRequest for sending the command.
  DCHECK(!cur_request_.get());
  context_.reset(new TestURLRequestContext);
  GURL url = config_.GetSpawnerUrl(command);
  allowed_port_ = std::make_unique<ScopedPortException>(url.EffectiveIntPort());
  cur_request_ = context_->CreateRequest(url, DEFAULT_PRIORITY, this);

  DCHECK(cur_request_);
  cur_request_->SetUserData(
      this, std::make_unique<SpawnerRequestData>(result_code, data_received));

  if (post_data.empty()) {
    cur_request_->set_method("GET");
  } else {
    cur_request_->set_method("POST");
    std::unique_ptr<UploadElementReader> reader(
        UploadOwnedBytesElementReader::CreateWithString(post_data));
    cur_request_->set_upload(
        ElementsUploadDataStream::CreateWithReader(std::move(reader), 0));
    HttpRequestHeaders headers;
    headers.SetHeader(HttpRequestHeaders::kContentType,
                      "application/json");
    cur_request_->SetExtraRequestHeaders(headers);
  }

  timeout_timer_ = std::make_unique<base::OneShotTimer>();
  timeout_timer_->Start(
      FROM_HERE, TestTimeouts::action_max_timeout(),
      base::Bind(&SpawnerCommunicator::OnTimeout, base::Unretained(this)));

  // Start the request.
  cur_request_->Start();
}

void SpawnerCommunicator::OnTimeout() {
  DCHECK(io_thread_.task_runner()->BelongsToCurrentThread());

  SpawnerRequestData* data =
      static_cast<SpawnerRequestData*>(cur_request_->GetUserData(this));
  DCHECK(data);

  // Set the result code and cancel the timed-out task.
  int result = cur_request_->CancelWithError(ERR_TIMED_OUT);
  OnSpawnerCommandCompleted(cur_request_.get(), result);
}

void SpawnerCommunicator::OnSpawnerCommandCompleted(URLRequest* request,
                                                    int net_error) {
  DCHECK(io_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(ERR_IO_PENDING, net_error);

  if (!cur_request_.get())
    return;
  DCHECK_EQ(request, cur_request_.get());
  SpawnerRequestData* data =
      static_cast<SpawnerRequestData*>(cur_request_->GetUserData(this));
  DCHECK(data);

  // If request has failed, return the error code.
  if (net_error != OK) {
    LOG(ERROR) << "request failed, error: " << ErrorToString(net_error);
    data->SetResultCode(net_error);
  } else if (request->GetResponseCode() != 200) {
    LOG(ERROR) << "Spawner server returned bad status: "
               << request->response_headers()->GetStatusLine() << ", "
               << data->data_received();
    data->SetResultCode(ERR_FAILED);
  } else {
    DCHECK_EQ(1, data->response_started_count());
  }

  if (!data->IsResultOK()) {
    // Clear the buffer of received data if any net error happened.
    data->ClearReceivedData();
  }

  // Clear current request to indicate the completion of sending a command
  // to spawner server and getting the result.
  cur_request_.reset();
  context_.reset();
  timeout_timer_.reset();

  // Wakeup the caller in user thread.
  event_.Signal();
}

void SpawnerCommunicator::ReadResult(URLRequest* request) {
  DCHECK(io_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(request, cur_request_.get());
  SpawnerRequestData* data =
      static_cast<SpawnerRequestData*>(cur_request_->GetUserData(this));
  DCHECK(data);

  IOBuffer* buf = data->buf();
  // Read as many bytes as are available synchronously.
  while (true) {
    int rv = request->Read(buf, kBufferSize);
    if (rv == ERR_IO_PENDING)
      return;

    if (rv < 0) {
      OnSpawnerCommandCompleted(request, rv);
      return;
    }

    if (!data->ConsumeBytesRead(rv)) {
      OnSpawnerCommandCompleted(request, rv);
      return;
    }
  }
}

void SpawnerCommunicator::OnResponseStarted(URLRequest* request,
                                            int net_error) {
  DCHECK(io_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(request, cur_request_.get());
  DCHECK_NE(ERR_IO_PENDING, net_error);

  SpawnerRequestData* data =
      static_cast<SpawnerRequestData*>(cur_request_->GetUserData(this));
  DCHECK(data);

  data->IncreaseResponseStartedCount();

  if (net_error != OK) {
    OnSpawnerCommandCompleted(request, net_error);
    return;
  }

  ReadResult(request);
}

void SpawnerCommunicator::OnReadCompleted(URLRequest* request, int num_bytes) {
  DCHECK(io_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_NE(ERR_IO_PENDING, num_bytes);

  if (!cur_request_.get())
    return;
  DCHECK_EQ(request, cur_request_.get());
  SpawnerRequestData* data =
      static_cast<SpawnerRequestData*>(cur_request_->GetUserData(this));
  DCHECK(data);

  if (data->ConsumeBytesRead(num_bytes)) {
    // Keep reading.
    ReadResult(request);
  } else {
    // |bytes_read| < 0
    int net_error = num_bytes;
    OnSpawnerCommandCompleted(request, net_error);
  }
}

bool SpawnerCommunicator::StartServer(const std::string& arguments,
                                      std::string* server_data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Send the start command to spawner server to start the Python test server
  // on remote machine.
  int result = SendCommandAndWaitForResult("start", arguments, server_data);
  return result == OK;
}

bool SpawnerCommunicator::StopServer(uint16_t port) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // It's OK to stop the SpawnerCommunicator without starting it. Some tests
  // have test server on their test fixture but do not actually use it.
  if (!io_thread_.IsRunning())
    return true;

  // When the test is done, ask the test server spawner to kill the test server
  // on the remote machine.
  std::string server_return_data;
  std::string command = base::StringPrintf("kill?port=%" PRIu16, port);
  int result =
      SendCommandAndWaitForResult(command, std::string(), &server_return_data);
  Shutdown();
  if (result != OK || server_return_data != "killed")
    return false;
  return true;
}

}  // namespace net
