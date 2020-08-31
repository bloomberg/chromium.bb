// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/device_service.h"

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/system_connector.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/device_service.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "content/browser/wake_lock/wake_lock_context_host.h"
#include "content/public/android/content_jni_headers/ContentNfcDelegate_jni.h"
#endif

namespace content {

namespace {

// SharedURLLoaderFactory for device service, backed by
// GetContentClient()->browser()->GetSystemSharedURLLoaderFactory().
class DeviceServiceURLLoaderFactory : public network::SharedURLLoaderFactory {
 public:
  DeviceServiceURLLoaderFactory() = default;

  // mojom::URLLoaderFactory implementation:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    GetContentClient()
        ->browser()
        ->GetSystemSharedURLLoaderFactory()
        ->CreateLoaderAndStart(std::move(receiver), routing_id, request_id,
                               options, url_request, std::move(client),
                               traffic_annotation);
  }

  // SharedURLLoaderFactory implementation:
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    GetContentClient()->browser()->GetSystemSharedURLLoaderFactory()->Clone(
        std::move(receiver));
  }

  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
    return std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
        this);
  }

 private:
  friend class base::RefCounted<DeviceServiceURLLoaderFactory>;
  ~DeviceServiceURLLoaderFactory() override = default;

  DISALLOW_COPY_AND_ASSIGN(DeviceServiceURLLoaderFactory);
};

void BindDeviceServiceReceiver(
    mojo::PendingReceiver<device::mojom::DeviceService> receiver) {
  // This task runner may be used by some device service implementation bits
  // to interface with dbus client code, which in turn imposes some subtle
  // thread affinity on the clients. We therefore require a single-thread
  // runner.
  scoped_refptr<base::SingleThreadTaskRunner> device_blocking_task_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  // Bind the lifetime of the service instance to that of the sequence it's
  // running on.
  static base::NoDestructor<
      base::SequenceLocalStorageSlot<std::unique_ptr<device::DeviceService>>>
      service_slot;
  auto& service = service_slot->GetOrCreateValue();

  if (service) {
    service->AddReceiver(std::move(receiver));
    return;
  }

#if defined(OS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaGlobalRef<jobject> java_nfc_delegate;
  java_nfc_delegate.Reset(Java_ContentNfcDelegate_create(env));
  DCHECK(!java_nfc_delegate.is_null());

  // See the comments on wake_lock_context_host.h, content_browser_client.h
  // and ContentNfcDelegate.java respectively for comments on those
  // parameters.
  service = device::CreateDeviceService(
      device_blocking_task_runner,
      base::CreateSingleThreadTaskRunner({BrowserThread::IO}),
      base::MakeRefCounted<DeviceServiceURLLoaderFactory>(),
      content::GetNetworkConnectionTracker(),
      GetContentClient()->browser()->GetGeolocationApiKey(),
      GetContentClient()->browser()->ShouldUseGmsCoreGeolocationProvider(),
      base::BindRepeating(&WakeLockContextHost::GetNativeViewForContext),
      base::BindRepeating(&ContentBrowserClient::OverrideSystemLocationProvider,
                          base::Unretained(GetContentClient()->browser())),
      std::move(java_nfc_delegate), std::move(receiver));
#else
  service = device::CreateDeviceService(
      device_blocking_task_runner,
      base::CreateSingleThreadTaskRunner({BrowserThread::IO}),
      base::MakeRefCounted<DeviceServiceURLLoaderFactory>(),
      content::GetNetworkConnectionTracker(),
      GetContentClient()->browser()->GetGeolocationApiKey(),
      base::BindRepeating(&ContentBrowserClient::OverrideSystemLocationProvider,
                          base::Unretained(GetContentClient()->browser())),
      std::move(receiver));
#endif
}

}  // namespace

device::mojom::DeviceService& GetDeviceService() {
  static base::NoDestructor<base::SequenceLocalStorageSlot<
      mojo::Remote<device::mojom::DeviceService>>>
      remote_slot;
  mojo::Remote<device::mojom::DeviceService>& remote =
      remote_slot->GetOrCreateValue();
  if (!remote) {
    // This may be called very early in startup, too early for some Device
    // Service initialization steps (for example, in browser test environments,
    // the Device Service's connection to the Network Service could deadlock).
    // We post a task to defer until the main message loop has started, when
    // initialization is reliably safe.
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&BindDeviceServiceReceiver,
                                  remote.BindNewPipeAndPassReceiver()));
  }
  return *remote.get();
}

}  // namespace content
