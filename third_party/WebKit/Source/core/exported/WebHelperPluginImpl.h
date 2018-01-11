/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebHelperPluginImpl_h
#define WebHelperPluginImpl_h

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"
#include "public/web/WebHelperPlugin.h"

namespace blink {

class HTMLObjectElement;
class WebLocalFrameImpl;
class WebPluginContainerImpl;

// Utility class to host helper plugins for media. Internally, it creates a
// detached HTMLPluginElement to host the plugin and uses
// LocalFrameClient::createPlugin() to instantiate the requested plugin.
class WebHelperPluginImpl final : public WebHelperPlugin {
  USING_FAST_MALLOC(WebHelperPluginImpl);

 public:
  // WebHelperPlugin methods:
  WebPlugin* GetPlugin() override;
  void Destroy() override;

 private:
  friend class WebHelperPlugin;

  WebHelperPluginImpl() = default;

  bool Initialize(const String& plugin_type, WebLocalFrameImpl*);
  void ReallyDestroy();

  Persistent<HTMLObjectElement> object_element_;
  Persistent<WebPluginContainerImpl> plugin_container_;

  DISALLOW_COPY_AND_ASSIGN(WebHelperPluginImpl);
};

}  // namespace blink

#endif  // WebHelperPluginImpl_h
