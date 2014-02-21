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

#ifndef HTMLMediaElement_h
#define HTMLMediaElement_h

#include "core/dom/ActiveDOMObject.h"
#include "core/events/GenericEventQueue.h"
#include "core/html/HTMLElement.h"
#include "core/html/MediaControllerInterface.h"
#include "core/html/track/TextTrack.h"
#include "core/html/track/TextTrackCue.h"
#include "core/html/track/vtt/VTTCue.h"
#include "platform/PODIntervalTree.h"
#include "platform/graphics/media/MediaPlayer.h"
#include "public/platform/WebMimeRegistry.h"

namespace blink {
class WebContentDecryptionModule;
class WebInbandTextTrack;
class WebLayer;
}

namespace WebCore {

#if ENABLE(WEB_AUDIO)
class AudioSourceProvider;
class MediaElementAudioSourceNode;
#endif
class ContentType;
class Event;
class ExceptionState;
class HTMLSourceElement;
class HTMLTrackElement;
class KURL;
class MediaController;
class MediaControls;
class MediaError;
class MediaKeys;
class HTMLMediaSource;
class TextTrackList;
class TimeRanges;
class URLRegistry;

typedef PODIntervalTree<double, TextTrackCue*> CueIntervalTree;
typedef CueIntervalTree::IntervalType CueInterval;
typedef Vector<CueInterval> CueList;

// FIXME: The inheritance from MediaPlayerClient here should be private inheritance.
// But it can't be until the Chromium WebMediaPlayerClientImpl class is fixed so it
// no longer depends on typecasting a MediaPlayerClient to an HTMLMediaElement.

class HTMLMediaElement : public HTMLElement, public MediaPlayerClient, public ActiveDOMObject, public MediaControllerInterface
    , private TextTrackClient
{
public:
    static blink::WebMimeRegistry::SupportsType supportsType(const ContentType&, const String& keySystem = String());

    static void setMediaStreamRegistry(URLRegistry*);
    static bool isMediaStreamURL(const String& url);

    MediaPlayer* player() const { return m_player.get(); }

    virtual bool isVideo() const = 0;
    virtual bool hasVideo() const OVERRIDE { return false; }
    virtual bool hasAudio() const OVERRIDE FINAL;

    bool supportsSave() const;

    blink::WebLayer* platformLayer() const;

    enum DelayedActionType {
        LoadMediaResource = 1 << 0,
        LoadTextTrackResource = 1 << 1,
        TextTrackChangesNotification = 1 << 2
    };
    void scheduleDelayedAction(DelayedActionType);

    bool isActive() const { return m_active; }

    // error state
    PassRefPtr<MediaError> error() const;

    // network state
    void setSrc(const AtomicString&);
    const KURL& currentSrc() const { return m_currentSrc; }

    enum NetworkState { NETWORK_EMPTY, NETWORK_IDLE, NETWORK_LOADING, NETWORK_NO_SOURCE };
    NetworkState networkState() const;

    String preload() const;
    void setPreload(const AtomicString&);

    PassRefPtr<TimeRanges> buffered() const;
    void load();
    String canPlayType(const String& mimeType, const String& keySystem = String()) const;

    // ready state
    enum ReadyState { HAVE_NOTHING, HAVE_METADATA, HAVE_CURRENT_DATA, HAVE_FUTURE_DATA, HAVE_ENOUGH_DATA };
    ReadyState readyState() const;
    bool seeking() const;

    // playback state
    virtual double currentTime() const OVERRIDE FINAL;
    virtual void setCurrentTime(double, ExceptionState&) OVERRIDE FINAL;
    virtual double duration() const OVERRIDE FINAL;
    virtual bool paused() const OVERRIDE FINAL;
    double defaultPlaybackRate() const;
    void setDefaultPlaybackRate(double);
    double playbackRate() const;
    void setPlaybackRate(double);
    void updatePlaybackRate();
    PassRefPtr<TimeRanges> played();
    PassRefPtr<TimeRanges> seekable() const;
    bool ended() const;
    bool autoplay() const;
    bool loop() const;
    void setLoop(bool b);
    virtual void play() OVERRIDE FINAL;
    virtual void pause() OVERRIDE FINAL;

    // statistics
    unsigned webkitAudioDecodedByteCount() const;
    unsigned webkitVideoDecodedByteCount() const;

    // media source extensions
    void closeMediaSource();
    void durationChanged(double duration);

    // encrypted media extensions (v0.1b)
    void webkitGenerateKeyRequest(const String& keySystem, PassRefPtr<Uint8Array> initData, ExceptionState&);
    void webkitGenerateKeyRequest(const String& keySystem, ExceptionState&);
    void webkitAddKey(const String& keySystem, PassRefPtr<Uint8Array> key, PassRefPtr<Uint8Array> initData, const String& sessionId, ExceptionState&);
    void webkitAddKey(const String& keySystem, PassRefPtr<Uint8Array> key, ExceptionState&);
    void webkitCancelKeyRequest(const String& keySystem, const String& sessionId, ExceptionState&);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitkeyadded);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitkeyerror);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitkeymessage);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitneedkey);

    // encrypted media extensions (WD)
    MediaKeys* mediaKeys() const { return m_mediaKeys.get(); }
    void setMediaKeys(MediaKeys*, ExceptionState&);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(needkey);

    // controls
    bool controls() const;
    void setControls(bool);
    virtual double volume() const OVERRIDE FINAL;
    virtual void setVolume(double, ExceptionState&) OVERRIDE FINAL;
    virtual bool muted() const OVERRIDE FINAL;
    virtual void setMuted(bool) OVERRIDE FINAL;

    virtual void beginScrubbing() OVERRIDE FINAL;
    virtual void endScrubbing() OVERRIDE FINAL;

    virtual bool canPlay() const OVERRIDE FINAL;

    PassRefPtr<TextTrack> addTextTrack(const AtomicString& kind, const AtomicString& label, const AtomicString& language, ExceptionState&);
    PassRefPtr<TextTrack> addTextTrack(const AtomicString& kind, const AtomicString& label, ExceptionState& exceptionState) { return addTextTrack(kind, label, emptyAtom, exceptionState); }
    PassRefPtr<TextTrack> addTextTrack(const AtomicString& kind, ExceptionState& exceptionState) { return addTextTrack(kind, emptyAtom, emptyAtom, exceptionState); }

    TextTrackList* textTracks();
    CueList currentlyActiveCues() const { return m_currentlyActiveCues; }

    void addTrack(TextTrack*);
    void removeTrack(TextTrack*);
    void removeAllInbandTracks();
    void closeCaptionTracksChanged();
    void notifyMediaPlayerOfTextTrackChanges();

    void didAddTrack(HTMLTrackElement*);
    void didRemoveTrack(HTMLTrackElement*);

    virtual void mediaPlayerDidAddTrack(blink::WebInbandTextTrack*) OVERRIDE FINAL;
    virtual void mediaPlayerDidRemoveTrack(blink::WebInbandTextTrack*) OVERRIDE FINAL;
    // FIXME: Remove this when WebMediaPlayerClientImpl::loadInternal does not depend on it.
    virtual KURL mediaPlayerPosterURL() OVERRIDE { return KURL(); }

    struct TrackGroup {
        enum GroupKind { CaptionsAndSubtitles, Description, Chapter, Metadata, Other };

        TrackGroup(GroupKind kind)
            : visibleTrack(nullptr)
            , defaultTrack(nullptr)
            , kind(kind)
            , hasSrcLang(false)
        {
        }

        Vector<RefPtr<TextTrack> > tracks;
        RefPtr<TextTrack> visibleTrack;
        RefPtr<TextTrack> defaultTrack;
        GroupKind kind;
        bool hasSrcLang;
    };

    void configureTextTrackGroupForLanguage(const TrackGroup&) const;
    void configureTextTracks();
    void configureTextTrackGroup(const TrackGroup&);

    bool textTracksAreReady() const;
    enum VisibilityChangeAssumption {
        AssumeNoVisibleChange,
        AssumeVisibleChange
    };
    void configureTextTrackDisplay(VisibilityChangeAssumption);
    void updateTextTrackDisplay();
    void textTrackReadyStateChanged(TextTrack*);

    // TextTrackClient
    virtual void textTrackKindChanged(TextTrack*) OVERRIDE FINAL;
    virtual void textTrackModeChanged(TextTrack*) OVERRIDE FINAL;
    virtual void textTrackAddCues(TextTrack*, const TextTrackCueList*) OVERRIDE FINAL;
    virtual void textTrackRemoveCues(TextTrack*, const TextTrackCueList*) OVERRIDE FINAL;
    virtual void textTrackAddCue(TextTrack*, PassRefPtr<TextTrackCue>) OVERRIDE FINAL;
    virtual void textTrackRemoveCue(TextTrack*, PassRefPtr<TextTrackCue>) OVERRIDE FINAL;

    // EventTarget function.
    // Both Node (via HTMLElement) and ActiveDOMObject define this method, which
    // causes an ambiguity error at compile time. This class's constructor
    // ensures that both implementations return document, so return the result
    // of one of them here.
    virtual ExecutionContext* executionContext() const OVERRIDE FINAL { return HTMLElement::executionContext(); }

    bool hasSingleSecurityOrigin() const { return !m_player || m_player->hasSingleSecurityOrigin(); }

    bool isFullscreen() const;
    virtual void enterFullscreen() OVERRIDE FINAL;
    void exitFullscreen();

    virtual bool hasClosedCaptions() const OVERRIDE FINAL;
    virtual bool closedCaptionsVisible() const OVERRIDE FINAL;
    virtual void setClosedCaptionsVisible(bool) OVERRIDE FINAL;

    MediaControls* mediaControls() const;

    void sourceWasRemoved(HTMLSourceElement*);
    void sourceWasAdded(HTMLSourceElement*);

    bool isPlaying() const { return m_playing; }

    // ActiveDOMObject functions.
    virtual bool hasPendingActivity() const OVERRIDE FINAL;
    virtual void contextDestroyed() OVERRIDE FINAL;

