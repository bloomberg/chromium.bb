/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#include "core/html/HTMLMediaElement.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptEventListener.h"
#include "core/HTMLNames.h"
#include "core/css/MediaList.h"
#include "core/dom/Attribute.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/Fullscreen.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/Event.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLMediaSource.h"
#include "core/html/HTMLSourceElement.h"
#include "core/html/HTMLTrackElement.h"
#include "core/html/MediaError.h"
#include "core/html/MediaFragmentURIParser.h"
#include "core/html/TimeRanges.h"
#include "core/html/shadow/MediaControls.h"
#include "core/html/track/AudioTrack.h"
#include "core/html/track/AudioTrackList.h"
#include "core/html/track/AutomaticTrackSelection.h"
#include "core/html/track/CueTimeline.h"
#include "core/html/track/InbandTextTrack.h"
#include "core/html/track/TextTrackContainer.h"
#include "core/html/track/TextTrackList.h"
#include "core/html/track/VideoTrack.h"
#include "core/html/track/VideoTrackList.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/LayoutVideo.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/ChromeClient.h"
#include "core/page/NetworkStateNotifier.h"
#include "platform/ContentType.h"
#include "platform/Logging.h"
#include "platform/MIMETypeFromURL.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/UserGestureIndicator.h"
#include "platform/audio/AudioBus.h"
#include "platform/audio/AudioSourceProviderClient.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebAudioSourceProvider.h"
#include "public/platform/WebContentDecryptionModule.h"
#include "public/platform/WebInbandTextTrack.h"
#include "wtf/CurrentTime.h"
#include "wtf/MainThread.h"
#include "wtf/MathExtras.h"
#include "wtf/text/CString.h"
#include <limits>


