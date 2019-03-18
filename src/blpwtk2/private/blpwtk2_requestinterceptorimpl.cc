/*
 * Copyright (C) 2018 Bloomberg Finance L.P.
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

#include <blpwtk2_requestinterceptorimpl.h>
#include <blpwtk2_resourcerequestjob.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_stringref.h>
#include <blpwtk2_resourceloader.h>

namespace blpwtk2 {

RequestInterceptorImpl::RequestInterceptorImpl() {
}

RequestInterceptorImpl::~RequestInterceptorImpl() {
}

net::URLRequestJob* RequestInterceptorImpl::MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const {

  StringRef url = request->url().spec();

  if (Statics::inProcessResourceLoader &&
      Statics::inProcessResourceLoader->canHandleURL(url)) {
    return new ResourceRequestJob(request, network_delegate);
  }

  return nullptr;
}

net::URLRequestJob* RequestInterceptorImpl::MaybeInterceptRedirect(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const GURL& location) const {
  return nullptr;
}

net::URLRequestJob* RequestInterceptorImpl::MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const {
  return nullptr;
}

}  // close namespace blpwtk2

