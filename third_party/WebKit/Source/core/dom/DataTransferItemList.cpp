/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/DataTransferItemList.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/DataTransferItem.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/page/Frame.h"
#include "core/platform/chromium/ChromiumDataObject.h"
#include "core/platform/chromium/ClipboardChromium.h"

namespace WebCore {

PassRefPtr<DataTransferItemList> DataTransferItemList::create(PassRefPtr<ClipboardChromium> clipboard, PassRefPtr<ChromiumDataObject> list)
{
    return adoptRef(new DataTransferItemList(clipboard, list));
}

DataTransferItemList::~DataTransferItemList()
{
}

size_t DataTransferItemList::length() const
{
    if (!m_clipboard->canReadTypes())
        return 0;
    return m_dataObject->length();
}

PassRefPtr<DataTransferItem> DataTransferItemList::item(unsigned long index)
{
    if (!m_clipboard->canReadTypes())
        return 0;
    RefPtr<ChromiumDataObjectItem> item = m_dataObject->item(index);
    if (!item)
        return 0;

    return DataTransferItem::create(m_clipboard, item);
}

void DataTransferItemList::deleteItem(unsigned long index, ExceptionState& es)
{
    if (!m_clipboard->canWriteData()) {
        es.throwUninformativeAndGenericDOMException(InvalidStateError);
        return;
    }
    m_dataObject->deleteItem(index);
}

void DataTransferItemList::clear()
{
    if (!m_clipboard->canWriteData())
        return;
    m_dataObject->clearAll();
}

PassRefPtr<DataTransferItem> DataTransferItemList::add(const String& data, const String& type, ExceptionState& es)
{
    if (!m_clipboard->canWriteData())
        return 0;
    RefPtr<ChromiumDataObjectItem> item = m_dataObject->add(data, type, es);
    if (!item)
        return 0;
    return DataTransferItem::create(m_clipboard, item);
}

PassRefPtr<DataTransferItem> DataTransferItemList::add(PassRefPtr<File> file)
{
    if (!m_clipboard->canWriteData())
        return 0;
    RefPtr<ChromiumDataObjectItem> item = m_dataObject->add(file, m_clipboard->frame()->document()->scriptExecutionContext());
    if (!item)
        return 0;
    return DataTransferItem::create(m_clipboard, item);
}

DataTransferItemList::DataTransferItemList(PassRefPtr<ClipboardChromium> clipboard, PassRefPtr<ChromiumDataObject> dataObject)
    : m_clipboard(clipboard)
    , m_dataObject(dataObject)
{
    ScriptWrappable::init(this);
}

} // namespace WebCore