namespace blink {

#if !LOG_DISABLED
static String urlForLoggingMedia(const KURL& url)
{
    static const unsigned maximumURLLengthForLogging = 128;

    if (url.string().length() < maximumURLLengthForLogging)
        return url.string();
    return url.string().substring(0, maximumURLLengthForLogging) + "...";
}

static const char* boolString(bool val)
{
    return val ? "true" : "false";
}
#endif

#ifndef LOG_MEDIA_EVENTS
// Default to not logging events because so many are generated they can overwhelm the rest of
// the logging.
#define LOG_MEDIA_EVENTS 0
#endif

#ifndef LOG_CACHED_TIME_WARNINGS
// Default to not logging warnings about excessive drift in the cached media time because it adds a
// fair amount of overhead and logging.
#define LOG_CACHED_TIME_WARNINGS 0
#endif

// URL protocol used to signal that the media source API is being used.
static const char mediaSourceBlobProtocol[] = "blob";

using namespace HTMLNames;

typedef WillBeHeapHashSet<RawPtrWillBeWeakMember<HTMLMediaElement>> WeakMediaElementSet;
typedef WillBeHeapHashMap<RawPtrWillBeWeakMember<Document>, WeakMediaElementSet> DocumentElementSetMap;
static DocumentElementSetMap& documentToElementSetMap()
{
    DEFINE_STATIC_LOCAL(OwnPtrWillBePersistent<DocumentElementSetMap>, map, (adoptPtrWillBeNoop(new DocumentElementSetMap())));
    return *map;
}

static void addElementToDocumentMap(HTMLMediaElement* element, Document* document)
{
    DocumentElementSetMap& map = documentToElementSetMap();
    WeakMediaElementSet set = map.take(document);
    set.add(element);
    map.add(document, set);
}

static void removeElementFromDocumentMap(HTMLMediaElement* element, Document* document)
{
    DocumentElementSetMap& map = documentToElementSetMap();
    WeakMediaElementSet set = map.take(document);
    set.remove(element);
    if (!set.isEmpty())
        map.add(document, set);
}

class AudioSourceProviderClientLockScope {
    STACK_ALLOCATED();
public:
    AudioSourceProviderClientLockScope(HTMLMediaElement& element)
        : m_client(element.audioSourceNode())
    {
        if (m_client)
            m_client->lock();
    }
    ~AudioSourceProviderClientLockScope()
    {
        if (m_client)
            m_client->unlock();
    }

private:
    Member<AudioSourceProviderClient> m_client;
};

static const AtomicString& AudioKindToString(WebMediaPlayerClient::AudioTrackKind kind)
{
    switch (kind) {
    case WebMediaPlayerClient::AudioTrackKindNone:
        return emptyAtom;
    case WebMediaPlayerClient::AudioTrackKindAlternative:
        return AudioTrack::alternativeKeyword();
    case WebMediaPlayerClient::AudioTrackKindDescriptions:
        return AudioTrack::descriptionsKeyword();
    case WebMediaPlayerClient::AudioTrackKindMain:
        return AudioTrack::mainKeyword();
    case WebMediaPlayerClient::AudioTrackKindMainDescriptions:
        return AudioTrack::mainDescriptionsKeyword();
    case WebMediaPlayerClient::AudioTrackKindTranslation:
        return AudioTrack::translationKeyword();
    case WebMediaPlayerClient::AudioTrackKindCommentary:
        return AudioTrack::commentaryKeyword();
    }

    ASSERT_NOT_REACHED();
    return emptyAtom;
}

static const AtomicString& VideoKindToString(WebMediaPlayerClient::VideoTrackKind kind)
{
    switch (kind) {
    case WebMediaPlayerClient::VideoTrackKindNone:
        return emptyAtom;
    case WebMediaPlayerClient::VideoTrackKindAlternative:
        return VideoTrack::alternativeKeyword();
    case WebMediaPlayerClient::VideoTrackKindCaptions:
        return VideoTrack::captionsKeyword();
    case WebMediaPlayerClient::VideoTrackKindMain:
        return VideoTrack::mainKeyword();
    case WebMediaPlayerClient::VideoTrackKindSign:
        return VideoTrack::signKeyword();
    case WebMediaPlayerClient::VideoTrackKindSubtitles:
        return VideoTrack::subtitlesKeyword();
    case WebMediaPlayerClient::VideoTrackKindCommentary:
        return VideoTrack::commentaryKeyword();
    }

    ASSERT_NOT_REACHED();
    return emptyAtom;
}

static bool canLoadURL(const KURL& url, const ContentType& contentType, const String& keySystem)
{
    DEFINE_STATIC_LOCAL(const String, codecs, ("codecs"));

    String contentMIMEType = contentType.type().lower();
    String contentTypeCodecs = contentType.parameter(codecs);

    // If the MIME type is missing or is not meaningful, try to figure it out from the URL.
    if (contentMIMEType.isEmpty() || contentMIMEType == "application/octet-stream" || contentMIMEType == "text/plain") {
        if (url.protocolIsData())
            contentMIMEType = mimeTypeFromDataURL(url.string());
    }

    // If no MIME type is specified, always attempt to load.
    if (contentMIMEType.isEmpty())
        return true;

    // 4.8.10.3 MIME types - In the absence of a specification to the contrary, the MIME type "application/octet-stream"
    // when used with parameters, e.g. "application/octet-stream;codecs=theora", is a type that the user agent knows
    // it cannot render.
    if (contentMIMEType != "application/octet-stream" || contentTypeCodecs.isEmpty()) {
        WebMimeRegistry::SupportsType supported = Platform::current()->mimeRegistry()->supportsMediaMIMEType(contentMIMEType, contentTypeCodecs, keySystem.lower());
        return supported > WebMimeRegistry::IsNotSupported;
    }

    return false;
}

void HTMLMediaElement::recordAutoplayMetric(AutoplayMetrics metric)
{
    Platform::current()->histogramEnumeration("Blink.MediaElement.Autoplay", metric, NumberOfAutoplayMetrics);
}

WebMimeRegistry::SupportsType HTMLMediaElement::supportsType(const ContentType& contentType, const String& keySystem)
{
    DEFINE_STATIC_LOCAL(const String, codecs, ("codecs"));

    if (!RuntimeEnabledFeatures::mediaEnabled())
        return WebMimeRegistry::IsNotSupported;

    String type = contentType.type().lower();
    // The codecs string is not lower-cased because MP4 values are case sensitive
    // per http://tools.ietf.org/html/rfc4281#page-7.
    String typeCodecs = contentType.parameter(codecs);
    String system = keySystem.lower();

    if (type.isEmpty())
        return WebMimeRegistry::IsNotSupported;

    // 4.8.10.3 MIME types - The canPlayType(type) method must return the empty string if type is a type that the
    // user agent knows it cannot render or is the type "application/octet-stream"
    if (type == "application/octet-stream")
        return WebMimeRegistry::IsNotSupported;

    return Platform::current()->mimeRegistry()->supportsMediaMIMEType(type, typeCodecs, system);
}

URLRegistry* HTMLMediaElement::s_mediaStreamRegistry = 0;

void HTMLMediaElement::setMediaStreamRegistry(URLRegistry* registry)
{
    ASSERT(!s_mediaStreamRegistry);
    s_mediaStreamRegistry = registry;
}

bool HTMLMediaElement::isMediaStreamURL(const String& url)
{
    return s_mediaStreamRegistry ? s_mediaStreamRegistry->contains(url) : false;
}

HTMLMediaElement::HTMLMediaElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , ActiveDOMObject(&document)
    , m_loadTimer(this, &HTMLMediaElement::loadTimerFired)
    , m_progressEventTimer(this, &HTMLMediaElement::progressEventTimerFired)
    , m_playbackProgressTimer(this, &HTMLMediaElement::playbackProgressTimerFired)
    , m_audioTracksTimer(this, &HTMLMediaElement::audioTracksTimerFired)
    , m_playedTimeRanges()
    , m_asyncEventQueue(GenericEventQueue::create(this))
    , m_playbackRate(1.0f)
    , m_defaultPlaybackRate(1.0f)
    , m_networkState(NETWORK_EMPTY)
    , m_readyState(HAVE_NOTHING)
    , m_readyStateMaximum(HAVE_NOTHING)
    , m_volume(1.0f)
    , m_lastSeekTime(0)
    , m_previousProgressTime(std::numeric_limits<double>::max())
    , m_duration(std::numeric_limits<double>::quiet_NaN())
    , m_lastTimeUpdateEventWallTime(0)
    , m_lastTimeUpdateEventMovieTime(0)
    , m_defaultPlaybackStartPosition(0)
    , m_loadState(WaitingForSource)
    , m_deferredLoadState(NotDeferred)
    , m_deferredLoadTimer(this, &HTMLMediaElement::deferredLoadTimerFired)
    , m_webLayer(nullptr)
    , m_displayMode(Unknown)
    , m_cachedTime(std::numeric_limits<double>::quiet_NaN())
    , m_fragmentEndTime(std::numeric_limits<double>::quiet_NaN())
    , m_pendingActionFlags(0)
    , m_userGestureRequiredForPlay(false)
    , m_playing(false)
    , m_shouldDelayLoadEvent(false)
    , m_haveFiredLoadedData(false)
    , m_autoplaying(true)
    , m_muted(false)
    , m_paused(true)
    , m_seeking(false)
    , m_sentStalledEvent(false)
    , m_sentEndEvent(false)
    , m_closedCaptionsVisible(false)
    , m_completelyLoaded(false)
    , m_havePreparedToPlay(false)
    , m_tracksAreReady(true)
    , m_haveVisibleTextTrack(false)
    , m_processingPreferenceChange(false)
    , m_remoteRoutesAvailable(false)
    , m_playingRemotely(false)
    , m_isFinalizing(false)
    , m_initialPlayWithoutUserGesture(false)
    , m_autoplayMediaCounted(false)
    , m_inOverlayFullscreenVideo(false)
    , m_audioTracks(AudioTrackList::create(*this))
    , m_videoTracks(VideoTrackList::create(*this))
    , m_textTracks(nullptr)
    , m_audioSourceNode(nullptr)
    , m_autoplayHelper(*this)
{
#if ENABLE(OILPAN)
    ThreadState::current()->registerPreFinalizer(this);
#endif
    ASSERT(RuntimeEnabledFeatures::mediaEnabled());

    WTF_LOG(Media, "HTMLMediaElement::HTMLMediaElement(%p)", this);

    if (document.settings() && document.settings()->mediaPlaybackRequiresUserGesture())
        m_userGestureRequiredForPlay = true;

    setHasCustomStyleCallbacks();
    addElementToDocumentMap(this, &document);
}

HTMLMediaElement::~HTMLMediaElement()
{
    WTF_LOG(Media, "HTMLMediaElement::~HTMLMediaElement(%p)", this);

#if !ENABLE(OILPAN)
    // HTMLMediaElement and m_asyncEventQueue always become unreachable
    // together. So HTMLMediaElement and m_asyncEventQueue are destructed in
    // the same GC. We don't need to close it explicitly in Oilpan.
    m_asyncEventQueue->close();

    setShouldDelayLoadEvent(false);

    if (m_textTracks)
        m_textTracks->clearOwner();
    m_audioTracks->shutdown();
    m_videoTracks->shutdown();

    closeMediaSource();

    removeElementFromDocumentMap(this, &document());

    // Destroying the player may cause a resource load to be canceled,
    // which could result in LocalDOMWindow::dispatchWindowLoadEvent() being
    // called via ResourceFetch::didLoadResource() then
    // FrameLoader::checkCompleted(). To prevent load event dispatching during
    // object destruction, we use Document::incrementLoadEventDelayCount().
    // See http://crbug.com/275223 for more details.
    document().incrementLoadEventDelayCount();

    clearMediaPlayerAndAudioSourceProviderClientWithoutLocking();

    document().decrementLoadEventDelayCount();
#endif

    // m_audioSourceNode is explicitly cleared by AudioNode::dispose().
    // Since AudioNode::dispose() is guaranteed to be always called before
    // the AudioNode is destructed, m_audioSourceNode is explicitly cleared
    // even if the AudioNode and the HTMLMediaElement die together.
    ASSERT(!m_audioSourceNode);
}

#if ENABLE(OILPAN)
void HTMLMediaElement::dispose()
{
    // If the HTMLMediaElement dies with the Document we are not
    // allowed to touch the Document to adjust delay load event counts
    // from the destructor, as the Document could have been already
    // destructed.
    //
    // Work around that restriction by accessing the Document from
    // a prefinalizer action instead, updating its delayed load count.
    // If needed - if the Document has been detached and informed its
    // ContextLifecycleObservers (which HTMLMediaElement is) that
    // it is being destroyed, the connection to the Document will
    // have been severed already, but in that case there is no need
    // to update the delayed load count. But if the Document hasn't
    // been detached cleanly from any frame or it isn't dying in the
    // same GC, we do update the delayed load count from the prefinalizer.
    if (ActiveDOMObject::executionContext())
        setShouldDelayLoadEvent(false);

    // If the MediaSource object survived, notify that the media element
    // didn't.
    if (Heap::isHeapObjectAlive(m_mediaSource))
        closeMediaSource();

    // Oilpan: the player must be released, but the player object
    // cannot safely access this player client any longer as parts of
    // it may have been finalized already (like the media element's
    // supplementable table.)  Handled for now by entering an
    // is-finalizing state, which is explicitly checked for if the
    // player tries to access the media element during shutdown.
    //
    // FIXME: Oilpan: move the media player to the heap instead and
    // avoid having to finalize it from here; this whole #if block
    // could then be removed (along with the state bit it depends on.)
    // crbug.com/378229
    m_isFinalizing = true;

    clearMediaPlayerAndAudioSourceProviderClientWithoutLocking();
}
#endif

void HTMLMediaElement::didMoveToNewDocument(Document& oldDocument)
{
    WTF_LOG(Media, "HTMLMediaElement::didMoveToNewDocument(%p)", this);

    if (m_shouldDelayLoadEvent) {
        document().incrementLoadEventDelayCount();
        // Note: Keeping the load event delay count increment on oldDocument that was added
        // when m_shouldDelayLoadEvent was set so that destruction of m_webMediaPlayer can not
        // cause load event dispatching in oldDocument.
    } else {
        // Incrementing the load event delay count so that destruction of m_webMediaPlayer can not
        // cause load event dispatching in oldDocument.
        oldDocument.incrementLoadEventDelayCount();
    }

    removeElementFromDocumentMap(this, &oldDocument);
    addElementToDocumentMap(this, &document());

    // FIXME: This is a temporary fix to prevent this object from causing the
    // MediaPlayer to dereference LocalFrame and FrameLoader pointers from the
    // previous document. This restarts the load, as if the src attribute had been set.
    // A proper fix would provide a mechanism to allow this object to refresh
    // the MediaPlayer's LocalFrame and FrameLoader references on
    // document changes so that playback can be resumed properly.
    clearMediaPlayer(LoadMediaResource);
    scheduleDelayedAction(LoadMediaResource);

    // Decrement the load event delay count on oldDocument now that m_webMediaPlayer has been destroyed
    // and there is no risk of dispatching a load event from within the destructor.
    oldDocument.decrementLoadEventDelayCount();

    ActiveDOMObject::didMoveToNewExecutionContext(&document());
    HTMLElement::didMoveToNewDocument(oldDocument);
}

bool HTMLMediaElement::supportsFocus() const
{
    if (ownerDocument()->isMediaDocument())
        return false;

    // If no controls specified, we should still be able to focus the element if it has tabIndex.
    return shouldShowControls() || HTMLElement::supportsFocus();
}

bool HTMLMediaElement::isMouseFocusable() const
{
    return false;
}

void HTMLMediaElement::parseAttribute(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& value)
{
    if (name == srcAttr) {
        // Trigger a reload, as long as the 'src' attribute is present.
        if (!value.isNull()) {
            clearMediaPlayer(LoadMediaResource);
            scheduleDelayedAction(LoadMediaResource);
        }
    } else if (name == controlsAttr) {
        configureMediaControls();
    } else if (name == preloadAttr) {
        setPlayerPreload();
    } else if (name == disableremoteplaybackAttr) {
        UseCounter::count(document(), UseCounter::DisableRemotePlaybackAttribute);
    } else {
        HTMLElement::parseAttribute(name, oldValue, value);
    }
}

void HTMLMediaElement::finishParsingChildren()
{
    HTMLElement::finishParsingChildren();

    if (Traversal<HTMLTrackElement>::firstChild(*this))
        scheduleDelayedAction(LoadTextTrackResource);
}

bool HTMLMediaElement::layoutObjectIsNeeded(const ComputedStyle& style)
{
    return shouldShowControls() && HTMLElement::layoutObjectIsNeeded(style);
}

LayoutObject* HTMLMediaElement::createLayoutObject(const ComputedStyle&)
{
    return new LayoutMedia(this);
}

Node::InsertionNotificationRequest HTMLMediaElement::insertedInto(ContainerNode* insertionPoint)
{
    WTF_LOG(Media, "HTMLMediaElement::insertedInto(%p, %p)", this, insertionPoint);

    HTMLElement::insertedInto(insertionPoint);
    if (insertionPoint->inDocument()) {
        if (!getAttribute(srcAttr).isEmpty() && m_networkState == NETWORK_EMPTY)
            scheduleDelayedAction(LoadMediaResource);
    }

    return InsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLMediaElement::didNotifySubtreeInsertionsToDocument()
{
    configureMediaControls();
}

void HTMLMediaElement::removedFrom(ContainerNode* insertionPoint)
{
    WTF_LOG(Media, "HTMLMediaElement::removedFrom(%p, %p)", this, insertionPoint);

    HTMLElement::removedFrom(insertionPoint);
    if (insertionPoint->inActiveDocument()) {
        configureMediaControls();
        if (m_networkState > NETWORK_EMPTY)
            pauseInternal();
    }
}

void HTMLMediaElement::attach(const AttachContext& context)
{
    HTMLElement::attach(context);

    if (layoutObject())
        layoutObject()->updateFromElement();
}

void HTMLMediaElement::didRecalcStyle(StyleRecalcChange)
{
    if (layoutObject())
        layoutObject()->updateFromElement();
}

void HTMLMediaElement::scheduleDelayedAction(DelayedActionType actionType)
{
    WTF_LOG(Media, "HTMLMediaElement::scheduleDelayedAction(%p)", this);

    if ((actionType & LoadMediaResource) && !(m_pendingActionFlags & LoadMediaResource)) {
        prepareForLoad();
        m_pendingActionFlags |= LoadMediaResource;
    }

    if (actionType & LoadTextTrackResource)
        m_pendingActionFlags |= LoadTextTrackResource;

    if (!m_loadTimer.isActive())
        m_loadTimer.startOneShot(0, BLINK_FROM_HERE);
}

void HTMLMediaElement::scheduleNextSourceChild()
{
    // Schedule the timer to try the next <source> element WITHOUT resetting state ala prepareForLoad.
    m_pendingActionFlags |= LoadMediaResource;
    m_loadTimer.startOneShot(0, BLINK_FROM_HERE);
}

void HTMLMediaElement::scheduleEvent(const AtomicString& eventName)
{
    scheduleEvent(Event::createCancelable(eventName));
}

void HTMLMediaElement::scheduleEvent(PassRefPtrWillBeRawPtr<Event> event)
{
#if LOG_MEDIA_EVENTS
    WTF_LOG(Media, "HTMLMediaElement::scheduleEvent(%p) - scheduling '%s'", this, event->type().ascii().data());
#endif
    m_asyncEventQueue->enqueueEvent(event);
}

void HTMLMediaElement::loadTimerFired(Timer<HTMLMediaElement>*)
{
    if (m_pendingActionFlags & LoadTextTrackResource)
        honorUserPreferencesForAutomaticTextTrackSelection();

    if (m_pendingActionFlags & LoadMediaResource) {
        if (m_loadState == LoadingFromSourceElement)
            loadNextSourceChild();
        else
            loadInternal();
    }

    m_pendingActionFlags = 0;
}

MediaError* HTMLMediaElement::error() const
{
    return m_error;
}

void HTMLMediaElement::setSrc(const AtomicString& url)
{
    setAttribute(srcAttr, url);
}

HTMLMediaElement::NetworkState HTMLMediaElement::networkState() const
{
    return m_networkState;
}

String HTMLMediaElement::canPlayType(const String& mimeType, const String& keySystem) const
{
    WebMimeRegistry::SupportsType support = supportsType(ContentType(mimeType), keySystem);
    String canPlay;

    // 4.8.10.3
    switch (support) {
    case WebMimeRegistry::IsNotSupported:
        canPlay = emptyString();
        break;
    case WebMimeRegistry::MayBeSupported:
        canPlay = "maybe";
        break;
    case WebMimeRegistry::IsSupported:
        canPlay = "probably";
        break;
    }

    WTF_LOG(Media, "HTMLMediaElement::canPlayType(%p, %s, %s) -> %s", this, mimeType.utf8().data(), keySystem.utf8().data(), canPlay.utf8().data());

    return canPlay;
}

void HTMLMediaElement::recordMetricsIfPausing()
{
    // If not playing, then nothing to record.
    // TODO(liberato): test metrics.  this was m_paused.
    if (m_paused)
        return;

    const bool bailout = isBailout();

    // Record that play was paused.  We don't care if it was autoplay,
    // play(), or the user manually started it.
    recordAutoplayMetric(AnyPlaybackPaused);
    if (bailout)
        recordAutoplayMetric(AnyPlaybackBailout);

    // If this was a gestureless play, then record that separately.
    // These cover attr and play() gestureless starts.
    if (m_initialPlayWithoutUserGesture) {
        m_initialPlayWithoutUserGesture = false;

        recordAutoplayMetric(AutoplayPaused);

        if (bailout)
            recordAutoplayMetric(AutoplayBailout);
    }
}

void HTMLMediaElement::load()
{
    WTF_LOG(Media, "HTMLMediaElement::load(%p)", this);

    recordMetricsIfPausing();

    if (UserGestureIndicator::processingUserGesture() && m_userGestureRequiredForPlay) {
        recordAutoplayMetric(AutoplayEnabledThroughLoad);
        m_userGestureRequiredForPlay = false;
        // While usergesture-initiated load()s technically count as autoplayed,
        // they don't feel like such to the users and hence we don't want to
        // count them for the purposes of metrics.
        m_autoplayMediaCounted = true;
    }

    prepareForLoad();
    loadInternal();
    prepareToPlay();
}

void HTMLMediaElement::prepareForLoad()
{
    WTF_LOG(Media, "HTMLMediaElement::prepareForLoad(%p)", this);

    // Perform the cleanup required for the resource load algorithm to run.
    stopPeriodicTimers();
    m_loadTimer.stop();
    cancelDeferredLoad();
    // FIXME: Figure out appropriate place to reset LoadTextTrackResource if necessary and set m_pendingActionFlags to 0 here.
    m_pendingActionFlags &= ~LoadMediaResource;
    m_sentEndEvent = false;
    m_sentStalledEvent = false;
    m_haveFiredLoadedData = false;
    m_completelyLoaded = false;
    m_havePreparedToPlay = false;
    m_displayMode = Unknown;

    // 1 - Abort any already-running instance of the resource selection algorithm for this element.
    m_loadState = WaitingForSource;
    m_currentSourceNode = nullptr;

    // 2 - If there are any tasks from the media element's media element event task source in
    // one of the task queues, then remove those tasks.
    cancelPendingEventsAndCallbacks();

    // 3 - If the media element's networkState is set to NETWORK_LOADING or NETWORK_IDLE, queue
    // a task to fire a simple event named abort at the media element.
    if (m_networkState == NETWORK_LOADING || m_networkState == NETWORK_IDLE)
        scheduleEvent(EventTypeNames::abort);

    resetMediaPlayerAndMediaSource();

    // 4 - If the media element's networkState is not set to NETWORK_EMPTY, then run these substeps
    if (m_networkState != NETWORK_EMPTY) {
        // 4.1 - Queue a task to fire a simple event named emptied at the media element.
        scheduleEvent(EventTypeNames::emptied);

        // 4.2 - If a fetching process is in progress for the media element, the user agent should stop it.
        setNetworkState(NETWORK_EMPTY);

        // 4.3 - Forget the media element's media-resource-specific tracks.
        forgetResourceSpecificTracks();

        // 4.4 - If readyState is not set to HAVE_NOTHING, then set it to that state.
        m_readyState = HAVE_NOTHING;
        m_readyStateMaximum = HAVE_NOTHING;

        // 4.5 - If the paused attribute is false, then set it to true.
        m_paused = true;

        // 4.6 - If seeking is true, set it to false.
        m_seeking = false;

        // 4.7 - Set the current playback position to 0.
        //       Set the official playback position to 0.
        //       If this changed the official playback position, then queue a task to fire a simple event named timeupdate at the media element.
        // FIXME: Add support for firing this event.

        // 4.8 - Set the initial playback position to 0.
        // FIXME: Make this less subtle. The position only becomes 0 because the ready state is HAVE_NOTHING.
        invalidateCachedTime();

        // 4.9 - Set the timeline offset to Not-a-Number (NaN).
        // 4.10 - Update the duration attribute to Not-a-Number (NaN).

        cueTimeline().updateActiveCues(0);
    }

    // 5 - Set the playbackRate attribute to the value of the defaultPlaybackRate attribute.
    setPlaybackRate(defaultPlaybackRate());

    // 6 - Set the error attribute to null and the autoplaying flag to true.
    m_error = nullptr;
    m_autoplaying = true;

    // 7 - Invoke the media element's resource selection algorithm.

    // 8 - Note: Playback of any previously playing media resource for this element stops.

    // The resource selection algorithm
    // 1 - Set the networkState to NETWORK_NO_SOURCE
    setNetworkState(NETWORK_NO_SOURCE);

    // 2 - Asynchronously await a stable state.

    m_playedTimeRanges = TimeRanges::create();

    // FIXME: Investigate whether these can be moved into m_networkState != NETWORK_EMPTY block above
    // so they are closer to the relevant spec steps.
    m_lastSeekTime = 0;
    m_duration = std::numeric_limits<double>::quiet_NaN();

    // The spec doesn't say to block the load event until we actually run the asynchronous section
    // algorithm, but do it now because we won't start that until after the timer fires and the
    // event may have already fired by then.
    setShouldDelayLoadEvent(true);
    if (mediaControls())
        mediaControls()->reset();
}

void HTMLMediaElement::loadInternal()
{
    // HTMLMediaElement::textTracksAreReady will need "... the text tracks whose mode was not in the
    // disabled state when the element's resource selection algorithm last started".
    m_textTracksWhenResourceSelectionBegan.clear();
    if (m_textTracks) {
        for (unsigned i = 0; i < m_textTracks->length(); ++i) {
            TextTrack* track = m_textTracks->anonymousIndexedGetter(i);
            if (track->mode() != TextTrack::disabledKeyword())
                m_textTracksWhenResourceSelectionBegan.append(track);
        }
    }

    selectMediaResource();
}

void HTMLMediaElement::selectMediaResource()
{
    WTF_LOG(Media, "HTMLMediaElement::selectMediaResource(%p)", this);

    enum Mode { attribute, children };

    // 3 - If the media element has a src attribute, then let mode be attribute.
    Mode mode = attribute;
    if (!fastHasAttribute(srcAttr)) {
        // Otherwise, if the media element does not have a src attribute but has a source
        // element child, then let mode be children and let candidate be the first such
        // source element child in tree order.
        if (HTMLSourceElement* element = Traversal<HTMLSourceElement>::firstChild(*this)) {
            mode = children;
            m_nextChildNodeToConsider = element;
            m_currentSourceNode = nullptr;
        } else {
            // Otherwise the media element has neither a src attribute nor a source element
            // child: set the networkState to NETWORK_EMPTY, and abort these steps; the
            // synchronous section ends.
            m_loadState = WaitingForSource;
            setShouldDelayLoadEvent(false);
            setNetworkState(NETWORK_EMPTY);
            updateDisplayState();

            WTF_LOG(Media, "HTMLMediaElement::selectMediaResource(%p), nothing to load", this);
            return;
        }
    }

    // 4 - Set the media element's delaying-the-load-event flag to true (this delays the load event),
    // and set its networkState to NETWORK_LOADING.
    setShouldDelayLoadEvent(true);
    setNetworkState(NETWORK_LOADING);

    // 5 - Queue a task to fire a simple event named loadstart at the media element.
    scheduleEvent(EventTypeNames::loadstart);

    // 6 - If mode is attribute, then run these substeps
    if (mode == attribute) {
        m_loadState = LoadingFromSrcAttr;

        // If the src attribute's value is the empty string ... jump down to the failed step below
        KURL mediaURL = getNonEmptyURLAttribute(srcAttr);
        if (mediaURL.isEmpty()) {
            mediaLoadingFailed(WebMediaPlayer::NetworkStateFormatError);
            WTF_LOG(Media, "HTMLMediaElement::selectMediaResource(%p), empty 'src'", this);
            return;
        }

        if (!isSafeToLoadURL(mediaURL, Complain)) {
            mediaLoadingFailed(WebMediaPlayer::NetworkStateFormatError);
            return;
        }

        // No type or key system information is available when the url comes
        // from the 'src' attribute so MediaPlayer
        // will have to pick a media engine based on the file extension.
        ContentType contentType((String()));
        loadResource(mediaURL, contentType, String());
        WTF_LOG(Media, "HTMLMediaElement::selectMediaResource(%p), using 'src' attribute url", this);
        return;
    }

    // Otherwise, the source elements will be used
    loadNextSourceChild();
}

void HTMLMediaElement::loadNextSourceChild()
{
    ContentType contentType((String()));
    String keySystem;
    KURL mediaURL = selectNextSourceChild(&contentType, &keySystem, Complain);
    if (!mediaURL.isValid()) {
        waitForSourceChange();
        return;
    }

    // Reset the MediaPlayer and MediaSource if any
    resetMediaPlayerAndMediaSource();

    m_loadState = LoadingFromSourceElement;
    loadResource(mediaURL, contentType, keySystem);
}

void HTMLMediaElement::loadResource(const KURL& url, ContentType& contentType, const String& keySystem)
{
    ASSERT(isMainThread());
    ASSERT(isSafeToLoadURL(url, Complain));

    WTF_LOG(Media, "HTMLMediaElement::loadResource(%p, %s, %s, %s)", this, urlForLoggingMedia(url).utf8().data(), contentType.raw().utf8().data(), keySystem.utf8().data());

    LocalFrame* frame = document().frame();
    if (!frame) {
        mediaLoadingFailed(WebMediaPlayer::NetworkStateFormatError);
        return;
    }

    // The resource fetch algorithm
    setNetworkState(NETWORK_LOADING);

    // Set m_currentSrc *before* changing to the cache url, the fact that we are loading from the app
    // cache is an internal detail not exposed through the media element API.
    m_currentSrc = url;

    if (m_audioSourceNode)
        m_audioSourceNode->onCurrentSrcChanged(m_currentSrc);

    WTF_LOG(Media, "HTMLMediaElement::loadResource(%p) - m_currentSrc -> %s", this, urlForLoggingMedia(m_currentSrc).utf8().data());

    startProgressEventTimer();

    // Reset display mode to force a recalculation of what to show because we are resetting the player.
    setDisplayMode(Unknown);

    setPlayerPreload();

    if (fastHasAttribute(mutedAttr))
        m_muted = true;
    updateVolume();

    ASSERT(!m_mediaSource);

    bool attemptLoad = true;

    if (url.protocolIs(mediaSourceBlobProtocol)) {
        if (isMediaStreamURL(url.string())) {
            m_userGestureRequiredForPlay = false;
        } else {
            m_mediaSource = HTMLMediaSource::lookup(url.string());

            if (m_mediaSource) {
                if (!m_mediaSource->attachToElement(this)) {
                    // Forget our reference to the MediaSource, so we leave it alone
                    // while processing remainder of load failure.
                    m_mediaSource = nullptr;
                    attemptLoad = false;
                }
            }
        }
    }

    if (attemptLoad && canLoadURL(url, contentType, keySystem)) {
        ASSERT(!webMediaPlayer());

        if (!m_havePreparedToPlay && !autoplay() && preloadType() == WebMediaPlayer::PreloadNone) {
            WTF_LOG(Media, "HTMLMediaElement::loadResource(%p) : Delaying load because preload == 'none'", this);
            deferLoad();
        } else {
            startPlayerLoad();
        }
    } else {
        mediaLoadingFailed(WebMediaPlayer::NetworkStateFormatError);
    }

    // If there is no poster to display, allow the media engine to render video frames as soon as
    // they are available.
    updateDisplayState();

    if (layoutObject())
        layoutObject()->updateFromElement();
}

void HTMLMediaElement::startPlayerLoad()
{
    ASSERT(!m_webMediaPlayer);
    // Filter out user:pass as those two URL components aren't
    // considered for media resource fetches (including for the CORS
    // use-credentials mode.) That behavior aligns with Gecko, with IE
    // being more restrictive and not allowing fetches to such URLs.
    //
    // Spec reference: http://whatwg.org/c/#concept-media-load-resource
    //
    // FIXME: when the HTML spec switches to specifying resource
    // fetches in terms of Fetch (http://fetch.spec.whatwg.org), and
    // along with that potentially also specifying a setting for its
    // 'authentication flag' to control how user:pass embedded in a
    // media resource URL should be treated, then update the handling
    // here to match.
    KURL requestURL = m_currentSrc;
    if (!requestURL.user().isEmpty())
        requestURL.setUser(String());
    if (!requestURL.pass().isEmpty())
        requestURL.setPass(String());

    LocalFrame* frame = document().frame();
    // TODO(srirama.m): Figure out how frame can be null when
    // coming from executeDeferredLoad()
    if (!frame) {
        mediaLoadingFailed(WebMediaPlayer::NetworkStateFormatError);
        return;
    }

    KURL kurl(ParsedURLString, requestURL);
    m_webMediaPlayer = frame->loader().client()->createWebMediaPlayer(*this, kurl, this);
    if (!m_webMediaPlayer) {
        mediaLoadingFailed(WebMediaPlayer::NetworkStateFormatError);
        return;
    }

    if (layoutObject())
        layoutObject()->setShouldDoFullPaintInvalidation();
    // Make sure if we create/re-create the WebMediaPlayer that we update our wrapper.
    m_audioSourceProvider.wrap(m_webMediaPlayer->audioSourceProvider());
    m_webMediaPlayer->setVolume(effectiveMediaVolume());

    m_webMediaPlayer->setPoster(posterImageURL());

    m_webMediaPlayer->setPreload(effectivePreloadType());

    m_webMediaPlayer->load(loadType(), kurl, corsMode());

    if (isFullscreen()) {
        // This handles any transition to or from fullscreen overlay mode.
        frame->chromeClient().enterFullScreenForElement(this);
    }
}

void HTMLMediaElement::setPlayerPreload()
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->setPreload(effectivePreloadType());

    if (loadIsDeferred() && preloadType() != WebMediaPlayer::PreloadNone)
        startDeferredLoad();
}

bool HTMLMediaElement::loadIsDeferred() const
{
    return m_deferredLoadState != NotDeferred;
}

void HTMLMediaElement::deferLoad()
{
    // This implements the "optional" step 3 from the resource fetch algorithm.
    ASSERT(!m_deferredLoadTimer.isActive());
    ASSERT(m_deferredLoadState == NotDeferred);
    // 1. Set the networkState to NETWORK_IDLE.
    // 2. Queue a task to fire a simple event named suspend at the element.
    changeNetworkStateFromLoadingToIdle();
    // 3. Queue a task to set the element's delaying-the-load-event
    // flag to false. This stops delaying the load event.
    m_deferredLoadTimer.startOneShot(0, BLINK_FROM_HERE);
    // 4. Wait for the task to be run.
    m_deferredLoadState = WaitingForStopDelayingLoadEventTask;
    // Continued in executeDeferredLoad().
}

void HTMLMediaElement::cancelDeferredLoad()
{
    m_deferredLoadTimer.stop();
    m_deferredLoadState = NotDeferred;
}

void HTMLMediaElement::executeDeferredLoad()
{
    ASSERT(m_deferredLoadState >= WaitingForTrigger);

    // resource fetch algorithm step 3 - continued from deferLoad().

    // 5. Wait for an implementation-defined event (e.g. the user requesting that the media element begin playback).
    // This is assumed to be whatever 'event' ended up calling this method.
    cancelDeferredLoad();
    // 6. Set the element's delaying-the-load-event flag back to true (this
    // delays the load event again, in case it hasn't been fired yet).
    setShouldDelayLoadEvent(true);
    // 7. Set the networkState to NETWORK_LOADING.
    setNetworkState(NETWORK_LOADING);

    startProgressEventTimer();

    startPlayerLoad();
}

void HTMLMediaElement::startDeferredLoad()
{
    if (m_deferredLoadState == WaitingForTrigger) {
        executeDeferredLoad();
        return;
    }
    if (m_deferredLoadState == ExecuteOnStopDelayingLoadEventTask)
        return;
    ASSERT(m_deferredLoadState == WaitingForStopDelayingLoadEventTask);
    m_deferredLoadState = ExecuteOnStopDelayingLoadEventTask;
}

void HTMLMediaElement::deferredLoadTimerFired(Timer<HTMLMediaElement>*)
{
    setShouldDelayLoadEvent(false);

    if (m_deferredLoadState == ExecuteOnStopDelayingLoadEventTask) {
        executeDeferredLoad();
        return;
    }
    ASSERT(m_deferredLoadState == WaitingForStopDelayingLoadEventTask);
    m_deferredLoadState = WaitingForTrigger;
}

WebMediaPlayer::LoadType HTMLMediaElement::loadType() const
{
    if (m_mediaSource)
        return WebMediaPlayer::LoadTypeMediaSource;

    if (isMediaStreamURL(m_currentSrc.string()))
        return WebMediaPlayer::LoadTypeMediaStream;

    return WebMediaPlayer::LoadTypeURL;
}

bool HTMLMediaElement::textTracksAreReady() const
{
    // 4.8.10.12.1 Text track model
    // ...
    // The text tracks of a media element are ready if all the text tracks whose mode was not
    // in the disabled state when the element's resource selection algorithm last started now
    // have a text track readiness state of loaded or failed to load.
    for (unsigned i = 0; i < m_textTracksWhenResourceSelectionBegan.size(); ++i) {
        if (m_textTracksWhenResourceSelectionBegan[i]->readinessState() == TextTrack::Loading
            || m_textTracksWhenResourceSelectionBegan[i]->readinessState() == TextTrack::NotLoaded)
            return false;
    }

    return true;
}

void HTMLMediaElement::textTrackReadyStateChanged(TextTrack* track)
{
    if (webMediaPlayer()&& m_textTracksWhenResourceSelectionBegan.contains(track)) {
        if (track->readinessState() != TextTrack::Loading)
            setReadyState(static_cast<ReadyState>(webMediaPlayer()->readyState()));
    } else {
        // The track readiness state might have changed as a result of the user
        // clicking the captions button. In this case, a check whether all the
        // resources have failed loading should be done in order to hide the CC button.
        if (mediaControls() && track->readinessState() == TextTrack::FailedToLoad)
            mediaControls()->refreshClosedCaptionsButtonVisibility();
    }
}

void HTMLMediaElement::textTrackModeChanged(TextTrack* track)
{
    // Mark this track as "configured" so configureTextTracks won't change the mode again.
    if (track->trackType() == TextTrack::TrackElement)
        track->setHasBeenConfigured(true);

    configureTextTrackDisplay();

    ASSERT(textTracks()->contains(track));
    textTracks()->scheduleChangeEvent();
}

bool HTMLMediaElement::isSafeToLoadURL(const KURL& url, InvalidURLAction actionIfInvalid)
{
    if (!url.isValid()) {
        WTF_LOG(Media, "HTMLMediaElement::isSafeToLoadURL(%p, %s) -> FALSE because url is invalid", this, urlForLoggingMedia(url).utf8().data());
        return false;
    }

    LocalFrame* frame = document().frame();
    if (!frame || !document().securityOrigin()->canDisplay(url)) {
        if (actionIfInvalid == Complain)
            FrameLoader::reportLocalLoadFailed(frame, url.elidedString());
        WTF_LOG(Media, "HTMLMediaElement::isSafeToLoadURL(%p, %s) -> FALSE rejected by SecurityOrigin", this, urlForLoggingMedia(url).utf8().data());
        return false;
    }

    if (!document().contentSecurityPolicy()->allowMediaFromSource(url)) {
        WTF_LOG(Media, "HTMLMediaElement::isSafeToLoadURL(%p, %s) -> rejected by Content Security Policy", this, urlForLoggingMedia(url).utf8().data());
        return false;
    }

    return true;
}

bool HTMLMediaElement::isMediaDataCORSSameOrigin(SecurityOrigin* origin) const
{
    // hasSingleSecurityOrigin() tells us whether the origin in the src is
    // the same as the actual request (i.e. after redirect).
    // didPassCORSAccessCheck() means it was a successful CORS-enabled fetch
    // (vs. non-CORS-enabled or failed).
    // taintsCanvas() does checkAccess() on the URL plus allow data sources,
    // to ensure that it is not a URL that requires CORS (basically same
    // origin).
    return hasSingleSecurityOrigin() && ((webMediaPlayer() && webMediaPlayer()->didPassCORSAccessCheck()) || !origin->taintsCanvas(currentSrc()));
}

void HTMLMediaElement::startProgressEventTimer()
{
    if (m_progressEventTimer.isActive())
        return;

    m_previousProgressTime = WTF::currentTime();
    // 350ms is not magic, it is in the spec!
    m_progressEventTimer.startRepeating(0.350, BLINK_FROM_HERE);
}

void HTMLMediaElement::waitForSourceChange()
{
    WTF_LOG(Media, "HTMLMediaElement::waitForSourceChange(%p)", this);

    stopPeriodicTimers();
    m_loadState = WaitingForSource;

    // 6.17 - Waiting: Set the element's networkState attribute to the NETWORK_NO_SOURCE value
    setNetworkState(NETWORK_NO_SOURCE);

    // 6.18 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
    setShouldDelayLoadEvent(false);

    updateDisplayState();

    if (layoutObject())
        layoutObject()->updateFromElement();
}

void HTMLMediaElement::noneSupported()
{
    WTF_LOG(Media, "HTMLMediaElement::noneSupported(%p)", this);

    stopPeriodicTimers();
    m_loadState = WaitingForSource;
    m_currentSourceNode = nullptr;

    // 4.8.10.5
    // 6 - Reaching this step indicates that the media resource failed to load or that the given
    // URL could not be resolved. In one atomic operation, run the following steps:

    // 6.1 - Set the error attribute to a new MediaError object whose code attribute is set to
    // MEDIA_ERR_SRC_NOT_SUPPORTED.
    m_error = MediaError::create(MediaError::MEDIA_ERR_SRC_NOT_SUPPORTED);

    // 6.2 - Forget the media element's media-resource-specific text tracks.
    forgetResourceSpecificTracks();

    // 6.3 - Set the element's networkState attribute to the NETWORK_NO_SOURCE value.
    setNetworkState(NETWORK_NO_SOURCE);

    // 7 - Queue a task to fire a simple event named error at the media element.
    scheduleEvent(EventTypeNames::error);

    closeMediaSource();

    // 8 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
    setShouldDelayLoadEvent(false);

    // 9 - Abort these steps. Until the load() method is invoked or the src attribute is changed,
    // the element won't attempt to load another resource.

    updateDisplayState();

    if (layoutObject())
        layoutObject()->updateFromElement();
}

void HTMLMediaElement::mediaEngineError(MediaError* err)
{
    ASSERT(m_readyState >= HAVE_METADATA);
    WTF_LOG(Media, "HTMLMediaElement::mediaEngineError(%p, %d)", this, static_cast<int>(err->code()));

    // 1 - The user agent should cancel the fetching process.
    stopPeriodicTimers();
    m_loadState = WaitingForSource;

    // 2 - Set the error attribute to a new MediaError object whose code attribute is
    // set to MEDIA_ERR_NETWORK/MEDIA_ERR_DECODE.
    m_error = err;

    // 3 - Queue a task to fire a simple event named error at the media element.
    scheduleEvent(EventTypeNames::error);

    // 4 - Set the element's networkState attribute to the NETWORK_IDLE value.
    setNetworkState(NETWORK_IDLE);

    // 5 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
    setShouldDelayLoadEvent(false);

    // 6 - Abort the overall resource selection algorithm.
    m_currentSourceNode = nullptr;
}

void HTMLMediaElement::cancelPendingEventsAndCallbacks()
{
    WTF_LOG(Media, "HTMLMediaElement::cancelPendingEventsAndCallbacks(%p)", this);
    m_asyncEventQueue->cancelAllEvents();

    for (HTMLSourceElement* source = Traversal<HTMLSourceElement>::firstChild(*this); source; source = Traversal<HTMLSourceElement>::nextSibling(*source))
        source->cancelPendingErrorEvent();
}

void HTMLMediaElement::networkStateChanged()
{
    setNetworkState(webMediaPlayer()->networkState());
}

void HTMLMediaElement::mediaLoadingFailed(WebMediaPlayer::NetworkState error)
{
    stopPeriodicTimers();

    // If we failed while trying to load a <source> element, the movie was never parsed, and there are more
    // <source> children, schedule the next one
    if (m_readyState < HAVE_METADATA && m_loadState == LoadingFromSourceElement) {

        // resource selection algorithm
        // Step 9.Otherwise.9 - Failed with elements: Queue a task, using the DOM manipulation task source, to fire a simple event named error at the candidate element.
        if (m_currentSourceNode)
            m_currentSourceNode->scheduleErrorEvent();
        else
            WTF_LOG(Media, "HTMLMediaElement::setNetworkState(%p) - error event not sent, <source> was removed", this);

        // 9.Otherwise.10 - Asynchronously await a stable state. The synchronous section consists of all the remaining steps of this algorithm until the algorithm says the synchronous section has ended.

        // 9.Otherwise.11 - Forget the media element's media-resource-specific tracks.
        forgetResourceSpecificTracks();

        if (havePotentialSourceChild()) {
            WTF_LOG(Media, "HTMLMediaElement::setNetworkState(%p) - scheduling next <source>", this);
            scheduleNextSourceChild();
        } else {
            WTF_LOG(Media, "HTMLMediaElement::setNetworkState(%p) - no more <source> elements, waiting", this);
            waitForSourceChange();
        }

        return;
    }

    if (error == WebMediaPlayer::NetworkStateNetworkError && m_readyState >= HAVE_METADATA)
        mediaEngineError(MediaError::create(MediaError::MEDIA_ERR_NETWORK));
    else if (error == WebMediaPlayer::NetworkStateDecodeError)
        mediaEngineError(MediaError::create(MediaError::MEDIA_ERR_DECODE));
    else if ((error == WebMediaPlayer::NetworkStateFormatError
        || error == WebMediaPlayer::NetworkStateNetworkError)
        && m_loadState == LoadingFromSrcAttr)
        noneSupported();

    updateDisplayState();
    if (mediaControls())
        mediaControls()->reset();
}

void HTMLMediaElement::setNetworkState(WebMediaPlayer::NetworkState state)
{
    WTF_LOG(Media, "HTMLMediaElement::setNetworkState(%p, %d) - current state is %d", this, static_cast<int>(state), static_cast<int>(m_networkState));

    if (state == WebMediaPlayer::NetworkStateEmpty) {
        // Just update the cached state and leave, we can't do anything.
        setNetworkState(NETWORK_EMPTY);
        return;
    }

    if (state == WebMediaPlayer::NetworkStateFormatError
        || state == WebMediaPlayer::NetworkStateNetworkError
        || state == WebMediaPlayer::NetworkStateDecodeError) {
        mediaLoadingFailed(state);
        return;
    }

    if (state == WebMediaPlayer::NetworkStateIdle) {
        if (m_networkState > NETWORK_IDLE) {
            changeNetworkStateFromLoadingToIdle();
            setShouldDelayLoadEvent(false);
        } else {
            setNetworkState(NETWORK_IDLE);
        }
    }

    if (state == WebMediaPlayer::NetworkStateLoading) {
        if (m_networkState < NETWORK_LOADING || m_networkState == NETWORK_NO_SOURCE)
            startProgressEventTimer();
        setNetworkState(NETWORK_LOADING);
        m_completelyLoaded = false;
    }

    if (state == WebMediaPlayer::NetworkStateLoaded) {
        if (m_networkState != NETWORK_IDLE)
            changeNetworkStateFromLoadingToIdle();
        m_completelyLoaded = true;
    }
}

void HTMLMediaElement::changeNetworkStateFromLoadingToIdle()
{
    m_progressEventTimer.stop();

    // Schedule one last progress event so we guarantee that at least one is fired
    // for files that load very quickly.
    if (webMediaPlayer() && webMediaPlayer()->didLoadingProgress())
        scheduleEvent(EventTypeNames::progress);
    scheduleEvent(EventTypeNames::suspend);
    setNetworkState(NETWORK_IDLE);
}

void HTMLMediaElement::readyStateChanged()
{
    setReadyState(static_cast<ReadyState>(webMediaPlayer()->readyState()));
}

void HTMLMediaElement::setReadyState(ReadyState state)
{
    WTF_LOG(Media, "HTMLMediaElement::setReadyState(%p, %d) - current state is %d,", this, static_cast<int>(state), static_cast<int>(m_readyState));

    // Set "wasPotentiallyPlaying" BEFORE updating m_readyState, potentiallyPlaying() uses it
    bool wasPotentiallyPlaying = potentiallyPlaying();

    ReadyState oldState = m_readyState;
    ReadyState newState = state;

    bool tracksAreReady = textTracksAreReady();

    if (newState == oldState && m_tracksAreReady == tracksAreReady)
        return;

    m_tracksAreReady = tracksAreReady;

    if (tracksAreReady) {
        m_readyState = newState;
    } else {
        // If a media file has text tracks the readyState may not progress beyond HAVE_FUTURE_DATA until
        // the text tracks are ready, regardless of the state of the media file.
        if (newState <= HAVE_METADATA)
            m_readyState = newState;
        else
            m_readyState = HAVE_CURRENT_DATA;
    }

    if (oldState > m_readyStateMaximum)
        m_readyStateMaximum = oldState;

    if (m_networkState == NETWORK_EMPTY)
        return;

    if (m_seeking) {
        // 4.8.10.9, step 9 note: If the media element was potentially playing immediately before
        // it started seeking, but seeking caused its readyState attribute to change to a value
        // lower than HAVE_FUTURE_DATA, then a waiting will be fired at the element.
        if (wasPotentiallyPlaying && m_readyState < HAVE_FUTURE_DATA)
            scheduleEvent(EventTypeNames::waiting);

        // 4.8.10.9 steps 12-14
        if (m_readyState >= HAVE_CURRENT_DATA)
            finishSeek();
    } else {
        if (wasPotentiallyPlaying && m_readyState < HAVE_FUTURE_DATA) {
            // 4.8.10.8
            scheduleTimeupdateEvent(false);
            scheduleEvent(EventTypeNames::waiting);
        }
    }

    if (m_readyState >= HAVE_METADATA && oldState < HAVE_METADATA) {
        createPlaceholderTracksIfNecessary();

        selectInitialTracksIfNecessary();

        MediaFragmentURIParser fragmentParser(m_currentSrc);
        m_fragmentEndTime = fragmentParser.endTime();

        m_duration = duration();
        scheduleEvent(EventTypeNames::durationchange);

        if (isHTMLVideoElement())
            scheduleEvent(EventTypeNames::resize);
        scheduleEvent(EventTypeNames::loadedmetadata);

        bool jumped = false;
        if (m_defaultPlaybackStartPosition > 0) {
            seek(m_defaultPlaybackStartPosition);
            jumped = true;
        }
        m_defaultPlaybackStartPosition = 0;

        double initialPlaybackPosition = fragmentParser.startTime();
        if (std::isnan(initialPlaybackPosition))
            initialPlaybackPosition = 0;

        if (!jumped && initialPlaybackPosition > 0) {
            m_sentEndEvent = false;
            UseCounter::count(document(), UseCounter::HTMLMediaElementSeekToFragmentStart);
            seek(initialPlaybackPosition);
            jumped = true;
        }

        if (mediaControls())
            mediaControls()->reset();
        if (layoutObject())
            layoutObject()->updateFromElement();
    }

    bool shouldUpdateDisplayState = false;

    if (m_readyState >= HAVE_CURRENT_DATA && oldState < HAVE_CURRENT_DATA && !m_haveFiredLoadedData) {
        m_haveFiredLoadedData = true;
        shouldUpdateDisplayState = true;
        scheduleEvent(EventTypeNames::loadeddata);
        setShouldDelayLoadEvent(false);
    }

    bool isPotentiallyPlaying = potentiallyPlaying();
    if (m_readyState == HAVE_FUTURE_DATA && oldState <= HAVE_CURRENT_DATA && tracksAreReady) {
        scheduleEvent(EventTypeNames::canplay);
        if (isPotentiallyPlaying)
            scheduleEvent(EventTypeNames::playing);
        shouldUpdateDisplayState = true;
    }

    if (m_readyState == HAVE_ENOUGH_DATA && oldState < HAVE_ENOUGH_DATA && tracksAreReady) {
        if (oldState <= HAVE_CURRENT_DATA) {
            scheduleEvent(EventTypeNames::canplay);
            if (isPotentiallyPlaying)
                scheduleEvent(EventTypeNames::playing);
        }

        // Check for autoplay, and record metrics about it if needed.
        if (shouldAutoplay(RecordMetricsBehavior::DoRecord)) {
            // If the autoplay experiment says that it's okay to play now,
            // then don't require a user gesture.
            m_autoplayHelper.becameReadyToPlay();

            if (!m_userGestureRequiredForPlay) {
                m_paused = false;
                invalidateCachedTime();
                scheduleEvent(EventTypeNames::play);
                scheduleEvent(EventTypeNames::playing);
                m_autoplaying = false;
            }
        }

        scheduleEvent(EventTypeNames::canplaythrough);

        shouldUpdateDisplayState = true;
    }

    if (shouldUpdateDisplayState) {
        updateDisplayState();
        if (mediaControls())
            mediaControls()->refreshClosedCaptionsButtonVisibility();
    }

    updatePlayState();
    cueTimeline().updateActiveCues(currentTime());
}

void HTMLMediaElement::progressEventTimerFired(Timer<HTMLMediaElement>*)
{
    if (m_networkState != NETWORK_LOADING)
        return;

    double time = WTF::currentTime();
    double timedelta = time - m_previousProgressTime;

    if (webMediaPlayer() && webMediaPlayer()->didLoadingProgress()) {
        scheduleEvent(EventTypeNames::progress);
        m_previousProgressTime = time;
        m_sentStalledEvent = false;
        if (layoutObject())
            layoutObject()->updateFromElement();
    } else if (timedelta > 3.0 && !m_sentStalledEvent) {
        scheduleEvent(EventTypeNames::stalled);
        m_sentStalledEvent = true;
        setShouldDelayLoadEvent(false);
    }
}

void HTMLMediaElement::addPlayedRange(double start, double end)
{
    WTF_LOG(Media, "HTMLMediaElement::addPlayedRange(%p, %f, %f)", this, start, end);
    if (!m_playedTimeRanges)
        m_playedTimeRanges = TimeRanges::create();
    m_playedTimeRanges->add(start, end);
}

bool HTMLMediaElement::supportsSave() const
{
    return webMediaPlayer() && webMediaPlayer()->supportsSave();
}

void HTMLMediaElement::prepareToPlay()
{
    WTF_LOG(Media, "HTMLMediaElement::prepareToPlay(%p)", this);
    if (m_havePreparedToPlay)
        return;
    m_havePreparedToPlay = true;

    if (loadIsDeferred())
        startDeferredLoad();
}

void HTMLMediaElement::seek(double time)
{
    WTF_LOG(Media, "HTMLMediaElement::seek(%p, %f)", this, time);

    // 2 - If the media element's readyState is HAVE_NOTHING, abort these steps.
    if (m_readyState == HAVE_NOTHING)
        return;

    // If the media engine has been told to postpone loading data, let it go ahead now.
    if (preloadType() < WebMediaPlayer::PreloadAuto && m_readyState < HAVE_FUTURE_DATA)
        prepareToPlay();

    // Get the current time before setting m_seeking, m_lastSeekTime is returned once it is set.
    refreshCachedTime();
    // This is needed to avoid getting default playback start position from currentTime().
    double now = m_cachedTime;

    // 3 - If the element's seeking IDL attribute is true, then another instance of this algorithm is
    // already running. Abort that other instance of the algorithm without waiting for the step that
    // it is running to complete.
    // Nothing specific to be done here.

    // 4 - Set the seeking IDL attribute to true.
    // The flag will be cleared when the engine tells us the time has actually changed.
    m_seeking = true;

    // 6 - If the new playback position is later than the end of the media resource, then let it be the end
    // of the media resource instead.
    time = std::min(time, duration());

    // 7 - If the new playback position is less than the earliest possible position, let it be that position instead.
    time = std::max(time, 0.0);

    // Ask the media engine for the time value in the movie's time scale before comparing with current time. This
    // is necessary because if the seek time is not equal to currentTime but the delta is less than the movie's
    // time scale, we will ask the media engine to "seek" to the current movie time, which may be a noop and
    // not generate a timechanged callback. This means m_seeking will never be cleared and we will never
    // fire a 'seeked' event.
    double mediaTime = webMediaPlayer()->mediaTimeForTimeValue(time);
    if (time != mediaTime) {
        WTF_LOG(Media, "HTMLMediaElement::seek(%p, %f) - media timeline equivalent is %f", this, time, mediaTime);
        time = mediaTime;
    }

    // 8 - If the (possibly now changed) new playback position is not in one of the ranges given in the
    // seekable attribute, then let it be the position in one of the ranges given in the seekable attribute
    // that is the nearest to the new playback position. ... If there are no ranges given in the seekable
    // attribute then set the seeking IDL attribute to false and abort these steps.
    TimeRanges* seekableRanges = seekable();

    if (!seekableRanges->length()) {
        m_seeking = false;
        return;
    }
    time = seekableRanges->nearest(time, now);

    if (m_playing && m_lastSeekTime < now)
        addPlayedRange(m_lastSeekTime, now);

    m_lastSeekTime = time;
    m_sentEndEvent = false;

    // 10 - Queue a task to fire a simple event named seeking at the element.
    scheduleEvent(EventTypeNames::seeking);

    // 11 - Set the current playback position to the given new playback position.
    webMediaPlayer()->seek(time);

    m_initialPlayWithoutUserGesture = false;

    // 14-17 are handled, if necessary, when the engine signals a readystate change or otherwise
    // satisfies seek completion and signals a time change.
}

void HTMLMediaElement::finishSeek()
{
    WTF_LOG(Media, "HTMLMediaElement::finishSeek(%p)", this);

    // 14 - Set the seeking IDL attribute to false.
    m_seeking = false;

    // 16 - Queue a task to fire a simple event named timeupdate at the element.
    scheduleTimeupdateEvent(false);

    // 17 - Queue a task to fire a simple event named seeked at the element.
    scheduleEvent(EventTypeNames::seeked);

    setDisplayMode(Video);
}

HTMLMediaElement::ReadyState HTMLMediaElement::readyState() const
{
    return m_readyState;
}

bool HTMLMediaElement::hasAudio() const
{
    return webMediaPlayer() && webMediaPlayer()->hasAudio();
}

bool HTMLMediaElement::seeking() const
{
    return m_seeking;
}

void HTMLMediaElement::refreshCachedTime() const
{
    if (!webMediaPlayer() || m_readyState < HAVE_METADATA)
        return;

    m_cachedTime = webMediaPlayer()->currentTime();
}

void HTMLMediaElement::invalidateCachedTime()
{
    WTF_LOG(Media, "HTMLMediaElement::invalidateCachedTime(%p)", this);
    m_cachedTime = std::numeric_limits<double>::quiet_NaN();
}

// playback state
double HTMLMediaElement::currentTime() const
{
    if (m_defaultPlaybackStartPosition)
        return m_defaultPlaybackStartPosition;

    if (m_readyState == HAVE_NOTHING)
        return 0;

    if (m_seeking) {
        WTF_LOG(Media, "HTMLMediaElement::currentTime(%p) - seeking, returning %f", this, m_lastSeekTime);
        return m_lastSeekTime;
    }

    if (!std::isnan(m_cachedTime) && m_paused) {
#if LOG_CACHED_TIME_WARNINGS
        static const double minCachedDeltaForWarning = 0.01;
        double delta = m_cachedTime - webMediaPlayer()->currentTime();
        if (delta > minCachedDeltaForWarning)
            WTF_LOG(Media, "HTMLMediaElement::currentTime(%p) - WARNING, cached time is %f seconds off of media time when paused", this, delta);
#endif
        return m_cachedTime;
    }

    refreshCachedTime();

    return m_cachedTime;
}

void HTMLMediaElement::setCurrentTime(double time)
{
    // If the media element's readyState is HAVE_NOTHING, then set the default
    // playback start position to that time.
    if (m_readyState == HAVE_NOTHING) {
        m_defaultPlaybackStartPosition = time;
        return;
    }

    seek(time);
}

double HTMLMediaElement::duration() const
{
    // FIXME: remove m_webMediaPlayer check once we figure out how
    // m_webMediaPlayer is going out of sync with readystate.
    // m_webMediaPlayer is cleared but readystate is not set to HAVE_NOTHING.
    if (!m_webMediaPlayer || m_readyState < HAVE_METADATA)
        return std::numeric_limits<double>::quiet_NaN();

    // FIXME: Refactor so m_duration is kept current (in both MSE and
    // non-MSE cases) once we have transitioned from HAVE_NOTHING ->
    // HAVE_METADATA. Currently, m_duration may be out of date for at least MSE
    // case because MediaSource and SourceBuffer do not notify the element
    // directly upon duration changes caused by endOfStream, remove, or append
    // operations; rather the notification is triggered by the WebMediaPlayer
    // implementation observing that the underlying engine has updated duration
    // and notifying the element to consult its MediaSource for current
    // duration. See http://crbug.com/266644

    if (m_mediaSource)
        return m_mediaSource->duration();

    return webMediaPlayer()->duration();
}

bool HTMLMediaElement::paused() const
{
    return m_paused;
}

double HTMLMediaElement::defaultPlaybackRate() const
{
    return m_defaultPlaybackRate;
}

void HTMLMediaElement::setDefaultPlaybackRate(double rate)
{
    if (m_defaultPlaybackRate == rate)
        return;

    m_defaultPlaybackRate = rate;
    scheduleEvent(EventTypeNames::ratechange);
}

double HTMLMediaElement::playbackRate() const
{
    return m_playbackRate;
}

void HTMLMediaElement::setPlaybackRate(double rate)
{
    WTF_LOG(Media, "HTMLMediaElement::setPlaybackRate(%p, %f)", this, rate);

    if (m_playbackRate != rate) {
        m_playbackRate = rate;
        invalidateCachedTime();
        scheduleEvent(EventTypeNames::ratechange);
    }

    updatePlaybackRate();
}

HTMLMediaElement::DirectionOfPlayback HTMLMediaElement::directionOfPlayback() const
{
    return m_playbackRate >= 0 ? Forward : Backward;
}

void HTMLMediaElement::updatePlaybackRate()
{
    // FIXME: remove m_webMediaPlayer check once we figure out how
    // m_webMediaPlayer is going out of sync with readystate.
    // m_webMediaPlayer is cleared but readystate is not set to HAVE_NOTHING.
    if (m_webMediaPlayer && potentiallyPlaying())
        webMediaPlayer()->setRate(playbackRate());
}

bool HTMLMediaElement::ended() const
{
    // 4.8.10.8 Playing the media resource
    // The ended attribute must return true if the media element has ended
    // playback and the direction of playback is forwards, and false otherwise.
    return endedPlayback() && directionOfPlayback() == Forward;
}

bool HTMLMediaElement::autoplay() const
{
    return fastHasAttribute(autoplayAttr);
}

bool HTMLMediaElement::shouldAutoplay(const RecordMetricsBehavior recordMetrics)
{
    if (m_autoplaying && m_paused && autoplay()) {
        if (recordMetrics == RecordMetricsBehavior::DoRecord)
            autoplayMediaEncountered();

        if (document().isSandboxed(SandboxAutomaticFeatures)) {
            if (recordMetrics == RecordMetricsBehavior::DoRecord)
                recordAutoplayMetric(AutoplayDisabledBySandbox);
            return false;
        }

        return true;
    }

    return false;
}

String HTMLMediaElement::preload() const
{
    switch (preloadType()) {
    case WebMediaPlayer::PreloadNone:
        return "none";
    case WebMediaPlayer::PreloadMetaData:
        return "metadata";
    case WebMediaPlayer::PreloadAuto:
        return "auto";
    }

    ASSERT_NOT_REACHED();
    return String();
}

void HTMLMediaElement::setPreload(const AtomicString& preload)
{
    WTF_LOG(Media, "HTMLMediaElement::setPreload(%p, %s)", this, preload.utf8().data());
    setAttribute(preloadAttr, preload);
}

WebMediaPlayer::Preload HTMLMediaElement::preloadType() const
{
    // Force preload to none for cellular connections.
    if (networkStateNotifier().isCellularConnectionType()) {
        UseCounter::count(document(), UseCounter::HTMLMediaElementPreloadForcedNone);
        return WebMediaPlayer::PreloadNone;
    }

    const AtomicString& preload = fastGetAttribute(preloadAttr);
    if (equalIgnoringCase(preload, "none")) {
        UseCounter::count(document(), UseCounter::HTMLMediaElementPreloadNone);
        return WebMediaPlayer::PreloadNone;
    }
    if (equalIgnoringCase(preload, "metadata")) {
        UseCounter::count(document(), UseCounter::HTMLMediaElementPreloadMetadata);
        return WebMediaPlayer::PreloadMetaData;
    }
    if (equalIgnoringCase(preload, "auto")) {
        UseCounter::count(document(), UseCounter::HTMLMediaElementPreloadAuto);
        return WebMediaPlayer::PreloadAuto;
    }

    // "The attribute's missing value default is user-agent defined, though the
    // Metadata state is suggested as a compromise between reducing server load
    // and providing an optimal user experience."

    // The spec does not define an invalid value default:
    // https://www.w3.org/Bugs/Public/show_bug.cgi?id=28950

    // TODO(philipj): Try to make "metadata" the default preload state:
    // https://crbug.com/310450
    UseCounter::count(document(), UseCounter::HTMLMediaElementPreloadDefault);
    return WebMediaPlayer::PreloadAuto;
}

WebMediaPlayer::Preload HTMLMediaElement::effectivePreloadType() const
{
    return autoplay() ? WebMediaPlayer::PreloadAuto : preloadType();
}

void HTMLMediaElement::play()
{
    WTF_LOG(Media, "HTMLMediaElement::play(%p)", this);

    m_autoplayHelper.playMethodCalled();

    if (!UserGestureIndicator::processingUserGesture()) {
        autoplayMediaEncountered();

        if (m_userGestureRequiredForPlay) {
            recordAutoplayMetric(PlayMethodFailed);
            String message = ExceptionMessages::failedToExecute("play", "HTMLMediaElement", "API can only be initiated by a user gesture.");
            document().addConsoleMessage(ConsoleMessage::create(JSMessageSource, WarningMessageLevel, message));
            return;
        }
    } else if (m_userGestureRequiredForPlay) {
        if (m_autoplayMediaCounted)
            recordAutoplayMetric(AutoplayManualStart);
        m_userGestureRequiredForPlay = false;
    }

    playInternal();
}

void HTMLMediaElement::playInternal()
{
    WTF_LOG(Media, "HTMLMediaElement::playInternal(%p)", this);

    // Always return the buffering strategy to normal when not paused,
    // regardless of the cause. (In contrast with aggressive buffering which is
    // only enabled by pause(), not pauseInternal().)
    if (webMediaPlayer())
        webMediaPlayer()->setBufferingStrategy(WebMediaPlayer::BufferingStrategy::Normal);

    // 4.8.10.9. Playing the media resource
    if (m_networkState == NETWORK_EMPTY)
        scheduleDelayedAction(LoadMediaResource);

    // Generally "ended" and "looping" are exclusive. Here, the loop attribute
    // is ignored to seek back to start in case loop was set after playback
    // ended. See http://crbug.com/364442
    if (endedPlayback(LoopCondition::Ignored))
        seek(0);

    if (m_paused) {
        m_paused = false;
        invalidateCachedTime();
        scheduleEvent(EventTypeNames::play);

        if (m_readyState <= HAVE_CURRENT_DATA)
            scheduleEvent(EventTypeNames::waiting);
        else if (m_readyState >= HAVE_FUTURE_DATA)
            scheduleEvent(EventTypeNames::playing);
    }
    m_autoplaying = false;

    updatePlayState();
}

void HTMLMediaElement::autoplayMediaEncountered()
{
    if (!m_autoplayMediaCounted) {
        m_autoplayMediaCounted = true;
        recordAutoplayMetric(AutoplayMediaFound);

        if (!m_userGestureRequiredForPlay)
            m_initialPlayWithoutUserGesture = true;
    }
}

bool HTMLMediaElement::isBailout() const
{
    // We count the user as having bailed-out on the video if they watched
    // less than one minute and less than 50% of it.
    const double playedTime = currentTime();
    const double progress = playedTime / duration();
    return (playedTime < 60) && (progress < 0.5);
}

void HTMLMediaElement::pause()
{
    WTF_LOG(Media, "HTMLMediaElement::pause(%p)", this);

    // Only buffer aggressively on a user-initiated pause. Other types of pauses
    // (which go directly to pauseInternal()) should not cause this behavior.
    if (webMediaPlayer() && UserGestureIndicator::processingUserGesture())
        webMediaPlayer()->setBufferingStrategy(WebMediaPlayer::BufferingStrategy::Aggressive);

    pauseInternal();
}

void HTMLMediaElement::pauseInternal()
{
    WTF_LOG(Media, "HTMLMediaElement::pauseInternal(%p)", this);

    if (m_networkState == NETWORK_EMPTY)
        scheduleDelayedAction(LoadMediaResource);

    m_autoplayHelper.pauseMethodCalled();

    m_autoplaying = false;

    if (!m_paused) {
        recordMetricsIfPausing();

        m_paused = true;
        scheduleTimeupdateEvent(false);
        scheduleEvent(EventTypeNames::pause);
    }

    updatePlayState();
}

void HTMLMediaElement::requestRemotePlayback()
{
    ASSERT(m_remoteRoutesAvailable);
    webMediaPlayer()->requestRemotePlayback();
}

void HTMLMediaElement::requestRemotePlaybackControl()
{
    ASSERT(m_remoteRoutesAvailable);
    webMediaPlayer()->requestRemotePlaybackControl();
}

void HTMLMediaElement::closeMediaSource()
{
    if (!m_mediaSource)
        return;

    m_mediaSource->close();
    m_mediaSource = nullptr;
}

bool HTMLMediaElement::loop() const
{
    return fastHasAttribute(loopAttr);
}

void HTMLMediaElement::setLoop(bool b)
{
    WTF_LOG(Media, "HTMLMediaElement::setLoop(%p, %s)", this, boolString(b));
    setBooleanAttribute(loopAttr, b);
}

bool HTMLMediaElement::shouldShowControls() const
{
    LocalFrame* frame = document().frame();

    // always show controls when scripting is disabled
    if (frame && !frame->script().canExecuteScripts(NotAboutToExecuteScript))
        return true;

    // Always show controls when in full screen mode.
    if (isFullscreen())
        return true;

    return fastHasAttribute(controlsAttr);
}

double HTMLMediaElement::volume() const
{
    return m_volume;
}

void HTMLMediaElement::setVolume(double vol, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "HTMLMediaElement::setVolume(%p, %f)", this, vol);

