// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/shared_storage_worklet/shared_storage.h"

#include "base/logging.h"
#include "content/services/shared_storage_worklet/shared_storage_iterator.h"
#include "content/services/shared_storage_worklet/worklet_v8_helper.h"
#include "gin/arguments.h"
#include "gin/dictionary.h"
#include "gin/handle.h"

namespace shared_storage_worklet {

SharedStorage::SharedStorage(mojom::SharedStorageWorkletServiceClient* client)
    : client_(client) {}

SharedStorage::~SharedStorage() = default;

gin::WrapperInfo SharedStorage::kWrapperInfo = {gin::kEmbedderNativeGin};

gin::ObjectTemplateBuilder SharedStorage::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<SharedStorage>::GetObjectTemplateBuilder(isolate)
      .SetMethod("set", &SharedStorage::Set)
      .SetMethod("append", &SharedStorage::Append)
      .SetMethod("delete", &SharedStorage::Delete)
      .SetMethod("clear", &SharedStorage::Clear)
      .SetMethod("get", &SharedStorage::Get)
      .SetMethod("keys", &SharedStorage::Keys)
      .SetMethod("entries", &SharedStorage::Entries)
      .SetMethod("length", &SharedStorage::Length);
}

const char* SharedStorage::GetTypeName() {
  return "SharedStorage";
}

v8::Local<v8::Promise> SharedStorage::Set(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();

  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(args->GetHolderCreationContext())
          .ToLocalChecked();

  v8::Local<v8::Promise> promise = resolver->GetPromise();

  std::string arg0_key;
  if (!args->GetNext(&arg0_key)) {
    resolver
        ->Reject(
            args->GetHolderCreationContext(),
            gin::StringToV8(
                isolate,
                "Missing or invalid \"key\" argument in sharedStorage.set()"))
        .ToChecked();
    return promise;
  }

  std::string arg1_value;
  if (!args->GetNext(&arg1_value)) {
    resolver
        ->Reject(
            args->GetHolderCreationContext(),
            gin::StringToV8(
                isolate,
                "Missing or invalid \"value\" argument in sharedStorage.set()"))
        .ToChecked();
    return promise;
  }

  gin::Dictionary arg2_options_dict = gin::Dictionary::CreateEmpty(isolate);

  v8::Local<v8::Value> arg2_options;
  if (args->GetNext(&arg2_options)) {
    if (!gin::ConvertFromV8(isolate, arg2_options, &arg2_options_dict)) {
      resolver
          ->Reject(args->GetHolderCreationContext(),
                   gin::StringToV8(
                       isolate,
                       "Invalid \"options\" argument in sharedStorage.set()"))
          .ToChecked();
      return promise;
    }
  }

  bool ignore_if_present = false;
  arg2_options_dict.Get<bool>("ignoreIfPresent", &ignore_if_present);

  client_->SharedStorageSet(
      arg0_key, arg1_value, ignore_if_present,
      base::BindOnce(&SharedStorage::OnVoidOperationFinished,
                     weak_ptr_factory_.GetWeakPtr(), isolate,
                     v8::Global<v8::Promise::Resolver>(isolate, resolver)));

  return promise;
}

v8::Local<v8::Promise> SharedStorage::Append(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();

  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(args->GetHolderCreationContext())
          .ToLocalChecked();

  v8::Local<v8::Promise> promise = resolver->GetPromise();

  std::string arg0_key;
  if (!args->GetNext(&arg0_key)) {
    resolver
        ->Reject(args->GetHolderCreationContext(),
                 gin::StringToV8(isolate,
                                 "Missing or invalid \"key\" argument in "
                                 "sharedStorage.append()"))
        .ToChecked();
    return promise;
  }

  std::string arg1_value;
  if (!args->GetNext(&arg1_value)) {
    resolver
        ->Reject(args->GetHolderCreationContext(),
                 gin::StringToV8(isolate,
                                 "Missing or invalid \"value\" argument in "
                                 "sharedStorage.append()"))
        .ToChecked();
    return promise;
  }

  client_->SharedStorageAppend(
      arg0_key, arg1_value,
      base::BindOnce(&SharedStorage::OnVoidOperationFinished,
                     weak_ptr_factory_.GetWeakPtr(), isolate,
                     v8::Global<v8::Promise::Resolver>(isolate, resolver)));

  return promise;
}

v8::Local<v8::Promise> SharedStorage::Delete(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();

  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(args->GetHolderCreationContext())
          .ToLocalChecked();

  v8::Local<v8::Promise> promise = resolver->GetPromise();

  std::string arg0_key;
  if (!args->GetNext(&arg0_key)) {
    resolver
        ->Reject(args->GetHolderCreationContext(),
                 gin::StringToV8(isolate,
                                 "Missing or invalid \"key\" argument in "
                                 "sharedStorage.delete()"))
        .ToChecked();
    return promise;
  }

  client_->SharedStorageDelete(
      arg0_key,
      base::BindOnce(&SharedStorage::OnVoidOperationFinished,
                     weak_ptr_factory_.GetWeakPtr(), isolate,
                     v8::Global<v8::Promise::Resolver>(isolate, resolver)));

  return promise;
}

