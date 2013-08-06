/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
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

#include "modules/filesystem/FileWriterSync.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/FileError.h"
#include "modules/filesystem/AsyncFileWriter.h"

namespace WebCore {

void FileWriterSync::write(Blob* data, ExceptionState& es)
{
    ASSERT(writer());
    ASSERT(m_complete);
    if (!data) {
        es.throwDOMException(TypeMismatchError, FileError::typeMismatchErrorMessage);
        return;
    }

    prepareForWrite();
    writer()->write(position(), data);
    writer()->waitForOperationToComplete();
    ASSERT(m_complete);
    if (m_error) {
        FileError::throwDOMException(es, m_error);
        return;
    }
    setPosition(position() + data->size());
    if (position() > length())
        setLength(position());
}

void FileWriterSync::seek(long long position, ExceptionState& es)
{
    ASSERT(writer());
    ASSERT(m_complete);
    seekInternal(position);
}

void FileWriterSync::truncate(long long offset, ExceptionState& es)
{
    ASSERT(writer());
    ASSERT(m_complete);
    if (offset < 0) {
        es.throwDOMException(InvalidStateError, FileError::invalidStateErrorMessage);
        return;
    }
    prepareForWrite();
    writer()->truncate(offset);
    writer()->waitForOperationToComplete();
    ASSERT(m_complete);
    if (m_error) {
        FileError::throwDOMException(es, m_error);
        return;
    }
    if (offset < position())
        setPosition(offset);
    setLength(offset);
}

void FileWriterSync::didWrite(long long bytes, bool complete)
{
    ASSERT(m_error == FileError::OK);
    ASSERT(!m_complete);
#ifndef NDEBUG
    m_complete = complete;
#else
    ASSERT_UNUSED(complete, complete);
#endif
}

void FileWriterSync::didTruncate()
{
    ASSERT(m_error == FileError::OK);
    ASSERT(!m_complete);
#ifndef NDEBUG
    m_complete = true;
#endif
}

void FileWriterSync::didFail(FileError::ErrorCode error)
{
    ASSERT(m_error == FileError::OK);
    m_error = error;
    ASSERT(!m_complete);
#ifndef NDEBUG
    m_complete = true;
#endif
}

FileWriterSync::FileWriterSync()
    : m_error(FileError::OK)
#ifndef NDEBUG
    , m_complete(true)
#endif
{
    ScriptWrappable::init(this);
}

void FileWriterSync::prepareForWrite()
{
    ASSERT(m_complete);
    m_error = FileError::OK;
#ifndef NDEBUG
    m_complete = false;
#endif
}

FileWriterSync::~FileWriterSync()
{
}


} // namespace WebCore
