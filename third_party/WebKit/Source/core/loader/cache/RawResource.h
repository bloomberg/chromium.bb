/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef RawResource_h
#define RawResource_h

#include "core/loader/cache/Resource.h"
#include "core/loader/cache/ResourceClient.h"

namespace WebCore {
class RawResourceCallback;
class RawResourceClient;

class RawResource : public Resource {
public:
    RawResource(const ResourceRequest&, Type);

    // FIXME: AssociatedURLLoader shouldn't be a DocumentThreadableLoader and therefore shouldn't
    // use RawResource. However, it is, and it needs to be able to defer loading.
    // This can be fixed by splitting CORS preflighting out of DocumentThreacableLoader.
    virtual void setDefersLoading(bool);

    virtual void setDataBufferingPolicy(DataBufferingPolicy);

    void clear();

    virtual bool canReuse(const ResourceRequest&) const;

private:
    virtual void didAddClient(ResourceClient*);
    virtual void appendData(const char*, int) OVERRIDE;

    virtual bool shouldIgnoreHTTPStatusCodeErrors() const { return true; }

    virtual void willSendRequest(ResourceRequest&, const ResourceResponse&);
    virtual void responseReceived(const ResourceResponse&);
    virtual void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent);
    virtual void didDownloadData(int);

    struct RedirectPair {
    public:
        explicit RedirectPair(const ResourceRequest& request, const ResourceResponse& redirectResponse)
            : m_request(request)
            , m_redirectResponse(redirectResponse)
        {
        }

        const ResourceRequest m_request;
        const ResourceResponse m_redirectResponse;
    };

    Vector<RedirectPair> m_redirectChain;
};


class RawResourceClient : public ResourceClient {
public:
    virtual ~RawResourceClient() { }
    static ResourceClientType expectedType() { return RawResourceType; }
    virtual ResourceClientType resourceClientType() const { return expectedType(); }

    virtual void dataSent(Resource*, unsigned long long /* bytesSent */, unsigned long long /* totalBytesToBeSent */) { }
    virtual void responseReceived(Resource*, const ResourceResponse&) { }
    virtual void dataReceived(Resource*, const char* /* data */, int /* length */) { }
    virtual void redirectReceived(Resource*, ResourceRequest&, const ResourceResponse&) { }
    virtual void dataDownloaded(Resource*, int) { }
};

}

#endif // RawResource_h
