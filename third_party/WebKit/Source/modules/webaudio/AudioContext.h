/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AudioContext_h
#define AudioContext_h

#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventListener.h"
#include "modules/EventTargetModules.h"
#include "modules/webaudio/AsyncAudioDecoder.h"
#include "modules/webaudio/AudioDestinationNode.h"
#include "platform/audio/AudioBus.h"
#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"
#include "wtf/MainThread.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/Threading.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class AnalyserNode;
class AudioBuffer;
class AudioBufferCallback;
class AudioBufferSourceNode;
class AudioListener;
class AudioSummingJunction;
class BiquadFilterNode;
class ChannelMergerNode;
class ChannelSplitterNode;
class ConvolverNode;
class DelayNode;
class Document;
class DynamicsCompressorNode;
class ExceptionState;
class GainNode;
class HTMLMediaElement;
class MediaElementAudioSourceNode;
class MediaStreamAudioDestinationNode;
class MediaStreamAudioSourceNode;
class OscillatorNode;
class PannerNode;
class PeriodicWave;
class ScriptProcessorNode;
class WaveShaperNode;

// AudioContext is the cornerstone of the web audio API and all AudioNodes are created from it.
// For thread safety between the audio thread and the main thread, it has a rendering graph locking mechanism.

class AudioContext : public RefCountedGarbageCollectedWillBeGarbageCollectedFinalized<AudioContext>, public ActiveDOMObject, public EventTargetWithInlineData {
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollected<AudioContext>);
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(AudioContext);
public:
    // Create an AudioContext for rendering to the audio hardware.
    static AudioContext* create(Document&, ExceptionState&);

    virtual ~AudioContext();

    virtual void trace(Visitor*) OVERRIDE;

    bool isInitialized() const { return m_isInitialized; }
    bool isOfflineContext() { return m_isOfflineContext; }

    // Document notification
    virtual void stop() OVERRIDE FINAL;
    virtual bool hasPendingActivity() const OVERRIDE;

    AudioDestinationNode* destination() { return m_destinationNode.get(); }
    size_t currentSampleFrame() const { return m_destinationNode->currentSampleFrame(); }
    double currentTime() const { return m_destinationNode->currentTime(); }
    float sampleRate() const { return m_destinationNode->sampleRate(); }

    AudioBuffer* createBuffer(unsigned numberOfChannels, size_t numberOfFrames, float sampleRate, ExceptionState&);

    // Asynchronous audio file data decoding.
    void decodeAudioData(ArrayBuffer*, PassOwnPtrWillBeRawPtr<AudioBufferCallback>, PassOwnPtrWillBeRawPtr<AudioBufferCallback>, ExceptionState&);

    AudioListener* listener() { return m_listener.get(); }

    // The AudioNode create methods are called on the main thread (from JavaScript).
    AudioBufferSourceNode* createBufferSource();
    MediaElementAudioSourceNode* createMediaElementSource(HTMLMediaElement*, ExceptionState&);
    MediaStreamAudioSourceNode* createMediaStreamSource(MediaStream*, ExceptionState&);
    MediaStreamAudioDestinationNode* createMediaStreamDestination();
    GainNode* createGain();
    BiquadFilterNode* createBiquadFilter();
    WaveShaperNode* createWaveShaper();
    DelayNode* createDelay(ExceptionState&);
    DelayNode* createDelay(double maxDelayTime, ExceptionState&);
    PannerNode* createPanner();
    ConvolverNode* createConvolver();
    DynamicsCompressorNode* createDynamicsCompressor();
    AnalyserNode* createAnalyser();
    ScriptProcessorNode* createScriptProcessor(ExceptionState&);
    ScriptProcessorNode* createScriptProcessor(size_t bufferSize, ExceptionState&);
    ScriptProcessorNode* createScriptProcessor(size_t bufferSize, size_t numberOfInputChannels, ExceptionState&);
    ScriptProcessorNode* createScriptProcessor(size_t bufferSize, size_t numberOfInputChannels, size_t numberOfOutputChannels, ExceptionState&);
    ChannelSplitterNode* createChannelSplitter(ExceptionState&);
    ChannelSplitterNode* createChannelSplitter(size_t numberOfOutputs, ExceptionState&);
    ChannelMergerNode* createChannelMerger(ExceptionState&);
    ChannelMergerNode* createChannelMerger(size_t numberOfInputs, ExceptionState&);
    OscillatorNode* createOscillator();
    PeriodicWave* createPeriodicWave(Float32Array* real, Float32Array* imag, ExceptionState&);

    // When a source node has no more processing to do (has finished playing), then it tells the context to dereference it.
    void notifyNodeFinishedProcessing(AudioNode*);

    // Called at the start of each render quantum.
    void handlePreRenderTasks();

    // Called at the end of each render quantum.
    void handlePostRenderTasks();

    // Called periodically at the end of each render quantum to dereference finished source nodes.
    void derefFinishedSourceNodes();

    void registerLiveAudioSummingJunction(AudioSummingJunction&);
    void registerLiveNode(AudioNode&);

    // AudioContext can pull node(s) at the end of each render quantum even when they are not connected to any downstream nodes.
    // These two methods are called by the nodes who want to add/remove themselves into/from the automatic pull lists.
    void addAutomaticPullNode(AudioNode*);
    void removeAutomaticPullNode(AudioNode*);

    // Called right before handlePostRenderTasks() to handle nodes which need to be pulled even when they are not connected to anything.
    void processAutomaticPullNodes(size_t framesToProcess);

    // Keeps track of the number of connections made.
    void incrementConnectionCount()
    {
        ASSERT(isMainThread());
        m_connectionCount++;
    }

    unsigned connectionCount() const { return m_connectionCount; }

    //
    // Thread Safety and Graph Locking:
    //

    void setAudioThread(ThreadIdentifier thread) { m_audioThread = thread; } // FIXME: check either not initialized or the same
    ThreadIdentifier audioThread() const { return m_audioThread; }
    bool isAudioThread() const;

    // mustReleaseLock is set to true if we acquired the lock in this method call and caller must unlock(), false if it was previously acquired.
    void lock(bool& mustReleaseLock);

    // Returns true if we own the lock.
    // mustReleaseLock is set to true if we acquired the lock in this method call and caller must unlock(), false if it was previously acquired.
    bool tryLock(bool& mustReleaseLock);

    void unlock();

    // Returns true if this thread owns the context's lock.
    bool isGraphOwner() const;

    // Returns the maximum numuber of channels we can support.
    static unsigned maxNumberOfChannels() { return MaxNumberOfChannels;}

    class AutoLocker {
        STACK_ALLOCATED();
    public:
        explicit AutoLocker(AudioContext* context)
            : m_context(context)
        {
            ASSERT(context);
            context->lock(m_mustReleaseLock);
        }

        ~AutoLocker()
        {
            if (m_mustReleaseLock)
                m_context->unlock();
        }
    private:
        Member<AudioContext> m_context;
        bool m_mustReleaseLock;
    };

    // In AudioNode::breakConnection() and deref(), a tryLock() is used for
    // calling actual processing, but if it fails keep track here.
    void addDeferredBreakConnection(AudioNode&);

    // In the audio thread at the start of each render cycle, we'll call this.
    void handleDeferredAudioNodeTasks();

    // Only accessed when the graph lock is held.
    void markSummingJunctionDirty(AudioSummingJunction*);
    // Only accessed when the graph lock is held. Must be called on the main thread.
    void removeMarkedSummingJunction(AudioSummingJunction*);
    void markAudioNodeOutputDirty(AudioNodeOutput*);
    void removeMarkedAudioNodeOutput(AudioNodeOutput*);
    void disposeOutputs(AudioNode&);

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE FINAL;
    virtual ExecutionContext* executionContext() const OVERRIDE FINAL;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(complete);

    void startRendering();
    void fireCompletionEvent();

    static unsigned s_hardwareContextCount;

