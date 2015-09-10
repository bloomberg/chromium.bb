/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "core/html/FormDataList.h"

#include "core/fileapi/File.h"
#include "platform/network/FormDataEncoder.h"
#include "platform/text/LineEnding.h"
#include "wtf/CurrentTime.h"

namespace blink {

FormDataList::FormDataList(const WTF::TextEncoding& c)
    : m_encoding(c)
{
}

void FormDataList::appendItem(const FormDataList::Item& item)
{
    m_items.append(item);
}

File* FormDataList::Item::file() const
{
    ASSERT(blob());
    // The spec uses the passed filename when inserting entries into the list.
    // Here, we apply the filename (if present) as an override when extracting
    // items.
    // FIXME: Consider applying the name during insertion.

    if (blob()->isFile()) {
        File* file = toFile(blob());
        if (filename().isNull())
            return file;
        return file->clone(filename());
    }

    String filename = m_filename;
    if (filename.isNull())
        filename = "blob";
    return File::create(filename, currentTimeMS(), blob()->blobDataHandle());
}

PassRefPtr<EncodedFormData> FormDataList::createFormData(EncodedFormData::EncodingType encodingType)
{
    RefPtr<EncodedFormData> result = EncodedFormData::create();
    appendKeyValuePairItemsTo(result.get(), m_encoding, false, encodingType);
    return result.release();
}

PassRefPtr<EncodedFormData> FormDataList::createMultiPartFormData()
{
    RefPtr<EncodedFormData> result = EncodedFormData::create();
    appendKeyValuePairItemsTo(result.get(), m_encoding, true);
    return result.release();
}

void FormDataList::appendKeyValuePairItemsTo(EncodedFormData* formData, const WTF::TextEncoding& encoding, bool isMultiPartForm, EncodedFormData::EncodingType encodingType)
{
    if (isMultiPartForm)
        formData->setBoundary(FormDataEncoder::generateUniqueBoundaryString());

    Vector<char> encodedData;

    for (const Item& item : items()) {
        if (isMultiPartForm) {
            Vector<char> header;
            FormDataEncoder::beginMultiPartHeader(header, formData->boundary().data(), item.key());

            // If the current type is blob, then we also need to include the filename
            if (item.blob()) {
                String name;
                if (item.blob()->isFile()) {
                    File* file = toFile(item.blob());
                    // For file blob, use the filename (or relative path if it is present) as the name.
                    name = file->webkitRelativePath().isEmpty() ? file->name() : file->webkitRelativePath();

                    // If a filename is passed in FormData.append(), use it
                    // instead of the file blob's name.
                    if (!item.filename().isNull())
                        name = item.filename();
                } else {
                    // For non-file blob, use the filename if it is passed in
                    // FormData.append().
                    if (!item.filename().isNull())
                        name = item.filename();
                    else
                        name = "blob";
                }

                // We have to include the filename=".." part in the header, even if the filename is empty
                FormDataEncoder::addFilenameToMultiPartHeader(header, encoding, name);

                // Add the content type if available, or "application/octet-stream" otherwise (RFC 1867).
                String contentType;
                if (item.blob()->type().isEmpty())
                    contentType = "application/octet-stream";
                else
                    contentType = item.blob()->type();
                FormDataEncoder::addContentTypeToMultiPartHeader(header, contentType.latin1());
            }

            FormDataEncoder::finishMultiPartHeader(header);

            // Append body
            formData->appendData(header.data(), header.size());
            if (item.blob()) {
                if (item.blob()->hasBackingFile()) {
                    File* file = toFile(item.blob());
                    // Do not add the file if the path is empty.
                    if (!file->path().isEmpty())
                        formData->appendFile(file->path());
                    if (!file->fileSystemURL().isEmpty())
                        formData->appendFileSystemURL(file->fileSystemURL());
                } else {
                    formData->appendBlob(item.blob()->uuid(), item.blob()->blobDataHandle());
                }
            } else {
                formData->appendData(item.data().data(), item.data().length());
            }
            formData->appendData("\r\n", 2);
        } else {
            FormDataEncoder::addKeyValuePairAsFormData(encodedData, item.key(), item.data(), encodingType);
        }
    }

    if (isMultiPartForm)
        FormDataEncoder::addBoundaryToMultiPartHeader(encodedData, formData->boundary().data(), true);

    formData->appendData(encodedData.data(), encodedData.size());
}

CString FormDataList::encodeAndNormalize(const String& string) const
{
    CString encodedString = m_encoding.encode(string, WTF::EntitiesForUnencodables);
    return normalizeLineEndingsToCRLF(encodedString);
}

DEFINE_TRACE(FormDataList)
{
    visitor->trace(m_items);
}

DEFINE_TRACE(FormDataList::Item)
{
    visitor->trace(m_blob);
}

} // namespace blink