#if ENABLE(WEB_AUDIO)
    MediaElementAudioSourceNode* audioSourceNode() { return m_audioSourceNode; }
    void setAudioSourceNode(MediaElementAudioSourceNode*);

    AudioSourceProvider* audioSourceProvider();
#endif

    enum InvalidURLAction { DoNothing, Complain };
    bool isSafeToLoadURL(const KURL&, InvalidURLAction);

    MediaController* controller() const;
    void setController(PassRefPtr<MediaController>); // Resets the MediaGroup and sets the MediaController.

protected:
    HTMLMediaElement(const QualifiedName&, Document&);
    virtual ~HTMLMediaElement();

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void finishParsingChildren() OVERRIDE FINAL;
    virtual bool isURLAttribute(const Attribute&) const OVERRIDE;
    virtual void attach(const AttachContext& = AttachContext()) OVERRIDE;

    virtual void didMoveToNewDocument(Document& oldDocument) OVERRIDE;

    enum DisplayMode { Unknown, Poster, PosterWaitingForVideo, Video };
    DisplayMode displayMode() const { return m_displayMode; }
    virtual void setDisplayMode(DisplayMode mode) { m_displayMode = mode; }

    virtual bool isMediaElement() const OVERRIDE FINAL { return true; }

    void setControllerInternal(PassRefPtr<MediaController>);

    // Restrictions to change default behaviors.
    enum BehaviorRestrictionFlags {
        NoRestrictions = 0,
        RequireUserGestureForPlayRestriction = 1 << 0,
        RequireUserGestureForFullscreenRestriction = 1 << 1,
    };
    typedef unsigned BehaviorRestrictions;

    bool userGestureRequiredForPlay() const { return m_restrictions & RequireUserGestureForPlayRestriction; }
    bool userGestureRequiredForFullscreen() const { return m_restrictions & RequireUserGestureForFullscreenRestriction; }

    void addBehaviorRestriction(BehaviorRestrictions restriction) { m_restrictions |= restriction; }
    void removeBehaviorRestriction(BehaviorRestrictions restriction) { m_restrictions &= ~restriction; }

    bool ignoreTrackDisplayUpdateRequests() const { return m_ignoreTrackDisplayUpdate > 0; }
    void beginIgnoringTrackDisplayUpdateRequests();
    void endIgnoringTrackDisplayUpdateRequests();

