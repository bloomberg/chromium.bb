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

#ifndef INCLUDED_BLPWTK2_RESOURCECONTEXT_H
#define INCLUDED_BLPWTK2_RESOURCECONTEXT_H

#include <blpwtk2_config.h>
#include <blpwtk2_string.h>

namespace blpwtk2 {

class Blob;
class StringRef;

class BLPWTK2_EXPORT HTTPHeaderVisitor {
  public:

    virtual void visitHeader(const StringRef& name, const StringRef& value) = 0;

  protected:

    virtual ~HTTPHeaderVisitor();
};

class BLPWTK2_EXPORT HTTPBodyVisitor {
  public:

    virtual void visitBodyElement(const Blob& element) = 0;

  protected:

    virtual ~HTTPBodyVisitor();
};

class BLPWTK2_EXPORT URLRequest {
  public:
    using AppCacheHostID = String;

    // From WebURLRequest.h:
    enum class CachePolicy {
        UseProtocolCachePolicy, // normal load
        ReloadIgnoringCacheData, // reload
        ReturnCacheDataElseLoad, // back/forward or encoding change - allow stale data
        ReturnCacheDataDontLoad, // results of a post - allow stale data and only use cache
        ReloadBypassingCache, // end-to-end reload
    };

    // From WebURLRequest.h:
    enum class Priority {
        Unresolved = -1,
        VeryLow,
        Low,
        Medium,
        High,
        VeryHigh,
    };

    virtual String url() const = 0;
    virtual String firstPartyForCookies() const = 0;
    virtual bool allowStoredCredentials() const = 0;
    virtual CachePolicy cachePolicy() const = 0;
    virtual String httpMethod() const = 0;
    virtual String httpHeaderField(const StringRef& name) const = 0;
    virtual void visitHTTPHeaderFields(HTTPHeaderVisitor* visitor) const = 0;
    virtual void visitHTTPBody(HTTPBodyVisitor* visitor) const = 0;
    virtual bool reportUploadProgress() const = 0;
    virtual bool reportRawHeaders() const = 0;
    virtual bool hasUserGesture() const = 0;
    virtual int requesterID() const = 0;
    virtual AppCacheHostID appCacheHostID() const = 0;
    virtual Priority priority() const = 0;

  protected:
    virtual ~URLRequest();
};

// A ResourceContext will be created for each resource that can be handled by
// the application-installed 'blpwtk2::ResourceLoader'.  The ResourceLoader can
// push data into the ResourceContext, set HTTP headers etc.
//
// Note that all methods in 'ResourceContext' must be invoked in the renderer
// thread.
class BLPWTK2_EXPORT ResourceContext {
  public:
    virtual const URLRequest* request() = 0;

    // Replace the current status line with the provided one (the specified
    // 'newStatus' should not have any EOL).  For example,
    // "HTTP/1.1 404 Not Found".  If this method is not called, the default
    // "HTTP/1.1 200 OK" is used.  The behavior is undefined if this is called
    // after 'addResponseData' is called.
    virtual void replaceStatusLine(const StringRef& newStatus) = 0;

    // Add a header to the response.  The specified 'header' has to be a single
    // header without EOL termination, just "<header-name>: <header-values>".
    // If a header with the same name is already stored, the two headers are
    // not merged together by this method; the one provided is simply put at
    // the end of the list.  The behavior is undefined if this is called after
    // 'addResponseData' is called.
    virtual void addResponseHeader(const StringRef& header) = 0;

    // Returns true if the response contains the specified header-value pair.
    // Both name and value are compared case insensitively.
    virtual bool hasResponseHeaderValue(const StringRef& name,
                                        const StringRef& value) const = 0;

    // Returns true if the response contains the specified header.
    // The name is compared case insensitively.
    virtual bool hasReponseHeader(const StringRef& name) const = 0;

    // Add response data for this resource.
    virtual void addResponseData(const char* buffer, int length) = 0;

    // Notify that there was a failure when loading the resource.  This must be
    // followed by an immediate call to 'finish()' to cleanup the
    // ResourceContext.
    virtual void failed() = 0;

    // Finish the resource.  This must be called after all the response data
    // has been added, or if the resource has been cancelled.  The
    // ResourceContext will be deleted when this method is called, so do not
    // use this context after this call.
    virtual void finish() = 0;

  protected:
    // Destroy this ResourceContext.  Note that clients of blpwtk2 should not
    // delete this object.  It will be deleted automatically after 'finish()'
    // is called.
    virtual ~ResourceContext();
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_RESOURCECONTEXT_H

// vim: ts=4 et