    if (m_volume == vol)
        return;

    if (vol < 0.0f || vol > 1.0f) {
        exceptionState.throwDOMException(IndexSizeError, ExceptionMessages::indexOutsideRange("volume", vol, 0.0, ExceptionMessages::InclusiveBound, 1.0, ExceptionMessages::InclusiveBound));
        return;
    }

    m_volume = vol;
    updateVolume();
    scheduleEvent(EventTypeNames::volumechange);
}

bool HTMLMediaElement::muted() const
{
    return m_muted;
}

void HTMLMediaElement::setMuted(bool muted)
{
    WTF_LOG(Media, "HTMLMediaElement::setMuted(%p, %s)", this, boolString(muted));

    if (m_muted == muted)
        return;

    m_muted = muted;

    m_autoplayHelper.mutedChanged();

    updateVolume();

    scheduleEvent(EventTypeNames::volumechange);
}

void HTMLMediaElement::updateVolume()
{
    if (webMediaPlayer())
        webMediaPlayer()->setVolume(effectiveMediaVolume());

    if (mediaControls())
        mediaControls()->updateVolume();
}

double HTMLMediaElement::effectiveMediaVolume() const
{
    if (m_muted)
        return 0;

    return m_volume;
}

// The spec says to fire periodic timeupdate events (those sent while playing) every
// "15 to 250ms", we choose the slowest frequency
static const double maxTimeupdateEventFrequency = 0.25;

