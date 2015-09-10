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
