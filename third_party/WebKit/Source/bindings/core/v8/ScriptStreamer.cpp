// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/ScriptStreamer.h"

#include "bindings/core/v8/ScriptStreamerThread.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/PendingScript.h"
#include "core/fetch/ScriptResource.h"
#include "core/frame/Settings.h"
#include "platform/SharedBuffer.h"
#include "wtf/MainThread.h"
#include "wtf/text/TextEncodingRegistry.h"

namespace blink {

// SourceStream implements the streaming interface towards V8. The main
// functionality is preparing the data to give to V8 on main thread, and
// actually giving the data (via GetMoreData which is called on a background
// thread).
class SourceStream : public v8::ScriptCompiler::ExternalSourceStream {
    WTF_MAKE_NONCOPYABLE(SourceStream);
public:
    SourceStream(ScriptStreamer* streamer)
        : v8::ScriptCompiler::ExternalSourceStream()
        , m_streamer(streamer)
        , m_backgroundThreadWaitingForData(false)
        , m_dataPointer(0)
        , m_dataLength(0)
        , m_dataPosition(0)
        , m_loadingFinished(false)
        , m_cancelled(false)
        , m_allDataReturned(false) { }

    virtual ~SourceStream() { }

    // Called by V8 on a background thread. Should block until we can return
    // some data.
    virtual size_t GetMoreData(const uint8_t** src) OVERRIDE
    {
        ASSERT(!isMainThread());
        MutexLocker locker(m_mutex);
        if (m_cancelled || m_allDataReturned)
            return 0;
        // The main thread might be modifying the data buffer of the Resource,
        // so we cannot read it here. Post a task to the main thread and wait
        // for it to complete.
        m_backgroundThreadWaitingForData = true;
        m_dataPointer = src;
        m_dataLength = 0;
        callOnMainThread(WTF::bind(&SourceStream::prepareDataOnMainThread, this));
        m_condition.wait(m_mutex);
        if (m_cancelled || m_allDataReturned)
            return 0;
        return m_dataLength;
    }

    void didFinishLoading()
    {
        ASSERT(isMainThread());
        if (m_streamer->resource()->errorOccurred()) {
            // The background thread might be waiting for data. Wake it up. V8
            // will perceive this as EOS and call the completion callback. At
            // that point we will resume the normal "script loaded" code path
            // (see HTMLScriptRunner::notifyFinished which doesn't differentiate
            // between scripts which loaded successfully and scripts which
            // failed to load).
            cancel();
            return;
        }
        {
            MutexLocker locker(m_mutex);
            m_loadingFinished = true;
            if (!m_backgroundThreadWaitingForData)
                return;
        }
        prepareDataOnMainThread();
    }

    void didReceiveData()
    {
        ASSERT(isMainThread());
        prepareDataOnMainThread();
    }

