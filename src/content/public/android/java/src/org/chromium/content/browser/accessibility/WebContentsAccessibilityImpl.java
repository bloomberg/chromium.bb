// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ReceiverCallNotAllowedException;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.text.SpannableString;
import android.text.style.LocaleSpan;
import android.text.style.SuggestionSpan;
import android.text.style.URLSpan;
import android.util.SparseArray;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewParent;
import android.view.ViewStructure;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityManager.AccessibilityStateChangeListener;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;
import android.view.accessibility.AccessibilityNodeProvider;
import android.view.inputmethod.EditorInfo;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.UserData;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.content.browser.WindowEventObserver;
import org.chromium.content.browser.WindowEventObserverManager;
import org.chromium.content.browser.accessibility.AccessibilityDelegate.AccessibilityCoordinates;
import org.chromium.content.browser.accessibility.captioning.CaptioningController;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl.UserDataFactory;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * Implementation of {@link WebContentsAccessibility} interface.
 * Native accessibility for a {@link WebContents}. Actual native instance is
 * created lazily upon the first request from Android framework on
 *{@link AccessibilityNodeProvider}, and shares the lifetime with {@link WebContents}.
 */
@JNINamespace("content")
public class WebContentsAccessibilityImpl extends AccessibilityNodeProvider
        implements AccessibilityStateChangeListener, WebContentsAccessibility, WindowEventObserver,
                   UserData, BrowserAccessibilityState.Listener {
    // The following constants have been hard coded so we can support actions newer than our
    // minimum SDK without having to break methods into a series of subclasses.
    // Constants defined by AccessibilityNodeInfo per SDK
    // source: https://developer.android.com/reference/android/R.id.html

    // Constants defined in the K SDK. (API Level 19, Android 4)
    private static final int ACTION_COLLAPSE = 0x00080000;
    private static final int ACTION_EXPAND = 0x00040000;

    // Constants defined in the L SDK. (API Level 21+22, Android 5)
    private static final int ACTION_SET_TEXT = 0x00200000;
    private static final String ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE =
            "ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE";

    // Constants defined in the M SDK. (API Level 23, Android 6)
    public static final int ACTION_CONTEXT_CLICK = 0x0102003c;
    public static final int ACTION_SHOW_ON_SCREEN = 0x01020036;
    public static final int ACTION_SCROLL_UP = 0x01020038;
    public static final int ACTION_SCROLL_DOWN = 0x0102003a;
    public static final int ACTION_SCROLL_LEFT = 0x01020039;
    public static final int ACTION_SCROLL_RIGHT = 0x0102003b;
    public static final int ACTION_SCROLL_TO_POSITION = 0x01020037;

    // Constants defined in the N SDK. (API Level 24+25, Android 7)
    public static final int ACTION_SET_PROGRESS = 0x0102003d;
    private static final String ACTION_ARGUMENT_PROGRESS_VALUE =
            "android.view.accessibility.action.ARGUMENT_PROGRESS_VALUE";

    // Constants defined in the O SDK. (API Level 26+27, Android 8)
    public static final int ACTION_MOVE_WINDOW = 0x01020042;

    // Constants defined in the P SDK. (API Level 28, Android 9)
    public static final int ACTION_SHOW_TOOLTIP = 0x01020044;
    public static final int ACTION_HIDE_TOOLTIP = 0x01020045;

    // Constants defined in the Q SDK. (API Level 29, Android 10)
    public static final int ACTION_PAGE_UP = 0x01020046;
    public static final int ACTION_PAGE_DOWN = 0x01020047;
    public static final int ACTION_PAGE_LEFT = 0x01020048;
    public static final int ACTION_PAGE_RIGHT = 0x01020049;

    // Constants defined in the R SDK. (API Level 30, Android 11)
    public static final int ACTION_IME_ENTER = 0x01020054;
    public static final int ACTION_PRESS_AND_HOLD = 0x0102004a;

    // Constants defined for AccessibilityNodeInfo Bundle extras keys.
    public static final String EXTRAS_KEY_CHROME_ROLE = "AccessibilityNodeInfo.chromeRole";
    public static final String EXTRAS_KEY_HAS_IMAGE = "AccessibilityNodeInfo.hasImage";
    public static final String EXTRAS_KEY_HINT = "AccessibilityNodeInfo.hint";
    public static final String EXTRAS_KEY_OFFSCREEN = "AccessibilityNodeInfo.offscreen";
    public static final String EXTRAS_KEY_ROLE_DESCRIPTION =
            "AccessibilityNodeInfo.roleDescription";
    public static final String EXTRAS_KEY_SUPPORTED_ELEMENTS =
            "ACTION_ARGUMENT_HTML_ELEMENT_STRING_VALUES";
    public static final String EXTRAS_KEY_TARGET_URL = "AccessibilityNodeInfo.targetUrl";
    public static final String EXTRAS_KEY_UNCLIPPED_TOP = "AccessibilityNodeInfo.unclippedTop";
    public static final String EXTRAS_KEY_UNCLIPPED_BOTTOM =
            "AccessibilityNodeInfo.unclippedBottom";
    public static final String EXTRAS_KEY_URL = "url";

    // Constant for no granularity selected.
    private static final int NO_GRANULARITY_SELECTED = 0;

    // Delay times for throttling of successive AccessibilityEvents in milliseconds.
    private static final int ACCESSIBILITY_EVENT_DELAY_DEFAULT = 100;
    private static final int ACCESSIBILITY_EVENT_DELAY_HOVER = 50;

    // Throttle time for content invalid utterances. Content invalid will only be announced at most
    // once per this time interval in milliseconds for a given focused node.
    private static final int CONTENT_INVALID_THROTTLE_DELAY = 4500;

    // These are constant names of UMA histograms, and values for custom count histogram.
    private static final String PERCENTAGE_DROPPED_HISTOGRAM =
            "Accessibility.Android.OnDemand.PercentageDropped";
    private static final String EVENTS_DROPPED_HISTOGRAM =
            "Accessibility.Android.OnDemand.EventsDropped";
    private static final int EVENTS_DROPPED_HISTOGRAM_MIN_BUCKET = 1;
    private static final int EVENTS_DROPPED_HISTOGRAM_MAX_BUCKET = 10000;
    private static final int EVENTS_DROPPED_HISTOGRAM_BUCKET_COUNT = 100;

    private static SparseArray<AccessibilityAction> sAccessibilityActionMap =
            new SparseArray<AccessibilityAction>();

    private final AccessibilityDelegate mDelegate;
    protected AccessibilityManager mAccessibilityManager;
    protected final Context mContext;
    private String mProductVersion;
    protected long mNativeObj;
    private Rect mAccessibilityFocusRect;
    private boolean mIsHovering;
    private int mLastHoverId = View.NO_ID;
    private int mCurrentRootId;
    protected View mView;
    private boolean mUserHasTouchExplored;
    private boolean mPendingScrollToMakeNodeVisible;
    private boolean mNotifyFrameInfoInitializedCalled;
    private boolean mAccessibilityEnabledForTesting;
    private int mSelectionGranularity;
    private int mAccessibilityFocusId;
    private int mSelectionNodeId;
    private View mAutofillPopupView;
    private CaptioningController mCaptioningController;
    private boolean mIsCurrentlyExtendingSelection;
    private int mSelectionStart;
    private int mCursorIndex;
    private String mSupportedHtmlElementTypes;

    // Tracker for all actions performed and events sent by this instance, used for testing.
    private AccessibilityActionAndEventTracker mTracker;

    // Whether or not the next selection event should be fired. We only want to sent one traverse
    // and one selection event per granularity move, this ensures no double events while still
    // sending events when the user is using other assistive technology (e.g. external keyboard)
    private boolean mSuppressNextSelectionEvent;

    // Whether native accessibility is allowed.
    private boolean mNativeAccessibilityAllowed;

    // Whether accessibility focus should be set to the page when it finishes loading.
    // This only applies if an accessibility service like TalkBack is running.
    // This is desirable behavior for a browser window, but not for an embedded
    // WebView.
    private boolean mShouldFocusOnPageLoad;

    // If true, the web contents are obscured by another view and we shouldn't
    // return an AccessibilityNodeProvider or process touch exploration events.
    private boolean mIsObscuredByAnotherView;

    // Accessibility touch exploration state.
    private boolean mTouchExplorationEnabled;

    // This array maps a given virtualViewId to an |AccessibilityNodeInfo| for that view. We use
    // this to update a node quickly rather than building from one scratch each time.
    private SparseArray<AccessibilityNodeInfo> mNodeInfoCache = new SparseArray<>();

    // This handles the dispatching of accessibility events. It acts as an intermediary where we can
    // apply throttling rules, delay event construction, etc.
    private AccessibilityEventDispatcher mEventDispatcher;
    private String mSystemLanguageTag;
    private BroadcastReceiver mBroadcastReceiver;

    // These track the last focused content invalid view id and the last time we reported content
    // invalid for that node. Used to ensure we report content invalid on a node once per interval.
    private int mLastContentInvalidViewId;
    private long mLastContentInvalidUtteranceTime;

    // These track the total number of enqueued events, and the total number of dispatched events,
    // so we can report the percentage/number of dropped events.
    private int mTotalEnqueuedEvents;
    private int mTotalDispatchedEvents;

    /**
     * Create a WebContentsAccessibilityImpl object.
     */
    private static class Factory implements UserDataFactory<WebContentsAccessibilityImpl> {
        @Override
        public WebContentsAccessibilityImpl create(WebContents webContents) {
            return createForDelegate(new WebContentsAccessibilityDelegate(webContents));
        }
    }

    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<WebContentsAccessibilityImpl> INSTANCE = new Factory();
    }

    public static WebContentsAccessibilityImpl fromWebContents(WebContents webContents) {
        return ((WebContentsImpl) webContents)
                .getOrSetUserData(
                        WebContentsAccessibilityImpl.class, UserDataFactoryLazyHolder.INSTANCE);
    }

    public static WebContentsAccessibilityImpl fromDelegate(AccessibilityDelegate delegate) {
        // If WebContents exists, {@link #fromWebContents} should be used.
        assert delegate.getWebContents() == null;
        return createForDelegate(delegate);
    }

    private static WebContentsAccessibilityImpl createForDelegate(AccessibilityDelegate delegate) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return new RWebContentsAccessibility(delegate);
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return new PieWebContentsAccessibility(delegate);
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            return new OWebContentsAccessibility(delegate);
        }
        return new WebContentsAccessibilityImpl(delegate);
    }

    protected WebContentsAccessibilityImpl(AccessibilityDelegate delegate) {
        mDelegate = delegate;
        mView = mDelegate.getContainerView();
        mContext = mView.getContext();
        mProductVersion = mDelegate.getProductVersion();
        mAccessibilityManager =
                (AccessibilityManager) mContext.getSystemService(Context.ACCESSIBILITY_SERVICE);
        if (mDelegate.getWebContents() != null) {
            mCaptioningController = new CaptioningController(mDelegate.getWebContents());
            WindowEventObserverManager.from(mDelegate.getWebContents()).addObserver(this);
        } else {
            refreshState();
        }
        mDelegate.setOnScrollPositionChangedCallback(
                () -> handleScrollPositionChanged(mAccessibilityFocusId));

        BrowserAccessibilityState.addListener(this);

        // Define our delays on a per event type basis.
        Map<Integer, Integer> eventThrottleDelays = new HashMap<Integer, Integer>();
        eventThrottleDelays.put(
                AccessibilityEvent.TYPE_VIEW_SCROLLED, ACCESSIBILITY_EVENT_DELAY_DEFAULT);
        eventThrottleDelays.put(
                AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED, ACCESSIBILITY_EVENT_DELAY_DEFAULT);
        eventThrottleDelays.put(
                AccessibilityEvent.TYPE_VIEW_HOVER_ENTER, ACCESSIBILITY_EVENT_DELAY_HOVER);

        // Define events to throttle without regard for |virtualViewId|.
        Set<Integer> viewIndependentEvents = new HashSet<Integer>();
        viewIndependentEvents.add(AccessibilityEvent.TYPE_VIEW_HOVER_ENTER);

        mEventDispatcher =
                new AccessibilityEventDispatcher(new AccessibilityEventDispatcher.Client() {
                    @Override
                    public void postRunnable(Runnable toPost, long delayInMilliseconds) {
                        mView.postDelayed(toPost, delayInMilliseconds);
                    }

                    @Override
                    public void removeRunnable(Runnable toRemove) {
                        mView.removeCallbacks(toRemove);
                    }

                    @Override
                    public boolean dispatchEvent(int virtualViewId, int eventType) {
                        AccessibilityEvent event =
                                buildAccessibilityEvent(virtualViewId, eventType);
                        if (event == null) return false;

                        requestSendAccessibilityEvent(event);

                        // Always send the ENTER and then the EXIT event, to match a standard
                        // Android View.
                        if (eventType == AccessibilityEvent.TYPE_VIEW_HOVER_ENTER) {
                            AccessibilityEvent exitEvent = buildAccessibilityEvent(
                                    mLastHoverId, AccessibilityEvent.TYPE_VIEW_HOVER_EXIT);
                            if (exitEvent != null) {
                                requestSendAccessibilityEvent(exitEvent);
                                mLastHoverId = virtualViewId;
                            } else if (virtualViewId != View.NO_ID
                                    && mLastHoverId != virtualViewId) {
                                // If IDs become mismatched, or on first hover, this will sync the
                                // values again so all further hovers have correct event pairing.
                                mLastHoverId = virtualViewId;
                            }
                        }

                        return true;
                    }
                }, eventThrottleDelays, viewIndependentEvents, new HashSet<Integer>(), false);

        if (mDelegate.getNativeAXTree() != 0) {
            initializeNativeWithAXTreeUpdate(mDelegate.getNativeAXTree());
        }
        // If the AXTree is not provided, native is initialized lazily, when node provider is
        // actually requested.
    }

    /**
     * Called after the native a11y part is initialized. Overridable by subclasses
     * to do initialization that is not required until the native is set up.
     */
    protected void onNativeInit() {
        mAccessibilityFocusId = View.NO_ID;
        mSelectionNodeId = View.NO_ID;
        mIsHovering = false;
        mCurrentRootId = View.NO_ID;

        mSupportedHtmlElementTypes =
                WebContentsAccessibilityImplJni.get().getSupportedHtmlElementTypes(
                        mNativeObj, WebContentsAccessibilityImpl.this);
        mBroadcastReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                mSystemLanguageTag = Locale.getDefault().toLanguageTag();
            }
        };

        // Register a broadcast receiver for locale change.
        if (mView.isAttachedToWindow()) registerLocaleChangeReceiver();

        // Define an initial set of relevant events if OnDemand feature is enabled.
        if (ContentFeatureList.isEnabled(ContentFeatureList.ON_DEMAND_ACCESSIBILITY_EVENTS)) {
            int serviceEventMask = BrowserAccessibilityState.getAccessibilityServiceEventTypeMask();
            mEventDispatcher.updateRelevantEventTypes(convertMaskToEventTypes(serviceEventMask));
            mEventDispatcher.setOnDemandEnabled(true);
        }
    }

    @CalledByNative
    protected void onNativeObjectDestroyed() {
        mNativeObj = 0;
    }

    protected boolean isNativeInitialized() {
        return mNativeObj != 0;
    }

    private boolean isEnabled() {
        return isNativeInitialized() ? WebContentsAccessibilityImplJni.get().isEnabled(
                       mNativeObj, WebContentsAccessibilityImpl.this)
                                     : false;
    }

    @VisibleForTesting
    @Override
    public void setAccessibilityEnabledForTesting() {
        mAccessibilityEnabledForTesting = true;
    }

    @VisibleForTesting
    @Override
    public void addSpellingErrorForTesting(int virtualViewId, int startOffset, int endOffset) {
        WebContentsAccessibilityImplJni.get().addSpellingErrorForTesting(mNativeObj,
                WebContentsAccessibilityImpl.this, virtualViewId, startOffset, endOffset);
    }

    @VisibleForTesting
    public void setMaxContentChangedEventsToFireForTesting(int maxEvents) {
        WebContentsAccessibilityImplJni.get().setMaxContentChangedEventsToFireForTesting(
                mNativeObj, WebContentsAccessibilityImpl.this, maxEvents);
    }

    @VisibleForTesting
    public int getMaxContentChangedEventsToFireForTesting() {
        return WebContentsAccessibilityImplJni.get().getMaxContentChangedEventsToFireForTesting(
                mNativeObj);
    }

    @VisibleForTesting
    public void setAccessibilityTrackerForTesting(AccessibilityActionAndEventTracker tracker) {
        mTracker = tracker;
    }

    @VisibleForTesting
    public void signalEndOfTestForTesting() {
        WebContentsAccessibilityImplJni.get().signalEndOfTestForTesting(mNativeObj);
    }

    @VisibleForTesting
    public void forceRecordUMAHistogramsForTesting() {
        recordUMAHistograms();
    }

    @CalledByNative
    public void handleEndOfTestSignal() {
        // We have received a signal that we have reached the end of a unit test. If we have a
        // tracker listening, set the test is complete.
        if (mTracker != null) {
            mTracker.signalEndOfTest();
        }
    }

    // WindowEventObserver

    @Override
    public void onDetachedFromWindow() {
        mAccessibilityManager.removeAccessibilityStateChangeListener(this);
        mCaptioningController.stopListening();
        if (!isNativeInitialized()) return;
        ContextUtils.getApplicationContext().unregisterReceiver(mBroadcastReceiver);

        // If the OnDemand feature is enabled, log UMA metrics and reset counters.
        if (ContentFeatureList.isEnabled(ContentFeatureList.ON_DEMAND_ACCESSIBILITY_EVENTS)) {
            recordUMAHistograms();
        }
    }

    // Helper method to record UMA histograms for OnDemand feature and reset counters.
    private void recordUMAHistograms() {
        // If we did not enqueue any events, we can ignore the data as a trivial case.
        if (mTotalEnqueuedEvents > 0) {
            // Log the percentage dropped (dispatching 0 events should be 100% dropped).
            RecordHistogram.recordPercentageHistogram(PERCENTAGE_DROPPED_HISTOGRAM,
                    100 - (int) (mTotalDispatchedEvents * 1.0 / mTotalEnqueuedEvents * 100.0));

            // Log the total number of dropped events.
            RecordHistogram.recordCustomCountHistogram(EVENTS_DROPPED_HISTOGRAM,
                    mTotalEnqueuedEvents - mTotalDispatchedEvents,
                    EVENTS_DROPPED_HISTOGRAM_MIN_BUCKET, EVENTS_DROPPED_HISTOGRAM_MAX_BUCKET,
                    EVENTS_DROPPED_HISTOGRAM_BUCKET_COUNT);
        }

        // Reset counters.
        mTotalEnqueuedEvents = 0;
        mTotalDispatchedEvents = 0;
    }

    @Override
    public void onAttachedToWindow() {
        mAccessibilityManager.addAccessibilityStateChangeListener(this);
        refreshState();
        mCaptioningController.startListening();
        registerLocaleChangeReceiver();
    }

    private void registerLocaleChangeReceiver() {
        if (!isNativeInitialized()) return;
        try {
            IntentFilter filter = new IntentFilter(Intent.ACTION_LOCALE_CHANGED);
            ContextUtils.getApplicationContext().registerReceiver(mBroadcastReceiver, filter);
        } catch (ReceiverCallNotAllowedException e) {
            // WebView may be running inside a BroadcastReceiver, in which case registerReceiver is
            // not allowed.
        }
        mSystemLanguageTag = Locale.getDefault().toLanguageTag();
    }

    @Override
    public void onWindowAndroidChanged(WindowAndroid windowAndroid) {
        // Delete this object when switching between WindowAndroids/Activities.
        if (mDelegate.getWebContents() != null) {
            WindowEventObserverManager.from(mDelegate.getWebContents()).removeObserver(this);
            ((WebContentsImpl) mDelegate.getWebContents())
                    .removeUserData(WebContentsAccessibilityImpl.class);
        }

        deleteEarly();
    }

    @Override
    public void destroy() {
        if (mDelegate.getWebContents() == null) deleteEarly();
    }

    protected void deleteEarly() {
        if (mNativeObj != 0) {
            WebContentsAccessibilityImplJni.get().deleteEarly(mNativeObj);
            assert mNativeObj == 0;
        }
    }

    /**
     * Refresh a11y state with that of {@link AccessibilityManager}.
     */
    public void refreshState() {
        setState(mAccessibilityManager.isEnabled());
    }

    // AccessibilityNodeProvider

    @Override
    public AccessibilityNodeProvider getAccessibilityNodeProvider() {
        if (mIsObscuredByAnotherView) return null;

        if (!isNativeInitialized()) {
            if (!mNativeAccessibilityAllowed) return null;
            if (mDelegate.getWebContents() != null) {
                mNativeObj = WebContentsAccessibilityImplJni.get().init(
                        WebContentsAccessibilityImpl.this, mDelegate.getWebContents());
            } else {
                return null;
            }
            onNativeInit();
        }
        if (!isEnabled()) {
            boolean screenReaderMode = BrowserAccessibilityState.screenReaderMode();
            WebContentsAccessibilityImplJni.get().enable(
                    mNativeObj, WebContentsAccessibilityImpl.this, screenReaderMode);
            return null;
        }
        return this;
    }

    protected void initializeNativeWithAXTreeUpdate(long nativeAxTree) {
        assert !isNativeInitialized();

        mNativeObj = WebContentsAccessibilityImplJni.get().initWithAXTree(
                WebContentsAccessibilityImpl.this, nativeAxTree);
        onNativeInit();
    }

    @CalledByNative
    public void clearNodeInfoCacheForGivenId(int virtualViewId) {
        // Recycle and remove the element in our cache for this |virtualViewId|.
        if (mNodeInfoCache.get(virtualViewId) != null) {
            mNodeInfoCache.get(virtualViewId).recycle();
            mNodeInfoCache.remove(virtualViewId);
        }
    }

    @Override
    public AccessibilityNodeInfo createAccessibilityNodeInfo(int virtualViewId) {
        if (!isAccessibilityEnabled()) {
            return null;
        }
        int rootId = WebContentsAccessibilityImplJni.get().getRootId(
                mNativeObj, WebContentsAccessibilityImpl.this);

        if (virtualViewId == View.NO_ID) {
            return createNodeForHost(rootId);
        }

        if (!isFrameInfoInitialized()) {
            return null;
        }

        // We need to create an |AccessibilityNodeInfo| object for this |virtualViewId|. If we have
        // one in our cache, then communicate this so web_contents_accessibility_android.cc
        // will update a fraction of the object and for the rest leverage what is already there.
        if (mNodeInfoCache.get(virtualViewId) != null) {
            AccessibilityNodeInfo cachedNode =
                    AccessibilityNodeInfo.obtain(mNodeInfoCache.get(virtualViewId));

            if (WebContentsAccessibilityImplJni.get().updateCachedAccessibilityNodeInfo(
                        mNativeObj, WebContentsAccessibilityImpl.this, cachedNode, virtualViewId)) {
                // After successfully re-populating this cached node, update the accessibility
                // focus since this would not be included in the update call, and set the
                // available actions accordingly, then return result.
                cachedNode.setAccessibilityFocused(mAccessibilityFocusId == virtualViewId);

                if (mAccessibilityFocusId == virtualViewId) {
                    addAction(cachedNode, AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS);
                    removeAction(cachedNode, AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS);
                } else {
                    removeAction(
                            cachedNode, AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS);
                    addAction(cachedNode, AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS);
                }

                return cachedNode;
            } else {
                // If the node is no longer valid, wipe it from the cache and return null
                mNodeInfoCache.get(virtualViewId).recycle();
                mNodeInfoCache.remove(virtualViewId);
                return null;
            }

        } else {
            // If we have no copy of this node in our cache, build a new one from scratch.
            final AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain(mView);
            info.setPackageName(mContext.getPackageName());
            info.setSource(mView, virtualViewId);

            if (virtualViewId == rootId) {
                info.setParent(mView);
            }

            if (WebContentsAccessibilityImplJni.get().populateAccessibilityNodeInfo(
                        mNativeObj, WebContentsAccessibilityImpl.this, info, virtualViewId)) {
                // After successfully populating this node, add it to our cache then return.
                mNodeInfoCache.put(virtualViewId, AccessibilityNodeInfo.obtain(info));
                return info;
            } else {
                info.recycle();
                return null;
            }
        }
    }

    @Override
    public List<AccessibilityNodeInfo> findAccessibilityNodeInfosByText(
            String text, int virtualViewId) {
        return new ArrayList<AccessibilityNodeInfo>();
    }

    private static boolean isValidMovementGranularity(int granularity) {
        switch (granularity) {
            case AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER:
            case AccessibilityNodeInfo.MOVEMENT_GRANULARITY_WORD:
            case AccessibilityNodeInfo.MOVEMENT_GRANULARITY_LINE:
                return true;
        }
        return false;
    }

    // AccessibilityStateChangeListener
    // TODO(dmazzoni): have BrowserAccessibilityState monitor this and merge
    // into BrowserAccessibilityStateListener.

    @Override
    public void onAccessibilityStateChanged(boolean enabled) {
        setState(enabled);
    }

    // BrowserAccessibilityStateListener

    @Override
    public void onBrowserAccessibilityStateChanged(boolean newScreenReaderEnabledState) {
        if (!isAccessibilityEnabled()) return;

        // Update the AXMode based on screen reader status.
        WebContentsAccessibilityImplJni.get().setAXMode(
                mNativeObj, WebContentsAccessibilityImpl.this, newScreenReaderEnabledState);

        // Update the list of events we dispatch to enabled services.
        if (ContentFeatureList.isEnabled(ContentFeatureList.ON_DEMAND_ACCESSIBILITY_EVENTS)) {
            int serviceEventMask = BrowserAccessibilityState.getAccessibilityServiceEventTypeMask();
            mEventDispatcher.updateRelevantEventTypes(convertMaskToEventTypes(serviceEventMask));
        }
    }

    public Set<Integer> convertMaskToEventTypes(int serviceEventTypes) {
        Set<Integer> relevantEventTypes = new HashSet<Integer>();
        int eventTypeBit;

        while (serviceEventTypes != 0) {
            eventTypeBit = (1 << Integer.numberOfTrailingZeros(serviceEventTypes));
            relevantEventTypes.add(eventTypeBit);
            serviceEventTypes &= ~eventTypeBit;
        }

        return relevantEventTypes;
    }

    // WebContentsAccessibility

    @Override
    public void setObscuredByAnotherView(boolean isObscured) {
        if (isObscured != mIsObscuredByAnotherView) {
            mIsObscuredByAnotherView = isObscured;
            sendAccessibilityEvent(View.NO_ID, AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
        }
    }

    @Override
    public boolean isTouchExplorationEnabled() {
        return mTouchExplorationEnabled;
    }

    @Override
    public void setState(boolean state) {
        if (!state) {
            mNativeAccessibilityAllowed = false;
            mTouchExplorationEnabled = false;
        } else {
            mNativeAccessibilityAllowed = true;
            mTouchExplorationEnabled = mAccessibilityManager.isTouchExplorationEnabled();
        }
    }

    @Override
    public void setShouldFocusOnPageLoad(boolean on) {
        mShouldFocusOnPageLoad = on;
    }

    @Override
    public boolean supportsAction(int action) {
        // TODO(dmazzoni): implement this.
        return false;
    }

    @Override
    public boolean performAction(int action, Bundle arguments) {
        // TODO(dmazzoni): implement this.
        return false;
    }

    @TargetApi(Build.VERSION_CODES.M)
    @Override
    public void onProvideVirtualStructure(
            final ViewStructure structure, final boolean ignoreScrollOffset) {
        // Do not collect accessibility tree in incognito mode
        if (mDelegate.isIncognito()) {
            structure.setChildCount(0);
            return;
        }
        structure.setChildCount(1);
        final ViewStructure viewRoot = structure.asyncNewChild(0);
        viewRoot.setClassName("");
        viewRoot.setHint(mProductVersion);

        WebContents webContents = mDelegate.getWebContents();
        if (webContents != null && !webContents.isDestroyed()) {
            Bundle extras = viewRoot.getExtras();
            extras.putCharSequence(EXTRAS_KEY_URL, webContents.getVisibleUrl().getSpec());
        }

        mDelegate.requestAccessibilitySnapshot(viewRoot, new Runnable() {
            @Override
            public void run() {
                viewRoot.asyncCommit();
            }
        });
    }

    @Override
    public boolean performAction(int virtualViewId, int action, Bundle arguments) {
        // We don't support any actions on the host view or nodes
        // that are not (any longer) in the tree.
        if (!isAccessibilityEnabled()
                || !WebContentsAccessibilityImplJni.get().isNodeValid(
                        mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId)) {
            return false;
        }

        if (mTracker != null) mTracker.addAction(action, arguments);

        switch (action) {
            case AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS:
                if (!moveAccessibilityFocusToId(virtualViewId)) return true;
                if (!mIsHovering) {
                    scrollToMakeNodeVisible(mAccessibilityFocusId);
                } else {
                    mPendingScrollToMakeNodeVisible = true;
                }
                return true;
            case AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS:
                // ALWAYS respond with TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED whether we thought
                // it had focus or not, so that the Android framework cache is correct.
                sendAccessibilityEvent(
                        virtualViewId, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED);
                if (mAccessibilityFocusId == virtualViewId) {
                    WebContentsAccessibilityImplJni.get().moveAccessibilityFocus(mNativeObj,
                            WebContentsAccessibilityImpl.this, mAccessibilityFocusId, View.NO_ID);
                    mAccessibilityFocusId = View.NO_ID;
                    mAccessibilityFocusRect = null;
                }
                if (mLastHoverId == virtualViewId) {
                    sendAccessibilityEvent(mLastHoverId, AccessibilityEvent.TYPE_VIEW_HOVER_EXIT);
                    mLastHoverId = View.NO_ID;
                }
                return true;
            case AccessibilityNodeInfo.ACTION_CLICK:
                if (!mView.hasFocus()) mView.requestFocus();
                performClick(virtualViewId);
                return true;
            case AccessibilityNodeInfo.ACTION_FOCUS:
                if (!mView.hasFocus()) mView.requestFocus();
                WebContentsAccessibilityImplJni.get().focus(
                        mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId);
                return true;
            case AccessibilityNodeInfo.ACTION_CLEAR_FOCUS:
                WebContentsAccessibilityImplJni.get().blur(
                        mNativeObj, WebContentsAccessibilityImpl.this);
                return true;
            case AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT: {
                if (arguments == null) return false;
                String elementType = arguments.getString(
                        AccessibilityNodeInfo.ACTION_ARGUMENT_HTML_ELEMENT_STRING);
                if (elementType == null) return false;
                elementType = elementType.toUpperCase(Locale.US);
                return jumpToElementType(
                        virtualViewId, elementType, /*forwards*/ true, /*canWrap*/ false);
            }
            case AccessibilityNodeInfo.ACTION_PREVIOUS_HTML_ELEMENT: {
                if (arguments == null) return false;
                String elementType = arguments.getString(
                        AccessibilityNodeInfo.ACTION_ARGUMENT_HTML_ELEMENT_STRING);
                if (elementType == null) return false;
                elementType = elementType.toUpperCase(Locale.US);
                return jumpToElementType(virtualViewId, elementType, /*forwards*/ false,
                        /*canWrap*/ virtualViewId == mCurrentRootId);
            }
            case ACTION_SET_TEXT: {
                if (!WebContentsAccessibilityImplJni.get().isEditableText(
                            mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId)) {
                    return false;
                }
                if (arguments == null) return false;
                String newText = arguments.getString(ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE);
                if (newText == null) return false;
                WebContentsAccessibilityImplJni.get().setTextFieldValue(
                        mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId, newText);
                // Match Android framework and set the cursor to the end of the text field.
                WebContentsAccessibilityImplJni.get().setSelection(mNativeObj,
                        WebContentsAccessibilityImpl.this, virtualViewId, newText.length(),
                        newText.length());
                return true;
            }
            case AccessibilityNodeInfo.ACTION_SET_SELECTION: {
                if (!WebContentsAccessibilityImplJni.get().isEditableText(
                            mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId)) {
                    return false;
                }
                int selectionStart = 0;
                int selectionEnd = 0;
                if (arguments != null) {
                    selectionStart = arguments.getInt(
                            AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT);
                    selectionEnd = arguments.getInt(
                            AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT);
                }
                WebContentsAccessibilityImplJni.get().setSelection(mNativeObj,
                        WebContentsAccessibilityImpl.this, virtualViewId, selectionStart,
                        selectionEnd);
                return true;
            }
            case AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY: {
                if (arguments == null) return false;
                int granularity = arguments.getInt(
                        AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT);
                boolean extend = arguments.getBoolean(
                        AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN);
                if (!isValidMovementGranularity(granularity)) {
                    return false;
                }
                return nextAtGranularity(granularity, extend, virtualViewId);
            }
            case AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY: {
                if (arguments == null) return false;
                int granularity = arguments.getInt(
                        AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT);
                boolean extend = arguments.getBoolean(
                        AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN);
                if (!isValidMovementGranularity(granularity)) {
                    return false;
                }
                return previousAtGranularity(granularity, extend, virtualViewId);
            }
            case AccessibilityNodeInfo.ACTION_SCROLL_FORWARD:
                return scrollForward(virtualViewId);
            case AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD:
                return scrollBackward(virtualViewId);
            case AccessibilityNodeInfo.ACTION_CUT:
                if (mDelegate.getWebContents() != null) {
                    ((WebContentsImpl) mDelegate.getWebContents()).cut();
                    return true;
                }
                return false;
            case AccessibilityNodeInfo.ACTION_COPY:
                if (mDelegate.getWebContents() != null) {
                    ((WebContentsImpl) mDelegate.getWebContents()).copy();
                    return true;
                }
                return false;
            case AccessibilityNodeInfo.ACTION_PASTE:
                if (mDelegate.getWebContents() != null) {
                    ((WebContentsImpl) mDelegate.getWebContents()).paste();
                    return true;
                }
                return false;
            case AccessibilityNodeInfo.ACTION_COLLAPSE:
            case AccessibilityNodeInfo.ACTION_EXPAND:
                // If something is collapsible or expandable, just activate it to toggle.
                performClick(virtualViewId);
                return true;
            case ACTION_SHOW_ON_SCREEN:
                scrollToMakeNodeVisible(virtualViewId);
                return true;
            case ACTION_CONTEXT_CLICK:
            case AccessibilityNodeInfo.ACTION_LONG_CLICK:
                WebContentsAccessibilityImplJni.get().showContextMenu(
                        mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId);
                return true;
            case ACTION_SCROLL_UP:
            case ACTION_PAGE_UP:
                return WebContentsAccessibilityImplJni.get().scroll(mNativeObj,
                        WebContentsAccessibilityImpl.this, virtualViewId, ScrollDirection.UP,
                        action == ACTION_PAGE_UP);
            case ACTION_SCROLL_DOWN:
            case ACTION_PAGE_DOWN:
                return WebContentsAccessibilityImplJni.get().scroll(mNativeObj,
                        WebContentsAccessibilityImpl.this, virtualViewId, ScrollDirection.DOWN,
                        action == ACTION_PAGE_DOWN);
            case ACTION_SCROLL_LEFT:
            case ACTION_PAGE_LEFT:
                return WebContentsAccessibilityImplJni.get().scroll(mNativeObj,
                        WebContentsAccessibilityImpl.this, virtualViewId, ScrollDirection.LEFT,
                        action == ACTION_PAGE_LEFT);
            case ACTION_SCROLL_RIGHT:
            case ACTION_PAGE_RIGHT:
                return WebContentsAccessibilityImplJni.get().scroll(mNativeObj,
                        WebContentsAccessibilityImpl.this, virtualViewId, ScrollDirection.RIGHT,
                        action == ACTION_PAGE_RIGHT);
            case ACTION_SET_PROGRESS:
                if (arguments == null) return false;
                float value = arguments.getFloat(ACTION_ARGUMENT_PROGRESS_VALUE, -1);
                if (value == -1) return false;
                return WebContentsAccessibilityImplJni.get().setRangeValue(
                        mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId, value);
            case ACTION_IME_ENTER:
                if (mDelegate.getWebContents() != null) {
                    if (ImeAdapterImpl.fromWebContents(mDelegate.getWebContents()) != null) {
                        // We send an unspecified action to ensure Enter key is hit
                        return ImeAdapterImpl.fromWebContents(mDelegate.getWebContents())
                                .performEditorAction(EditorInfo.IME_ACTION_UNSPECIFIED);
                    }
                }
                return false;
            default:
                break;
        }
        return false;
    }

    @Override
    public void onAutofillPopupDisplayed(View autofillPopupView) {
        if (isAccessibilityEnabled()) {
            mAutofillPopupView = autofillPopupView;
            WebContentsAccessibilityImplJni.get().onAutofillPopupDisplayed(
                    mNativeObj, WebContentsAccessibilityImpl.this);
        }
    }

    @Override
    public void onAutofillPopupDismissed() {
        if (isAccessibilityEnabled()) {
            WebContentsAccessibilityImplJni.get().onAutofillPopupDismissed(
                    mNativeObj, WebContentsAccessibilityImpl.this);
            mAutofillPopupView = null;
        }
    }

    @Override
    public void onAutofillPopupAccessibilityFocusCleared() {
        if (isAccessibilityEnabled()) {
            int id = WebContentsAccessibilityImplJni.get()
                             .getIdForElementAfterElementHostingAutofillPopup(
                                     mNativeObj, WebContentsAccessibilityImpl.this);
            if (id == 0) return;

            moveAccessibilityFocusToId(id);
            scrollToMakeNodeVisible(mAccessibilityFocusId);
        }
    }

    // Returns true if the hover event is to be consumed by accessibility feature.
    @CalledByNative
    private boolean onHoverEvent(int action) {
        if (!isAccessibilityEnabled()) {
            return false;
        }

        if (action == MotionEvent.ACTION_HOVER_EXIT) {
            mIsHovering = false;
            return true;
        }

        mIsHovering = true;
        mUserHasTouchExplored = true;
        return true;
    }

    @Override
    public boolean onHoverEventNoRenderer(MotionEvent event) {
        if (!onHoverEvent(event.getAction())) return false;

        float x = event.getX() + mDelegate.getAccessibilityCoordinates().getScrollX();
        float y = event.getY() + mDelegate.getAccessibilityCoordinates().getScrollY();
        return WebContentsAccessibilityImplJni.get().onHoverEventNoRenderer(mNativeObj, this, x, y);
    }

    /**
     * Notify us when the frame info is initialized,
     * the first time, since until that point, we can't use AccessibilityCoordinates to transform
     * web coordinates to screen coordinates.
     */
    @CalledByNative
    private void notifyFrameInfoInitialized() {
        if (mNotifyFrameInfoInitializedCalled) return;

        mNotifyFrameInfoInitializedCalled = true;

        // Invalidate the container view, since the chrome accessibility tree is now
        // ready and listed as the child of the container view.
        sendAccessibilityEvent(View.NO_ID, AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);

        // (Re-) focus focused element, since we weren't able to create an
        // AccessibilityNodeInfo for this element before.
        if (!mShouldFocusOnPageLoad) return;
        if (mAccessibilityFocusId != View.NO_ID) {
            moveAccessibilityFocusToIdAndRefocusIfNeeded(mAccessibilityFocusId);
        }
    }

    private boolean jumpToElementType(
            int virtualViewId, String elementType, boolean forwards, boolean canWrap) {
        int id = WebContentsAccessibilityImplJni.get().findElementType(mNativeObj,
                WebContentsAccessibilityImpl.this, virtualViewId, elementType, forwards, canWrap);
        if (id == 0) return false;

        moveAccessibilityFocusToId(id);
        scrollToMakeNodeVisible(mAccessibilityFocusId);
        return true;
    }

    private void setGranularityAndUpdateSelection(int granularity) {
        mSelectionGranularity = granularity;

        if (WebContentsAccessibilityImplJni.get().isEditableText(
                    mNativeObj, WebContentsAccessibilityImpl.this, mAccessibilityFocusId)
                && WebContentsAccessibilityImplJni.get().isFocused(
                        mNativeObj, WebContentsAccessibilityImpl.this, mAccessibilityFocusId)) {
            // If selection/cursor are "unassigned" (e.g. first user swipe), then assign as needed
            if (mSelectionStart == -1) {
                mSelectionStart =
                        WebContentsAccessibilityImplJni.get().getEditableTextSelectionStart(
                                mNativeObj, WebContentsAccessibilityImpl.this,
                                mAccessibilityFocusId);
            }
            if (mCursorIndex == -1) {
                mCursorIndex = WebContentsAccessibilityImplJni.get().getEditableTextSelectionEnd(
                        mNativeObj, WebContentsAccessibilityImpl.this, mAccessibilityFocusId);
            }
        }
    }

    private boolean nextAtGranularity(int granularity, boolean extendSelection, int virtualViewId) {
        if (virtualViewId != mSelectionNodeId) return false;
        setGranularityAndUpdateSelection(granularity);

        // This calls finishGranularityMoveNext when it's done.
        // If we are extending or starting a selection, pass the current cursor index, otherwise
        // default to selection start, which will be the position at the end of the last move
        if (extendSelection && mIsCurrentlyExtendingSelection) {
            return WebContentsAccessibilityImplJni.get().nextAtGranularity(mNativeObj,
                    WebContentsAccessibilityImpl.this, mSelectionGranularity, extendSelection,
                    virtualViewId, mCursorIndex);
        } else {
            return WebContentsAccessibilityImplJni.get().nextAtGranularity(mNativeObj,
                    WebContentsAccessibilityImpl.this, mSelectionGranularity, extendSelection,
                    virtualViewId, mSelectionStart);
        }
    }

    private boolean previousAtGranularity(
            int granularity, boolean extendSelection, int virtualViewId) {
        if (virtualViewId != mSelectionNodeId) return false;
        setGranularityAndUpdateSelection(granularity);

        // This calls finishGranularityMovePrevious when it's done.
        return WebContentsAccessibilityImplJni.get().previousAtGranularity(mNativeObj,
                WebContentsAccessibilityImpl.this, mSelectionGranularity, extendSelection,
                virtualViewId, mCursorIndex);
    }

    @CalledByNative
    private void finishGranularityMoveNext(
            String text, boolean extendSelection, int itemStartIndex, int itemEndIndex) {
        // Prepare to send both a selection and a traversal event in sequence.
        AccessibilityEvent selectionEvent = buildAccessibilityEvent(
                mSelectionNodeId, AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED);
        if (selectionEvent == null) return;

        AccessibilityEvent traverseEvent = buildAccessibilityEvent(mSelectionNodeId,
                AccessibilityEvent.TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY);
        if (traverseEvent == null) {
            selectionEvent.recycle();
            return;
        }

        // Build selection event dependent on whether user is extending selection or not
        if (extendSelection) {
            // User started selecting, set the selection start point (only set once per selection)
            if (!mIsCurrentlyExtendingSelection) {
                mIsCurrentlyExtendingSelection = true;
                mSelectionStart = itemStartIndex;
            }

            selectionEvent.setFromIndex(mSelectionStart);
            selectionEvent.setToIndex(itemEndIndex);

        } else {
            // User is no longer selecting, or wasn't originally, reset values
            mIsCurrentlyExtendingSelection = false;
            mSelectionStart = itemEndIndex;

            // Set selection to/from indices to new cursor position, itemEndIndex with forwards nav
            selectionEvent.setFromIndex(itemEndIndex);
            selectionEvent.setToIndex(itemEndIndex);
        }

        // Moving forwards, cursor is now at end of granularity move (itemEndIndex)
        mCursorIndex = itemEndIndex;
        selectionEvent.setItemCount(text.length());

        // Call back to native code to update selection
        setSelection(selectionEvent);

        // Build traverse event, set appropriate action
        traverseEvent.setFromIndex(itemStartIndex);
        traverseEvent.setToIndex(itemEndIndex);
        traverseEvent.setItemCount(text.length());
        traverseEvent.setMovementGranularity(mSelectionGranularity);
        traverseEvent.setContentDescription(text);
        traverseEvent.setAction(AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY);

        requestSendAccessibilityEvent(selectionEvent);
        requestSendAccessibilityEvent(traverseEvent);

        // Suppress the next event since we have already sent traverse and selection for this move
        mSuppressNextSelectionEvent = true;
    }

    @CalledByNative
    private void finishGranularityMovePrevious(
            String text, boolean extendSelection, int itemStartIndex, int itemEndIndex) {
        // Prepare to send both a selection and a traversal event in sequence.
        AccessibilityEvent selectionEvent = buildAccessibilityEvent(
                mSelectionNodeId, AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED);
        if (selectionEvent == null) return;

        AccessibilityEvent traverseEvent = buildAccessibilityEvent(mSelectionNodeId,
                AccessibilityEvent.TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY);
        if (traverseEvent == null) {
            selectionEvent.recycle();
            return;
        }

        // Build selection event dependent on whether user is extending selection or not
        if (extendSelection) {
            // User started selecting, set the selection start point (only set once per selection)
            if (!mIsCurrentlyExtendingSelection) {
                mIsCurrentlyExtendingSelection = true;
                mSelectionStart = itemEndIndex;
            }

            selectionEvent.setFromIndex(mSelectionStart);
            selectionEvent.setToIndex(itemStartIndex);

        } else {
            // User is no longer selecting, or wasn't originally, reset values
            mIsCurrentlyExtendingSelection = false;
            mSelectionStart = itemStartIndex;

            // Set selection to/from indices to new cursor position, itemStartIndex with back nav
            selectionEvent.setFromIndex(itemStartIndex);
            selectionEvent.setToIndex(itemStartIndex);
        }

        // Moving backwards, cursor is now at the start of the granularity move (itemStartIndex)
        mCursorIndex = itemStartIndex;
        selectionEvent.setItemCount(text.length());

        // Call back to native code to update selection
        setSelection(selectionEvent);

        // Build traverse event, set appropriate action
        traverseEvent.setFromIndex(itemStartIndex);
        traverseEvent.setToIndex(itemEndIndex);
        traverseEvent.setItemCount(text.length());
        traverseEvent.setMovementGranularity(mSelectionGranularity);
        traverseEvent.setContentDescription(text);
        traverseEvent.setAction(AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY);

        requestSendAccessibilityEvent(selectionEvent);
        requestSendAccessibilityEvent(traverseEvent);

        // Suppress the next event since we have already sent traverse and selection for this move
        mSuppressNextSelectionEvent = true;
    }

    private void scrollToMakeNodeVisible(int virtualViewId) {
        if (mDelegate.scrollToMakeNodeVisible(getAbsolutePositionForNode(virtualViewId))) return;

        WebContentsAccessibilityImplJni.get().scrollToMakeNodeVisible(
                mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId);
    }

    private void performClick(int virtualViewId) {
        if (mDelegate.performClick(getAbsolutePositionForNode(virtualViewId))) return;

        WebContentsAccessibilityImplJni.get().click(
                mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId);
    }

    private void setSelection(AccessibilityEvent selectionEvent) {
        if (WebContentsAccessibilityImplJni.get().isEditableText(
                    mNativeObj, WebContentsAccessibilityImpl.this, mSelectionNodeId)
                && WebContentsAccessibilityImplJni.get().isFocused(
                        mNativeObj, WebContentsAccessibilityImpl.this, mSelectionNodeId)) {
            WebContentsAccessibilityImplJni.get().setSelection(mNativeObj,
                    WebContentsAccessibilityImpl.this, mSelectionNodeId,
                    selectionEvent.getFromIndex(), selectionEvent.getToIndex());
        }
    }

    private boolean scrollForward(int virtualViewId) {
        if (WebContentsAccessibilityImplJni.get().isSlider(
                    mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId)) {
            return WebContentsAccessibilityImplJni.get().adjustSlider(
                    mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId, true);
        } else {
            return WebContentsAccessibilityImplJni.get().scroll(mNativeObj,
                    WebContentsAccessibilityImpl.this, virtualViewId, ScrollDirection.FORWARD,
                    false);
        }
    }

    private boolean scrollBackward(int virtualViewId) {
        if (WebContentsAccessibilityImplJni.get().isSlider(
                    mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId)) {
            return WebContentsAccessibilityImplJni.get().adjustSlider(
                    mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId, false);
        } else {
            return WebContentsAccessibilityImplJni.get().scroll(mNativeObj,
                    WebContentsAccessibilityImpl.this, virtualViewId, ScrollDirection.BACKWARD,
                    false);
        }
    }

    private boolean moveAccessibilityFocusToId(int newAccessibilityFocusId) {
        if (newAccessibilityFocusId == mAccessibilityFocusId) return false;

        WebContentsAccessibilityImplJni.get().moveAccessibilityFocus(mNativeObj,
                WebContentsAccessibilityImpl.this, mAccessibilityFocusId, newAccessibilityFocusId);

        mAccessibilityFocusId = newAccessibilityFocusId;
        mAccessibilityFocusRect = null;
        // Used to store the node (edit text field) that has input focus but not a11y focus.
        // Usually while the user is typing in an edit text field, a11y is on the IME and input
        // focus is on the edit field. Granularity move needs to know where the input focus is.
        mSelectionNodeId = mAccessibilityFocusId;
        mSelectionGranularity = NO_GRANULARITY_SELECTED;
        mIsCurrentlyExtendingSelection = false;
        mSelectionStart = -1;
        mCursorIndex = WebContentsAccessibilityImplJni.get().getTextLength(
                mNativeObj, WebContentsAccessibilityImpl.this, mAccessibilityFocusId);
        mSuppressNextSelectionEvent = false;

        if (WebContentsAccessibilityImplJni.get().isAutofillPopupNode(
                    mNativeObj, WebContentsAccessibilityImpl.this, mAccessibilityFocusId)) {
            mAutofillPopupView.requestFocus();
        }

        sendAccessibilityEvent(
                mAccessibilityFocusId, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED);
        return true;
    }

    private void moveAccessibilityFocusToIdAndRefocusIfNeeded(int newAccessibilityFocusId) {
        // Work around a bug in the Android framework where it doesn't fully update the object
        // with accessibility focus even if you send it a WINDOW_CONTENT_CHANGED. To work around
        // this, clear focus and then set focus again.
        if (newAccessibilityFocusId == mAccessibilityFocusId) {
            sendAccessibilityEvent(newAccessibilityFocusId,
                    AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED);
            mAccessibilityFocusId = View.NO_ID;
        }
        moveAccessibilityFocusToId(newAccessibilityFocusId);
    }

    /**
     * Send a WINDOW_CONTENT_CHANGED event after a short delay. This helps throttle such
     * events from firing too quickly during animations, for example.
     */
    @CalledByNative
    private void sendDelayedWindowContentChangedEvent() {
        sendAccessibilityEvent(View.NO_ID, AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
    }

    private void sendAccessibilityEvent(int virtualViewId, int eventType) {
        // The container view is indicated by a virtualViewId of NO_ID; post these events directly
        // since there's no web-specific information to attach.
        if (virtualViewId == View.NO_ID) {
            mView.sendAccessibilityEvent(eventType);
            return;
        }

        // Do not send an event when we want to suppress this event, update flag for next event
        if (mSuppressNextSelectionEvent
                && eventType == AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED) {
            mSuppressNextSelectionEvent = false;
            return;
        }

        mTotalEnqueuedEvents++;
        mEventDispatcher.enqueueEvent(virtualViewId, eventType);
    }

    private AccessibilityEvent buildAccessibilityEvent(int virtualViewId, int eventType) {
        // If accessibility is disabled, node is invalid, or we don't have any frame info,
        // then the virtual hierarchy doesn't exist in the view of the Android framework,
        // so should never send any events.
        if (!isAccessibilityEnabled() || !isFrameInfoInitialized()
                || !WebContentsAccessibilityImplJni.get().isNodeValid(
                        mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId)) {
            return null;
        }

        // This is currently needed if we want Android to visually highlight
        // the item that has accessibility focus. In practice, this doesn't seem to slow
        // things down, because it's only called when the accessibility focus moves.
        // TODO(dmazzoni): remove this if/when Android framework fixes bug.
        mView.postInvalidate();

        final AccessibilityEvent event = AccessibilityEvent.obtain(eventType);
        event.setPackageName(mContext.getPackageName());
        event.setSource(mView, virtualViewId);
        if (eventType == AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED) {
            event.setContentChangeTypes(AccessibilityEvent.CONTENT_CHANGE_TYPE_SUBTREE);
        }
        if (!WebContentsAccessibilityImplJni.get().populateAccessibilityEvent(mNativeObj,
                    WebContentsAccessibilityImpl.this, event, virtualViewId, eventType)) {
            event.recycle();
            return null;
        }
        return event;
    }

    private Bundle getOrCreateBundleForAccessibilityEvent(AccessibilityEvent event) {
        Bundle bundle = (Bundle) event.getParcelableData();
        if (bundle == null) {
            bundle = new Bundle();
            event.setParcelableData(bundle);
        }
        return bundle;
    }

    @Override
    public boolean isAccessibilityEnabled() {
        return isNativeInitialized()
                && (mAccessibilityEnabledForTesting || mAccessibilityManager.isEnabled());
    }

    private AccessibilityNodeInfo createNodeForHost(int rootId) {
        // Since we don't want the parent to be focusable, but we can't remove
        // actions from a node, copy over the necessary fields.
        final AccessibilityNodeInfo result = AccessibilityNodeInfo.obtain(mView);
        final AccessibilityNodeInfo source = AccessibilityNodeInfo.obtain(mView);
        mView.onInitializeAccessibilityNodeInfo(source);

        // Copy over parent and screen bounds.
        Rect rect = new Rect();
        source.getBoundsInParent(rect);
        result.setBoundsInParent(rect);
        source.getBoundsInScreen(rect);
        result.setBoundsInScreen(rect);

        // Set up the parent view, if applicable.
        final ViewParent parent = mView.getParentForAccessibility();
        if (parent instanceof View) {
            result.setParent((View) parent);
        }

        // Populate the minimum required fields.
        result.setVisibleToUser(source.isVisibleToUser());
        result.setEnabled(source.isEnabled());
        result.setPackageName(source.getPackageName());
        result.setClassName(source.getClassName());

        // Add the Chrome root node.
        if (isFrameInfoInitialized()) {
            result.addChild(mView, rootId);
        }

        return result;
    }

    /**
     * Returns whether or not the frame info is initialized, meaning we can safely
     * convert web coordinates to screen coordinates. When this is first initialized,
     * notifyFrameInfoInitialized is called - but we shouldn't check whether or not
     * that method was called as a way to determine if frame info is valid because
     * notifyFrameInfoInitialized might not be called at all if AccessibilityCoordinates
     * gets initialized first.
     */
    private boolean isFrameInfoInitialized() {
        if (mDelegate.getWebContents() == null && mNativeObj == 0) {
            // We already got frame info since WebContents finished its lifecycle.
            return true;
        }

        AccessibilityCoordinates ac = mDelegate.getAccessibilityCoordinates();
        return ac.getContentWidthCss() != 0.0 || ac.getContentHeightCss() != 0.0;
    }

    @CalledByNative
    private void handlePageLoaded(int id) {
        // If |mShouldFocusOnPageLoad| is false, that means this is a WebView and we should disable
        // the image descriptions alt-text hints.
        if (!mShouldFocusOnPageLoad) {
            WebContentsAccessibilityImplJni.get().setIsRunningAsWebView(
                    mNativeObj, WebContentsAccessibilityImpl.this, true);
            return;
        }
        if (mUserHasTouchExplored) return;
        moveAccessibilityFocusToIdAndRefocusIfNeeded(id);
    }

    @CalledByNative
    private void handleFocusChanged(int id) {
        // If |mShouldFocusOnPageLoad| is false, that means this is a WebView and
        // we should avoid moving accessibility focus when the page loads, but more
        // generally we should avoid moving accessibility focus whenever it's not
        // already within this WebView.
        if (!mShouldFocusOnPageLoad && mAccessibilityFocusId == View.NO_ID) return;

        sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_FOCUSED);
        moveAccessibilityFocusToId(id);
    }

    @CalledByNative
    private void handleCheckStateChanged(int id) {
        // If the node has accessibility focus, fire TYPE_VIEW_CLICKED event. This check ensures
        // only necessary announcements are made (e.g. changing a radio group selection
        // would erroneously announce "checked not checked" without this check)
        if (mAccessibilityFocusId == id) {
            sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_CLICKED);
        }
    }

    @CalledByNative
    private void handleClicked(int id) {
        sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_CLICKED);
    }

    @CalledByNative
    private void handleTextSelectionChanged(int id) {
        sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED);
    }

    @CalledByNative
    private void handleEditableTextChanged(int id) {
        sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED);
    }

    @CalledByNative
    private void handleSliderChanged(int id) {
        // If the node has accessibility focus, fire TYPE_VIEW_SELECTED, which triggers
        // TalkBack to announce the change. If not, fire TYPE_VIEW_SCROLLED, which
        // does not trigger an immediate announcement but still ensures some event is fired.
        if (mAccessibilityFocusId == id) {
            sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_SELECTED);
        } else {
            sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_SCROLLED);
        }
    }

    @CalledByNative
    private void handleContentChanged(int id) {
        int rootId = WebContentsAccessibilityImplJni.get().getRootId(
                mNativeObj, WebContentsAccessibilityImpl.this);
        if (rootId != mCurrentRootId) {
            mCurrentRootId = rootId;
            sendAccessibilityEvent(View.NO_ID, AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
        } else {
            sendAccessibilityEvent(id, AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
        }
    }

    @CalledByNative
    private void handleNavigate() {
        mAccessibilityFocusId = View.NO_ID;
        mAccessibilityFocusRect = null;
        mUserHasTouchExplored = false;
        // Invalidate the host, since its child is now gone.
        sendAccessibilityEvent(View.NO_ID, AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
    }

    @CalledByNative
    protected void handleScrollPositionChanged(int id) {
        sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_SCROLLED);
    }

    @CalledByNative
    private void handleScrolledToAnchor(int id) {
        moveAccessibilityFocusToId(id);
    }

    @CalledByNative
    private void handleHover(int id) {
        if (mLastHoverId == id) return;
        if (!mIsHovering) return;

        sendAccessibilityEvent(id, AccessibilityEvent.TYPE_VIEW_HOVER_ENTER);
    }

    @CalledByNative
    protected void handleDialogModalOpened(int virtualViewId) {
        // Requires P or higher.
    }

    @CalledByNative
    private void announceLiveRegionText(String text) {
        if (isAccessibilityEnabled()) {
            AccessibilityEvent event =
                    AccessibilityEvent.obtain(AccessibilityEvent.TYPE_ANNOUNCEMENT);
            if (event == null) return;

            event.getText().add(text);
            event.setContentDescription(null);
            requestSendAccessibilityEvent(event);
        }
    }

    @CalledByNative
    private void setAccessibilityNodeInfoParent(AccessibilityNodeInfo node, int parentId) {
        node.setParent(mView, parentId);
    }

    @CalledByNative
    private void addAccessibilityNodeInfoChildren(AccessibilityNodeInfo node, int[] childIds) {
        for (int childId : childIds) {
            node.addChild(mView, childId);
        }
    }

    @CalledByNative
    private void setAccessibilityNodeInfoBooleanAttributes(AccessibilityNodeInfo node,
            int virtualViewId, boolean checkable, boolean checked, boolean clickable,
            boolean contentInvalid, boolean enabled, boolean focusable, boolean focused,
            boolean hasImage, boolean password, boolean scrollable, boolean selected,
            boolean visibleToUser) {
        node.setCheckable(checkable);
        node.setChecked(checked);
        node.setClickable(clickable);
        node.setEnabled(enabled);
        node.setFocusable(focusable);
        node.setFocused(focused);
        node.setPassword(password);
        node.setScrollable(scrollable);
        node.setSelected(selected);
        node.setVisibleToUser(visibleToUser);

        // In the special case that we have invalid content on a focused field, we only want to
        // report that to the user at most once per {@link CONTENT_INVALID_THROTTLE_DELAY} time
        // interval, to be less jarring to the user.
        if (contentInvalid && focused) {
            if (virtualViewId == mLastContentInvalidViewId) {
                // If we are focused on the same node as before, check if it has been longer than
                // our delay since our last utterance, and if so, report invalid content and update
                // our last reported time, otherwise suppress reporting content invalid.
                if (Calendar.getInstance().getTimeInMillis() - mLastContentInvalidUtteranceTime
                        >= CONTENT_INVALID_THROTTLE_DELAY) {
                    mLastContentInvalidUtteranceTime = Calendar.getInstance().getTimeInMillis();
                    node.setContentInvalid(true);
                }
            } else {
                // When we are focused on a new node, report as normal and track new time.
                mLastContentInvalidViewId = virtualViewId;
                mLastContentInvalidUtteranceTime = Calendar.getInstance().getTimeInMillis();
                node.setContentInvalid(true);
            }
        } else {
            // For non-focused fields we want to set contentInvalid as normal.
            node.setContentInvalid(contentInvalid);
        }

        if (hasImage) {
            Bundle bundle = node.getExtras();
            bundle.putCharSequence(EXTRAS_KEY_HAS_IMAGE, "true");
        }

        node.setMovementGranularities(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER
                | AccessibilityNodeInfo.MOVEMENT_GRANULARITY_WORD
                | AccessibilityNodeInfo.MOVEMENT_GRANULARITY_LINE);

        node.setAccessibilityFocused(mAccessibilityFocusId == virtualViewId);
    }

    // The Android SDK requires us to call AccessibilityNodeInfo.addAction and
    // AccessibilityNodeInfo.removeAction with an AccessibilityAction argument, but to simplify
    // things, we just cache a set of AccessibilityActions mapped by their ID.
    protected void addAction(AccessibilityNodeInfo node, int actionId) {
        AccessibilityAction action = sAccessibilityActionMap.get(actionId);
        if (action == null) {
            action = new AccessibilityAction(actionId, null);
            sAccessibilityActionMap.put(actionId, action);
        }
        node.addAction(action);
    }

    protected void removeAction(AccessibilityNodeInfo node, int actionId) {
        AccessibilityAction action = sAccessibilityActionMap.get(actionId);
        if (action == null) {
            action = new AccessibilityAction(actionId, null);
            sAccessibilityActionMap.put(actionId, action);
        }
        node.removeAction(action);
    }

    @CalledByNative
    private void addAccessibilityNodeInfoActions(AccessibilityNodeInfo node, int virtualViewId,
            boolean canScrollForward, boolean canScrollBackward, boolean canScrollUp,
            boolean canScrollDown, boolean canScrollLeft, boolean canScrollRight, boolean clickable,
            boolean editableText, boolean enabled, boolean focusable, boolean focused,
            boolean isCollapsed, boolean isExpanded, boolean hasNonEmptyValue,
            boolean hasNonEmptyInnerText, boolean isSeekControl, boolean isForm) {
        addAction(node, AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT);
        addAction(node, AccessibilityNodeInfo.ACTION_PREVIOUS_HTML_ELEMENT);
        addAction(node, ACTION_SHOW_ON_SCREEN);
        addAction(node, ACTION_CONTEXT_CLICK);

        // We choose to not add ACTION_LONG_CLICK to nodes to prevent verbose utterances.
        // addAction(node, AccessibilityNodeInfo.ACTION_LONG_CLICK);

        if (hasNonEmptyInnerText) {
            addAction(node, AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY);
            addAction(node, AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY);
        }

        if (editableText && enabled) {
            // TODO: don't support actions that modify it if it's read-only (but
            // SET_SELECTION and COPY are okay).
            addAction(node, ACTION_SET_TEXT);
            addAction(node, AccessibilityNodeInfo.ACTION_PASTE);
            addAction(node, ACTION_IME_ENTER);

            if (hasNonEmptyValue) {
                addAction(node, AccessibilityNodeInfo.ACTION_SET_SELECTION);
                addAction(node, AccessibilityNodeInfo.ACTION_CUT);
                addAction(node, AccessibilityNodeInfo.ACTION_COPY);
            }
        }

        if (canScrollForward) {
            addAction(node, AccessibilityNodeInfo.ACTION_SCROLL_FORWARD);
        }

        if (canScrollBackward) {
            addAction(node, AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD);
        }

        if (canScrollUp) {
            addAction(node, ACTION_SCROLL_UP);
            addAction(node, ACTION_PAGE_UP);
        }

        if (canScrollDown) {
            addAction(node, ACTION_SCROLL_DOWN);
            addAction(node, ACTION_PAGE_DOWN);
        }

        if (canScrollLeft) {
            addAction(node, ACTION_SCROLL_LEFT);
            addAction(node, ACTION_PAGE_LEFT);
        }

        if (canScrollRight) {
            addAction(node, ACTION_SCROLL_RIGHT);
            addAction(node, ACTION_PAGE_RIGHT);
        }

        if (focusable) {
            if (focused) {
                addAction(node, AccessibilityNodeInfo.ACTION_CLEAR_FOCUS);
            } else {
                addAction(node, AccessibilityNodeInfo.ACTION_FOCUS);
            }
        }

        if (mAccessibilityFocusId == virtualViewId) {
            addAction(node, AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS);
        } else {
            addAction(node, AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS);
        }

        if (clickable) {
            addAction(node, AccessibilityNodeInfo.ACTION_CLICK);
        }

        if (isCollapsed) {
            addAction(node, ACTION_EXPAND);
        }

        if (isExpanded) {
            addAction(node, ACTION_COLLAPSE);
        }

        if (isSeekControl) {
            addAction(node, ACTION_SET_PROGRESS);
        }
    }

    @CalledByNative
    private void setAccessibilityNodeInfoBaseAttributes(AccessibilityNodeInfo node, boolean isRoot,
            String className, String role, String roleDescription, String hint, String targetUrl,
            boolean canOpenPopup, boolean dismissable, boolean multiLine, int inputType,
            int liveRegion, String errorMessage) {
        node.setClassName(className);

        Bundle bundle = node.getExtras();
        bundle.putCharSequence(EXTRAS_KEY_CHROME_ROLE, role);
        bundle.putCharSequence(EXTRAS_KEY_ROLE_DESCRIPTION, roleDescription);
        bundle.putCharSequence(EXTRAS_KEY_HINT, hint);
        if (!targetUrl.isEmpty()) {
            bundle.putCharSequence(EXTRAS_KEY_TARGET_URL, targetUrl);
        }
        if (isRoot) {
            bundle.putCharSequence(EXTRAS_KEY_SUPPORTED_ELEMENTS, mSupportedHtmlElementTypes);
        }

        node.setCanOpenPopup(canOpenPopup);
        node.setDismissable(dismissable);
        node.setMultiLine(multiLine);
        node.setInputType(inputType);

        // Deliberately don't call setLiveRegion because TalkBack speaks
        // the entire region anytime it changes. Instead Chrome will
        // call announceLiveRegionText() only on the nodes that change.
        // node.setLiveRegion(liveRegion);

        // We only apply the |errorMessage| if {@link setAccessibilityNodeInfoBooleanAttributes}
        // set |contentInvalid| to true based on throttle delay.
        if (node.isContentInvalid()) {
            node.setError(errorMessage);
        }
    }

    @SuppressLint("NewApi")
    @CalledByNative
    protected void setAccessibilityNodeInfoText(AccessibilityNodeInfo node, String text,
            boolean annotateAsLink, boolean isEditableText, String language, int[] suggestionStarts,
            int[] suggestionEnds, String[] suggestions, String stateDescription) {
        CharSequence computedText = computeText(
                text, annotateAsLink, language, suggestionStarts, suggestionEnds, suggestions);

        // For pre-Android R, we add stateDescription to text for backwards compatibility.
        if (stateDescription != null && !stateDescription.isEmpty()) {
            computedText = computedText + ", " + stateDescription;
        }

        // We expose the nested structure of links, which results in the roles of all nested nodes
        // being read. Use content description in the case of links to prevent verbose TalkBack
        if (annotateAsLink) {
            node.setContentDescription(computedText);
        } else {
            node.setText(computedText);
        }
    }

    protected boolean areInlineTextBoxesLoaded(int virtualViewId) {
        return WebContentsAccessibilityImplJni.get().areInlineTextBoxesLoaded(
                mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId);
    }

    protected void loadInlineTextBoxes(int virtualViewId) {
        WebContentsAccessibilityImplJni.get().loadInlineTextBoxes(
                mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId);
    }

    protected int[] getCharacterBoundingBoxes(
            int virtualViewId, int positionInfoStartIndex, int positionInfoLength) {
        return WebContentsAccessibilityImplJni.get().getCharacterBoundingBoxes(mNativeObj,
                WebContentsAccessibilityImpl.this, virtualViewId, positionInfoStartIndex,
                positionInfoLength);
    }

    protected CharSequence computeText(String text, boolean annotateAsLink, String language,
            int[] suggestionStarts, int[] suggestionEnds, String[] suggestions) {
        CharSequence charSequence = text;
        if (annotateAsLink) {
            SpannableString spannable = new SpannableString(text);
            spannable.setSpan(new URLSpan(""), 0, spannable.length(), 0);
            charSequence = spannable;
        }
        if (!language.isEmpty() && !language.equals(mSystemLanguageTag)) {
            SpannableString spannable;
            if (charSequence instanceof SpannableString) {
                spannable = (SpannableString) charSequence;
            } else {
                spannable = new SpannableString(charSequence);
            }
            Locale locale = Locale.forLanguageTag(language);
            spannable.setSpan(new LocaleSpan(locale), 0, spannable.length(), 0);
            charSequence = spannable;
        }

        if (suggestionStarts != null && suggestionStarts.length > 0) {
            assert suggestionEnds != null;
            assert suggestionEnds.length == suggestionStarts.length;
            assert suggestions != null;
            assert suggestions.length == suggestionStarts.length;

            SpannableString spannable;
            if (charSequence instanceof SpannableString) {
                spannable = (SpannableString) charSequence;
            } else {
                spannable = new SpannableString(charSequence);
            }

            int spannableLen = spannable.length();
            for (int i = 0; i < suggestionStarts.length; i++) {
                int start = suggestionStarts[i];
                int end = suggestionEnds[i];
                // Ignore any spans outside the range of the spannable string.
                if (start < 0 || start > spannableLen || end < 0 || end > spannableLen
                        || start > end) {
                    continue;
                }

                String[] suggestionArray = new String[1];
                suggestionArray[0] = suggestions[i];
                int flags = SuggestionSpan.FLAG_MISSPELLED;
                SuggestionSpan suggestionSpan =
                        new SuggestionSpan(mContext, suggestionArray, flags);
                spannable.setSpan(suggestionSpan, start, end, 0);
            }
            charSequence = spannable;
        }

        return charSequence;
    }

    protected void convertWebRectToAndroidCoordinates(Rect rect, Bundle extras) {
        // Offset by the scroll position.
        AccessibilityCoordinates ac = mDelegate.getAccessibilityCoordinates();
        rect.offset(-(int) ac.getScrollX(), -(int) ac.getScrollY());

        // Convert CSS (web) pixels to Android View pixels
        rect.left = (int) ac.fromLocalCssToPix(rect.left);
        rect.top = (int) ac.fromLocalCssToPix(rect.top);
        rect.bottom = (int) ac.fromLocalCssToPix(rect.bottom);
        rect.right = (int) ac.fromLocalCssToPix(rect.right);

        // Offset by the location of the web content within the view.
        rect.offset(0, (int) ac.getContentOffsetYPix());

        // Finally offset by the location of the view within the screen.
        final int[] viewLocation = new int[2];
        mView.getLocationOnScreen(viewLocation);
        rect.offset(viewLocation[0], viewLocation[1]);

        // Clip to the viewport bounds, and add unclipped values to the Bundle.
        int viewportRectTop = viewLocation[1] + (int) ac.getContentOffsetYPix();
        int viewportRectBottom = viewportRectTop + ac.getLastFrameViewportHeightPixInt();
        if (rect.top < viewportRectTop) {
            extras.putInt(EXTRAS_KEY_UNCLIPPED_TOP, rect.top);
            rect.top = viewportRectTop;
        }
        if (rect.bottom > viewportRectBottom) {
            extras.putInt(EXTRAS_KEY_UNCLIPPED_BOTTOM, rect.bottom);
            rect.bottom = viewportRectBottom;
        }
    }

    protected void requestSendAccessibilityEvent(AccessibilityEvent event) {
        // If there is no parent, then the event can be ignored. In general the parent is only
        // transiently null (such as during teardown, switching tabs...). Also ensure that
        // accessibility is still enabled, throttling may result in events sent late.
        if (mView.getParent() != null && isAccessibilityEnabled()) {
            mTotalDispatchedEvents++;
            if (mTracker != null) mTracker.addEvent(event);
            try {
                mView.getParent().requestSendAccessibilityEvent(mView, event);
            } catch (IllegalStateException ignored) {
                // During boot-up of some content shell tests, events will erroneously be sent even
                // though the AccessibilityManager is not enabled, resulting in a crash.
                // TODO(mschillaci): Address flakiness to remove this try/catch, crbug.com/1186376.
            }
        }
    }

    private Rect getAbsolutePositionForNode(int virtualViewId) {
        int[] coords = WebContentsAccessibilityImplJni.get().getAbsolutePositionForNode(
                mNativeObj, WebContentsAccessibilityImpl.this, virtualViewId);
        if (coords == null) return null;

        return new Rect(coords[0], coords[1], coords[2], coords[3]);
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoLocation(AccessibilityNodeInfo node,
            final int virtualViewId, int absoluteLeft, int absoluteTop, int parentRelativeLeft,
            int parentRelativeTop, int width, int height, boolean isRootNode, boolean isOffscreen) {
        // First set the bounds in parent.
        Rect boundsInParent = new Rect(parentRelativeLeft, parentRelativeTop,
                parentRelativeLeft + width, parentRelativeTop + height);
        if (isRootNode) {
            // Offset of the web content relative to the View.
            AccessibilityCoordinates ac = mDelegate.getAccessibilityCoordinates();
            boundsInParent.offset(0, (int) ac.getContentOffsetYPix());
        }
        node.setBoundsInParent(boundsInParent);

        Rect rect = new Rect(absoluteLeft, absoluteTop, absoluteLeft + width, absoluteTop + height);
        convertWebRectToAndroidCoordinates(rect, node.getExtras());

        node.setBoundsInScreen(rect);

        // For nodes that are considered visible to the user, but are offscreen (because they are
        // scrolled offscreen or obscured from view but not programmatically hidden, e.g. through
        // CSS), add to the extras Bundle to inform interested accessibility services.
        if (isOffscreen) {
            node.getExtras().putBoolean(EXTRAS_KEY_OFFSCREEN, true);
        } else {
            // In case of a cached node, remove the offscreen extra if it is there.
            if (node.getExtras().containsKey(EXTRAS_KEY_OFFSCREEN)) {
                node.getExtras().remove(EXTRAS_KEY_OFFSCREEN);
            }
        }

        // Work around a bug in the Android framework where if the object with accessibility
        // focus moves, the accessibility focus rect is not updated - both the visual highlight,
        // and the location on the screen that's clicked if you double-tap. To work around this,
        // when we know the object with accessibility focus moved, move focus away and then
        // move focus right back to it, which tricks Android into updating its bounds.
        if (virtualViewId == mAccessibilityFocusId && virtualViewId != mCurrentRootId) {
            if (mAccessibilityFocusRect == null) {
                mAccessibilityFocusRect = rect;
            } else if (!mAccessibilityFocusRect.equals(rect)) {
                mAccessibilityFocusRect = rect;
                moveAccessibilityFocusToIdAndRefocusIfNeeded(virtualViewId);
            }
        }
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoCollectionInfo(
            AccessibilityNodeInfo node, int rowCount, int columnCount, boolean hierarchical) {
        node.setCollectionInfo(
                AccessibilityNodeInfo.CollectionInfo.obtain(rowCount, columnCount, hierarchical));
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoCollectionItemInfo(AccessibilityNodeInfo node,
            int rowIndex, int rowSpan, int columnIndex, int columnSpan, boolean heading) {
        node.setCollectionItemInfo(AccessibilityNodeInfo.CollectionItemInfo.obtain(
                rowIndex, rowSpan, columnIndex, columnSpan, heading));
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoRangeInfo(
            AccessibilityNodeInfo node, int rangeType, float min, float max, float current) {
        node.setRangeInfo(AccessibilityNodeInfo.RangeInfo.obtain(rangeType, min, max, current));
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoViewIdResourceName(
            AccessibilityNodeInfo node, String viewIdResourceName) {
        node.setViewIdResourceName(viewIdResourceName);
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoOAttributes(
            AccessibilityNodeInfo node, boolean hasCharacterLocations, String hint) {
        // Requires O or higher.
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoPaneTitle(AccessibilityNodeInfo node, String title) {
        // Requires P or higher.
    }

    @CalledByNative
    protected void setAccessibilityNodeInfoSelectionAttrs(
            AccessibilityNodeInfo node, int startIndex, int endIndex) {
        node.setEditable(true);
        node.setTextSelection(startIndex, endIndex);
    }

    @CalledByNative
    private void setAccessibilityEventBaseAttributes(AccessibilityEvent event, boolean checked,
            boolean enabled, boolean password, boolean scrollable, int currentItemIndex,
            int itemCount, int scrollX, int scrollY, int maxScrollX, int maxScrollY,
            String className) {
        event.setChecked(checked);
        event.setEnabled(enabled);
        event.setPassword(password);
        event.setScrollable(scrollable);
        event.setCurrentItemIndex(currentItemIndex);
        event.setItemCount(itemCount);
        event.setScrollX(scrollX);
        event.setScrollY(scrollY);
        event.setMaxScrollX(maxScrollX);
        event.setMaxScrollY(maxScrollY);
        event.setClassName(className);
    }

    @CalledByNative
    private void setAccessibilityEventTextChangedAttrs(AccessibilityEvent event, int fromIndex,
            int addedCount, int removedCount, String beforeText, String text) {
        event.setFromIndex(fromIndex);
        event.setAddedCount(addedCount);
        event.setRemovedCount(removedCount);
        event.setBeforeText(beforeText);
        event.getText().add(text);
    }

    @CalledByNative
    private void setAccessibilityEventSelectionAttrs(
            AccessibilityEvent event, int fromIndex, int toIndex, int itemCount, String text) {
        event.setFromIndex(fromIndex);
        event.setToIndex(toIndex);
        event.setItemCount(itemCount);
        event.getText().add(text);
    }

    boolean isCompatAutofillOnlyPossibleAccessibilityConsumer() {
        // Compatibility Autofill is only available on Android P+.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            return false;
        }
        // The Android Autofill CompatibilityBridge, which is responsible for translating
        // Accessibility information to Autofill events, directly hooks into the
        // AccessibilityManager via an AccessibilityPolicy rather than by running an
        // AccessibilityService. We can thus check whether it is the only consumer of Accessibility
        // information by reading the names of active accessibility services from settings.
        //
        // Note that the CompatibilityBridge makes getEnabledAccessibilityServicesList return a mock
        // service to indicate its presence. It is thus easier to read the setting directly than
        // to filter out this service from the returned list. Furthermore, since Accessibility is
        // only initialized if there is at least one actual service or if Autofill is enabled,
        // there is no need to check that Autofill is enabled here.
        //
        // https://cs.android.com/android/platform/superproject/+/HEAD:frameworks/base/core/java/android/view/autofill/AutofillManager.java;l=2817;drc=dd7d52f9632a0dbb8b14b69520c5ea31e0b3b4a2
        String activeServices = Settings.Secure.getString(
                mContext.getContentResolver(), Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES);
        if (activeServices != null && !activeServices.isEmpty()) {
            return false;
        }
        return true;
    }

    /**
     * On Android O and higher, we should respect whatever is displayed in a password box and
     * report that via accessibility APIs, whether that's the unobscured password, or all dots.
     * However, we deviate from this rule if the only consumer of accessibility information is
     * Autofill in order to allow third-party Autofill services to save the real, unmasked password.
     *
     * Previous to O, shouldExposePasswordText() returns a system setting
     * that determines whether we should return the unobscured password or all
     * dots, independent of what was displayed visually.
     */
    @CalledByNative
    boolean shouldRespectDisplayedPasswordText() {
        if (isCompatAutofillOnlyPossibleAccessibilityConsumer()) {
            return false;
        }
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
    }

    /**
     * Only relevant prior to Android O, see shouldRespectDisplayedPasswordText, unless the only
     * Accessibility consumer is compatibility Autofill.
     */
    @CalledByNative
    boolean shouldExposePasswordText() {
        // Should always expose the actual password text to Autofill so that third-party Autofill
        // services can save it rather than obtain only the masking characters.
        if (isCompatAutofillOnlyPossibleAccessibilityConsumer()) {
            return true;
        }

        ContentResolver contentResolver = mContext.getContentResolver();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            return (Settings.System.getInt(contentResolver, Settings.System.TEXT_SHOW_PASSWORD, 1)
                    == 1);
        }

        return (Settings.Secure.getInt(
                        contentResolver, Settings.Secure.ACCESSIBILITY_SPEAK_PASSWORD, 0)
                == 1);
    }

    @NativeMethods
    interface Natives {
        long init(WebContentsAccessibilityImpl caller, WebContents webContents);
        long initWithAXTree(WebContentsAccessibilityImpl caller, long axTreePtr);
        void deleteEarly(long nativeWebContentsAccessibilityAndroid);
        void onAutofillPopupDisplayed(
                long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller);
        void onAutofillPopupDismissed(
                long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller);
        int getIdForElementAfterElementHostingAutofillPopup(
                long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller);
        int getRootId(
                long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller);
        boolean isNodeValid(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        boolean isAutofillPopupNode(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        boolean isEditableText(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        boolean isFocused(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        int getEditableTextSelectionStart(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        int getEditableTextSelectionEnd(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        int[] getAbsolutePositionForNode(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        boolean updateCachedAccessibilityNodeInfo(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, AccessibilityNodeInfo info, int id);
        boolean populateAccessibilityNodeInfo(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, AccessibilityNodeInfo info, int id);
        boolean populateAccessibilityEvent(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, AccessibilityEvent event, int id,
                int eventType);
        void click(long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller,
                int id);
        void focus(long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller,
                int id);
        void blur(long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller);
        void scrollToMakeNodeVisible(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        int findElementType(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int startId, String elementType,
                boolean forwards, boolean canWrapToLastElement);
        void setTextFieldValue(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id, String newValue);
        void setSelection(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id, int start, int end);
        boolean nextAtGranularity(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int selectionGranularity,
                boolean extendSelection, int id, int cursorIndex);
        boolean previousAtGranularity(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int selectionGranularity,
                boolean extendSelection, int id, int cursorIndex);
        boolean adjustSlider(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id, boolean increment);
        void moveAccessibilityFocus(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int oldId, int newId);
        boolean isSlider(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        boolean scroll(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id, int direction, boolean pageScroll);
        boolean setRangeValue(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id, float value);
        String getSupportedHtmlElementTypes(
                long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller);
        void showContextMenu(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        boolean isEnabled(
                long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller);
        void enable(long nativeWebContentsAccessibilityAndroid, WebContentsAccessibilityImpl caller,
                boolean screenReaderMode);
        void setAXMode(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, boolean screenReaderMode);
        boolean areInlineTextBoxesLoaded(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        void loadInlineTextBoxes(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        int[] getCharacterBoundingBoxes(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id, int start, int len);
        int getTextLength(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id);
        void addSpellingErrorForTesting(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int id, int startOffset, int endOffset);
        void setMaxContentChangedEventsToFireForTesting(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, int maxEvents);
        int getMaxContentChangedEventsToFireForTesting(long nativeWebContentsAccessibilityAndroid);
        void signalEndOfTestForTesting(long nativeWebContentsAccessibilityAndroid);
        void setIsRunningAsWebView(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, boolean isWebView);
        boolean onHoverEventNoRenderer(long nativeWebContentsAccessibilityAndroid,
                WebContentsAccessibilityImpl caller, float x, float y);
    }
}