void HTMLMediaElement::startPlaybackProgressTimer()
{
    if (m_playbackProgressTimer.isActive())
        return;

    m_previousProgressTime = WTF::currentTime();
    m_playbackProgressTimer.startRepeating(maxTimeupdateEventFrequency, BLINK_FROM_HERE);
}

void HTMLMediaElement::playbackProgressTimerFired(Timer<HTMLMediaElement>*)
{
    if (!std::isnan(m_fragmentEndTime) && currentTime() >= m_fragmentEndTime && directionOfPlayback() == Forward) {
        m_fragmentEndTime = std::numeric_limits<double>::quiet_NaN();
        if (!m_paused) {
            UseCounter::count(document(), UseCounter::HTMLMediaElementPauseAtFragmentEnd);
            // changes paused to true and fires a simple event named pause at the media element.
            pauseInternal();
        }
    }

    if (!m_seeking)
        scheduleTimeupdateEvent(true);

    if (!playbackRate())
        return;

    if (!m_paused && mediaControls())
        mediaControls()->playbackProgressed();

    cueTimeline().updateActiveCues(currentTime());
}

void HTMLMediaElement::scheduleTimeupdateEvent(bool periodicEvent)
{
    double now = WTF::currentTime();
    double movieTime = currentTime();

    bool haveNotRecentlyFiredTimeupdate = (now - m_lastTimeUpdateEventWallTime) >= maxTimeupdateEventFrequency;
    bool movieTimeHasProgressed = movieTime != m_lastTimeUpdateEventMovieTime;

    // Non-periodic timeupdate events must always fire as mandated by the spec,
    // otherwise we shouldn't fire duplicate periodic timeupdate events when the
    // movie time hasn't changed.
    if (!periodicEvent || (haveNotRecentlyFiredTimeupdate && movieTimeHasProgressed)) {
        scheduleEvent(EventTypeNames::timeupdate);
        m_lastTimeUpdateEventWallTime = now;
        m_lastTimeUpdateEventMovieTime = movieTime;
    }
}

