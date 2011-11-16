// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_PLUGIN_STREAM_URL_H_
#define WEBKIT_PLUGINS_NPAPI_PLUGIN_STREAM_URL_H_

#include "webkit/plugins/npapi/plugin_stream.h"
#include "webkit/plugins/npapi/webplugin.h"
#include "googleurl/src/gurl.h"

namespace webkit {
namespace npapi {

class PluginInstance;

// A NPAPI Stream based on a URL.
class PluginStreamUrl : public PluginStream,
                        public WebPluginResourceClient {
 public:
  // Create a new stream for sending to the plugin by fetching
  // a URL. If notifyNeeded is set, then the plugin will be notified
  // when the stream has been fully sent to the plugin.  Initialize
  // must be called before the object is used.
  PluginStreamUrl(unsigned long resource_id,
                  const GURL &url,
                  PluginInstance *instance,
                  bool notify_needed,
                  void *notify_data);
  virtual ~PluginStreamUrl();

  // Stop sending the stream to the client.
  // Overrides the base Close so we can cancel our fetching the URL if
  // it is still loading.
  virtual bool Close(NPReason reason) OVERRIDE;

  virtual WebPluginResourceClient* AsResourceClient() OVERRIDE;

  virtual void CancelRequest() OVERRIDE;

  //
  // WebPluginResourceClient methods
  //
  virtual void WillSendRequest(const GURL& url, int http_status_code) OVERRIDE;
  virtual void DidReceiveResponse(const std::string& mime_type,
                                  const std::string& headers,
                                  uint32 expected_length,
                                  uint32 last_modified,
                                  bool request_is_seekable) OVERRIDE;
  virtual void DidReceiveData(const char* buffer,
                              int length,
                              int data_offset) OVERRIDE;
  virtual void DidFinishLoading() OVERRIDE;
  virtual void DidFail() OVERRIDE;
  virtual bool IsMultiByteResponseExpected() OVERRIDE;
  virtual int ResourceId() OVERRIDE;

 private:
  GURL url_;
  unsigned long id_;

  DISALLOW_COPY_AND_ASSIGN(PluginStreamUrl);
};

}  // namespace npapi
}  // namespace webkit

#endif // WEBKIT_PLUGINS_NPAPI_PLUGIN_STREAM_URL_H_
