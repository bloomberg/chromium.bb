// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerInstalledScriptsManager.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/threading/thread_checker.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "modules/serviceworkers/ServiceWorkerThread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/StringBuilder.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"

namespace blink {

using RawScriptData = ThreadSafeScriptContainer::RawScriptData;

namespace {

// Receiver is a class to read a Mojo data pipe. Received data are stored in
// chunks. Lives on the IO thread. Receiver is owned by Internal via
// BundledReceivers. It is created to read the script body or metadata from a
// data pipe, and is destroyed when the read finishes.
class Receiver {
 public:
  using BytesChunk = WebVector<char>;

  Receiver(mojo::ScopedDataPipeConsumerHandle handle, uint64_t total_bytes)
      : handle_(std::move(handle)),
        watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
        remaining_bytes_(total_bytes) {}

  void Start(base::OnceClosure callback) {
    if (!handle_.is_valid()) {
      std::move(callback).Run();
      return;
    }
    callback_ = std::move(callback);
    // Unretained is safe because |watcher_| is owned by |this|.
    MojoResult rv = watcher_.Watch(
        handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
        WTF::BindRepeating(&Receiver::OnReadable, WTF::Unretained(this)));
    DCHECK_EQ(MOJO_RESULT_OK, rv);
    watcher_.ArmOrNotify();
  }

  void OnReadable(MojoResult) {
    // It isn't necessary to handle MojoResult here since BeginReadDataRaw()
    // returns an equivalent error.
    const void* buffer = nullptr;
    uint32_t bytes_read = 0;
    MojoResult rv =
        handle_->BeginReadData(&buffer, &bytes_read, MOJO_READ_DATA_FLAG_NONE);
    switch (rv) {
      case MOJO_RESULT_BUSY:
      case MOJO_RESULT_INVALID_ARGUMENT:
        NOTREACHED();
        return;
      case MOJO_RESULT_FAILED_PRECONDITION:
        // Closed by peer.
        OnCompleted();
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        watcher_.ArmOrNotify();
        return;
      case MOJO_RESULT_OK:
        break;
      default:
        // mojo::BeginReadDataRaw() should not return any other values.
        // Notify the error to the browser by resetting the handle even though
        // it's in the middle of data transfer.
        OnCompleted();
        return;
    }

    if (bytes_read > 0)
      chunks_.emplace_back(static_cast<const char*>(buffer), bytes_read);

    rv = handle_->EndReadData(bytes_read);
    DCHECK_EQ(rv, MOJO_RESULT_OK);
    CHECK_GE(remaining_bytes_, bytes_read);
    remaining_bytes_ -= bytes_read;
    watcher_.ArmOrNotify();
  }

  bool is_running() const { return handle_.is_valid(); }
  bool has_received_all_data() const { return remaining_bytes_ == 0; }

  WTF::Vector<BytesChunk> TakeChunks() {
    DCHECK(!is_running());
    return std::move(chunks_);
  }

 private:
  void OnCompleted() {
    handle_.reset();
    watcher_.Cancel();
    if (!has_received_all_data())
      chunks_.clear();
    DCHECK(callback_);
    std::move(callback_).Run();
  }

  base::OnceClosure callback_;
  mojo::ScopedDataPipeConsumerHandle handle_;
  mojo::SimpleWatcher watcher_;

  WTF::Vector<BytesChunk> chunks_;
  uint64_t remaining_bytes_;
};

// BundledReceivers is a helper class to wait for the end of reading body and
// meta data. Lives on the IO thread.
class BundledReceivers {
 public:
  BundledReceivers(mojo::ScopedDataPipeConsumerHandle meta_data_handle,
                   uint64_t meta_data_size,
                   mojo::ScopedDataPipeConsumerHandle body_handle,
                   uint64_t body_size)
      : meta_data_(std::move(meta_data_handle), meta_data_size),
        body_(std::move(body_handle), body_size) {}

  // Starts reading the pipes and invokes |callback| when both are finished.
  void Start(base::OnceClosure callback) {
    base::RepeatingClosure wait_all_closure =
        base::BarrierClosure(2, std::move(callback));
    meta_data_.Start(wait_all_closure);
    body_.Start(wait_all_closure);
  }

