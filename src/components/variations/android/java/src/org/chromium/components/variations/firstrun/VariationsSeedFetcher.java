// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.variations.firstrun;

import android.content.SharedPreferences;
import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.BuildConfig;
import org.chromium.components.variations.VariationsSwitches;
import org.chromium.net.ChromiumNetworkAdapter;
import org.chromium.net.NetworkTrafficAnnotationTag;

import java.io.IOException;
import java.io.InputStream;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.SocketTimeoutException;
import java.net.URL;
import java.net.UnknownHostException;
import java.util.Arrays;
import java.util.Date;

/**
 * Fetches the variations seed before the actual first run of Chrome.
 */
public class VariationsSeedFetcher {
    private static final String TAG = "VariationsSeedFetch";

    private static final NetworkTrafficAnnotationTag TRAFFIC_ANNOTATION =
            NetworkTrafficAnnotationTag.createComplete("chrome_variations_android",
                    "semantics {"
                            + "  sender: 'Chrome Variations Service (Android)'"
                            + "  description:"
                            + "      'The variations service is responsible for determining the '"
                            + "      'state of field trials in Chrome. These field trials '"
                            + "      'typically configure either A/B experiments, or launched '"
                            + "      'features – oftentimes, critical security features.'"
                            + "  trigger: 'This request is made once, on Chrome\'s first run, to '"
                            + "           'determine the initial state Chrome should be in.'"
                            + "  data: 'None.'"
                            + "  destination: GOOGLE_OWNED_SERVICE"
                            + "}"
                            + "policy {"
                            + "  cookies_allowed: NO"
                            + "  setting: 'Cannot be disabled in Settings. Chrome Variations are '"
                            + "           'an essential part of Chrome releases.'"
                            + "  policy_exception_justification:"
                            + "      'The ChromeVariations policy is only implemented on desktop '"
                            + "      'and ChromeOS.'"
                            + "}");

    @IntDef({VariationsPlatform.ANDROID, VariationsPlatform.ANDROID_WEBVIEW})
    @Retention(RetentionPolicy.SOURCE)
    public @interface VariationsPlatform {
        int ANDROID = 0;
        int ANDROID_WEBVIEW = 1;
    }

    private static final String VARIATIONS_SERVER_URL =
            "https://clientservices.googleapis.com/chrome-variations/seed?osname=";

    private static final int READ_TIMEOUT = 3000; // time in ms
    private static final int REQUEST_TIMEOUT = 1000; // time in ms

    @VisibleForTesting
    public static final String SEED_FETCH_RESULT_HISTOGRAM = "Variations.FirstRun.SeedFetchResult";

    // Values for the "Variations.FirstRun.SeedFetchResult" sparse histogram, which also logs
    // HTTP result codes. These are negative so that they don't conflict with the HTTP codes.
    // These values should not be renumbered or re-used since they are logged to UMA.
    private static final int SEED_FETCH_RESULT_INVALID_DATE_HEADER = -4;
    private static final int SEED_FETCH_RESULT_UNKNOWN_HOST_EXCEPTION = -3;
    private static final int SEED_FETCH_RESULT_TIMEOUT = -2;
    @VisibleForTesting
    public static final int SEED_FETCH_RESULT_IOEXCEPTION = -1;

    @VisibleForTesting
    static final String VARIATIONS_INITIALIZED_PREF = "variations_initialized";

    // Synchronization lock to make singleton thread-safe.
    private static final Object sLock = new Object();

    private static VariationsSeedFetcher sInstance;

    @VisibleForTesting
    public VariationsSeedFetcher() {}

    public static VariationsSeedFetcher get() {
        // TODO(aberent) Check not running on UI thread. Doing so however makes Robolectric testing
        // of dependent classes difficult.
        synchronized (sLock) {
            if (sInstance == null) {
                sInstance = new VariationsSeedFetcher();
            }
            return sInstance;
        }
    }

    /**
     * Override the VariationsSeedFetcher, typically with a mock, for testing classes that depend on
     * this one.
     * @param fetcher the mock.
     */
    @VisibleForTesting
    public static void setVariationsSeedFetcherForTesting(VariationsSeedFetcher fetcher) {
        sInstance = fetcher;
    }

