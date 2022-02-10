// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import android.graphics.Color;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.flatbuffers.FlatBufferBuilder;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabUserAgent;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tab.WebContentsStateBridge;
import org.chromium.chrome.browser.tab.flatbuffer.CriticalPersistedTabDataFlatBuffer;
import org.chromium.chrome.browser.tab.flatbuffer.LaunchTypeAtCreation;
import org.chromium.chrome.browser.tab.flatbuffer.UserAgentType;
import org.chromium.chrome.browser.tab.proto.CriticalPersistedTabData.CriticalPersistedTabDataProto;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;
import org.chromium.url.GURL;

import java.nio.ByteBuffer;

/**
 * Data which is core to the app and must be retrieved as quickly as possible on startup.
 */
public class CriticalPersistedTabData extends PersistedTabData {
    private static final String TAG = "CriticalPTD";
    private static final Class<CriticalPersistedTabData> USER_DATA_KEY =
            CriticalPersistedTabData.class;

    private static final int UNSPECIFIED_THEME_COLOR = Color.TRANSPARENT;
    private static final String NULL_OPENER_APP_ID = " ";
    public static final long INVALID_TIMESTAMP = -1;

    /**
     * Title of the ContentViews webpage.
     */
    private String mTitle;

    /**
     * URL of the page currently loading. Used as a fall-back in case tab restore fails.
     */
    private GURL mUrl;
    private int mParentId;
    private int mRootId;
    private long mTimestampMillis;
    /**
     * Navigation state of the WebContents as returned by nativeGetContentsStateAsByteBuffer(),
     * stored to be inflated on demand using unfreezeContents(). If this is not null, there is no
     * WebContents around. Upon tab switch WebContents will be unfrozen and the variable will be set
     * to null.
     */
    private WebContentsState mWebContentsState;
    private int mContentStateVersion;
    private String mOpenerAppId;
    private int mThemeColor;
    /**
     * Saves how this tab was initially launched so that we can record metrics on how the
     * tab was created. This is different than {@link Tab#getLaunchType()}, since {@link
     * Tab#getLaunchType()} will be overridden to "FROM_RESTORE" during tab restoration.
     */
    private @Nullable @TabLaunchType Integer mTabLaunchTypeAtCreation;

    private ObserverList<CriticalPersistedTabDataObserver> mObservers =
            new ObserverList<CriticalPersistedTabDataObserver>();
    private boolean mShouldSaveForTesting;
    /** Tab level Request Desktop Site setting. */
    private @TabUserAgent int mUserAgent;

    @VisibleForTesting
    protected CriticalPersistedTabData(Tab tab) {
        super(tab,
                PersistedTabDataConfiguration.get(CriticalPersistedTabData.class, tab.isIncognito())
                        .getStorage(),
                PersistedTabDataConfiguration.get(CriticalPersistedTabData.class, tab.isIncognito())
                        .getId());
    }

    /**
     * @param tab {@link Tab} {@link CriticalPersistedTabData} is being stored for
     * @param parentId parent identiifer for the {@link Tab}
     * @param rootId root identifier for the {@link Tab}
     * @param timestampMillis creation timestamp for the {@link Tab}
     * @param contentStateVersion content state version for the {@link Tab}
     * @param openerAppId identifier for app opener
     * @param themeColor theme color
     * @param launchTypeAtCreation launch type at creation
     * @param userAgent user agent for the {@link Tab}
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    CriticalPersistedTabData(Tab tab, String url, String title, int parentId, int rootId,
            long timestampMillis, WebContentsState webContentsState, int contentStateVersion,
            String openerAppId, int themeColor,
            @Nullable @TabLaunchType Integer launchTypeAtCreation, @TabUserAgent int userAgent) {
        this(tab);
        mUrl = url == null || url.isEmpty() ? GURL.emptyGURL() : new GURL(url);
        mTitle = title;
        mParentId = parentId;
        mRootId = rootId;
        mTimestampMillis = timestampMillis;
        mWebContentsState = webContentsState;
        mContentStateVersion = contentStateVersion;
        mOpenerAppId = openerAppId;
        mThemeColor = themeColor;
        mTabLaunchTypeAtCreation = launchTypeAtCreation;
        mUserAgent = userAgent;
    }

    /**
     * @param tab {@link Tab} {@link CriticalPersistedTabData} is being stored for
     * @param data serialized {@link CriticalPersistedTabData}
     * @param storage {@link PersistedTabDataStorage} for {@link PersistedTabData}
     * @param persistedTabDataId unique identifier for {@link PersistedTabData} in
     * storage
     * This constructor is public because that is needed for the reflection
     * used in PersistedTabData.java
     */
    @VisibleForTesting
    protected CriticalPersistedTabData(
            Tab tab, ByteBuffer data, PersistedTabDataStorage storage, String persistedTabDataId) {
        super(tab, storage, persistedTabDataId);
        deserializeAndLog(data);
    }