  Receiver* meta_data() { return &meta_data_; }
  Receiver* body() { return &body_; }

 private:
  Receiver meta_data_;
  Receiver body_;
};

// Internal lives on the IO thread. This receives
// mojom::blink::ServiceWorkerScriptInfo for all installed scripts and then
// starts reading the body and meta data from the browser. This instance will be
// kept alive as long as the Mojo's connection is established.
class Internal : public mojom::blink::ServiceWorkerInstalledScriptsManager {
 public:
  // Called on the IO thread.
  // Creates and binds a new Internal instance to |request|.
  static void Create(
      scoped_refptr<ThreadSafeScriptContainer> script_container,
      mojom::blink::ServiceWorkerInstalledScriptsManagerRequest request) {
    mojo::MakeStrongBinding(
        std::make_unique<Internal>(std::move(script_container)),
        std::move(request));
  }

  Internal(scoped_refptr<ThreadSafeScriptContainer> script_container)
      : script_container_(std::move(script_container)), weak_factory_(this) {}

  ~Internal() override {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    // Wake up a waiting thread so it does not wait forever. If the script has
    // not been added yet, that means something went wrong. From here,
    // script_container_->Wait() will return false if the script hasn't been
    // added yet.
    script_container_->OnAllDataAddedOnIOThread();
  }

  // Implements mojom::blink::ServiceWorkerInstalledScriptsManager.
  // Called on the IO thread.
  void TransferInstalledScript(
      mojom::blink::ServiceWorkerScriptInfoPtr script_info) override {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    KURL script_url(script_info->script_url);
    auto receivers = std::make_unique<BundledReceivers>(
        std::move(script_info->meta_data), script_info->meta_data_size,
        std::move(script_info->body), script_info->body_size);
    receivers->Start(WTF::Bind(&Internal::OnScriptReceived,
                               weak_factory_.GetWeakPtr(),
                               std::move(script_info)));
    DCHECK(!running_receivers_.Contains(script_url));
    running_receivers_.insert(script_url, std::move(receivers));
  }

  // Called on the IO thread.
  void OnScriptReceived(mojom::blink::ServiceWorkerScriptInfoPtr script_info) {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    auto iter = running_receivers_.find(script_info->script_url);
    DCHECK(iter != running_receivers_.end());
    std::unique_ptr<BundledReceivers> receivers = std::move(iter->value);
    DCHECK(receivers);
    if (!receivers->body()->has_received_all_data() ||
        !receivers->meta_data()->has_received_all_data()) {
      script_container_->AddOnIOThread(script_info->script_url,
                                       RawScriptData::CreateInvalidInstance());
      running_receivers_.erase(iter);
      return;
    }

    auto script_data = RawScriptData::Create(
        script_info->encoding, receivers->body()->TakeChunks(),
        receivers->meta_data()->TakeChunks());
    for (auto it : script_info->headers) {
      script_data->AddHeader(it.key, it.value);
    }
    script_container_->AddOnIOThread(script_info->script_url,
                                     std::move(script_data));
    running_receivers_.erase(iter);
  }

