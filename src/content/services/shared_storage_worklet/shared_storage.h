// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_SHARED_STORAGE_WORKLET_SHARED_STORAGE_H_
#define CONTENT_SERVICES_SHARED_STORAGE_WORKLET_SHARED_STORAGE_H_

#include "base/memory/raw_ptr.h"
#include "content/services/shared_storage_worklet/public/mojom/shared_storage_worklet_service.mojom.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"

namespace gin {
class Arguments;
}  // namespace gin

namespace shared_storage_worklet {

class SharedStorage final : public gin::Wrappable<SharedStorage> {
 public:
  explicit SharedStorage(mojom::SharedStorageWorkletServiceClient* client);
  ~SharedStorage() override;

  static gin::WrapperInfo kWrapperInfo;

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  const char* GetTypeName() override;

 private:
  v8::Local<v8::Promise> Set(gin::Arguments* args);
  v8::Local<v8::Promise> Append(gin::Arguments* args);
  v8::Local<v8::Promise> Delete(gin::Arguments* args);
  v8::Local<v8::Promise> Clear(gin::Arguments* args);
  v8::Local<v8::Promise> Get(gin::Arguments* args);
  v8::Local<v8::Object> Keys(gin::Arguments* args);
  v8::Local<v8::Object> Entries(gin::Arguments* args);
  v8::Local<v8::Promise> Length(gin::Arguments* args);

  void OnVoidOperationFinished(
      v8::Isolate* isolate,
      v8::Global<v8::Promise::Resolver> global_resolver,
      bool success,
      const std::string& error_message);

  void OnStringRetrievalOperationFinished(
      v8::Isolate* isolate,
      v8::Global<v8::Promise::Resolver> global_resolver,
      bool success,
      const std::string& error_message,
      const std::string& result);

  void OnLengthOperationFinished(
      v8::Isolate* isolate,
      v8::Global<v8::Promise::Resolver> global_resolver,
      bool success,
      const std::string& error_message,
      uint32_t length);

  raw_ptr<mojom::SharedStorageWorkletServiceClient> client_;

  base::WeakPtrFactory<SharedStorage> weak_ptr_factory_{this};
};

}  // namespace shared_storage_worklet

#endif  // CONTENT_SERVICES_SHARED_STORAGE_WORKLET_SHARED_STORAGE_H_
