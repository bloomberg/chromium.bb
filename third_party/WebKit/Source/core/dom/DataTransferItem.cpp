/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/DataTransferItem.h"

#include "core/dom/Document.h"
#include "core/dom/StringCallback.h"
#include "core/page/Frame.h"
#include "core/platform/chromium/ChromiumDataObjectItem.h"
#include "core/platform/chromium/ClipboardChromium.h"

namespace WebCore {

const char DataTransferItem::kindString[] = "string";
const char DataTransferItem::kindFile[] = "file";

PassRefPtr<DataTransferItem> DataTransferItem::create(PassRefPtr<ClipboardChromium> clipboard, PassRefPtr<ChromiumDataObjectItem> item)
{
    return adoptRef(new DataTransferItem(clipboard, item));
}

DataTransferItem::~DataTransferItem()
{
}

String DataTransferItem::kind() const
{
    if (!m_clipboard->canReadTypes())
        return String();
    return m_item->kind();
}

String DataTransferItem::type() const
{
    if (!m_clipboard->canReadTypes())
        return String();
    return m_item->type();
}

void DataTransferItem::getAsString(PassRefPtr<StringCallback> callback) const
{
    if (!m_clipboard->canReadData())
        return;

    m_item->getAsString(callback, m_clipboard->frame()->document()->scriptExecutionContext());
}

PassRefPtr<Blob> DataTransferItem::getAsFile() const
{
    if (!m_clipboard->canReadData())
        return 0;

    return m_item->getAsFile();
}

DataTransferItem::DataTransferItem(PassRefPtr<ClipboardChromium> clipboard, PassRefPtr<ChromiumDataObjectItem> item)
    : m_clipboard(clipboard)
    , m_item(item)
{
    ScriptWrappable::init(this);
}


} // namespace WebCore

