// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ThreadSafeScriptContainer.h"

namespace blink {

// static
std::unique_ptr<ThreadSafeScriptContainer::RawScriptData>
ThreadSafeScriptContainer::RawScriptData::Create(
    const WTF::String& encoding,
    WTF::Vector<BytesChunk> script_text,
    WTF::Vector<BytesChunk> meta_data) {
  return WTF::WrapUnique(new RawScriptData(encoding, std::move(script_text),
                                           std::move(meta_data),
                                           true /* is_valid */));
}

// static
std::unique_ptr<ThreadSafeScriptContainer::RawScriptData>
ThreadSafeScriptContainer::RawScriptData::CreateInvalidInstance() {
  return WTF::WrapUnique(
      new RawScriptData(WTF::String() /* encoding */, WTF::Vector<BytesChunk>(),
                        WTF::Vector<BytesChunk>(), false /* is_valid */));
}

ThreadSafeScriptContainer::RawScriptData::RawScriptData(
    const WTF::String& encoding,
    WTF::Vector<BytesChunk> script_text,
    WTF::Vector<BytesChunk> meta_data,
    bool is_valid)
    : is_valid_(is_valid),
      encoding_(encoding.IsolatedCopy()),
      script_text_(std::move(script_text)),
      meta_data_(std::move(meta_data)),
      headers_(std::make_unique<CrossThreadHTTPHeaderMapData>()) {}

ThreadSafeScriptContainer::RawScriptData::~RawScriptData() = default;

void ThreadSafeScriptContainer::RawScriptData::AddHeader(
    const WTF::String& key,
    const WTF::String& value) {
  headers_->emplace_back(key.IsolatedCopy(), value.IsolatedCopy());
}

ThreadSafeScriptContainer::ThreadSafeScriptContainer()
    : are_all_data_added_(false) {}

void ThreadSafeScriptContainer::AddOnIOThread(
    const KURL& url,
    std::unique_ptr<RawScriptData> data) {
  WTF::MutexLocker locker(mutex_);
  DCHECK(!script_data_.Contains(url));
  // |script_data_| is also accessed on the worker thread, so make a deep copy
  // of |url| as key.
  script_data_.insert(url.Copy(), std::move(data));
  if (url == waiting_url_)
    waiting_cv_.Signal();
}

ThreadSafeScriptContainer::ScriptStatus
ThreadSafeScriptContainer::GetStatusOnWorkerThread(const KURL& url) {
  WTF::MutexLocker locker(mutex_);
  auto it = script_data_.find(url);
  if (it == script_data_.end())
    return ScriptStatus::kPending;
  if (!it->value)
    return ScriptStatus::kTaken;
  if (!it->value->IsValid())
    return ScriptStatus::kFailed;
  return ScriptStatus::kReceived;
}

void ThreadSafeScriptContainer::ResetOnWorkerThread(const KURL& url) {
  WTF::MutexLocker locker(mutex_);
  script_data_.erase(url);
}

bool ThreadSafeScriptContainer::WaitOnWorkerThread(const KURL& url) {
  WTF::MutexLocker locker(mutex_);
  DCHECK(waiting_url_.IsEmpty())
      << "The script container is unexpectedly shared among worker threads.";
  waiting_url_ = url;
  while (script_data_.find(url) == script_data_.end()) {
    // If waiting script hasn't been added yet though all data are received,
    // that means something went wrong.
    if (are_all_data_added_) {
      waiting_url_ = KURL();
      return false;
    }
    // This is possible to be waken up spuriously, so that it's necessary to
    // check if the entry is really added.
    waiting_cv_.Wait(mutex_);
  }
  waiting_url_ = KURL();
  // TODO(shimazu): Keep the status for each entries instead of using IsValid().
  const auto& data = script_data_.at(url);
  // Returns true if |data| has already been taken or is valid.
  return !data || data->IsValid();
}

std::unique_ptr<ThreadSafeScriptContainer::RawScriptData>
ThreadSafeScriptContainer::TakeOnWorkerThread(const KURL& url) {
  WTF::MutexLocker locker(mutex_);
  DCHECK(script_data_.Contains(url))
      << "Script should be added before calling Take.";
  return std::move(script_data_.find(url)->value);
}

void ThreadSafeScriptContainer::OnAllDataAddedOnIOThread() {
  WTF::MutexLocker locker(mutex_);
  are_all_data_added_ = true;
  waiting_cv_.Broadcast();
}

ThreadSafeScriptContainer::~ThreadSafeScriptContainer() = default;

}  // namespace blink
