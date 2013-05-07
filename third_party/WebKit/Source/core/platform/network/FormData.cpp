/*
 * Copyright (C) 2004, 2006, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2012 Digia Plc. and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "core/platform/network/FormData.h"

#include "core/dom/Document.h"
#include "core/fileapi/BlobURL.h"
#include "core/fileapi/File.h"
#include "core/html/FormDataList.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/platform/FileSystem.h"
#include "core/platform/MIMETypeRegistry.h"
#include "core/platform/PlatformMemoryInstrumentation.h"
#include "core/platform/network/BlobData.h"
#include "core/platform/network/FormDataBuilder.h"
#include "core/platform/text/TextEncoding.h"
#include <wtf/MemoryInstrumentationVector.h>

namespace WebCore {

inline FormData::FormData()
    : m_identifier(0)
    , m_alwaysStream(false)
    , m_containsPasswordData(false)
{
}

inline FormData::FormData(const FormData& data)
    : RefCounted<FormData>()
    , m_elements(data.m_elements)
    , m_identifier(data.m_identifier)
    , m_alwaysStream(false)
    , m_containsPasswordData(data.m_containsPasswordData)
{
}

FormData::~FormData()
{
}

PassRefPtr<FormData> FormData::create()
{
    return adoptRef(new FormData);
}

PassRefPtr<FormData> FormData::create(const void* data, size_t size)
{
    RefPtr<FormData> result = create();
    result->appendData(data, size);
    return result.release();
}

PassRefPtr<FormData> FormData::create(const CString& string)
{
    RefPtr<FormData> result = create();
    result->appendData(string.data(), string.length());
    return result.release();
}

PassRefPtr<FormData> FormData::create(const Vector<char>& vector)
{
    RefPtr<FormData> result = create();
    result->appendData(vector.data(), vector.size());
    return result.release();
}

PassRefPtr<FormData> FormData::create(const FormDataList& list, const TextEncoding& encoding, EncodingType encodingType)
{
    RefPtr<FormData> result = create();
    result->appendKeyValuePairItems(list, encoding, false, 0, encodingType);
    return result.release();
}

PassRefPtr<FormData> FormData::createMultiPart(const FormDataList& list, const TextEncoding& encoding, Document* document)
{
    RefPtr<FormData> result = create();
    result->appendKeyValuePairItems(list, encoding, true, document);
    return result.release();
}

PassRefPtr<FormData> FormData::copy() const
{
    return adoptRef(new FormData(*this));
}

PassRefPtr<FormData> FormData::deepCopy() const
{
    RefPtr<FormData> formData(create());

    formData->m_alwaysStream = m_alwaysStream;

    size_t n = m_elements.size();
    formData->m_elements.reserveInitialCapacity(n);
    for (size_t i = 0; i < n; ++i) {
        const FormDataElement& e = m_elements[i];
        switch (e.m_type) {
        case FormDataElement::data:
            formData->m_elements.uncheckedAppend(FormDataElement(e.m_data));
            break;
        case FormDataElement::encodedFile:
            formData->m_elements.uncheckedAppend(FormDataElement(e.m_filename, e.m_fileStart, e.m_fileLength, e.m_expectedFileModificationTime));
            break;
        case FormDataElement::encodedBlob:
            formData->m_elements.uncheckedAppend(FormDataElement(e.m_url));
            break;
        case FormDataElement::encodedURL:
            formData->m_elements.uncheckedAppend(FormDataElement(e.m_url, e.m_fileStart, e.m_fileLength, e.m_expectedFileModificationTime));
            break;
        }
    }
    return formData.release();
}

void FormData::appendData(const void* data, size_t size)
{
    if (m_elements.isEmpty() || m_elements.last().m_type != FormDataElement::data)
        m_elements.append(FormDataElement());
    FormDataElement& e = m_elements.last();
    size_t oldSize = e.m_data.size();
    e.m_data.grow(oldSize + size);
    memcpy(e.m_data.data() + oldSize, data, size);
}

void FormData::appendFile(const String& filename)
{
    m_elements.append(FormDataElement(filename, 0, BlobDataItem::toEndOfFile, invalidFileTime()));
}

void FormData::appendFileRange(const String& filename, long long start, long long length, double expectedModificationTime)
{
    m_elements.append(FormDataElement(filename, start, length, expectedModificationTime));
}

void FormData::appendBlob(const KURL& blobURL)
{
    m_elements.append(FormDataElement(blobURL));
}

void FormData::appendURL(const KURL& url)
{
    m_elements.append(FormDataElement(url, 0, BlobDataItem::toEndOfFile, invalidFileTime()));
}

void FormData::appendURLRange(const KURL& url, long long start, long long length, double expectedModificationTime)
{
    m_elements.append(FormDataElement(url, start, length, expectedModificationTime));
}

void FormData::appendKeyValuePairItems(const FormDataList& list, const TextEncoding& encoding, bool isMultiPartForm, Document* document, EncodingType encodingType)
{
    if (isMultiPartForm)
        m_boundary = FormDataBuilder::generateUniqueBoundaryString();

    Vector<char> encodedData;

    const Vector<FormDataList::Item>& items = list.items();
    size_t formDataListSize = items.size();
    ASSERT(!(formDataListSize % 2));
    for (size_t i = 0; i < formDataListSize; i += 2) {
        const FormDataList::Item& key = items[i];
        const FormDataList::Item& value = items[i + 1];
        if (isMultiPartForm) {
            Vector<char> header;
            FormDataBuilder::beginMultiPartHeader(header, m_boundary.data(), key.data());

            // If the current type is blob, then we also need to include the filename
            if (value.blob()) {
                String name;
                if (value.blob()->isFile()) {
                    File* file = toFile(value.blob());
                    // For file blob, use the filename (or relative path if it is present) as the name.
                    name = file->webkitRelativePath().isEmpty() ? file->name() : file->webkitRelativePath();

                    // If a filename is passed in FormData.append(), use it instead of the file blob's name.
                    if (!value.filename().isNull())
                        name = value.filename();
                } else {
                    // For non-file blob, use the filename if it is passed in FormData.append().
                    if (!value.filename().isNull())
                        name = value.filename();
                    else
                        name = "blob";
                }

                // We have to include the filename=".." part in the header, even if the filename is empty
                FormDataBuilder::addFilenameToMultiPartHeader(header, encoding, name);

                // Add the content type if available, or "application/octet-stream" otherwise (RFC 1867).
                String contentType;
                if (value.blob()->type().isEmpty())
                    contentType = "application/octet-stream";
                else
                    contentType = value.blob()->type();
                FormDataBuilder::addContentTypeToMultiPartHeader(header, contentType.latin1());
            }

            FormDataBuilder::finishMultiPartHeader(header);

            // Append body
            appendData(header.data(), header.size());
            if (value.blob()) {
                if (value.blob()->isFile()) {
                    File* file = toFile(value.blob());
                    // Do not add the file if the path is empty.
                    if (!file->path().isEmpty())
                        appendFile(file->path());
                    if (!file->fileSystemURL().isEmpty())
                        appendURL(file->fileSystemURL());
                }
                else
                    appendBlob(value.blob()->url());
            } else
                appendData(value.data().data(), value.data().length());
            appendData("\r\n", 2);
        } else {
            // Omit the name "isindex" if it's the first form data element.
            // FIXME: Why is this a good rule? Is this obsolete now?
            if (encodedData.isEmpty() && key.data() == "isindex")
                FormDataBuilder::encodeStringAsFormData(encodedData, value.data());
            else
                FormDataBuilder::addKeyValuePairAsFormData(encodedData, key.data(), value.data(), encodingType);
        }
    }

    if (isMultiPartForm)
        FormDataBuilder::addBoundaryToMultiPartHeader(encodedData, m_boundary.data(), true);

    appendData(encodedData.data(), encodedData.size());
}

void FormData::flatten(Vector<char>& data) const
{
    // Concatenate all the byte arrays, but omit any files.
    data.clear();
    size_t n = m_elements.size();
    for (size_t i = 0; i < n; ++i) {
        const FormDataElement& e = m_elements[i];
        if (e.m_type == FormDataElement::data)
            data.append(e.m_data.data(), static_cast<size_t>(e.m_data.size()));
    }
}

String FormData::flattenToString() const
{
    Vector<char> bytes;
    flatten(bytes);
    return Latin1Encoding().decode(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

void FormData::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, PlatformMemoryTypes::Loader);
    info.addMember(m_boundary, "boundary");
    info.addMember(m_elements, "elements");
}

void FormDataElement::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, PlatformMemoryTypes::Loader);
    info.addMember(m_data, "data");
    info.addMember(m_filename, "filename");
    info.addMember(m_url, "url");
}

} // namespace WebCore