    @VisibleForTesting
    protected HttpURLConnection getServerConnection(
            @VariationsPlatform int platform, String restrictMode, String milestone, String channel)
            throws MalformedURLException, IOException {
        String urlString = getConnectionString(platform, restrictMode, milestone, channel);
        URL url = new URL(urlString);
        return (HttpURLConnection) ChromiumNetworkAdapter.openConnection(url, TRAFFIC_ANNOTATION);
    }

    @VisibleForTesting
    protected String getConnectionString(@VariationsPlatform int platform, String restrictMode,
            String milestone, String channel) {
        String urlString = VARIATIONS_SERVER_URL;
        switch (platform) {
            case VariationsPlatform.ANDROID:
                urlString += "android";
                break;
            case VariationsPlatform.ANDROID_WEBVIEW:
                urlString += "android_webview";
                break;
            default:
                assert false;
        }
        if (restrictMode != null && !restrictMode.isEmpty()) {
            urlString += "&restrict=" + restrictMode;
        }
        if (milestone != null && !milestone.isEmpty()) {
            urlString += "&milestone=" + milestone;
        }

        String forcedChannel = CommandLine.getInstance().getSwitchValue(
                VariationsSwitches.FAKE_VARIATIONS_CHANNEL);
        if (forcedChannel != null) {
            channel = forcedChannel;
        }
        if (channel != null && !channel.isEmpty()) {
            urlString += "&channel=" + channel;
        }

        return urlString;
    }

    /**
     * Object holding information about the status of a seed download attempt.
     */
    public static class SeedFetchInfo {
        // The result of the download, containing either an HTTP status code or a negative
        // value representing a specific error. This value is suitable for recording to the
        // "Variations.FirstRun.SeedFetchResult" histogram. This is equal to
        // HttpURLConnection.HTTP_OK if and only if the fetch succeeded.
        public int seedFetchResult;

        // Information about the seed that was downloaded. Null if the download failed.
        public SeedInfo seedInfo;
    }

    /**
     * Object holding the seed data and related fields retrieved from HTTP headers.
     */
    public static class SeedInfo {
        // If you add fields, see VariationsTestUtils.
        public String signature;
        public String country;
        // Date according to the Variations server in milliseconds since UNIX epoch GMT.
        public long date;
        public boolean isGzipCompressed;
        public byte[] seedData;

        @Override
        public String toString() {
            if (BuildConfig.ENABLE_ASSERTS) {
                return "SeedInfo{signature=\"" + signature + "\" country=\"" + country
                        + "\" date=\"" + date + " isGzipCompressed=" + isGzipCompressed
                        + " seedData=" + Arrays.toString(seedData);
            }
            return super.toString();
        }
    }


    /**
     * Fetch the first run variations seed.
     * @param restrictMode The restrict mode parameter to pass to the server via a URL param.
     * @param milestone The milestone parameter to pass to the server via a URL param.
     * @param channel The channel parameter to pass to the server via a URL param.
     */
    public void fetchSeed(String restrictMode, String milestone, String channel) {
        assert !ThreadUtils.runningOnUiThread();
        // Prevent multiple simultaneous fetches
        synchronized (sLock) {
            SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
            // Early return if an attempt has already been made to fetch the seed, even if it
            // failed. Only attempt to get the initial Java seed once, since a failure probably
            // indicates a network problem that is unlikely to be resolved by a second attempt.
            // Note that VariationsSeedBridge.hasNativePref() is a pure Java function, reading an
            // Android preference that is set when the seed is fetched by the native code.
            if (prefs.getBoolean(VARIATIONS_INITIALIZED_PREF, false)
                    || VariationsSeedBridge.hasNativePref()) {
                return;
            }

            SeedFetchInfo fetchInfo =
                    downloadContent(VariationsPlatform.ANDROID, restrictMode, milestone, channel);
            if (fetchInfo.seedInfo != null) {
                SeedInfo info = fetchInfo.seedInfo;
                VariationsSeedBridge.setVariationsFirstRunSeed(info.seedData, info.signature,
                        info.country, info.date, info.isGzipCompressed);
            }
            // VARIATIONS_INITIALIZED_PREF should still be set to true when exceptions occur
            prefs.edit().putBoolean(VARIATIONS_INITIALIZED_PREF, true).apply();
        }
    }

