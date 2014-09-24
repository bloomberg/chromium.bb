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

// For passing data between the main thread (producer) and the streamer thread
// (consumer). The main thread prepares the data (copies it from Resource) and
// the streamer thread feeds it to V8.
class SourceStreamDataQueue {
    WTF_MAKE_NONCOPYABLE(SourceStreamDataQueue);
public:
    SourceStreamDataQueue()
        : m_finished(false) { }

    ~SourceStreamDataQueue()
    {
        while (!m_data.isEmpty()) {
            std::pair<const uint8_t*, size_t> next_data = m_data.takeFirst();
            delete[] next_data.first;
        }
    }

    void produce(const uint8_t* data, size_t length)
    {
        MutexLocker locker(m_mutex);
        m_data.append(std::make_pair(data, length));
        m_haveData.signal();
    }

    void finish()
    {
        MutexLocker locker(m_mutex);
        m_finished = true;
        m_haveData.signal();
    }

    void consume(const uint8_t** data, size_t* length)
    {
        MutexLocker locker(m_mutex);
        while (!tryGetData(data, length))
            m_haveData.wait(m_mutex);
    }

private:
    bool tryGetData(const uint8_t** data, size_t* length)
    {
        if (!m_data.isEmpty()) {
            std::pair<const uint8_t*, size_t> next_data = m_data.takeFirst();
            *data = next_data.first;
            *length = next_data.second;
            return true;
        }
        if (m_finished) {
            *length = 0;
            return true;
        }
        return false;
    }

    WTF::Deque<std::pair<const uint8_t*, size_t> > m_data;
    bool m_finished;
    Mutex m_mutex;
    ThreadCondition m_haveData;
};


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
        , m_cancelled(false)
        , m_dataPosition(0) { }

    virtual ~SourceStream() { }

    // Called by V8 on a background thread. Should block until we can return
    // some data.
    virtual size_t GetMoreData(const uint8_t** src) OVERRIDE
    {
        ASSERT(!isMainThread());
        {
            MutexLocker locker(m_mutex);
            if (m_cancelled)
                return 0;
        }
        size_t length = 0;
        // This will wait until there is data.
        m_dataQueue.consume(src, &length);
        {
            MutexLocker locker(m_mutex);
            if (m_cancelled)
                return 0;
        }
        return length;
    }

    void didFinishLoading()
    {
        ASSERT(isMainThread());
        m_dataQueue.finish();
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
        {
            MutexLocker locker(m_mutex);
            m_cancelled = true;
        }
        m_dataQueue.finish();
    }

private:
    void prepareDataOnMainThread()
    {
        ASSERT(isMainThread());
        // The Resource must still be alive; otherwise we should've cancelled
        // the streaming (if we have cancelled, the background thread is not
        // waiting).
        ASSERT(m_streamer->resource());

        if (m_streamer->resource()->cachedMetadata(V8ScriptRunner::tagForCodeCache())) {
            // The resource has a code cache, so it's unnecessary to stream and
            // parse the code. Cancel the streaming and resume the non-streaming
            // code path.
            m_streamer->suppressStreaming();
            {
                MutexLocker locker(m_mutex);
                m_cancelled = true;
            }
            m_dataQueue.finish();
            return;
        }

        if (!m_resourceBuffer) {
            // We don't have a buffer yet. Try to get it from the resource.
            SharedBuffer* buffer = m_streamer->resource()->resourceBuffer();
            if (!buffer)
                return;
            m_resourceBuffer = RefPtr<SharedBuffer>(buffer);
        }

        // Get as much data from the ResourceBuffer as we can.
        const char* data = 0;
        Vector<const char*> chunks;
        Vector<unsigned> chunkLengths;
        size_t dataLength = 0;
        while (unsigned length = m_resourceBuffer->getSomeData(data, m_dataPosition)) {
            // FIXME: Here we can limit based on the total length, if it turns
            // out that we don't want to give all the data we have (memory
            // vs. speed).
            chunks.append(data);
            chunkLengths.append(length);
            dataLength += length;
            m_dataPosition += length;
        }
        // Copy the data chunks into a new buffer, since we're going to give the
        // data to a background thread.
        if (dataLength > 0) {
            uint8_t* copiedData = new uint8_t[dataLength];
            unsigned offset = 0;
            for (size_t i = 0; i < chunks.size(); ++i) {
                memcpy(copiedData + offset, chunks[i], chunkLengths[i]);
                offset += chunkLengths[i];
            }
            m_dataQueue.produce(copiedData, dataLength);
        }
    }

    ScriptStreamer* m_streamer;

    // For coordinating between the main thread and background thread tasks.
    // Guarded by m_mutex.
    bool m_cancelled;
    Mutex m_mutex;

    unsigned m_dataPosition; // Only used by the main thread.
    RefPtr<SharedBuffer> m_resourceBuffer; // Only used by the main thread.
    SourceStreamDataQueue m_dataQueue; // Thread safe.
};

