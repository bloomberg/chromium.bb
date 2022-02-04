// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_LOADER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_LOADER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "content/services/auction_worklet/auction_downloader.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "v8/include/v8-persistent-handle.h"

namespace v8 {
class UnboundScript;
class WasmModuleObject;
}  // namespace v8

namespace auction_worklet {

class AuctionDownloader;

// Helper for downloading things and compiling them with V8 into an
// UnboundScript or WasmModuleObject on the V8 thread. Create via the
// appropriate subclass. That also provides the way extracting the appropriate
// type from Result.
class WorkletLoaderBase {
 public:
  // The result of loading JS or Wasm, memory-managing the underlying V8 object.
  //
  // This helps ensure that the script handle is deleted on the right thread
  // even in case when the callback handling the result is destroyed.
  class Result {
   public:
    Result();
    Result(scoped_refptr<AuctionV8Helper> v8_helper,
           v8::Global<v8::UnboundScript> script);
    Result(scoped_refptr<AuctionV8Helper> v8_helper,
           v8::Global<v8::WasmModuleObject> module);
    Result(Result&&);
    ~Result();

    Result& operator=(Result&&);

    // True if the script or module was loaded & compiled successfully.
    bool success() const;

   private:
    friend class WorkletLoader;
    friend class WorkletWasmLoader;

    // Will be deleted on v8_helper_->v8_runner(). See https://crbug.com/1231690
    // for why this is structured this way.
    struct V8Data {
      V8Data(scoped_refptr<AuctionV8Helper> v8_helper,
             v8::Global<v8::UnboundScript> script);
      V8Data(scoped_refptr<AuctionV8Helper> v8_helper,
             v8::Global<v8::WasmModuleObject> module);
      ~V8Data();

      scoped_refptr<AuctionV8Helper> v8_helper;
      // Normally exactly one of these will be set at construction, though
      // both may be empty in the Result taken ownership of by TakeScript().
      v8::Global<v8::UnboundScript> script;
      v8::Global<v8::WasmModuleObject> module;
    };

    std::unique_ptr<V8Data, base::OnTaskRunnerDeleter> state_;
  };

  using LoadWorkletCallback =
      base::OnceCallback<void(Result result,
                              absl::optional<std::string> error_msg)>;

  explicit WorkletLoaderBase(const WorkletLoaderBase&) = delete;
  WorkletLoaderBase& operator=(const WorkletLoaderBase&) = delete;

 protected:
  // Starts loading the resource on construction. Callback will be invoked
  // asynchronously once the data has been fetched and compiled or an error has
  // occurred, on the current thread. Destroying this is guaranteed to cancel
  // the callback. `mime_type` will inform both download checking and the
  // compilation method used.
  WorkletLoaderBase(network::mojom::URLLoaderFactory* url_loader_factory,
                    const GURL& source_url,
                    AuctionDownloader::MimeType mime_type,
                    scoped_refptr<AuctionV8Helper> v8_helper,
                    scoped_refptr<AuctionV8Helper::DebugId> debug_id,
                    LoadWorkletCallback load_worklet_callback);
  ~WorkletLoaderBase();

 private:
  void OnDownloadComplete(std::unique_ptr<std::string> body,
                          absl::optional<std::string> error_msg);

  static void HandleDownloadResultOnV8Thread(
      GURL source_url,
      AuctionDownloader::MimeType mime_type,
      scoped_refptr<AuctionV8Helper> v8_helper,
      scoped_refptr<AuctionV8Helper::DebugId> debug_id,
      std::unique_ptr<std::string> body,
      absl::optional<std::string> error_msg,
      scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
      base::WeakPtr<WorkletLoaderBase> weak_instance);

  static Result CompileJs(const std::string& body,
                          scoped_refptr<AuctionV8Helper> v8_helper,
                          const GURL& source_url,
                          AuctionV8Helper::DebugId* debug_id,
                          absl::optional<std::string>& error_msg);

  static Result CompileWasm(const std::string& body,
                            scoped_refptr<AuctionV8Helper> v8_helper,
                            const GURL& source_url,
                            AuctionV8Helper::DebugId* debug_id,
                            absl::optional<std::string>& error_msg);

  void DeliverCallbackOnUserThread(Result result,
                                   absl::optional<std::string> error_msg);

  const GURL source_url_;
  const AuctionDownloader::MimeType mime_type_;
  const scoped_refptr<AuctionV8Helper> v8_helper_;
  const scoped_refptr<AuctionV8Helper::DebugId> debug_id_;

  std::unique_ptr<AuctionDownloader> auction_downloader_;
  LoadWorkletCallback load_worklet_callback_;

  base::WeakPtrFactory<WorkletLoaderBase> weak_ptr_factory_{this};
};

// Utility for loading and compiling worklet JavaScript.
class WorkletLoader : public WorkletLoaderBase {
 public:
  // Starts loading the resource on construction. Callback will be invoked
  // asynchronously once the data has been fetched and compiled or an error has
  // occurred, on the current thread. Destroying this is guaranteed to cancel
  // the callback.
  WorkletLoader(network::mojom::URLLoaderFactory* url_loader_factory,
                const GURL& source_url,
                scoped_refptr<AuctionV8Helper> v8_helper,
                scoped_refptr<AuctionV8Helper::DebugId> debug_id,
                LoadWorkletCallback load_worklet_callback);

  // The returned value is a compiled script not bound to any context. It
  // can be repeatedly bound to different contexts and executed, without
  // persisting any state.  `result` will be cleared after this call.
  //
  // Should only be called on the V8 thread. Requires `result.success()` to be
  // true.
  static v8::Global<v8::UnboundScript> TakeScript(Result&& result);
};

class WorkletWasmLoader : public WorkletLoaderBase {
 public:
  // Starts loading the resource on construction. Callback will be invoked
  // asynchronously once the data has been fetched and compiled or an error has
  // occurred, on the current thread. Destroying this is guaranteed to cancel
  // the callback.
  WorkletWasmLoader(network::mojom::URLLoaderFactory* url_loader_factory,
                    const GURL& source_url,
                    scoped_refptr<AuctionV8Helper> v8_helper,
                    scoped_refptr<AuctionV8Helper::DebugId> debug_id,
                    LoadWorkletCallback load_worklet_callback);

  // The returned value is a module object. Since it's a JS object, it
  // should not be shared between contexts that must be isolated, as the code
  // can just set properties on it. Instead, create a new instance using
  // MakeModule() every time. `result` will not be changed by this call.
  //
  // This should only be called on the V8 thread, with a context active.
  // Requires `result.success()` to be true.
  static v8::MaybeLocal<v8::WasmModuleObject> MakeModule(const Result& result);
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_LOADER_H_