    void cancel()
    {
        ASSERT(isMainThread());
        // The script is no longer needed by the upper layers. Stop streaming
        // it. The next time GetMoreData is called (or woken up), it will return
        // 0, which will be interpreted as EOS by V8 and the parsing will
        // fail. ScriptStreamer::streamingComplete will be called, and at that
        // point we will release the references to SourceStream.
        MutexLocker locker(m_mutex);
        m_cancelled = true;
        m_backgroundThreadWaitingForData = false;
        m_condition.signal();
    }

private:
    void prepareDataOnMainThread()
    {
        ASSERT(isMainThread());
        MutexLocker locker(m_mutex);
        if (!m_backgroundThreadWaitingForData) {
            // It's possible that the background thread is not waiting. This
            // happens when 1) data arrives but V8 hasn't requested data yet, 2)
            // we have previously scheduled prepareDataOnMainThread, but some
            // other event (load cancelled or finished) already retrieved the
            // data and set m_backgroundThreadWaitingForData to false.
            return;
        }
        // The Resource must still be alive; otherwise we should've cancelled
        // the streaming (if we have cancelled, the background thread is not
        // waiting).
        ASSERT(m_streamer->resource());

        if (m_streamer->resource()->cachedMetadata(V8ScriptRunner::tagForCodeCache())) {
            // The resource has a code cache, so it's unnecessary to stream and
            // parse the code. Cancel the streaming and resume the non-streaming
            // code path.
            m_streamer->suppressStreaming();
            m_cancelled = true;
            m_backgroundThreadWaitingForData = false;
            m_condition.signal();
            return;
        }

        if (!m_resourceBuffer) {
            // We don't have a buffer yet. Try to get it from the resource.
            SharedBuffer* buffer = m_streamer->resource()->resourceBuffer();
            if (!buffer) {
                // The Resource hasn't created its SharedBuffer yet. At some
                // point (at latest when the script loading finishes) this
                // function will be called again and we will have data.
                if (m_loadingFinished) {
                    // This happens only in special cases (e.g., involving empty
                    // scripts).
                    m_allDataReturned = true;
                    m_backgroundThreadWaitingForData = false;
                    m_condition.signal();
                }
                return;
            }
            m_resourceBuffer = RefPtr<SharedBuffer>(buffer);
        }

        // Get as much data from the ResourceBuffer as we can.
        const char* data = 0;
        Vector<const char*> chunks;
        Vector<unsigned> chunkLengths;
        while (unsigned length = m_resourceBuffer->getSomeData(data, m_dataPosition)) {
            // FIXME: Here we can limit based on the total length, if it turns
            // out that we don't want to give all the data we have (memory
            // vs. speed).
            chunks.append(data);
            chunkLengths.append(length);
            m_dataLength += length;
            m_dataPosition += length;
        }
        // Copy the data chunks into a new buffer, since we're going to give the
        // data to a background thread.
        if (m_dataLength > 0) {
            uint8_t* copiedData = new uint8_t[m_dataLength];
            unsigned offset = 0;
            for (size_t i = 0; i < chunks.size(); ++i) {
                memcpy(copiedData + offset, chunks[i], chunkLengths[i]);
                offset += chunkLengths[i];
            }
            *m_dataPointer = copiedData;
            m_backgroundThreadWaitingForData = false;
            m_condition.signal();
        } else if (m_loadingFinished) {
            // We need to return data length 0 to V8 to signal that the data
            // ends.
            m_allDataReturned = true;
            m_backgroundThreadWaitingForData = false;
            m_condition.signal();
        }
        // Otherwise, we had scheduled prepareDataOnMainThread but we didn't
        // have any data to return. It will be scheduled again when the data
        // arrives.
    }

    ScriptStreamer* m_streamer;

    // For coordinating between the main thread and background thread tasks.
    // Guarded by m_mutex.
    bool m_backgroundThreadWaitingForData;
    const uint8_t** m_dataPointer;
    unsigned m_dataLength;
    unsigned m_dataPosition;
    bool m_loadingFinished;
    bool m_cancelled;
    bool m_allDataReturned;
    RefPtr<SharedBuffer> m_resourceBuffer; // Only used by the main thread.

