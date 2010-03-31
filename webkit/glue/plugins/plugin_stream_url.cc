// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/plugin_stream_url.h"

#include "webkit/glue/plugins/plugin_host.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/plugins/webplugin.h"

namespace NPAPI {

PluginStreamUrl::PluginStreamUrl(
    unsigned long resource_id,
    const GURL &url,
    PluginInstance *instance,
    bool notify_needed,
    void *notify_data)
    : PluginStream(instance, url.spec().c_str(), notify_needed, notify_data),
      url_(url),
      id_(resource_id) {
}

PluginStreamUrl::~PluginStreamUrl() {
  if (instance() && instance()->webplugin()) {
    instance()->webplugin()->ResourceClientDeleted(AsResourceClient());
  }
}

bool PluginStreamUrl::Close(NPReason reason) {
  // Protect the stream against it being destroyed or the whole plugin instance
  // being destroyed within the destroy stream handler.
  scoped_refptr<PluginStream> protect(this);
  CancelRequest();
  bool result = PluginStream::Close(reason);
  instance()->RemoveStream(this);
  return result;
}

void PluginStreamUrl::WillSendRequest(const GURL& url) {
  url_ = url;
  UpdateUrl(url.spec().c_str());
}

void PluginStreamUrl::DidReceiveResponse(const std::string& mime_type,
                                         const std::string& headers,
                                         uint32 expected_length,
                                         uint32 last_modified,
                                         bool request_is_seekable) {
  // Protect the stream against it being destroyed or the whole plugin instance
  // being destroyed within the new stream handler.
  scoped_refptr<PluginStream> protect(this);

  bool opened = Open(mime_type,
                     headers,
                     expected_length,
                     last_modified,
                     request_is_seekable);
  if (!opened) {
    CancelRequest();
    instance()->RemoveStream(this);
  } else {
    if (id_ > 0)
      instance()->webplugin()->SetDeferResourceLoading(id_, false);
  }
}

void PluginStreamUrl::DidReceiveData(const char* buffer, int length,
                                     int data_offset) {
  if (!open())
    return;

  // Protect the stream against it being destroyed or the whole plugin instance
  // being destroyed within the write handlers
  scoped_refptr<PluginStream> protect(this);

  if (length > 0) {
    // The PluginStreamUrl instance could get deleted if the plugin fails to
    // accept data in NPP_Write.
    if (Write(const_cast<char*>(buffer), length, data_offset) > 0) {
      if (id_ > 0)
        instance()->webplugin()->SetDeferResourceLoading(id_, false);
    }
  }
}

void PluginStreamUrl::DidFinishLoading() {
  if (!seekable()) {
    Close(NPRES_DONE);
  }
}

void PluginStreamUrl::DidFail() {
  Close(NPRES_NETWORK_ERR);
}

void PluginStreamUrl::CancelRequest() {
  if (id_ > 0) {
    if (instance()->webplugin()) {
      instance()->webplugin()->CancelResource(id_);
    }
    id_ = 0;
  }
}

} // namespace NPAPI
