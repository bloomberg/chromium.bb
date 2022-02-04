// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/shared_storage_worklet/shared_storage_iterator.h"

#include "content/services/shared_storage_worklet/worklet_v8_helper.h"
#include "gin/arguments.h"
#include "gin/dictionary.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-promise.h"

namespace shared_storage_worklet {

SharedStorageIterator::SharedStorageIterator(
    Mode mode,
    mojom::SharedStorageWorkletServiceClient* client)
    : mode_(mode) {
  switch (mode_) {
    case Mode::kKey:
      client->SharedStorageKeys(receiver_.BindNewPipeAndPassRemote(
          base::ThreadTaskRunnerHandle::Get()));
      break;
    case Mode::kKeyValue:
      client->SharedStorageEntries(receiver_.BindNewPipeAndPassRemote(
          base::ThreadTaskRunnerHandle::Get()));
      break;
  }
}

SharedStorageIterator::~SharedStorageIterator() = default;

gin::WrapperInfo SharedStorageIterator::kWrapperInfo = {
    gin::kEmbedderNativeGin};

gin::ObjectTemplateBuilder SharedStorageIterator::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<SharedStorageIterator>::GetObjectTemplateBuilder(isolate)
      .SetMethod(v8::Symbol::GetAsyncIterator(isolate),
                 &SharedStorageIterator::GetThisObject)
      .SetMethod("next", &SharedStorageIterator::Next);
}

const char* SharedStorageIterator::GetTypeName() {
  return "SharedStorageIterator";
}

v8::Local<v8::Object> SharedStorageIterator::GetThisObject(
    gin::Arguments* args) {
  return GetWrapper(args->isolate()).ToLocalChecked();
}

v8::Local<v8::Promise> SharedStorageIterator::Next(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  v8::Local<v8::Context> context = args->GetHolderCreationContext();

  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(context).ToLocalChecked();

  return NextHelper(isolate, resolver);
}

v8::Local<v8::Promise> SharedStorageIterator::NextHelper(
    v8::Isolate* isolate,
    v8::Local<v8::Promise::Resolver> resolver) {
  v8::Local<v8::Context> context = resolver->GetCreationContextChecked();
  v8::Local<v8::Promise> promise = resolver->GetPromise();

  if (has_error_) {
    resolver->Reject(context, gin::StringToV8(isolate, error_message_))
        .ToChecked();
    return promise;
  }

  if (!pending_entries_.empty()) {
    mojom::SharedStorageKeyAndOrValuePtr next_entry =
        std::move(pending_entries_.front());
    pending_entries_.pop_front();

    resolver->Resolve(context, CreateIteratorResult(isolate, next_entry))
        .ToChecked();
    return promise;
  }

  if (waiting_for_more_entries_) {
    pending_resolvers_.emplace_back(
        v8::Global<v8::Promise::Resolver>(isolate, resolver));
    DCHECK(!isolate_for_pending_resolvers_ ||
           isolate_for_pending_resolvers_ == isolate);
    isolate_for_pending_resolvers_ = isolate;
    return promise;
  }

  DCHECK(pending_resolvers_.empty());
  resolver->Resolve(context, CreateIteratorResultDone(isolate)).ToChecked();
  return promise;
}

void SharedStorageIterator::DidReadEntries(
    bool success,
    const std::string& error_message,
    std::vector<mojom::SharedStorageKeyAndOrValuePtr> entries,
    bool has_more_entries) {
  DCHECK(waiting_for_more_entries_);
  DCHECK(!has_error_);
  DCHECK(!(success && entries.empty() && has_more_entries));

  if (!success) {
    has_error_ = true;
    error_message_ = error_message;
  }

  pending_entries_.insert(pending_entries_.end(),
                          std::make_move_iterator(entries.begin()),
                          std::make_move_iterator(entries.end()));

  waiting_for_more_entries_ = has_more_entries;

  while (
      !pending_resolvers_.empty() &&
      (!pending_entries_.empty() || has_error_ || !waiting_for_more_entries_)) {
    v8::Isolate* isolate = isolate_for_pending_resolvers_;
    DCHECK(isolate);

    v8::Global<v8::Promise::Resolver> global_resolver =
        std::move(pending_resolvers_.front());
    pending_resolvers_.pop_front();

    WorkletV8Helper::HandleScope scope(isolate);
    v8::Local<v8::Promise::Resolver> next_resolver =
        global_resolver.Get(isolate);
    global_resolver.Reset();

    v8::Local<v8::Context> context = next_resolver->GetCreationContextChecked();
    v8::Context::Scope context_scope(context);

    NextHelper(isolate, next_resolver);
  }

  if (pending_resolvers_.empty())
    isolate_for_pending_resolvers_ = nullptr;
}

v8::Local<v8::Object> SharedStorageIterator::CreateIteratorResult(
    v8::Isolate* isolate,
    const mojom::SharedStorageKeyAndOrValuePtr& entry) {
  v8::Local<v8::Object> obj = v8::Object::New(isolate);
  gin::Dictionary dict(isolate, obj);
  dict.Set<bool>("done", false);

  switch (mode_) {
    case Mode::kKey:
      dict.Set<std::string>("value", entry->key);
      break;
    case Mode::kKeyValue:
      dict.Set<std::vector<std::string>>("value", {entry->key, entry->value});
      break;
  }
  return obj;
}

v8::Local<v8::Object> SharedStorageIterator::CreateIteratorResultDone(
    v8::Isolate* isolate) {
  v8::Local<v8::Object> obj = v8::Object::New(isolate);
  gin::Dictionary dict(isolate, obj);
  dict.Set<bool>("done", true);
  return obj;
}

}  // namespace shared_storage_worklet