    ThreadCondition m_condition;
    Mutex m_mutex;
};


bool ScriptStreamer::startStreaming(PendingScript& script, Settings* settings, ScriptState* scriptState)
{
    ASSERT(isMainThread());
    if (!settings || !settings->v8ScriptStreamingEnabled())
        return false;
    if (ScriptStreamerThread::shared()->isRunningTask()) {
        // At the moment we only have one thread for running the tasks. A new
        // task shouldn't be queued before the running task completes, because
        // the running task can block and wait for data from the network. At the
        // moment we are only streaming parser blocking scripts, but this code
        // can still be hit when multiple frames are loading simultaneously.
        return false;
    }
    ScriptResource* resource = script.resource();
    if (!resource->url().protocolIsInHTTPFamily())
        return false;
    if (resource->resourceToRevalidate()) {
        // This happens e.g., during reloads. We're actually not going to load
        // the current Resource of the PendingScript but switch to another
        // Resource -> don't stream.
        return false;
    }
    // We cannot filter out short scripts, even if we wait for the HTTP headers
    // to arrive. In general, the web servers don't seem to send the
    // Content-Length HTTP header for scripts.

    WTF::TextEncoding textEncoding(resource->encoding());
    const char* encodingName = textEncoding.name();

    // Here's a list of encodings we can use for streaming. These are
    // the canonical names.
    v8::ScriptCompiler::StreamedSource::Encoding encoding;
    if (strcmp(encodingName, "windows-1252") == 0
        || strcmp(encodingName, "ISO-8859-1") == 0
        || strcmp(encodingName, "US-ASCII") == 0) {
        encoding = v8::ScriptCompiler::StreamedSource::ONE_BYTE;
    } else if (strcmp(encodingName, "UTF-8") == 0) {
        encoding = v8::ScriptCompiler::StreamedSource::UTF8;
    } else {
        // We don't stream other encodings; especially we don't stream two byte
        // scripts to avoid the handling of byte order marks. Most scripts are
        // Latin1 or UTF-8 anyway, so this should be enough for most real world
        // purposes.
        return false;
    }

    if (scriptState->contextIsValid())
        return false;
    ScriptState::Scope scope(scriptState);

    // The Resource might go out of scope if the script is no longer needed. We
    // will soon call PendingScript::setStreamer, which makes the PendingScript
    // notify the ScriptStreamer when it is destroyed.
    RefPtr<ScriptStreamer> streamer = adoptRef(new ScriptStreamer(resource, encoding));

    // Decide what kind of cached data we should produce while streaming. By
    // default, we generate the parser cache for streamed scripts, to emulate
    // the non-streaming behavior (seeV8ScriptRunner::compileScript).
    v8::ScriptCompiler::CompileOptions compileOption = v8::ScriptCompiler::kProduceParserCache;
    if (settings->v8CacheOptions() == V8CacheOptionsCode)
        compileOption = v8::ScriptCompiler::kProduceCodeCache;
    v8::ScriptCompiler::ScriptStreamingTask* scriptStreamingTask = v8::ScriptCompiler::StartStreamingScript(scriptState->isolate(), &(streamer->m_source), compileOption);
    if (scriptStreamingTask) {
        ScriptStreamingTask* task = new ScriptStreamingTask(scriptStreamingTask, streamer.get());
        // ScriptStreamer needs to stay alive as long as the background task is
        // running. This is taken care of with a manual ref() & deref() pair;
        // the corresponding deref() is in streamingComplete.
        streamer->ref();
        script.setStreamer(streamer.release());
        ScriptStreamerThread::shared()->postTask(task);
        return true;
    }
    // Otherwise, V8 cannot stream the script.
    return false;
}

void ScriptStreamer::streamingComplete()
{
    ASSERT(isMainThread());
    // It's possible that the corresponding Resource was deleted before V8
    // finished streaming. In that case, the data or the notification is not
    // needed.
    if (m_detached) {
        deref();
        return;
    }

    // We have now streamed the whole script to V8 and it has parsed the
    // script. We're ready for the next step: compiling and executing the
    // script.
    m_parsingFinished = true;

    notifyFinishedToClient();

    // The background thread no longer holds an implicit reference.
    deref();
}

void ScriptStreamer::cancel()
{
    ASSERT(isMainThread());
    // The upper layer doesn't need the script any more, but streaming might
    // still be ongoing. Tell SourceStream to try to cancel it whenever it gets
    // the control the next time. It can also be that V8 has already completed
    // its operations and streamingComplete will be called soon.
    m_detached = true;
    m_resource = 0;
    m_stream->cancel();
}

void ScriptStreamer::notifyAppendData(ScriptResource* resource)
{
    ASSERT(isMainThread());
    ASSERT(m_resource == resource);
    m_stream->didReceiveData();
}

void ScriptStreamer::notifyFinished(Resource* resource)
{
    ASSERT(isMainThread());
    ASSERT(m_resource == resource);
    m_stream->didFinishLoading();
    m_loadingFinished = true;
    notifyFinishedToClient();
}

ScriptStreamer::ScriptStreamer(ScriptResource* resource, v8::ScriptCompiler::StreamedSource::Encoding encoding)
    : m_resource(resource)
    , m_detached(false)
    , m_stream(new SourceStream(this))
    , m_source(m_stream, encoding) // m_source takes ownership of m_stream.
    , m_client(0)
    , m_loadingFinished(false)
    , m_parsingFinished(false)
    , m_streamingSuppressed(false)
{
}

void ScriptStreamer::notifyFinishedToClient()
{
    ASSERT(isMainThread());
    // Usually, the loading will be finished first, and V8 will still need some
    // time to catch up. But the other way is possible too: if V8 detects a
    // parse error, the V8 side can complete before loading has finished. Send
    // the notification after both loading and V8 side operations have
    // completed. Here we also check that we have a client: it can happen that a
    // function calling notifyFinishedToClient was already scheduled in the task
    // queue and the upper layer decided that it's not interested in the script
    // and called removeClient.
    if (m_loadingFinished && m_parsingFinished && m_client)
        m_client->notifyFinished(m_resource);
}

} // namespace blink
