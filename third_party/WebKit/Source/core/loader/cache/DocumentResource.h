/*
    Copyright (C) 2010 Rob Buis <rwlbuis@gmail.com>
    Copyright (C) 2011 Cosmin Truta <ctruta@gmail.com>
    Copyright (C) 2012 University of Szeged
    Copyright (C) 2012 Renata Hodovan <reni@webkit.org>

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

#ifndef DocumentResource_h
#define DocumentResource_h

#include "core/loader/TextResourceDecoder.h"
#include "core/loader/cache/Resource.h"
#include "core/loader/cache/ResourceClient.h"
#include "core/loader/cache/ResourcePtr.h"

namespace WebCore {

class Document;

class DocumentResource : public Resource {
public:
    DocumentResource(const ResourceRequest&, Type);
    virtual ~DocumentResource();

    Document* document() const { return m_document.get(); }

    virtual void setEncoding(const String&);
    virtual String encoding() const;
    virtual void checkNotify() OVERRIDE;

private:
    PassRefPtr<Document> createDocument(const KURL&);

    RefPtr<Document> m_document;
    RefPtr<TextResourceDecoder> m_decoder;
};

class DocumentResourceClient : public ResourceClient {
public:
    virtual ~DocumentResourceClient() { }
    static ResourceClientType expectedType() { return DocumentType; }
    virtual ResourceClientType resourceClientType() const { return expectedType(); }
};

}

#endif // DocumentResource_h
