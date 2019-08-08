// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_RENDERER_WEBAPPLICATIONCACHEHOST_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_RENDERER_WEBAPPLICATIONCACHEHOST_IMPL_H_

#include "third_party/blink/renderer/core/exported/web_application_cache_host_impl.h"

namespace blink {
class WebLocalFrame;

class RendererWebApplicationCacheHostImpl : public WebApplicationCacheHostImpl {
 public:
  RendererWebApplicationCacheHostImpl(
      WebLocalFrame* web_frame,
      blink::WebApplicationCacheHostClient* client,
      const base::UnguessableToken& appcache_host_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // mojom::blink::AppCacheHostFrontend:
  void LogMessage(mojom::blink::ConsoleMessageLevel log_level,
                  const String& message) override;

  void SetSubresourceFactory(
      network::mojom::blink::URLLoaderFactoryPtr url_loader_factory) override;

 private:
  // Stores the WebLocalFrame. |this| keeps it as a raw pointer because |this|'s
  // owned by ApplicationCacheHost which is destroyed when
  // DocumentLoader::DetachFromFrame is called.
  WebLocalFrame* web_local_frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_RENDERER_WEBAPPLICATIONCACHEHOST_IMPL_H_
