/*
 * Copyright (C) 2014 Bloomberg Finance L.P.
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

#ifndef INCLUDED_BLPWTK2_RESOURCELOADER_H
#define INCLUDED_BLPWTK2_RESOURCELOADER_H

#include <blpwtk2_config.h>

namespace blpwtk2 {

class ResourceContext;
class StringRef;

// Applications can provide a custom resource loader by implementing this
// interface and installing it on startup via 'setInProcessResourceLoader' in
// 'blpwtk2::ToolkitCreateParams'.
//
// Note that all methods in 'ResourceLoader' will be invoked in the renderer
// thread.
class ResourceLoader {
  public:
    // Return true if this loader will handle the specified 'url', and false
    // otherwise.  If this method returns true, then 'start' will be invoked
    // for this URL.  Otherwise, blpwtk2 will use Chromium's default resource
    // loader.
    virtual bool canHandleURL(const StringRef& url) = 0;

    // Start loading the resource identified by the specified 'url' with the
    // specified 'context'.  The application can store any data pertaining to
    // the load request in the specified 'userData', which will be provided in
    // subsequent callbacks related to this request (for example, 'cancel').
    virtual void start(const StringRef& url,
                       ResourceContext* context,
                       void** userData) = 0;

    // Cancel loading the resource for the specified 'context'.  This is
    // invoked when the load request is aborted.  The ResourceLoader should
    // stop loading any additional data, and call 'context->finish()' to
    // cleanup the ResourceContext.
    virtual void cancel(ResourceContext* context,
                        void* userData) = 0;

  protected:
    // The application is responsible for destroying the resource loader after
    // shutting down 'blpwtk2::Toolkit'.
    BLPWTK2_EXPORT virtual ~ResourceLoader();
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_RESOURCELOADER_H

// vim: ts=4 et