v8::Local<v8::Promise> SharedStorage::Clear(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();

  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(args->GetHolderCreationContext())
          .ToLocalChecked();

  v8::Local<v8::Promise> promise = resolver->GetPromise();

  client_->SharedStorageClear(base::BindOnce(
      &SharedStorage::OnVoidOperationFinished, weak_ptr_factory_.GetWeakPtr(),
      isolate, v8::Global<v8::Promise::Resolver>(isolate, resolver)));

  return promise;
}

v8::Local<v8::Promise> SharedStorage::Get(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();

  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(args->GetHolderCreationContext())
          .ToLocalChecked();

  v8::Local<v8::Promise> promise = resolver->GetPromise();

  std::string arg0_key;
  if (!args->GetNext(&arg0_key)) {
    resolver
        ->Reject(
            args->GetHolderCreationContext(),
            gin::StringToV8(
                isolate,
                "Missing or invalid \"key\" argument in sharedStorage.get()"))
        .ToChecked();
    return promise;
  }

  client_->SharedStorageGet(
      arg0_key,
      base::BindOnce(&SharedStorage::OnStringRetrievalOperationFinished,
                     weak_ptr_factory_.GetWeakPtr(), isolate,
                     v8::Global<v8::Promise::Resolver>(isolate, resolver)));
  return promise;
}

v8::Local<v8::Object> SharedStorage::Keys(gin::Arguments* args) {
  return (new SharedStorageIterator(SharedStorageIterator::Mode::kKey, client_))
      ->GetWrapper(args->isolate())
      .ToLocalChecked();
}

v8::Local<v8::Object> SharedStorage::Entries(gin::Arguments* args) {
  return (new SharedStorageIterator(SharedStorageIterator::Mode::kKeyValue,
                                    client_))
      ->GetWrapper(args->isolate())
      .ToLocalChecked();
}

v8::Local<v8::Promise> SharedStorage::Length(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();

  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(args->GetHolderCreationContext())
          .ToLocalChecked();

  v8::Local<v8::Promise> promise = resolver->GetPromise();

  client_->SharedStorageLength(base::BindOnce(
      &SharedStorage::OnLengthOperationFinished, weak_ptr_factory_.GetWeakPtr(),
      isolate, v8::Global<v8::Promise::Resolver>(isolate, resolver)));

  return promise;
}

void SharedStorage::OnVoidOperationFinished(
    v8::Isolate* isolate,
    v8::Global<v8::Promise::Resolver> global_resolver,
    bool success,
    const std::string& error_message) {
  WorkletV8Helper::HandleScope scope(isolate);
  v8::Local<v8::Promise::Resolver> resolver = global_resolver.Get(isolate);
  v8::Local<v8::Context> context = resolver->GetCreationContextChecked();

  if (success) {
    resolver->Resolve(context, v8::Undefined(isolate)).ToChecked();
    return;
  }

  resolver->Reject(context, gin::StringToV8(isolate, error_message))
      .ToChecked();
}

void SharedStorage::OnStringRetrievalOperationFinished(
    v8::Isolate* isolate,
    v8::Global<v8::Promise::Resolver> global_resolver,
    bool success,
    const std::string& error_message,
    const std::string& result) {
  WorkletV8Helper::HandleScope scope(isolate);
  v8::Local<v8::Promise::Resolver> resolver = global_resolver.Get(isolate);
  v8::Local<v8::Context> context = resolver->GetCreationContextChecked();

  if (success) {
    resolver->Resolve(context, gin::StringToV8(isolate, result)).ToChecked();
    return;
  }

  resolver->Reject(context, gin::StringToV8(isolate, error_message))
      .ToChecked();
}

void SharedStorage::OnLengthOperationFinished(
    v8::Isolate* isolate,
    v8::Global<v8::Promise::Resolver> global_resolver,
    bool success,
    const std::string& error_message,
    uint32_t length) {
  WorkletV8Helper::HandleScope scope(isolate);
  v8::Local<v8::Promise::Resolver> resolver = global_resolver.Get(isolate);
  v8::Local<v8::Context> context = resolver->GetCreationContextChecked();

  if (success) {
    resolver->Resolve(context, gin::Converter<uint32_t>::ToV8(isolate, length))
        .ToChecked();
    return;
  }

  resolver->Reject(context, gin::StringToV8(isolate, error_message))
      .ToChecked();
}

}  // namespace shared_storage_worklet