void HTMLMediaElement::togglePlayState()
{
    if (paused())
        play();
    else
        pause();
}

AudioTrackList& HTMLMediaElement::audioTracks()
{
    ASSERT(RuntimeEnabledFeatures::audioVideoTracksEnabled());
    return *m_audioTracks;
}

void HTMLMediaElement::audioTrackChanged()
{
    WTF_LOG(Media, "HTMLMediaElement::audioTrackChanged(%p)", this);
    ASSERT(RuntimeEnabledFeatures::audioVideoTracksEnabled());

    audioTracks().scheduleChangeEvent();

    // FIXME: Add call on m_mediaSource to notify it of track changes once the SourceBuffer.audioTracks attribute is added.

    if (!m_audioTracksTimer.isActive())
        m_audioTracksTimer.startOneShot(0, BLINK_FROM_HERE);
}

void HTMLMediaElement::audioTracksTimerFired(Timer<HTMLMediaElement>*)
{
    Vector<WebMediaPlayer::TrackId> enabledTrackIds;
    for (unsigned i = 0; i < audioTracks().length(); ++i) {
        AudioTrack* track = audioTracks().anonymousIndexedGetter(i);
        if (track->enabled())
            enabledTrackIds.append(track->trackId());
    }

    webMediaPlayer()->enabledAudioTracksChanged(enabledTrackIds);
}

WebMediaPlayer::TrackId HTMLMediaElement::addAudioTrack(const WebString& id, WebMediaPlayerClient::AudioTrackKind kind, const WebString& label, const WebString& language, bool enabled)
{
    AtomicString kindString = AudioKindToString(kind);
    WTF_LOG(Media, "HTMLMediaElement::addAudioTrack(%p, '%s', '%s', '%s', '%s', %d)",
        this, id.utf8().data(), kindString.ascii().data(), label.utf8().data(), language.utf8().data(), enabled);

    if (!RuntimeEnabledFeatures::audioVideoTracksEnabled())
        return 0;

    AudioTrack* audioTrack = AudioTrack::create(id, kindString, label, language, enabled);
    audioTracks().add(audioTrack);

    return audioTrack->trackId();
}