private:
    void createMediaPlayer();

    virtual bool alwaysCreateUserAgentShadowRoot() const OVERRIDE FINAL { return true; }
    virtual bool areAuthorShadowsAllowed() const OVERRIDE FINAL { return false; }

    virtual bool hasCustomFocusLogic() const OVERRIDE FINAL;
    virtual bool supportsFocus() const OVERRIDE FINAL;
    virtual bool isMouseFocusable() const OVERRIDE FINAL;
    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE;
    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE FINAL;
    virtual void removedFrom(ContainerNode*) OVERRIDE FINAL;
    virtual void didRecalcStyle(StyleRecalcChange) OVERRIDE FINAL;

    virtual void didBecomeFullscreenElement() OVERRIDE FINAL;
    virtual void willStopBeingFullscreenElement() OVERRIDE FINAL;
    virtual bool isInteractiveContent() const OVERRIDE FINAL;

    // ActiveDOMObject functions.
    virtual void stop() OVERRIDE FINAL;

    virtual void updateDisplayState() { }

    void setReadyState(MediaPlayer::ReadyState);
    void setNetworkState(MediaPlayer::NetworkState);

    virtual void mediaPlayerNetworkStateChanged() OVERRIDE FINAL;
    virtual void mediaPlayerReadyStateChanged() OVERRIDE FINAL;
    virtual void mediaPlayerTimeChanged() OVERRIDE FINAL;
    virtual void mediaPlayerDurationChanged() OVERRIDE FINAL;
    virtual void mediaPlayerPlaybackStateChanged() OVERRIDE FINAL;
    virtual void mediaPlayerRequestFullscreen() OVERRIDE FINAL;
    virtual void mediaPlayerRequestSeek(double) OVERRIDE FINAL;
    virtual void mediaPlayerRepaint() OVERRIDE FINAL;
    virtual void mediaPlayerSizeChanged() OVERRIDE FINAL;

    virtual void mediaPlayerKeyAdded(const String& keySystem, const String& sessionId) OVERRIDE FINAL;
    virtual void mediaPlayerKeyError(const String& keySystem, const String& sessionId, MediaPlayerClient::MediaKeyErrorCode, unsigned short systemCode) OVERRIDE FINAL;
    virtual void mediaPlayerKeyMessage(const String& keySystem, const String& sessionId, const unsigned char* message, unsigned messageLength, const KURL& defaultURL) OVERRIDE FINAL;
    virtual bool mediaPlayerKeyNeeded(const String& contentType, const unsigned char* initData, unsigned initDataLength) OVERRIDE FINAL;

    virtual CORSMode mediaPlayerCORSMode() const OVERRIDE FINAL;

    virtual void mediaPlayerSetWebLayer(blink::WebLayer*) OVERRIDE FINAL;
    virtual void mediaPlayerSetOpaque(bool) OVERRIDE FINAL;
    virtual void mediaPlayerMediaSourceOpened(blink::WebMediaSource*) OVERRIDE FINAL;

    void loadTimerFired(Timer<HTMLMediaElement>*);
    void progressEventTimerFired(Timer<HTMLMediaElement>*);
    void playbackProgressTimerFired(Timer<HTMLMediaElement>*);
    void startPlaybackProgressTimer();
    void startProgressEventTimer();
    void stopPeriodicTimers();

    void seek(double time, ExceptionState&);
    void finishSeek();
    void checkIfSeekNeeded();
    void addPlayedRange(double start, double end);

    void scheduleTimeupdateEvent(bool periodicEvent);
    void scheduleEvent(const AtomicString& eventName);

    // loading
    void prepareForLoad();
    void loadInternal();
    void selectMediaResource();
    void loadResource(const KURL&, ContentType&, const String& keySystem);
    void scheduleNextSourceChild();
    void loadNextSourceChild();
    void userCancelledLoad();
    void clearMediaPlayer(int flags);
    void clearMediaPlayerAndAudioSourceProviderClient();
    bool havePotentialSourceChild();
    void noneSupported();
    void mediaEngineError(PassRefPtr<MediaError> err);
    void cancelPendingEventsAndCallbacks();
    void waitForSourceChange();
    void prepareToPlay();

    KURL selectNextSourceChild(ContentType*, String* keySystem, InvalidURLAction);

    void mediaLoadingFailed(MediaPlayer::NetworkState);

    void updateActiveTextTrackCues(double);
    HTMLTrackElement* showingTrackWithSameKind(HTMLTrackElement*) const;

    void markCaptionAndSubtitleTracksAsUnconfigured();

    // This does not check user gesture restrictions.
    void playInternal();

    void allowVideoRendering();

    void updateVolume();
    void updatePlayState();
    bool potentiallyPlaying() const;
    bool endedPlayback() const;
    bool stoppedDueToErrors() const;
    bool pausedForUserInteraction() const;
    bool couldPlayIfEnoughData() const;

    // Pauses playback without changing any states or generating events
    void setPausedInternal(bool);

    void setPlaybackRateInternal(double);

    void setShouldDelayLoadEvent(bool);
    void invalidateCachedTime();
    void refreshCachedTime() const;

    bool hasMediaControls() const;
    bool createMediaControls();
    void configureMediaControls();

    void prepareMediaFragmentURI();
    void applyMediaFragmentURI();

    virtual void* preDispatchEventHandler(Event*) OVERRIDE FINAL;

    void changeNetworkStateFromLoadingToIdle();

    void removeBehaviorsRestrictionsAfterFirstUserGesture();

    const AtomicString& mediaGroup() const;
    void setMediaGroup(const AtomicString&);
    void updateMediaController();
    bool isBlocked() const;
    bool isBlockedOnMediaController() const;
    bool isAutoplaying() const { return m_autoplaying; }

    // Currently we have both EME v0.1b and EME WD implemented in media element.
    // But we do not want to support both at the same time. The one used first
    // will be supported. Use |m_emeMode| to track this selection.
    // FIXME: Remove EmeMode once EME v0.1b support is removed. See crbug.com/249976.
    enum EmeMode { EmeModeNotSelected, EmeModePrefixed, EmeModeUnprefixed };

    // check (and set if necessary) the encrypted media extensions (EME) mode
    // (v0.1b or WD). Returns whether the mode is allowed and successfully set.
    bool setEmeMode(EmeMode, ExceptionState&);

    blink::WebContentDecryptionModule* contentDecryptionModule();
    void setMediaKeysInternal(MediaKeys*);

    Timer<HTMLMediaElement> m_loadTimer;
    Timer<HTMLMediaElement> m_progressEventTimer;
    Timer<HTMLMediaElement> m_playbackProgressTimer;
    RefPtr<TimeRanges> m_playedTimeRanges;
    OwnPtr<GenericEventQueue> m_asyncEventQueue;

    double m_playbackRate;
    double m_defaultPlaybackRate;
    NetworkState m_networkState;
    ReadyState m_readyState;
    ReadyState m_readyStateMaximum;
    KURL m_currentSrc;

    RefPtr<MediaError> m_error;

    double m_volume;
    double m_lastSeekTime;

    double m_previousProgressTime;

    // Cached duration to suppress duplicate events if duration unchanged.
    double m_duration;

    // The last time a timeupdate event was sent (wall clock).
    double m_lastTimeUpdateEventWallTime;

    // The last time a timeupdate event was sent in movie time.
    double m_lastTimeUpdateEventMovieTime;

    // Loading state.
    enum LoadState { WaitingForSource, LoadingFromSrcAttr, LoadingFromSourceElement };
    LoadState m_loadState;
    RefPtr<HTMLSourceElement> m_currentSourceNode;
    RefPtr<Node> m_nextChildNodeToConsider;

    OwnPtr<MediaPlayer> m_player;
    blink::WebLayer* m_webLayer;
    bool m_opaque;

    BehaviorRestrictions m_restrictions;

    MediaPlayer::Preload m_preload;

    DisplayMode m_displayMode;

    RefPtr<HTMLMediaSource> m_mediaSource;

    mutable double m_cachedTime;
    mutable double m_cachedTimeWallClockUpdateTime;
    mutable double m_minimumWallClockTimeToCacheMediaTime;

    double m_fragmentStartTime;
    double m_fragmentEndTime;

    typedef unsigned PendingActionFlags;
    PendingActionFlags m_pendingActionFlags;

    // FIXME: MediaElement has way too many state bits.
    bool m_playing : 1;
    bool m_shouldDelayLoadEvent : 1;
    bool m_haveFiredLoadedData : 1;
    bool m_active : 1;
    bool m_autoplaying : 1;
    bool m_muted : 1;
    bool m_paused : 1;
    bool m_seeking : 1;

    // data has not been loaded since sending a "stalled" event
    bool m_sentStalledEvent : 1;

    // time has not changed since sending an "ended" event
    bool m_sentEndEvent : 1;

    bool m_pausedInternal : 1;

    bool m_closedCaptionsVisible : 1;

    bool m_completelyLoaded : 1;
    bool m_havePreparedToPlay : 1;

    bool m_tracksAreReady : 1;
    bool m_haveVisibleTextTrack : 1;
    bool m_processingPreferenceChange : 1;
    double m_lastTextTrackUpdateTime;

    RefPtr<TextTrackList> m_textTracks;
    Vector<RefPtr<TextTrack> > m_textTracksWhenResourceSelectionBegan;

    CueIntervalTree m_cueTree;

    CueList m_currentlyActiveCues;
    int m_ignoreTrackDisplayUpdate;

#if ENABLE(WEB_AUDIO)
    // This is a weak reference, since m_audioSourceNode holds a reference to us.
    // The value is set just after the MediaElementAudioSourceNode is created.
    // The value is cleared in MediaElementAudioSourceNode::~MediaElementAudioSourceNode().
    MediaElementAudioSourceNode* m_audioSourceNode;
#endif

    friend class MediaController;
    RefPtr<MediaController> m_mediaController;

    friend class TrackDisplayUpdateScope;

    EmeMode m_emeMode;

    RefPtrWillBePersistent<MediaKeys> m_mediaKeys;

    static URLRegistry* s_mediaStreamRegistry;
};

#ifndef NDEBUG
// Template specializations required by PodIntervalTree in debug mode.
template <>
struct ValueToString<double> {
    static String string(const double value)
    {
        return String::number(value);
    }
};

template <>
struct ValueToString<TextTrackCue*> {
    static String string(TextTrackCue* const& cue)
    {
        return cue->toString();
    }
};
#endif

inline bool isHTMLMediaElement(const Node& node)
{
    return node.isElementNode() && toElement(node).isMediaElement();
}

DEFINE_NODE_TYPE_CASTS_WITH_FUNCTION(HTMLMediaElement);

} //namespace

#endif
