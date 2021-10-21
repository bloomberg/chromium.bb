// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_SHARED_STORAGE_WORKLET_URL_SELECTION_OPERATION_HANDLER_H_
#define CONTENT_SERVICES_SHARED_STORAGE_WORKLET_URL_SELECTION_OPERATION_HANDLER_H_

#include <map>

#include "content/services/shared_storage_worklet/public/mojom/shared_storage_worklet_service.mojom.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace gin {
class Arguments;
}  // namespace gin

namespace shared_storage_worklet {

class UrlSelectionOperationHandler {
 public:
  struct PendingRequest;

  UrlSelectionOperationHandler();

  ~UrlSelectionOperationHandler();

  void RegisterOperation(gin::Arguments* args);

  void RunOperation(
      v8::Local<v8::Context> context,
      const std::string& name,
      const std::vector<std::string>& urls,
      const std::vector<uint8_t>& serialized_data,
      mojom::SharedStorageWorkletService::RunURLSelectionOperationCallback
          callback);

  void OnPromiseFulfilled(PendingRequest* request, gin::Arguments* args);

  void OnPromiseRejected(PendingRequest* request, gin::Arguments* args);

 private:
  std::map<std::string, v8::Global<v8::Function>> operation_definition_map_;

  std::map<PendingRequest*, std::unique_ptr<PendingRequest>> pending_requests_;

  base::WeakPtrFactory<UrlSelectionOperationHandler> weak_ptr_factory_{this};
};

}  // namespace shared_storage_worklet

#endif  // CONTENT_SERVICES_SHARED_STORAGE_WORKLET_URL_SELECTION_OPERATION_HANDLER_H_