 private:
  THREAD_CHECKER(io_thread_checker_);
  HashMap<KURL, std::unique_ptr<BundledReceivers>> running_receivers_;
  scoped_refptr<ThreadSafeScriptContainer> script_container_;
  base::WeakPtrFactory<Internal> weak_factory_;
};

}  // namespace

ServiceWorkerInstalledScriptsManager::ServiceWorkerInstalledScriptsManager(
    WebVector<WebURL> installed_urls,
    mojom::blink::ServiceWorkerInstalledScriptsManagerRequest manager_request,
    mojom::blink::ServiceWorkerInstalledScriptsManagerHostPtrInfo
        manager_host_ptr,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : script_container_(base::MakeRefCounted<ThreadSafeScriptContainer>()) {
  // We're on the main thread now, but |installed_urls_| will be accessed on the
  // worker thread later, so make a deep copy of |url| as key.
  for (WebURL& url : installed_urls)
    installed_urls_.insert(KURL(url).Copy());
  manager_host_ =
      mojom::blink::ThreadSafeServiceWorkerInstalledScriptsManagerHostPtr::
          Create(mojom::blink::ServiceWorkerInstalledScriptsManagerHostPtr(
              std::move(manager_host_ptr)));
  io_task_runner->PostTask(
      FROM_HERE, ConvertToBaseCallback(
                     CrossThreadBind(&Internal::Create, script_container_,
                                     WTF::Passed(std::move(manager_request)))));
}

bool ServiceWorkerInstalledScriptsManager::IsScriptInstalled(
    const KURL& script_url) const {
  return installed_urls_.Contains(script_url);
}

InstalledScriptsManager::ScriptStatus
ServiceWorkerInstalledScriptsManager::GetScriptData(
    const KURL& script_url,
    InstalledScriptsManager::ScriptData* out_script_data) {
  DCHECK(!IsMainThread());
  // This blocks until the script is received from the browser.
  std::unique_ptr<RawScriptData> raw_script_data = GetRawScriptData(script_url);
  DCHECK(raw_script_data);
  if (!raw_script_data->IsValid()) {
    *out_script_data = InstalledScriptsManager::ScriptData();
    return ScriptStatus::kFailed;
  }

  // This is from WorkerScriptLoader::DidReceiveData.
  std::unique_ptr<TextResourceDecoder> decoder =
      TextResourceDecoder::Create(TextResourceDecoderOptions(
          TextResourceDecoderOptions::kPlainTextContent,
          raw_script_data->Encoding().IsEmpty()
              ? UTF8Encoding()
              : WTF::TextEncoding(raw_script_data->Encoding())));

  StringBuilder source_text_builder;
  for (const auto& chunk : raw_script_data->ScriptTextChunks())
    source_text_builder.Append(decoder->Decode(chunk.Data(), chunk.size()));

  std::unique_ptr<Vector<char>> meta_data;
  if (raw_script_data->MetaDataChunks().size() > 0) {
    size_t total_metadata_size = 0;
    for (const auto& chunk : raw_script_data->MetaDataChunks())
      total_metadata_size += chunk.size();
    meta_data = std::make_unique<Vector<char>>();
    meta_data->ReserveInitialCapacity(total_metadata_size);
    for (const auto& chunk : raw_script_data->MetaDataChunks())
      meta_data->Append(chunk.Data(), chunk.size());
  }

  InstalledScriptsManager::ScriptData script_data(
      script_url, source_text_builder.ToString(), std::move(meta_data),
      raw_script_data->TakeHeaders());
  *out_script_data = std::move(script_data);
  return ScriptStatus::kSuccess;
}

std::unique_ptr<RawScriptData>
ServiceWorkerInstalledScriptsManager::GetRawScriptData(const KURL& script_url) {
  if (!IsScriptInstalled(script_url))
    return nullptr;

  ThreadSafeScriptContainer::ScriptStatus status =
      script_container_->GetStatusOnWorkerThread(script_url);
  // If the script has already been taken, request the browser to send the
  // script.
  if (status == ThreadSafeScriptContainer::ScriptStatus::kTaken) {
    script_container_->ResetOnWorkerThread(script_url);
    (*manager_host_)->RequestInstalledScript(script_url);
    status = script_container_->GetStatusOnWorkerThread(script_url);
  }

  // If the script has not been received at this point, wait for arrival by
  // blocking the worker thread.
  if (status == ThreadSafeScriptContainer::ScriptStatus::kPending) {
    // Wait for arrival of the script.
    const bool success = script_container_->WaitOnWorkerThread(script_url);
    // It can fail due to an error on Mojo pipes.
    if (!success)
      return RawScriptData::CreateInvalidInstance();
    status = script_container_->GetStatusOnWorkerThread(script_url);
    DCHECK_NE(ThreadSafeScriptContainer::ScriptStatus::kPending, status);
  }

  if (status == ThreadSafeScriptContainer::ScriptStatus::kFailed)
    return RawScriptData::CreateInvalidInstance();
  DCHECK_EQ(ThreadSafeScriptContainer::ScriptStatus::kReceived, status);

  return script_container_->TakeOnWorkerThread(script_url);
}

}  // namespace blink