void HTMLMediaElement::removeAudioTrack(WebMediaPlayer::TrackId trackId)
{
    WTF_LOG(Media, "HTMLMediaElement::removeAudioTrack(%p)", this);

    if (!RuntimeEnabledFeatures::audioVideoTracksEnabled())
        return;

    audioTracks().remove(trackId);
}

VideoTrackList& HTMLMediaElement::videoTracks()
{
    ASSERT(RuntimeEnabledFeatures::audioVideoTracksEnabled());
    return *m_videoTracks;
}

void HTMLMediaElement::selectedVideoTrackChanged(WebMediaPlayer::TrackId* selectedTrackId)
{
    WTF_LOG(Media, "HTMLMediaElement::selectedVideoTrackChanged(%p)", this);
    ASSERT(RuntimeEnabledFeatures::audioVideoTracksEnabled());

    if (selectedTrackId)
        videoTracks().trackSelected(*selectedTrackId);

    // FIXME: Add call on m_mediaSource to notify it of track changes once the SourceBuffer.videoTracks attribute is added.

    webMediaPlayer()->selectedVideoTrackChanged(selectedTrackId);
}

WebMediaPlayer::TrackId HTMLMediaElement::addVideoTrack(const WebString& id, WebMediaPlayerClient::VideoTrackKind kind, const WebString& label, const WebString& language, bool selected)
{
    AtomicString kindString = VideoKindToString(kind);
    WTF_LOG(Media, "HTMLMediaElement::addVideoTrack(%p, '%s', '%s', '%s', '%s', %d)",
        this, id.utf8().data(), kindString.ascii().data(), label.utf8().data(), language.utf8().data(), selected);

    if (!RuntimeEnabledFeatures::audioVideoTracksEnabled())
        return 0;

    // If another track was selected (potentially by the user), leave it selected.
    if (selected && videoTracks().selectedIndex() != -1)
        selected = false;

    VideoTrack* videoTrack = VideoTrack::create(id, kindString, label, language, selected);
    videoTracks().add(videoTrack);

    return videoTrack->trackId();
}

void HTMLMediaElement::removeVideoTrack(WebMediaPlayer::TrackId trackId)
{
    WTF_LOG(Media, "HTMLMediaElement::removeVideoTrack(%p)", this);

    if (!RuntimeEnabledFeatures::audioVideoTracksEnabled())
        return;

    videoTracks().remove(trackId);
}

void HTMLMediaElement::addTextTrack(WebInbandTextTrack* webTrack)
{
    // 4.8.10.12.2 Sourcing in-band text tracks
    // 1. Associate the relevant data with a new text track and its corresponding new TextTrack object.
    InbandTextTrack* textTrack = InbandTextTrack::create(webTrack);

    // 2. Set the new text track's kind, label, and language based on the semantics of the relevant data,
    // as defined by the relevant specification. If there is no label in that data, then the label must
    // be set to the empty string.
    // 3. Associate the text track list of cues with the rules for updating the text track rendering appropriate
    // for the format in question.
    // 4. If the new text track's kind is metadata, then set the text track in-band metadata track dispatch type
    // as follows, based on the type of the media resource:
    // 5. Populate the new text track's list of cues with the cues parsed so far, folllowing the guidelines for exposing
    // cues, and begin updating it dynamically as necessary.
    //   - Thess are all done by the media engine.

    // 6. Set the new text track's readiness state to loaded.
    textTrack->setReadinessState(TextTrack::Loaded);

    // 7. Set the new text track's mode to the mode consistent with the user's preferences and the requirements of
    // the relevant specification for the data.
    //  - This will happen in honorUserPreferencesForAutomaticTextTrackSelection()
    scheduleDelayedAction(LoadTextTrackResource);

    // 8. Add the new text track to the media element's list of text tracks.
    // 9. Fire an event with the name addtrack, that does not bubble and is not cancelable, and that uses the TrackEvent
    // interface, with the track attribute initialized to the text track's TextTrack object, at the media element's
    // textTracks attribute's TextTrackList object.
    addTextTrack(textTrack);
}

void HTMLMediaElement::removeTextTrack(WebInbandTextTrack* webTrack)
{
    if (!m_textTracks)
        return;

    // This cast is safe because we created the InbandTextTrack with the WebInbandTextTrack
    // passed to mediaPlayerDidAddTextTrack.
    InbandTextTrack* textTrack = static_cast<InbandTextTrack*>(webTrack->client());
    if (!textTrack)
        return;

    removeTextTrack(textTrack);
}

void HTMLMediaElement::textTracksChanged()
{
    if (mediaControls())
        mediaControls()->refreshClosedCaptionsButtonVisibility();
}

void HTMLMediaElement::addTextTrack(TextTrack* track)
{
    textTracks()->append(track);

    textTracksChanged();
}

void HTMLMediaElement::removeTextTrack(TextTrack* track)
{
    m_textTracks->remove(track);

    textTracksChanged();
}

void HTMLMediaElement::forgetResourceSpecificTracks()
{
    // Implements the "forget the media element's media-resource-specific tracks" algorithm.
    // The order is explicitly specified as text, then audio, and finally video. Also
    // 'removetrack' events should not be fired.
    if (m_textTracks) {
        TrackDisplayUpdateScope scope(this->cueTimeline());
        m_textTracks->removeAllInbandTracks();
        textTracksChanged();
    }

    m_audioTracks->removeAll();
    m_videoTracks->removeAll();

    m_audioTracksTimer.stop();
}

TextTrack* HTMLMediaElement::addTextTrack(const AtomicString& kind, const AtomicString& label, const AtomicString& language, ExceptionState& exceptionState)
{
    // https://html.spec.whatwg.org/multipage/embedded-content.html#dom-media-addtexttrack

    // The addTextTrack(kind, label, language) method of media elements, when
    // invoked, must run the following steps:

    // 1. Create a new TextTrack object.
    // 2. Create a new text track corresponding to the new object, and set its
    //    text track kind to kind, its text track label to label, its text
    //    track language to language, ..., and its text track list of cues to
    //    an empty list.
    TextTrack* textTrack = TextTrack::create(kind, label, language);
    //    ..., its text track readiness state to the text track loaded state, ...
    textTrack->setReadinessState(TextTrack::Loaded);

    // 3. Add the new text track to the media element's list of text tracks.
    // 4. Queue a task to fire a trusted event with the name addtrack, that
    //    does not bubble and is not cancelable, and that uses the TrackEvent
    //    interface, with the track attribute initialised to the new text
    //    track's TextTrack object, at the media element's textTracks
    //    attribute's TextTrackList object.
    addTextTrack(textTrack);

    // Note: Due to side effects when changing track parameters, we have to
    // first append the track to the text track list.
    // FIXME: Since setMode() will cause a 'change' event to be queued on the
    // same task source as the 'addtrack' event (see above), the order is
    // wrong. (The 'change' event shouldn't be fired at all in this case...)

    // ..., its text track mode to the text track hidden mode, ...
    textTrack->setMode(TextTrack::hiddenKeyword());

    // 5. Return the new TextTrack object.
    return textTrack;
}

TextTrackList* HTMLMediaElement::textTracks()
{
    if (!m_textTracks)
        m_textTracks = TextTrackList::create(this);

    return m_textTracks.get();
}

void HTMLMediaElement::didAddTrackElement(HTMLTrackElement* trackElement)
{
    // 4.8.10.12.3 Sourcing out-of-band text tracks
    // When a track element's parent element changes and the new parent is a media element,
    // then the user agent must add the track element's corresponding text track to the
    // media element's list of text tracks ... [continues in TextTrackList::append]
    TextTrack* textTrack = trackElement->track();
    if (!textTrack)
        return;

    addTextTrack(textTrack);

    // Do not schedule the track loading until parsing finishes so we don't start before all tracks
    // in the markup have been added.
    if (isFinishedParsingChildren())
        scheduleDelayedAction(LoadTextTrackResource);
}

void HTMLMediaElement::didRemoveTrackElement(HTMLTrackElement* trackElement)
{
#if !LOG_DISABLED
    KURL url = trackElement->getNonEmptyURLAttribute(srcAttr);
    WTF_LOG(Media, "HTMLMediaElement::didRemoveTrackElement(%p) - 'src' is %s", this, urlForLoggingMedia(url).utf8().data());
#endif

    TextTrack* textTrack = trackElement->track();
    if (!textTrack)
        return;

    textTrack->setHasBeenConfigured(false);

    if (!m_textTracks)
        return;

    // 4.8.10.12.3 Sourcing out-of-band text tracks
    // When a track element's parent element changes and the old parent was a media element,
    // then the user agent must remove the track element's corresponding text track from the
    // media element's list of text tracks.
    removeTextTrack(textTrack);

    size_t index = m_textTracksWhenResourceSelectionBegan.find(textTrack);
    if (index != kNotFound)
        m_textTracksWhenResourceSelectionBegan.remove(index);
}

void HTMLMediaElement::honorUserPreferencesForAutomaticTextTrackSelection()
{
    if (!m_textTracks || !m_textTracks->length())
        return;

    AutomaticTrackSelection::Configuration configuration;
    if (m_processingPreferenceChange)
        configuration.disableCurrentlyEnabledTracks = true;
    if (m_closedCaptionsVisible)
        configuration.forceEnableSubtitleOrCaptionTrack = true;

    Settings* settings = document().settings();
    if (settings)
        configuration.textTrackKindUserPreference = settings->textTrackKindUserPreference();

    AutomaticTrackSelection trackSelection(configuration);
    trackSelection.perform(*m_textTracks);

    textTracksChanged();
}

bool HTMLMediaElement::havePotentialSourceChild()
{
    // Stash the current <source> node and next nodes so we can restore them after checking
    // to see there is another potential.
    RefPtrWillBeRawPtr<HTMLSourceElement> currentSourceNode = m_currentSourceNode;
    RefPtrWillBeRawPtr<Node> nextNode = m_nextChildNodeToConsider;

    KURL nextURL = selectNextSourceChild(0, 0, DoNothing);

    m_currentSourceNode = currentSourceNode;
    m_nextChildNodeToConsider = nextNode;

    return nextURL.isValid();
}

KURL HTMLMediaElement::selectNextSourceChild(ContentType* contentType, String* keySystem, InvalidURLAction actionIfInvalid)
{
#if !LOG_DISABLED
    // Don't log if this was just called to find out if there are any valid <source> elements.
    bool shouldLog = actionIfInvalid != DoNothing;
    if (shouldLog)
        WTF_LOG(Media, "HTMLMediaElement::selectNextSourceChild(%p)", this);
#endif

    if (!m_nextChildNodeToConsider) {
#if !LOG_DISABLED
        if (shouldLog)
            WTF_LOG(Media, "HTMLMediaElement::selectNextSourceChild(%p) -> 0x0000, \"\"", this);
#endif
        return KURL();
    }

    KURL mediaURL;
    Node* node;
    HTMLSourceElement* source = 0;
    String type;
    String system;
    bool lookingForStartNode = m_nextChildNodeToConsider;
    bool canUseSourceElement = false;

    NodeVector potentialSourceNodes;
    getChildNodes(*this, potentialSourceNodes);

    for (unsigned i = 0; !canUseSourceElement && i < potentialSourceNodes.size(); ++i) {
        node = potentialSourceNodes[i].get();
        if (lookingForStartNode && m_nextChildNodeToConsider != node)
            continue;
        lookingForStartNode = false;

        if (!isHTMLSourceElement(*node))
            continue;
        if (node->parentNode() != this)
            continue;

        source = toHTMLSourceElement(node);

        // If candidate does not have a src attribute, or if its src attribute's value is the empty string ... jump down to the failed step below
        mediaURL = source->getNonEmptyURLAttribute(srcAttr);
#if !LOG_DISABLED
        if (shouldLog)
            WTF_LOG(Media, "HTMLMediaElement::selectNextSourceChild(%p) - 'src' is %s", this, urlForLoggingMedia(mediaURL).utf8().data());
#endif
        if (mediaURL.isEmpty())
            goto checkAgain;

        type = source->type();
        // FIXME(82965): Add support for keySystem in <source> and set system from source.
        if (type.isEmpty() && mediaURL.protocolIsData())
            type = mimeTypeFromDataURL(mediaURL);
        if (!type.isEmpty() || !system.isEmpty()) {
#if !LOG_DISABLED
            if (shouldLog)
                WTF_LOG(Media, "HTMLMediaElement::selectNextSourceChild(%p) - 'type' is '%s' - key system is '%s'", this, type.utf8().data(), system.utf8().data());
#endif
            if (!supportsType(ContentType(type), system))
                goto checkAgain;
        }

        // Is it safe to load this url?
        if (!isSafeToLoadURL(mediaURL, actionIfInvalid))
            goto checkAgain;

        // Making it this far means the <source> looks reasonable.
        canUseSourceElement = true;

checkAgain:
        if (!canUseSourceElement && actionIfInvalid == Complain && source)
            source->scheduleErrorEvent();
    }

    if (canUseSourceElement) {
        if (contentType)
            *contentType = ContentType(type);
        if (keySystem)
            *keySystem = system;
        m_currentSourceNode = source;
        m_nextChildNodeToConsider = source->nextSibling();
    } else {
        m_currentSourceNode = nullptr;
        m_nextChildNodeToConsider = nullptr;
    }

#if !LOG_DISABLED
    if (shouldLog)
        WTF_LOG(Media, "HTMLMediaElement::selectNextSourceChild(%p) -> %p, %s", this, m_currentSourceNode.get(), canUseSourceElement ? urlForLoggingMedia(mediaURL).utf8().data() : "");
#endif
    return canUseSourceElement ? mediaURL : KURL();
}

void HTMLMediaElement::sourceWasAdded(HTMLSourceElement* source)
{
    WTF_LOG(Media, "HTMLMediaElement::sourceWasAdded(%p, %p)", this, source);

#if !LOG_DISABLED
    KURL url = source->getNonEmptyURLAttribute(srcAttr);
    WTF_LOG(Media, "HTMLMediaElement::sourceWasAdded(%p) - 'src' is %s", this, urlForLoggingMedia(url).utf8().data());
#endif

    // We should only consider a <source> element when there is not src attribute at all.
    if (fastHasAttribute(srcAttr))
        return;

    // 4.8.8 - If a source element is inserted as a child of a media element that has no src
    // attribute and whose networkState has the value NETWORK_EMPTY, the user agent must invoke
    // the media element's resource selection algorithm.
    if (networkState() == HTMLMediaElement::NETWORK_EMPTY) {
        scheduleDelayedAction(LoadMediaResource);
        m_nextChildNodeToConsider = source;
        return;
    }

    if (m_currentSourceNode && source == m_currentSourceNode->nextSibling()) {
        WTF_LOG(Media, "HTMLMediaElement::sourceWasAdded(%p) - <source> inserted immediately after current source", this);
        m_nextChildNodeToConsider = source;
        return;
    }

    if (m_nextChildNodeToConsider)
        return;

    if (m_loadState != WaitingForSource)
        return;

    // 4.8.9.5, resource selection algorithm, source elements section:
    // 21. Wait until the node after pointer is a node other than the end of the list. (This step might wait forever.)
    // 22. Asynchronously await a stable state...
    // 23. Set the element's delaying-the-load-event flag back to true (this delays the load event again, in case
    // it hasn't been fired yet).
    setShouldDelayLoadEvent(true);

    // 24. Set the networkState back to NETWORK_LOADING.
    setNetworkState(NETWORK_LOADING);

    // 25. Jump back to the find next candidate step above.
    m_nextChildNodeToConsider = source;
    scheduleNextSourceChild();
}