protected:
    explicit AudioContext(Document*);
    AudioContext(Document*, unsigned numberOfChannels, size_t numberOfFrames, float sampleRate);

    static bool isSampleRateRangeGood(float sampleRate);

private:
    void initialize();
    void uninitialize();

    // ExecutionContext calls stop twice.
    // We'd like to schedule only one stop action for them.
    bool m_isStopScheduled;
    bool m_isCleared;
    void clear();

    // Set to true when the destination node has been initialized and is ready to process data.
    bool m_isInitialized;

    // The context itself keeps a reference to all source nodes.  The source nodes, then reference all nodes they're connected to.
    // In turn, these nodes reference all nodes they're connected to.  All nodes are ultimately connected to the AudioDestinationNode.
    // When the context dereferences a source node, it will be deactivated from the rendering graph along with all other nodes it is
    // uniquely connected to.  See the AudioNode::ref() and AudioNode::deref() methods for more details.
    void refNode(AudioNode*);
    void derefNode(AudioNode*);

    // When the context goes away, there might still be some sources which haven't finished playing.
    // Make sure to dereference them here.
    void derefUnfinishedSourceNodes();

    Member<AudioDestinationNode> m_destinationNode;
    Member<AudioListener> m_listener;

    // Only accessed in the audio thread.
    // Oilpan: Since items are added to the vector by the audio thread (not registered to Oilpan),
    // we cannot use a HeapVector.
    GC_PLUGIN_IGNORE("http://crbug.com/404527")
    Vector<AudioNode*> m_finishedNodes;

    // List of source nodes. This is either accessed when the graph lock is
    // held, or on the main thread when the audio thread has finished.
    // Oilpan: This Vector holds connection references. We must call
    // AudioNode::makeConnection when we add an AudioNode to this, and must call
    // AudioNode::breakConnection() when we remove an AudioNode from this.
    HeapVector<Member<AudioNode> > m_referencedNodes;

    class AudioNodeDisposer {
    public:
        explicit AudioNodeDisposer(AudioNode& node) : m_node(node) { }
        ~AudioNodeDisposer();

    private:
        AudioNode& m_node;
    };
    HeapHashMap<WeakMember<AudioNode>, OwnPtr<AudioNodeDisposer> > m_liveNodes;

    class AudioSummingJunctionDisposer {
    public:
        explicit AudioSummingJunctionDisposer(AudioSummingJunction& junction) : m_junction(junction) { }
        ~AudioSummingJunctionDisposer();

    private:
        AudioSummingJunction& m_junction;
    };
    // The purpose of m_liveAudioSummingJunctions is to remove a dying
    // AudioSummingJunction from m_dirtySummingJunctions. However we put all of
    // AudioSummingJunction objects to m_liveAudioSummingJunctions to avoid
    // concurrent access to m_liveAudioSummingJunctions.
    HeapHashMap<WeakMember<AudioSummingJunction>, OwnPtr<AudioSummingJunctionDisposer> > m_liveAudioSummingJunctions;

    // These two HashSet must be accessed only when the graph lock is held.
    // Oilpan: These HashSet should be HeapHashSet<WeakMember<AudioNodeOutput>>
    // ideally. But it's difficult to lock them correctly during GC.
    // Oilpan: Since items are added to these hash sets by the audio thread (not registered to Oilpan),
    // we cannot use HeapHashSets.
    GC_PLUGIN_IGNORE("http://crbug.com/404527")
    HashSet<AudioSummingJunction*> m_dirtySummingJunctions;
    GC_PLUGIN_IGNORE("http://crbug.com/404527")
    HashSet<AudioNodeOutput*> m_dirtyAudioNodeOutputs;
    void handleDirtyAudioSummingJunctions();
    void handleDirtyAudioNodeOutputs();

    // For the sake of thread safety, we maintain a seperate Vector of automatic pull nodes for rendering in m_renderingAutomaticPullNodes.
    // It will be copied from m_automaticPullNodes by updateAutomaticPullNodes() at the very start or end of the rendering quantum.
    // Oilpan: Since items are added to the vector/hash set by the audio thread (not registered to Oilpan),
    // we cannot use a HeapVector/HeapHashSet.
    GC_PLUGIN_IGNORE("http://crbug.com/404527")
    HashSet<AudioNode*> m_automaticPullNodes;
    GC_PLUGIN_IGNORE("http://crbug.com/404527")
    Vector<AudioNode*> m_renderingAutomaticPullNodes;
    // m_automaticPullNodesNeedUpdating keeps track if m_automaticPullNodes is modified.
    bool m_automaticPullNodesNeedUpdating;
    void updateAutomaticPullNodes();

    unsigned m_connectionCount;

    // Graph locking.
    Mutex m_contextGraphMutex;
    volatile ThreadIdentifier m_audioThread;
    volatile ThreadIdentifier m_graphOwnerThread; // if the lock is held then this is the thread which owns it, otherwise == UndefinedThreadIdentifier

    // Only accessed in the audio thread.
    // Oilpan: Since items are added to these vectors by the audio thread (not registered to Oilpan),
    // we cannot use HeapVectors.
    GC_PLUGIN_IGNORE("http://crbug.com/404527")
    Vector<AudioNode*> m_deferredBreakConnectionList;

    Member<AudioBuffer> m_renderTarget;

    bool m_isOfflineContext;

    AsyncAudioDecoder m_audioDecoder;

    // This is considering 32 is large enough for multiple channels audio.
    // It is somewhat arbitrary and could be increased if necessary.
    enum { MaxNumberOfChannels = 32 };
};

} // namespace blink

#endif // AudioContext_h
