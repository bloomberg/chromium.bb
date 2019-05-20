/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <blpwtk2_webframeimpl.h>

#include <blpwtk2_stringref.h>
#include <blpwtk2_webcontentsettingsdelegate.h>

#include <base/logging.h>  // for CHECK
#include <third_party/blink/public/web/web_frame.h>
#include <third_party/blink/public/web/web_local_frame.h>

namespace blpwtk2 {

                        // ------------------
                        // class WebFrameImpl
                        // ------------------

WebFrameImpl::WebFrameImpl(blink::WebFrame *impl)
    : d_impl(impl)
    , d_contentSettingsDelegate(nullptr)
{
    if (d_impl->IsWebLocalFrame()) {
        d_impl->ToWebLocalFrame()->SetContentSettingsClient(this);
    }
}

WebFrameImpl::~WebFrameImpl()
{
    if (d_impl->IsWebLocalFrame()) {
        d_impl->ToWebLocalFrame()->SetContentSettingsClient(nullptr);
    }
}

v8::Local<v8::Context> WebFrameImpl::mainWorldScriptContext() const
{
    return d_impl->ToWebLocalFrame()->MainWorldScriptContext();
}

v8::Local<v8::Context> WebFrameImpl::mainWorldScriptContextForFrame(
                                          const blpwtk2::StringRef& name) const
{
    blink::WebFrame *frame =
        d_impl->ToWebLocalFrame()->FindFrameByName(toWebString(name));

    if (!frame || !frame->IsWebLocalFrame()) {
        return v8::Local<v8::Context>();
    }

    return frame->ToWebLocalFrame()->MainWorldScriptContext();
}

v8::Isolate* WebFrameImpl::scriptIsolate() const
{
    return d_impl->ScriptIsolate();
}

void WebFrameImpl::setContentSettingsDelegate(WebContentSettingsDelegate *contentSettingsDelegate)
{
    d_contentSettingsDelegate = contentSettingsDelegate;
}

// blink::WebContentSettingsClient overrides
bool WebFrameImpl::AllowRunningInsecureContent(
        bool                            enabledPerSettings,
        const blink::WebSecurityOrigin& securityOrigin,
        const blink::WebURL&            url)
{
    if (!d_contentSettingsDelegate) {
        return blink::WebContentSettingsClient::AllowRunningInsecureContent(
            enabledPerSettings, securityOrigin,url);
    }

    return d_contentSettingsDelegate->allowRunningInsecureContent(enabledPerSettings);
}

}  // close namespace blpwtk2

// vim: ts=4 et