    /**
     * TODO(crbug.com/1096142) asynchronous from can be removed
     * Acquire {@link CriticalPersistedTabData} from storage
     * @param tab {@link Tab} {@link CriticalPersistedTabData} is being stored for.
     *        At a minimum this needs to be a frozen {@link Tab} with an identifier
     *        and isIncognito values set.
     * @param callback callback to pass {@link PersistedTabData} back in
     */
    public static void from(Tab tab, Callback<CriticalPersistedTabData> callback) {
        PersistedTabData.from(tab,
                (data, storage, id, factoryCallback)
                        -> {
                    factoryCallback.onResult(new CriticalPersistedTabData(tab, data, storage, id));
                },
                (supplierCallback)
                        -> supplierCallback.onResult(
                                tab.isInitialized() ? CriticalPersistedTabData.build(tab) : null),
                CriticalPersistedTabData.class, callback);
    }

    /**
     * Acquire {@link CriticalPersistedTabData} from a {@link Tab} or create if it doesn't exist
     * @param  tab corresponding {@link Tab} for which {@link CriticalPersistedTabData} is sought
     * @return acquired or created {@link CriticalPersistedTabData}
     */
    public static CriticalPersistedTabData from(Tab tab) {
        return PersistedTabData.from(
                tab, CriticalPersistedTabData.class, () -> { return build(tab); });
    }

    /**
     * Synchronously restore serialized {@link CriticalPersistedTabData}
     * @param tabId identifier for the {@link Tab}
     * @param isIncognito true if the {@link Tab} is incognito
     * @return serialized {@link CriticalPersistedTabData}
     * TODO(crbug.com/1119452) rethink CriticalPersistedTabData contract
     */
    public static ByteBuffer restore(int tabId, boolean isIncognito) {
        PersistedTabDataConfiguration config =
                PersistedTabDataConfiguration.get(CriticalPersistedTabData.class, isIncognito);
        return config.getStorage().restore(tabId, config.getId());
    }

    /**
     * Asynchronously restore serialized {@link CriticalPersistedTabData}
     * @param tabId identifier for the {@link Tab}
     * @param isIncognito true if the {@link Tab} is incognito
     * @param callback the serialized {@link CriticalPersistedTabData} is passed back in
     */
    public static void restore(int tabId, boolean isIncognito, Callback<ByteBuffer> callback) {
        PersistedTabDataConfiguration config =
                PersistedTabDataConfiguration.get(CriticalPersistedTabData.class, isIncognito);
        config.getStorage().restore(tabId, config.getId(), callback);
    }