size_t ScriptStreamer::kSmallScriptThreshold = 30 * 1024;

bool ScriptStreamer::startStreaming(PendingScript& script, Settings* settings, ScriptState* scriptState)
{
    ASSERT(isMainThread());
    if (!settings || !settings->v8ScriptStreamingEnabled())
        return false;
    ScriptResource* resource = script.resource();
    ASSERT(!resource->isLoaded());
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
    // the non-streaming behavior (see V8ScriptRunner::compileScript).
    v8::ScriptCompiler::CompileOptions compileOption = v8::ScriptCompiler::kProduceParserCache;
    if (settings->v8CacheOptions() == V8CacheOptionsCode)
        compileOption = v8::ScriptCompiler::kProduceCodeCache;
    v8::ScriptCompiler::ScriptStreamingTask* scriptStreamingTask = v8::ScriptCompiler::StartStreamingScript(scriptState->isolate(), &(streamer->m_source), compileOption);
    if (scriptStreamingTask) {
        streamer->m_task = scriptStreamingTask;
        script.setStreamer(streamer.release());
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
    // needed. In addition, if the streaming is suppressed, the non-streaming
    // code path will resume after the resource has loaded, before the
    // background task finishes.
    if (m_detached || m_streamingSuppressed) {
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

void ScriptStreamer::suppressStreaming()
{
    ASSERT(!m_parsingFinished);
    ASSERT(!m_loadingFinished);
    m_streamingSuppressed = true;
}

void ScriptStreamer::notifyAppendData(ScriptResource* resource)
{
    ASSERT(isMainThread());
    ASSERT(m_resource == resource);
    if (m_streamingSuppressed)
        return;
    if (!m_firstDataChunkReceived) {
        m_firstDataChunkReceived = true;
        // Check the size of the first data chunk. The expectation is that if
        // the first chunk is small, there won't be a second one. In those
        // cases, it doesn't make sense to stream at all.
        if (resource->resourceBuffer()->size() < kSmallScriptThreshold) {
            suppressStreaming();
            return;
        }
        if (ScriptStreamerThread::shared()->isRunningTask()) {
            // At the moment we only have one thread for running the tasks. A
            // new task shouldn't be queued before the running task completes,
            // because the running task can block and wait for data from the
            // network. At the moment we are only streaming parser blocking
            // scripts, but this code can still be hit when multiple frames are
            // loading simultaneously.
            suppressStreaming();
            return;
        }
        ASSERT(m_task);
        // ScriptStreamer needs to stay alive as long as the background task is
        // running. This is taken care of with a manual ref() & deref() pair;
        // the corresponding deref() is in streamingComplete.
        ref();
        ScriptStreamingTask* task = new ScriptStreamingTask(m_task, this);
        ScriptStreamerThread::shared()->postTask(task);
        m_task = 0;
    }
    m_stream->didReceiveData();
}

void ScriptStreamer::notifyFinished(Resource* resource)
{
    ASSERT(isMainThread());
    ASSERT(m_resource == resource);
    // A special case: empty scripts. We didn't receive any data before this
    // notification. In that case, there won't be a "parsing complete"
    // notification either, and we should not wait for it.
    if (!m_firstDataChunkReceived)
        suppressStreaming();
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
    , m_task(0)
    , m_loadingFinished(false)
    , m_parsingFinished(false)
    , m_firstDataChunkReceived(false)
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
    if (isFinished() && m_client)
        m_client->notifyFinished(m_resource);
}

} // namespace blink