    private void recordFetchResultOrCode(int resultOrCode) {
        RecordHistogram.recordSparseHistogram(SEED_FETCH_RESULT_HISTOGRAM, resultOrCode);
    }

    private void recordSeedFetchTime(long timeDeltaMillis) {
        Log.i(TAG, "Fetched first run seed in " + timeDeltaMillis + " ms");
        RecordHistogram.recordTimesHistogram("Variations.FirstRun.SeedFetchTime", timeDeltaMillis);
    }

    private void recordSeedConnectTime(long timeDeltaMillis) {
        RecordHistogram.recordTimesHistogram(
                "Variations.FirstRun.SeedConnectTime", timeDeltaMillis);
    }

    /**
     * Download the variations seed data with platform and restrictMode.
     * @param platform the platform parameter to let server only return experiments which can be
     * run on that platform.
     * @param restrictMode the restrict mode parameter to pass to the server via a URL param.
     * @param milestone the milestone parameter to pass to the server via a URL param.
     * @param channel the channel parameter to pass to the server via a URL param.
     * @return the object holds the request result and seed data with its related header fields.
     */
    public SeedFetchInfo downloadContent(@VariationsPlatform int platform, String restrictMode,
            String milestone, String channel) {
        SeedFetchInfo fetchInfo = new SeedFetchInfo();
        HttpURLConnection connection = null;
        try {
            long startTimeMillis = SystemClock.elapsedRealtime();
            connection = getServerConnection(platform, restrictMode, milestone, channel);
            connection.setReadTimeout(READ_TIMEOUT);
            connection.setConnectTimeout(REQUEST_TIMEOUT);
            connection.setDoInput(true);
            connection.setRequestProperty("A-IM", "gzip");
            connection.connect();
            int responseCode = connection.getResponseCode();
            fetchInfo.seedFetchResult = responseCode;
            if (responseCode == HttpURLConnection.HTTP_OK) {
                recordSeedConnectTime(SystemClock.elapsedRealtime() - startTimeMillis);

                SeedInfo seedInfo = new SeedInfo();
                seedInfo.seedData = getRawSeed(connection);
                seedInfo.signature = getHeaderFieldOrEmpty(connection, "X-Seed-Signature");
                seedInfo.country = getHeaderFieldOrEmpty(connection, "X-Country");
                seedInfo.date = new Date().getTime();
                seedInfo.isGzipCompressed = getHeaderFieldOrEmpty(connection, "IM").equals("gzip");
                recordSeedFetchTime(SystemClock.elapsedRealtime() - startTimeMillis);
                fetchInfo.seedInfo = seedInfo;
            } else {
                String errorMsg = "Non-OK response code = " + responseCode;
                Log.w(TAG, errorMsg);
            }
        } catch (SocketTimeoutException e) {
            fetchInfo.seedFetchResult = SEED_FETCH_RESULT_TIMEOUT;
            Log.w(TAG, "SocketTimeoutException timeout when fetching variations seed.", e);
        } catch (UnknownHostException e) {
            fetchInfo.seedFetchResult = SEED_FETCH_RESULT_UNKNOWN_HOST_EXCEPTION;
            Log.w(TAG, "UnknownHostException unknown host when fetching variations seed.", e);
        } catch (IOException e) {
            fetchInfo.seedFetchResult = SEED_FETCH_RESULT_IOEXCEPTION;
            Log.w(TAG, "IOException when fetching variations seed.", e);
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
            recordFetchResultOrCode(fetchInfo.seedFetchResult);
            return fetchInfo;
        }
    }

    private String getHeaderFieldOrEmpty(HttpURLConnection connection, String name) {
        String headerField = connection.getHeaderField(name);
        if (headerField == null) {
            return "";
        }
        return headerField.trim();
    }

    private byte[] getRawSeed(HttpURLConnection connection) throws IOException {
        InputStream inputStream = null;
        try {
            inputStream = connection.getInputStream();
            return FileUtils.readStream(inputStream);
        } finally {
            if (inputStream != null) {
                inputStream.close();
            }
        }
    }
}
