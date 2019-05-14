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

#ifndef INCLUDED_BLPWTK2_RESOURCEREQUESTJOB_H
#define INCLUDED_BLPWTK2_RESOURCEREQUESTJOB_H

#include <net/url_request/url_request_job.h>
#include <url/gurl.h>

namespace blpwtk2 {

class ResourceRequestJob : public net::URLRequestJob {
 public:
  ResourceRequestJob(net::URLRequest* request,
                     net::NetworkDelegate* network_delegate);

 private:
  // net::URLRequestJob methods.
  void Start() override;
  bool GetMimeType(std::string* mime_type) const override;

  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestJob);
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_RESOURCEREQUESTJOB_H