void HTMLMediaElement::sourceWasRemoved(HTMLSourceElement* source)
{
    WTF_LOG(Media, "HTMLMediaElement::sourceWasRemoved(%p, %p)", this, source);

#if !LOG_DISABLED
    KURL url = source->getNonEmptyURLAttribute(srcAttr);
    WTF_LOG(Media, "HTMLMediaElement::sourceWasRemoved(%p) - 'src' is %s", this, urlForLoggingMedia(url).utf8().data());
#endif

    if (source != m_currentSourceNode && source != m_nextChildNodeToConsider)
        return;

    if (source == m_nextChildNodeToConsider) {
        if (m_currentSourceNode)
            m_nextChildNodeToConsider = m_currentSourceNode->nextSibling();
        WTF_LOG(Media, "HTMLMediaElement::sourceRemoved(%p) - m_nextChildNodeToConsider set to %p", this, m_nextChildNodeToConsider.get());
    } else if (source == m_currentSourceNode) {
        // Clear the current source node pointer, but don't change the movie as the spec says:
        // 4.8.8 - Dynamically modifying a source element and its attribute when the element is already
        // inserted in a video or audio element will have no effect.
        m_currentSourceNode = nullptr;
        WTF_LOG(Media, "HTMLMediaElement::sourceRemoved(%p) - m_currentSourceNode set to 0", this);
    }
}

void HTMLMediaElement::timeChanged()
{
    WTF_LOG(Media, "HTMLMediaElement::timeChanged(%p)", this);

    cueTimeline().updateActiveCues(currentTime());

    invalidateCachedTime();

    // 4.8.10.9 steps 12-14. Needed if no ReadyState change is associated with the seek.
    if (m_seeking && m_readyState >= HAVE_CURRENT_DATA && !webMediaPlayer()->seeking())
        finishSeek();

    // Always call scheduleTimeupdateEvent when the media engine reports a time discontinuity,
    // it will only queue a 'timeupdate' event if we haven't already posted one at the current
    // movie time.
    scheduleTimeupdateEvent(false);

    double now = currentTime();
    double dur = duration();

    // When the current playback position reaches the end of the media resource when the direction of
    // playback is forwards, then the user agent must follow these steps:
    if (!std::isnan(dur) && dur && now >= dur && directionOfPlayback() == Forward) {
        // If the media element has a loop attribute specified
        if (loop()) {
            m_sentEndEvent = false;
            //  then seek to the earliest possible position of the media resource and abort these steps.
            seek(0);
        } else {
            // If the media element has still ended playback, and the direction of playback is still
            // forwards, and paused is false,
            if (!m_paused) {
                // changes paused to true and fires a simple event named pause at the media element.
                m_paused = true;
                scheduleEvent(EventTypeNames::pause);
            }
            // Queue a task to fire a simple event named ended at the media element.
            if (!m_sentEndEvent) {
                m_sentEndEvent = true;
                scheduleEvent(EventTypeNames::ended);
            }
            recordMetricsIfPausing();
        }
    } else {
        m_sentEndEvent = false;
    }

    updatePlayState();
}

void HTMLMediaElement::durationChanged()
{
    WTF_LOG(Media, "HTMLMediaElement::durationChanged(%p)", this);
    // FIXME: Change WebMediaPlayer to convey the currentTime
    // when the duration change occured. The current WebMediaPlayer
    // implementations always clamp currentTime() to duration()
    // so the requestSeek condition here is always false.
    durationChanged(duration(), currentTime() > duration());
}

void HTMLMediaElement::durationChanged(double duration, bool requestSeek)
{
    WTF_LOG(Media, "HTMLMediaElement::durationChanged(%p, %f, %d)", this, duration, requestSeek);

    // Abort if duration unchanged.
    if (m_duration == duration)
        return;

    WTF_LOG(Media, "HTMLMediaElement::durationChanged(%p) : %f -> %f", this, m_duration, duration);
    m_duration = duration;
    scheduleEvent(EventTypeNames::durationchange);

    if (mediaControls())
        mediaControls()->reset();
    if (layoutObject())
        layoutObject()->updateFromElement();

    if (requestSeek)
        seek(duration);
}

void HTMLMediaElement::playbackStateChanged()
{
    WTF_LOG(Media, "HTMLMediaElement::playbackStateChanged(%p)", this);

    if (!webMediaPlayer())
        return;

    if (webMediaPlayer()->paused())
        pauseInternal();
    else
        playInternal();
}

void HTMLMediaElement::requestSeek(double time)
{
    // The player is the source of this seek request.
    setCurrentTime(time);
}

void HTMLMediaElement::remoteRouteAvailabilityChanged(bool routesAvailable)
{
    m_remoteRoutesAvailable = routesAvailable;
    if (mediaControls())
        mediaControls()->refreshCastButtonVisibility();
}

void HTMLMediaElement::connectedToRemoteDevice()
{
    m_playingRemotely = true;
    if (mediaControls())
        mediaControls()->startedCasting();
}

void HTMLMediaElement::disconnectedFromRemoteDevice()
{
    m_playingRemotely = false;
    if (mediaControls())
        mediaControls()->stoppedCasting();
}

// MediaPlayerPresentation methods
void HTMLMediaElement::repaint()
{
    if (m_webLayer)
        m_webLayer->invalidate();

    updateDisplayState();
    if (layoutObject())
        layoutObject()->setShouldDoFullPaintInvalidation();
}

void HTMLMediaElement::sizeChanged()
{
    WTF_LOG(Media, "HTMLMediaElement::sizeChanged(%p)", this);

    ASSERT(hasVideo()); // "resize" makes no sense absent video.
    if (m_readyState > HAVE_NOTHING && isHTMLVideoElement())
        scheduleEvent(EventTypeNames::resize);

    if (layoutObject())
        layoutObject()->updateFromElement();
}

TimeRanges* HTMLMediaElement::buffered() const
{
    if (m_mediaSource)
        return m_mediaSource->buffered();

    if (!webMediaPlayer())
        return TimeRanges::create();

    return TimeRanges::create(webMediaPlayer()->buffered());
}

TimeRanges* HTMLMediaElement::played()
{
    if (m_playing) {
        double time = currentTime();
        if (time > m_lastSeekTime)
            addPlayedRange(m_lastSeekTime, time);
    }

    if (!m_playedTimeRanges)
        m_playedTimeRanges = TimeRanges::create();

    return m_playedTimeRanges->copy();
}

TimeRanges* HTMLMediaElement::seekable() const
{
    if (!webMediaPlayer())
        return TimeRanges::create();

    if (m_mediaSource)
        return m_mediaSource->seekable();

    return TimeRanges::create(webMediaPlayer()->seekable());
}

bool HTMLMediaElement::potentiallyPlaying() const
{
    // "pausedToBuffer" means the media engine's rate is 0, but only because it had to stop playing
    // when it ran out of buffered data. A movie in this state is "potentially playing", modulo the
    // checks in couldPlayIfEnoughData().
    bool pausedToBuffer = m_readyStateMaximum >= HAVE_FUTURE_DATA && m_readyState < HAVE_FUTURE_DATA;
    return (pausedToBuffer || m_readyState >= HAVE_FUTURE_DATA) && couldPlayIfEnoughData();
}

bool HTMLMediaElement::couldPlayIfEnoughData() const
{
    return !paused() && !endedPlayback() && !stoppedDueToErrors();
}

bool HTMLMediaElement::endedPlayback(LoopCondition loopCondition) const
{
    double dur = duration();
    if (std::isnan(dur))
        return false;

    // 4.8.10.8 Playing the media resource

    // A media element is said to have ended playback when the element's
    // readyState attribute is HAVE_METADATA or greater,
    if (m_readyState < HAVE_METADATA)
        return false;

    // and the current playback position is the end of the media resource and the direction
    // of playback is forwards, Either the media element does not have a loop attribute specified,
    double now = currentTime();
    if (directionOfPlayback() == Forward)
        return dur > 0 && now >= dur && (loopCondition == LoopCondition::Ignored || !loop());

    // or the current playback position is the earliest possible position and the direction
    // of playback is backwards
    ASSERT(directionOfPlayback() == Backward);
    return now <= 0;
}

bool HTMLMediaElement::stoppedDueToErrors() const
{
    if (m_readyState >= HAVE_METADATA && m_error) {
        TimeRanges* seekableRanges = seekable();
        if (!seekableRanges->contain(currentTime()))
            return true;
    }

    return false;
}

void HTMLMediaElement::updatePlayState()
{
    bool isPlaying = webMediaPlayer() && !webMediaPlayer()->paused();
    bool shouldBePlaying = potentiallyPlaying();

    WTF_LOG(Media, "HTMLMediaElement::updatePlayState(%p) - shouldBePlaying = %s, isPlaying = %s",
        this, boolString(shouldBePlaying), boolString(isPlaying));

    if (shouldBePlaying) {
        setDisplayMode(Video);
        invalidateCachedTime();

        if (!isPlaying) {
            // Set rate, muted before calling play in case they were set before the media engine was setup.
            // The media engine should just stash the rate and muted values since it isn't already playing.
            webMediaPlayer()->setRate(playbackRate());
            updateVolume();
            webMediaPlayer()->play();
        }

        if (mediaControls())
            mediaControls()->playbackStarted();
        startPlaybackProgressTimer();
        m_playing = true;
        recordAutoplayMetric(AnyPlaybackStarted);

    } else { // Should not be playing right now
        if (isPlaying)
            webMediaPlayer()->pause();
        refreshCachedTime();

        m_playbackProgressTimer.stop();
        m_playing = false;
        double time = currentTime();
        if (time > m_lastSeekTime)
            addPlayedRange(m_lastSeekTime, time);

        if (couldPlayIfEnoughData())
            prepareToPlay();

        if (mediaControls())
            mediaControls()->playbackStopped();
    }

    if (layoutObject())
        layoutObject()->updateFromElement();
}

void HTMLMediaElement::stopPeriodicTimers()
{
    m_progressEventTimer.stop();
    m_playbackProgressTimer.stop();
}

void HTMLMediaElement::clearMediaPlayerAndAudioSourceProviderClientWithoutLocking()
{
    audioSourceProvider().setClient(nullptr);
    if (m_webMediaPlayer) {
        m_audioSourceProvider.wrap(nullptr);
        m_webMediaPlayer.clear();
    }
}

void HTMLMediaElement::clearMediaPlayer(int flags)
{
    forgetResourceSpecificTracks();

    closeMediaSource();

    cancelDeferredLoad();

    {
        AudioSourceProviderClientLockScope scope(*this);
        clearMediaPlayerAndAudioSourceProviderClientWithoutLocking();
    }

    stopPeriodicTimers();
    m_loadTimer.stop();

    m_pendingActionFlags &= ~flags;
    m_loadState = WaitingForSource;

    // We can't cast if we don't have a media player.
    m_remoteRoutesAvailable = false;
    m_playingRemotely = false;
    if (mediaControls())
        mediaControls()->refreshCastButtonVisibilityWithoutUpdate();

    if (layoutObject())
        layoutObject()->setShouldDoFullPaintInvalidation();
}

void HTMLMediaElement::stop()
{
    WTF_LOG(Media, "HTMLMediaElement::stop(%p)", this);

    recordMetricsIfPausing();

    // Close the async event queue so that no events are enqueued.
    cancelPendingEventsAndCallbacks();
    m_asyncEventQueue->close();

    // Stop the playback without generating events
    clearMediaPlayer(-1);
    m_readyState = HAVE_NOTHING;
    m_readyStateMaximum = HAVE_NOTHING;
    setNetworkState(NETWORK_EMPTY);
    setShouldDelayLoadEvent(false);
    m_currentSourceNode = nullptr;
    invalidateCachedTime();
    cueTimeline().updateActiveCues(0);
    m_playing = false;
    m_paused = true;
    m_seeking = false;

    if (layoutObject())
        layoutObject()->updateFromElement();

    stopPeriodicTimers();

    // Ensure that hasPendingActivity() is not preventing garbage collection, since otherwise this
    // media element will simply leak.
    ASSERT(!hasPendingActivity());
}

bool HTMLMediaElement::hasPendingActivity() const
{
    // The delaying-the-load-event flag is set by resource selection algorithm when looking for a
    // resource to load, before networkState has reached to NETWORK_LOADING.
    if (m_shouldDelayLoadEvent)
        return true;

    // When networkState is NETWORK_LOADING, progress and stalled events may be fired.
    if (m_networkState == NETWORK_LOADING)
        return true;

    // When playing or if playback may continue, timeupdate events may be fired.
    if (couldPlayIfEnoughData())
        return true;

    // When the seek finishes timeupdate and seeked events will be fired.
    if (m_seeking)
        return true;

    // When connected to a MediaSource, e.g. setting MediaSource.duration will cause a
    // durationchange event to be fired.
    if (m_mediaSource)
        return true;

    // Wait for any pending events to be fired.
    if (m_asyncEventQueue->hasPendingEvents())
        return true;

    return false;
}

bool HTMLMediaElement::isFullscreen() const
{
    return Fullscreen::isActiveFullScreenElement(*this);
}

void HTMLMediaElement::enterFullscreen()
{
    WTF_LOG(Media, "HTMLMediaElement::enterFullscreen(%p)", this);

    Fullscreen::from(document()).requestFullscreen(*this, Fullscreen::PrefixedRequest);
}

void HTMLMediaElement::exitFullscreen()
{
    WTF_LOG(Media, "HTMLMediaElement::exitFullscreen(%p)", this);

    Fullscreen::from(document()).exitFullscreen();
}

void HTMLMediaElement::didBecomeFullscreenElement()
{
    if (mediaControls())
        mediaControls()->enteredFullscreen();
    // Cache this in case the player is destroyed before leaving fullscreen.
    m_inOverlayFullscreenVideo = usesOverlayFullscreenVideo();
    if (m_inOverlayFullscreenVideo)
        document().layoutView()->compositor()->setNeedsCompositingUpdate(CompositingUpdateRebuildTree);
}

void HTMLMediaElement::willStopBeingFullscreenElement()
{
    if (mediaControls())
        mediaControls()->exitedFullscreen();
    if (m_inOverlayFullscreenVideo)
        document().layoutView()->compositor()->setNeedsCompositingUpdate(CompositingUpdateRebuildTree);
    m_inOverlayFullscreenVideo = false;
}

WebLayer* HTMLMediaElement::platformLayer() const
{
    return m_webLayer;
}

bool HTMLMediaElement::hasClosedCaptions() const
{
    if (m_textTracks) {
        for (unsigned i = 0; i < m_textTracks->length(); ++i) {
            TextTrack* track = m_textTracks->anonymousIndexedGetter(i);
            if (track->readinessState() == TextTrack::FailedToLoad)
                continue;

            if (track->kind() == TextTrack::captionsKeyword()
                || track->kind() == TextTrack::subtitlesKeyword())
                return true;
        }
    }
    return false;
}

bool HTMLMediaElement::closedCaptionsVisible() const
{
    return m_closedCaptionsVisible;
}

static void assertShadowRootChildren(ShadowRoot& shadowRoot)
{
#if ENABLE(ASSERT)
    // There can be up to two children, either or both of the text
    // track container and media controls. If both are present, the
    // text track container must be the first child.
    unsigned numberOfChildren = shadowRoot.countChildren();
    ASSERT(numberOfChildren <= 2);
    Node* firstChild = shadowRoot.firstChild();
    Node* lastChild = shadowRoot.lastChild();
    if (numberOfChildren == 1) {
        ASSERT(firstChild->isTextTrackContainer() || firstChild->isMediaControls());
    } else if (numberOfChildren == 2) {
        ASSERT(firstChild->isTextTrackContainer());
        ASSERT(lastChild->isMediaControls());
    }
#endif
}

