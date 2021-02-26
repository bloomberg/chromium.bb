// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_LOADER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_LOADER_FACTORY_H_

#include "base/macros.h"
#include "content/public/browser/non_network_url_loader_factory_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace chromeos {

// URLLoaderFactory that creates URLLoader instances for URLs with the
// externalfile scheme.
class ExternalFileURLLoaderFactory
    : public content::NonNetworkURLLoaderFactoryBase {
 public:
  // Returns mojo::PendingRemote to a newly constructed
  // ExternalFileURLLoaderFactory.  The factory is self-owned - it will delete
  // itself once there are no more receivers (including the receiver associated
  // with the returned mojo::PendingRemote and the receivers bound by the Clone
  // method).
  //
  // |render_process_host_id| is used to verify that the child process has
  // permission to request the URL. It should be
  // ChildProcessHost::kInvalidUniqueID for navigations.
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create(
      void* profile_id,
      int render_process_host_id);

 private:
  ExternalFileURLLoaderFactory(
      void* profile_id,
      int render_process_host_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);

  ~ExternalFileURLLoaderFactory() override;

  friend class ExternalFileURLLoaderFactoryTest;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

  void* profile_id_;
  const int render_process_host_id_;

  DISALLOW_COPY_AND_ASSIGN(ExternalFileURLLoaderFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_LOADER_FACTORY_H_
