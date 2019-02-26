// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_type_converter.h"

#include "content/public/common/referrer_type_converters.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"

namespace mojo {

// static
blink::mojom::FetchAPIRequestPtr
TypeConverter<blink::mojom::FetchAPIRequestPtr,
              content::ServiceWorkerFetchRequest>::
    Convert(const content::ServiceWorkerFetchRequest& request) {
  auto request_ptr = blink::mojom::FetchAPIRequest::New();
  request_ptr->mode = request.mode;
  request_ptr->is_main_resource_load = request.is_main_resource_load;
  request_ptr->request_context_type = request.request_context_type;
  request_ptr->frame_type = request.frame_type;
  request_ptr->url = request.url;
  request_ptr->method = request.method;
  request_ptr->headers.insert(request.headers.begin(), request.headers.end());
  request_ptr->referrer = blink::mojom::Referrer::From(request.referrer);
  request_ptr->credentials_mode = request.credentials_mode;
  request_ptr->cache_mode = request.cache_mode;
  request_ptr->redirect_mode = request.redirect_mode;
  if (!request.integrity.empty())
    request_ptr->integrity = request.integrity;
  request_ptr->keepalive = request.keepalive;
  if (!request.client_id.empty())
    request_ptr->client_id = request.client_id;
  request_ptr->is_reload = request.is_reload;
  request_ptr->is_history_navigation = request.is_history_navigation;
  return request_ptr;
}

content::ServiceWorkerFetchRequest TypeConverter<
    content::ServiceWorkerFetchRequest,
    blink::mojom::FetchAPIRequest>::Convert(const blink::mojom::FetchAPIRequest&
                                                request) {
  content::ServiceWorkerFetchRequest request_out;
  request_out.mode = request.mode;
  request_out.is_main_resource_load = request.is_main_resource_load;
  request_out.request_context_type = request.request_context_type;
  request_out.frame_type = request.frame_type;
  request_out.url = request.url;
  request_out.method = request.method;
  request_out.headers.insert(request.headers.begin(), request.headers.end());
  request_out.referrer = request.referrer.To<content::Referrer>();
  request_out.credentials_mode = request.credentials_mode;
  request_out.cache_mode = request.cache_mode;
  request_out.redirect_mode = request.redirect_mode;
  if (request.integrity)
    request_out.integrity = request.integrity.value();
  request_out.keepalive = request.keepalive;
  if (request.client_id)
    request_out.client_id = request.client_id.value();
  request_out.is_reload = request.is_reload;
  request_out.is_history_navigation = request.is_history_navigation;
  return request_out;
}

}  // namespace mojo