TextTrackContainer& HTMLMediaElement::ensureTextTrackContainer()
{
    ShadowRoot& shadowRoot = ensureUserAgentShadowRoot();
    assertShadowRootChildren(shadowRoot);

    Node* firstChild = shadowRoot.firstChild();
    if (firstChild && firstChild->isTextTrackContainer())
        return toTextTrackContainer(*firstChild);

    RefPtrWillBeRawPtr<TextTrackContainer> textTrackContainer = TextTrackContainer::create(document());

    // The text track container should be inserted before the media controls,
    // so that they are rendered behind them.
    shadowRoot.insertBefore(textTrackContainer, firstChild);

    assertShadowRootChildren(shadowRoot);

    return *textTrackContainer;
}

void HTMLMediaElement::updateTextTrackDisplay()
{
    WTF_LOG(Media, "HTMLMediaElement::updateTextTrackDisplay(%p)", this);

    ensureTextTrackContainer().updateDisplay(*this, TextTrackContainer::DidNotStartExposingControls);
}

void HTMLMediaElement::mediaControlsDidBecomeVisible()
{
    WTF_LOG(Media, "HTMLMediaElement::mediaControlsDidBecomeVisible(%p)", this);

    // When the user agent starts exposing a user interface for a video element,
    // the user agent should run the rules for updating the text track rendering
    // of each of the text tracks in the video element's list of text tracks ...
    if (isHTMLVideoElement() && closedCaptionsVisible())
        ensureTextTrackContainer().updateDisplay(*this, TextTrackContainer::DidStartExposingControls);
}

void HTMLMediaElement::setClosedCaptionsVisible(bool closedCaptionVisible)
{
    WTF_LOG(Media, "HTMLMediaElement::setClosedCaptionsVisible(%p, %s)", this, boolString(closedCaptionVisible));

    if (!hasClosedCaptions())
        return;

    m_closedCaptionsVisible = closedCaptionVisible;

    markCaptionAndSubtitleTracksAsUnconfigured();
    m_processingPreferenceChange = true;
    honorUserPreferencesForAutomaticTextTrackSelection();
    m_processingPreferenceChange = false;

    // As track visibility changed while m_processingPreferenceChange was set,
    // there was no call to updateTextTrackDisplay(). This call is not in the
    // spec, see the note in configureTextTrackDisplay().
    updateTextTrackDisplay();
}

void HTMLMediaElement::setTextTrackKindUserPreferenceForAllMediaElements(Document* document)
{
    WeakMediaElementSet elements = documentToElementSetMap().get(document);
    for (const auto& element : elements)
        element->automaticTrackSelectionForUpdatedUserPreference();
}

void HTMLMediaElement::automaticTrackSelectionForUpdatedUserPreference()
{
    if (!m_textTracks || !m_textTracks->length())
        return;

    markCaptionAndSubtitleTracksAsUnconfigured();
    m_processingPreferenceChange = true;
    m_closedCaptionsVisible = false;
    honorUserPreferencesForAutomaticTextTrackSelection();
    m_processingPreferenceChange = false;

    // If a track is set to 'showing' post performing automatic track selection,
    // set closed captions state to visible to update the CC button and display the track.
    m_closedCaptionsVisible = m_textTracks->hasShowingTracks();
    updateTextTrackDisplay();
}

void HTMLMediaElement::markCaptionAndSubtitleTracksAsUnconfigured()
{
    if (!m_textTracks)
        return;

    // Mark all tracks as not "configured" so that
    // honorUserPreferencesForAutomaticTextTrackSelection() will reconsider
    // which tracks to display in light of new user preferences (e.g. default
    // tracks should not be displayed if the user has turned off captions and
    // non-default tracks should be displayed based on language preferences if
    // the user has turned captions on).
    for (unsigned i = 0; i < m_textTracks->length(); ++i) {
        TextTrack* textTrack = m_textTracks->anonymousIndexedGetter(i);
        String kind = textTrack->kind();

        if (kind == TextTrack::subtitlesKeyword() || kind == TextTrack::captionsKeyword())
            textTrack->setHasBeenConfigured(false);
    }
}

unsigned HTMLMediaElement::webkitAudioDecodedByteCount() const
{
    if (!webMediaPlayer())
        return 0;
    return webMediaPlayer()->audioDecodedByteCount();
}

unsigned HTMLMediaElement::webkitVideoDecodedByteCount() const
{
    if (!webMediaPlayer())
        return 0;
    return webMediaPlayer()->videoDecodedByteCount();
}

bool HTMLMediaElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr || HTMLElement::isURLAttribute(attribute);
}

void HTMLMediaElement::setShouldDelayLoadEvent(bool shouldDelay)
{
    if (m_shouldDelayLoadEvent == shouldDelay)
        return;

    WTF_LOG(Media, "HTMLMediaElement::setShouldDelayLoadEvent(%p, %s)", this, boolString(shouldDelay));

    m_shouldDelayLoadEvent = shouldDelay;
    if (shouldDelay)
        document().incrementLoadEventDelayCount();
    else
        document().decrementLoadEventDelayCount();
}

MediaControls* HTMLMediaElement::mediaControls() const
{
    if (ShadowRoot* shadowRoot = userAgentShadowRoot()) {
        Node* lastChild = shadowRoot->lastChild();
        if (lastChild && lastChild->isMediaControls())
            return toMediaControls(lastChild);
    }

    return nullptr;
}

void HTMLMediaElement::ensureMediaControls()
{
    if (mediaControls())
        return;

    RefPtrWillBeRawPtr<MediaControls> mediaControls = MediaControls::create(*this);

    mediaControls->reset();
    if (isFullscreen())
        mediaControls->enteredFullscreen();

    ShadowRoot& shadowRoot = ensureUserAgentShadowRoot();
    assertShadowRootChildren(shadowRoot);

    // The media controls should be inserted after the text track container,
    // so that they are rendered in front of captions and subtitles.
    shadowRoot.appendChild(mediaControls);

    assertShadowRootChildren(shadowRoot);

    if (!shouldShowControls() || !inDocument())
        mediaControls->hide();
}

void HTMLMediaElement::configureMediaControls()
{
    if (!inDocument()) {
        if (mediaControls())
            mediaControls()->hide();
        return;
    }

    ensureMediaControls();
    mediaControls()->reset();
    if (shouldShowControls())
        mediaControls()->show();
    else
        mediaControls()->hide();
}

CueTimeline& HTMLMediaElement::cueTimeline()
{
    if (!m_cueTimeline)
        m_cueTimeline = adoptPtrWillBeNoop(new CueTimeline(*this));
    return *m_cueTimeline;
}

void HTMLMediaElement::configureTextTrackDisplay()
{
    ASSERT(m_textTracks);
    WTF_LOG(Media, "HTMLMediaElement::configureTextTrackDisplay(%p)", this);

    if (m_processingPreferenceChange)
        return;

    m_haveVisibleTextTrack = m_textTracks->hasShowingTracks();
    m_closedCaptionsVisible = m_haveVisibleTextTrack;

    if (!m_haveVisibleTextTrack && !mediaControls())
        return;

    if (mediaControls())
        mediaControls()->changedClosedCaptionsVisibility();

    cueTimeline().updateActiveCues(currentTime());

    // Note: The "time marches on" algorithm (updateActiveCues) runs the "rules
    // for updating the text track rendering" (updateTextTrackDisplay) only for
    // "affected tracks", i.e. tracks where the the active cues have changed.
    // This misses cues in tracks that changed mode between hidden and showing.
    // This appears to be a spec bug, which we work around here:
    // https://www.w3.org/Bugs/Public/show_bug.cgi?id=28236
    updateTextTrackDisplay();
}

void* HTMLMediaElement::preDispatchEventHandler(Event* event)
{
    if (event && event->type() == EventTypeNames::webkitfullscreenchange)
        configureMediaControls();

    return nullptr;
}

// TODO(srirama.m): Refactor this and clearMediaPlayer to the extent possible.
void HTMLMediaElement::resetMediaPlayerAndMediaSource()
{
    closeMediaSource();

    {
        AudioSourceProviderClientLockScope scope(*this);
        clearMediaPlayerAndAudioSourceProviderClientWithoutLocking();
    }

    // We haven't yet found out if any remote routes are available.
    m_remoteRoutesAvailable = false;
    m_playingRemotely = false;

    if (m_audioSourceNode)
        audioSourceProvider().setClient(m_audioSourceNode);
}

void HTMLMediaElement::setAudioSourceNode(AudioSourceProviderClient* sourceNode)
{
    ASSERT(isMainThread());
    m_audioSourceNode = sourceNode;

    AudioSourceProviderClientLockScope scope(*this);
    audioSourceProvider().setClient(m_audioSourceNode);
}

void HTMLMediaElement::setAllowHiddenVolumeControls(bool allow)
{
    ensureMediaControls();
    mediaControls()->setAllowHiddenVolumeControls(allow);
}

WebMediaPlayer::CORSMode HTMLMediaElement::corsMode() const
{
    const AtomicString& crossOriginMode = fastGetAttribute(crossoriginAttr);
    if (crossOriginMode.isNull())
        return WebMediaPlayer::CORSModeUnspecified;
    if (equalIgnoringCase(crossOriginMode, "use-credentials"))
        return WebMediaPlayer::CORSModeUseCredentials;
    return WebMediaPlayer::CORSModeAnonymous;
}

void HTMLMediaElement::setWebLayer(WebLayer* webLayer)
{
    if (webLayer == m_webLayer)
        return;

    // If either of the layers is null we need to enable or disable compositing. This is done by triggering a style recalc.
    if ((!m_webLayer || !webLayer)
#if ENABLE(OILPAN)
        && !m_isFinalizing
#endif
        )
        setNeedsCompositingUpdate();

    if (m_webLayer)
        GraphicsLayer::unregisterContentsLayer(m_webLayer);
    m_webLayer = webLayer;
    if (m_webLayer)
        GraphicsLayer::registerContentsLayer(m_webLayer);
}

void HTMLMediaElement::mediaSourceOpened(WebMediaSource* webMediaSource)
{
    m_mediaSource->setWebMediaSourceAndOpen(adoptPtr(webMediaSource));
}

bool HTMLMediaElement::isInteractiveContent() const
{
    return fastHasAttribute(controlsAttr);
}

void HTMLMediaElement::defaultEventHandler(Event* event)
{
    if (event->type() == EventTypeNames::focusin) {
        if (mediaControls())
            mediaControls()->mediaElementFocused();
    }
    HTMLElement::defaultEventHandler(event);
}

DEFINE_TRACE(HTMLMediaElement)
{
#if ENABLE(OILPAN)
    visitor->trace(m_playedTimeRanges);
    visitor->trace(m_asyncEventQueue);
    visitor->trace(m_error);
    visitor->trace(m_currentSourceNode);
    visitor->trace(m_nextChildNodeToConsider);
    visitor->trace(m_mediaSource);
    visitor->trace(m_audioTracks);
    visitor->trace(m_videoTracks);
    visitor->trace(m_cueTimeline);
    visitor->trace(m_textTracks);
    visitor->trace(m_textTracksWhenResourceSelectionBegan);
    visitor->trace(m_audioSourceProvider);
    visitor->template registerWeakMembers<HTMLMediaElement, &HTMLMediaElement::clearWeakMembers>(this);
    visitor->trace(m_autoplayHelper);
    HeapSupplementable<HTMLMediaElement>::trace(visitor);
#endif
    HTMLElement::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

void HTMLMediaElement::createPlaceholderTracksIfNecessary()
{
    if (!RuntimeEnabledFeatures::audioVideoTracksEnabled())
        return;

    // Create a placeholder audio track if the player says it has audio but it didn't explicitly announce the tracks.
    if (hasAudio() && !audioTracks().length())
        addAudioTrack("audio", WebMediaPlayerClient::AudioTrackKindMain, "Audio Track", "", true);

    // Create a placeholder video track if the player says it has video but it didn't explicitly announce the tracks.
    if (webMediaPlayer() && webMediaPlayer()->hasVideo() && !videoTracks().length())
        addVideoTrack("video", WebMediaPlayerClient::VideoTrackKindMain, "Video Track", "", true);
}

void HTMLMediaElement::selectInitialTracksIfNecessary()
{
    if (!RuntimeEnabledFeatures::audioVideoTracksEnabled())
        return;

    // Enable the first audio track if an audio track hasn't been enabled yet.
    if (audioTracks().length() > 0 && !audioTracks().hasEnabledTrack())
        audioTracks().anonymousIndexedGetter(0)->setEnabled(true);

    // Select the first video track if a video track hasn't been selected yet.
    if (videoTracks().length() > 0 && videoTracks().selectedIndex() == -1)
        videoTracks().anonymousIndexedGetter(0)->setSelected(true);
}

bool HTMLMediaElement::isUserGestureRequiredForPlay() const
{
    return m_userGestureRequiredForPlay;
}

void HTMLMediaElement::removeUserGestureRequirement()
{
    m_userGestureRequiredForPlay = false;
}

void HTMLMediaElement::setInitialPlayWithoutUserGestures(bool value)
{
    m_initialPlayWithoutUserGesture = value;
}

void HTMLMediaElement::setNetworkState(NetworkState state)
{
    if (m_networkState != state) {
        m_networkState = state;
        if (MediaControls* controls = mediaControls())
            controls->networkStateChanged();
    }
}

void HTMLMediaElement::notifyPositionMayHaveChanged(const IntRect& visibleRect)
{
    m_autoplayHelper.positionChanged(visibleRect);
}

void HTMLMediaElement::updatePositionNotificationRegistration()
{
    m_autoplayHelper.updatePositionNotificationRegistration();
}

// TODO(liberato): remove once autoplay gesture override experiment concludes.
void HTMLMediaElement::triggerAutoplayViewportCheckForTesting()
{
    m_autoplayHelper.triggerAutoplayViewportCheckForTesting();
}

void HTMLMediaElement::clearWeakMembers(Visitor* visitor)
{
    if (!Heap::isHeapObjectAlive(m_audioSourceNode))
        audioSourceProvider().setClient(nullptr);
}

void HTMLMediaElement::AudioSourceProviderImpl::wrap(WebAudioSourceProvider* provider)
{
    MutexLocker locker(provideInputLock);

    if (m_webAudioSourceProvider && provider != m_webAudioSourceProvider)
        m_webAudioSourceProvider->setClient(nullptr);

    m_webAudioSourceProvider = provider;
    if (m_webAudioSourceProvider)
        m_webAudioSourceProvider->setClient(m_client.get());
}

void HTMLMediaElement::AudioSourceProviderImpl::setClient(AudioSourceProviderClient* client)
{
    MutexLocker locker(provideInputLock);

    if (client)
        m_client = new HTMLMediaElement::AudioClientImpl(client);
    else
        m_client.clear();

    if (m_webAudioSourceProvider)
        m_webAudioSourceProvider->setClient(m_client.get());
}

void HTMLMediaElement::AudioSourceProviderImpl::provideInput(AudioBus* bus, size_t framesToProcess)
{
    ASSERT(bus);

    MutexTryLocker tryLocker(provideInputLock);
    if (!tryLocker.locked() || !m_webAudioSourceProvider || !m_client.get()) {
        bus->zero();
        return;
    }

    // Wrap the AudioBus channel data using WebVector.
    size_t n = bus->numberOfChannels();
    WebVector<float*> webAudioData(n);
    for (size_t i = 0; i < n; ++i)
        webAudioData[i] = bus->channel(i)->mutableData();

    m_webAudioSourceProvider->provideInput(webAudioData, framesToProcess);
}

void HTMLMediaElement::AudioClientImpl::setFormat(size_t numberOfChannels, float sampleRate)
{
    if (m_client)
        m_client->setFormat(numberOfChannels, sampleRate);
}

DEFINE_TRACE(HTMLMediaElement::AudioClientImpl)
{
    visitor->trace(m_client);
}

DEFINE_TRACE(HTMLMediaElement::AudioSourceProviderImpl)
{
    visitor->trace(m_client);
}

}