    /**
     * @param tab {@link Tab} associated with the {@link CriticalPersistedTabData}
     * @param serialized {@link CriticalPersistedTabData} in serialized form
     * @param isCriticalPersistedTabDataEnabled true if CriticalPersistedData is enabled
     * as the storage/retrieval method
     */
    public static void build(Tab tab, ByteBuffer serialized, boolean isStorageRetrievalEnabled) {
        PersistedTabData.build(tab, (data, storage, id, callback) -> {
            callback.onResult(new CriticalPersistedTabData(tab, data, storage, id));
        }, serialized, CriticalPersistedTabData.class, (res) -> {});
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static CriticalPersistedTabData build(Tab tab) {
        // CriticalPersistedTabData is initialized with default values
        CriticalPersistedTabData criticalPersistedTabData = new CriticalPersistedTabData(tab, "",
                "", Tab.INVALID_TAB_ID, tab.getId(), INVALID_TIMESTAMP, null, -1, "",
                UNSPECIFIED_THEME_COLOR, null, TabUserAgent.DEFAULT);
        criticalPersistedTabData.save();
        return criticalPersistedTabData;
    }

    @Override
    boolean deserialize(@Nullable ByteBuffer bytes) {
        try (TraceEvent e = TraceEvent.scoped("CriticalPersistedTabData.Deserialize")) {
            CriticalPersistedTabDataFlatBuffer deserialized =
                    CriticalPersistedTabDataFlatBuffer.getRootAsCriticalPersistedTabDataFlatBuffer(
                            bytes);
            mParentId = deserialized.parentId();
            mRootId = deserialized.rootId();
            mTimestampMillis = deserialized.timestampMillis();
            mWebContentsState =
                    new WebContentsState(deserialized.webContentsStateBytesAsByteBuffer().slice());
            mWebContentsState.setVersion(WebContentsState.CONTENTS_STATE_CURRENT_VERSION);
            mUrl = mWebContentsState.getVirtualUrlFromState() == null
                    ? GURL.emptyGURL()
                    : new GURL(mWebContentsState.getVirtualUrlFromState());
            mTitle = mWebContentsState.getDisplayTitleFromState();
            mContentStateVersion = deserialized.contentStateVersion();
            mOpenerAppId = NULL_OPENER_APP_ID.equals(deserialized.openerAppId())
                    ? null
                    : deserialized.openerAppId();
            mThemeColor = deserialized.themeColor();
            mTabLaunchTypeAtCreation = getLaunchType(deserialized.launchTypeAtCreation());
            mUserAgent = getTabUserAgentType(deserialized.userAgent());
            return true;
        }
    }

    @Override
    public String getUmaTag() {
        return "Critical";
    }

    @VisibleForTesting
    static @Nullable @TabLaunchType Integer getLaunchType(int flatBufferLaunchType) {
        switch (flatBufferLaunchType) {
            case LaunchTypeAtCreation.FROM_LINK:
                return TabLaunchType.FROM_LINK;
            case LaunchTypeAtCreation.FROM_EXTERNAL_APP:
                return TabLaunchType.FROM_EXTERNAL_APP;
            case LaunchTypeAtCreation.FROM_CHROME_UI:
                return TabLaunchType.FROM_CHROME_UI;
            case LaunchTypeAtCreation.FROM_RESTORE:
                return TabLaunchType.FROM_RESTORE;
            case LaunchTypeAtCreation.FROM_LONGPRESS_FOREGROUND:
                return TabLaunchType.FROM_LONGPRESS_FOREGROUND;
            case LaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND:
                return TabLaunchType.FROM_LONGPRESS_BACKGROUND;
            case LaunchTypeAtCreation.FROM_REPARENTING:
                return TabLaunchType.FROM_REPARENTING;
            case LaunchTypeAtCreation.FROM_LAUNCHER_SHORTCUT:
                return TabLaunchType.FROM_LAUNCHER_SHORTCUT;
            case LaunchTypeAtCreation.FROM_SPECULATIVE_BACKGROUND_CREATION:
                return TabLaunchType.FROM_SPECULATIVE_BACKGROUND_CREATION;
            case LaunchTypeAtCreation.FROM_BROWSER_ACTIONS:
                return TabLaunchType.FROM_BROWSER_ACTIONS;
            case LaunchTypeAtCreation.FROM_LAUNCH_NEW_INCOGNITO_TAB:
                return TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB;
            case LaunchTypeAtCreation.FROM_STARTUP:
                return TabLaunchType.FROM_STARTUP;
            case LaunchTypeAtCreation.FROM_START_SURFACE:
                return TabLaunchType.FROM_START_SURFACE;
            case LaunchTypeAtCreation.FROM_TAB_GROUP_UI:
                return TabLaunchType.FROM_TAB_GROUP_UI;
            case LaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND_IN_GROUP:
                return TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP;
            case LaunchTypeAtCreation.FROM_APP_WIDGET:
                return TabLaunchType.FROM_APP_WIDGET;
            case LaunchTypeAtCreation.SIZE:
                return TabLaunchType.SIZE;
            default:
                assert false : "Unexpected deserialization of LaunchAtCreationType: "
                               + flatBufferLaunchType;
                // shouldn't happen
                return null;
        }
    }

    @VisibleForTesting
    static int getLaunchType(@Nullable @TabLaunchType Integer tabLaunchType) {
        if (tabLaunchType == null) {
            return LaunchTypeAtCreation.UNKNOWN;
        }
        switch (tabLaunchType) {
            case TabLaunchType.FROM_LINK:
                return LaunchTypeAtCreation.FROM_LINK;
            case TabLaunchType.FROM_EXTERNAL_APP:
                return LaunchTypeAtCreation.FROM_EXTERNAL_APP;
            case TabLaunchType.FROM_CHROME_UI:
                return LaunchTypeAtCreation.FROM_CHROME_UI;
            case TabLaunchType.FROM_RESTORE:
                return LaunchTypeAtCreation.FROM_RESTORE;
            case TabLaunchType.FROM_LONGPRESS_FOREGROUND:
                return LaunchTypeAtCreation.FROM_LONGPRESS_FOREGROUND;
            case TabLaunchType.FROM_LONGPRESS_BACKGROUND:
                return LaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND;
            case TabLaunchType.FROM_REPARENTING:
                return LaunchTypeAtCreation.FROM_REPARENTING;
            case TabLaunchType.FROM_LAUNCHER_SHORTCUT:
                return LaunchTypeAtCreation.FROM_LAUNCHER_SHORTCUT;
            case TabLaunchType.FROM_SPECULATIVE_BACKGROUND_CREATION:
                return LaunchTypeAtCreation.FROM_SPECULATIVE_BACKGROUND_CREATION;
            case TabLaunchType.FROM_BROWSER_ACTIONS:
                return LaunchTypeAtCreation.FROM_BROWSER_ACTIONS;
            case TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB:
                return LaunchTypeAtCreation.FROM_LAUNCH_NEW_INCOGNITO_TAB;
            case TabLaunchType.FROM_STARTUP:
                return LaunchTypeAtCreation.FROM_STARTUP;
            case TabLaunchType.FROM_START_SURFACE:
                return LaunchTypeAtCreation.FROM_START_SURFACE;
            case TabLaunchType.FROM_TAB_GROUP_UI:
                return LaunchTypeAtCreation.FROM_TAB_GROUP_UI;
            case TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP:
                return LaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND_IN_GROUP;
            case TabLaunchType.FROM_APP_WIDGET:
                return LaunchTypeAtCreation.FROM_APP_WIDGET;
            case TabLaunchType.SIZE:
                return LaunchTypeAtCreation.SIZE;
            default:
                assert false : "Unexpected serialization of LaunchAtCreationType: " + tabLaunchType;
                // shouldn't happen
                return LaunchTypeAtCreation.UNKNOWN;
        }
    }

    @VisibleForTesting
    static @TabUserAgent int getTabUserAgentType(int flatbufferUserAgentType) {
        switch (flatbufferUserAgentType) {
            case UserAgentType.DEFAULT:
                return TabUserAgent.DEFAULT;
            case UserAgentType.MOBILE:
                return TabUserAgent.MOBILE;
            case UserAgentType.DESKTOP:
                return TabUserAgent.DESKTOP;
            case UserAgentType.UNSET:
                return TabUserAgent.UNSET;
            case UserAgentType.USER_AGENT_SIZE:
                return TabUserAgent.SIZE;
            default:
                assert false : "Unexpected deserialization of UserAgentType: "
                               + flatbufferUserAgentType;
                // shouldn't happen
                return TabUserAgent.DEFAULT;
        }
    }

    @VisibleForTesting
    static int getUserAgentType(@TabUserAgent int userAgent) {
        switch (userAgent) {
            case TabUserAgent.DEFAULT:
                return UserAgentType.DEFAULT;
            case TabUserAgent.MOBILE:
                return UserAgentType.MOBILE;
            case TabUserAgent.DESKTOP:
                return UserAgentType.DESKTOP;
            case TabUserAgent.UNSET:
                return UserAgentType.UNSET;
            case TabUserAgent.SIZE:
                return UserAgentType.USER_AGENT_SIZE;
            default:
                assert false : "Unexpected serialization of UserAgentType: " + userAgent;
                // shouldn't happen
                return UserAgentType.USER_AGENT_UNKNOWN;
        }
    }

    private static WebContentsState getWebContentsStateFromTab(Tab tab) {
        // Native call returns null when buffer allocation needed to serialize the state failed.
        ByteBuffer buffer = getWebContentsStateAsByteBuffer(tab);
        if (buffer == null) return null;

        WebContentsState state = new WebContentsState(buffer);
        state.setVersion(WebContentsState.CONTENTS_STATE_CURRENT_VERSION);
        return state;
    }

    /** Returns an ByteBuffer representing the state of the Tab's WebContents. */
    private static ByteBuffer getWebContentsStateAsByteBuffer(Tab tab) {
        LoadUrlParams pendingLoadParams = tab.getPendingLoadParams();
        if (pendingLoadParams == null) {
            return WebContentsStateBridge.getContentsStateAsByteBuffer(tab.getWebContents());
        } else {
            Referrer referrer = pendingLoadParams.getReferrer();
            return WebContentsStateBridge.createSingleNavigationStateAsByteBuffer(
                    pendingLoadParams.getUrl(), referrer != null ? referrer.getUrl() : null,
                    // Policy will be ignored for null referrer url, 0 is just a placeholder.
                    referrer != null ? referrer.getPolicy() : 0,
                    pendingLoadParams.getInitiatorOrigin(), tab.isIncognito());
        }
    }

    // TODO(crbug.com/1220678) Change PersistedTabData saves to use ByteBuffer instead of byte[]
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @Override
    public Supplier<ByteBuffer> getSerializeSupplier() {
        CriticalPersistedTabDataProto.Builder builder;
        final WebContentsState webContentsState;
        final ByteBuffer byteBuffer;
        final String openerAppId;
        final int parentId;
        final int rootId;
        final long timestampMillis;
        final int webContentsStateVersion;
        final int themeColor;
        final int launchType;
        final int userAgentType;
        FlatBufferBuilder fbb = new FlatBufferBuilder();
        try (TraceEvent e = TraceEvent.scoped("CriticalPersistedTabData.PreSerialize")) {
            webContentsState = mWebContentsState == null ? getWebContentsStateFromTab(mTab)
                                                         : mWebContentsState;
            byteBuffer = webContentsState == null ? null : webContentsState.buffer();
            if (byteBuffer != null) {
                byteBuffer.rewind();
            }
            openerAppId = mOpenerAppId;
            parentId = mParentId;
            rootId = mRootId;
            timestampMillis = mTimestampMillis;
            webContentsStateVersion = mContentStateVersion;
            themeColor = mThemeColor;
            launchType = getLaunchType(mTabLaunchTypeAtCreation);
            userAgentType = getUserAgentType(mUserAgent);
        }
        return () -> {
            try (TraceEvent e = TraceEvent.scoped("CriticalPersistedTabData.Serialize")) {
                int wcs = CriticalPersistedTabDataFlatBuffer.createWebContentsStateBytesVector(fbb,
                        byteBuffer == null ? ByteBuffer.allocate(0).put(new byte[] {})
                                           : byteBuffer);
                int oaid =
                        fbb.createString(mOpenerAppId == null ? NULL_OPENER_APP_ID : mOpenerAppId);
                CriticalPersistedTabDataFlatBuffer.startCriticalPersistedTabDataFlatBuffer(fbb);
                CriticalPersistedTabDataFlatBuffer.addParentId(fbb, parentId);
                CriticalPersistedTabDataFlatBuffer.addRootId(fbb, rootId);
                CriticalPersistedTabDataFlatBuffer.addTimestampMillis(fbb, timestampMillis);
                CriticalPersistedTabDataFlatBuffer.addWebContentsStateBytes(fbb, wcs);
                CriticalPersistedTabDataFlatBuffer.addContentStateVersion(
                        fbb, webContentsStateVersion);
                CriticalPersistedTabDataFlatBuffer.addOpenerAppId(fbb, oaid);
                CriticalPersistedTabDataFlatBuffer.addThemeColor(fbb, themeColor);
                CriticalPersistedTabDataFlatBuffer.addLaunchTypeAtCreation(fbb, launchType);
                CriticalPersistedTabDataFlatBuffer.addUserAgent(fbb, userAgentType);
                int r = CriticalPersistedTabDataFlatBuffer.endCriticalPersistedTabDataFlatBuffer(
                        fbb);
                fbb.finish(r);

                return fbb.dataBuffer();
            }
        };
    }

    protected static byte[] getContentStateByteArray(ByteBuffer buffer) {
        byte[] contentsStateBytes = new byte[buffer.limit()];
        buffer.rewind();
        buffer.get(contentsStateBytes);
        return contentsStateBytes;
    }

    @Override
    public void save() {
        if (shouldSave()) {
            super.save();
        }
    }

    @Override
    public void delete() {
        super.delete();
    }

    /**
     * Encapsulates use cases where saving is disabled - as taken from TabPersistentStore.java to
     * ensure feature parity.
     * @return
     */
    @VisibleForTesting
    protected boolean shouldSave() {
        if (mShouldSaveForTesting) {
            return true;
        }
        if (getUrl() == null || TextUtils.isEmpty(getUrl().getSpec())) {
            return false;
        }
        if (UrlUtilities.isNTPUrl(getUrl().getSpec()) && !mTab.canGoBack()
                && !mTab.canGoForward()) {
            return false;
        }
        if (isTabUrlContentScheme(getUrl().getSpec())) {
            return false;
        }
        return true;
    }

    private boolean isTabUrlContentScheme(String url) {
        return url != null && url.startsWith(UrlConstants.CONTENT_SCHEME);
    }

    @Override
    public void destroy() {
        mObservers.clear();
    }

    /**
     * @return title of the {@link Tab}
     */
    public String getTitle() {
        return mTitle;
    }

    /**
     * @param title of the {@link Tab} to set
     */
    public void setTitle(String title) {
        if (TextUtils.equals(title, mTitle)) {
            return;
        }
        mTitle = title;
        save();
    }

    /**
     * @return {@link GURL} for the {@link Tab}
     */
    public GURL getUrl() {
        return mUrl;
    }

    /**
     * Set {@link GURL} for the {@link Tab}
     * @param url to set
     */
    public void setUrl(GURL url) {
        if ((url == null && mUrl == null) || (url != null && url.equals(mUrl))) {
            return;
        }
        mUrl = url;
        save();
    }

    /**
     * @return identifier for the {@link Tab}
     */
    public int getTabId() {
        return mTab.getId();
    }

    /**
     * @return root identifier for the {@link Tab}
     */
    public int getRootId() {
        return mRootId;
    }

    /**
     * Set root id
     */
    public void setRootId(int rootId) {
        if (mRootId == rootId) return;
        // TODO(crbug.com/1059640) add in setters for all mutable fields
        mRootId = rootId;
        for (CriticalPersistedTabDataObserver observer : mObservers) {
            observer.onRootIdChanged(mTab, rootId);
        }
        mTab.setIsTabStateDirty(true);
        save();
    }

    /**
     * @return parent identifier for the {@link Tab}
     */
    public int getParentId() {
        return mParentId;
    }

    /**
     * Set parent identifier for the {@link Tab}
     */
    public void setParentId(int parentId) {
        if (mParentId == parentId) {
            return;
        }
        mParentId = parentId;
        save();
    }

    /**
     * @return timestamp in milliseconds for the {@link Tab}
     */
    public long getTimestampMillis() {
        return mTimestampMillis;
    }

    /**
     * Set the timestamp.
     * @param timestamp the timestamp
     */
    public void setTimestampMillis(long timestamp) {
        if (mTimestampMillis == timestamp) {
            return;
        }
        mTimestampMillis = timestamp;
        for (CriticalPersistedTabDataObserver observer : mObservers) {
            observer.onTimestampChanged(mTab, timestamp);
        }
        save();
    }

    /**
     * @return content state bytes for the {@link Tab}
     */
    public WebContentsState getWebContentsState() {
        return mWebContentsState;
    }

    public void setWebContentsState(WebContentsState webContentsState) {
        if ((webContentsState == null && mWebContentsState == null)
                || (webContentsState != null && webContentsState.equals(mWebContentsState))) {
            return;
        }
        mWebContentsState = webContentsState;
        save();
    }

    /**
     * @return content state version for the {@link Tab}
     */
    public int getContentStateVersion() {
        return mContentStateVersion;
    }

    /**
     * @return opener app id
     */
    public String getOpenerAppId() {
        return mOpenerAppId;
    }

    /**
     * @return theme color
     */
    public int getThemeColor() {
        return mThemeColor;
    }

    /**
     * @return launch type at creation
     */
    public @Nullable @TabLaunchType Integer getTabLaunchTypeAtCreation() {
        return mTabLaunchTypeAtCreation;
    }

    public void setLaunchTypeAtCreation(@Nullable @TabLaunchType Integer launchTypeAtCreation) {
        if ((launchTypeAtCreation == null && mTabLaunchTypeAtCreation == null)
                || (launchTypeAtCreation != null
                        && launchTypeAtCreation.equals(mTabLaunchTypeAtCreation))) {
            return;
        }
        mTabLaunchTypeAtCreation = launchTypeAtCreation;
        save();
    }

    /**
     * @return user agent type for the {@link Tab}
     */
    public @TabUserAgent int getUserAgent() {
        return mUserAgent;
    }

    /**
     * Set user agent type for the {@link Tab}
     */
    public void setUserAgent(@TabUserAgent int userAgent) {
        if (mUserAgent == userAgent) {
            return;
        }
        mUserAgent = userAgent;
        save();
    }

    /**
     * Add a {@link CriticalPersistedTabDataObserver}
     * @param criticalPersistedTabDataObserver the observer
     */
    public void addObserver(CriticalPersistedTabDataObserver criticalPersistedTabDataObserver) {
        mObservers.addObserver(criticalPersistedTabDataObserver);
    }

    /**
     * Remove a {@link CriticalPersistedTabDataObserver}
     * @param criticalPersistedTabDataObserver the observer
     */
    public void removeObserver(CriticalPersistedTabDataObserver criticalPersistedTabDataObserver) {
        mObservers.removeObserver(criticalPersistedTabDataObserver);
    }

    @VisibleForTesting
    public void setShouldSaveForTesting(boolean shouldSaveForTesting) {
        mShouldSaveForTesting = shouldSaveForTesting;
    }
}
