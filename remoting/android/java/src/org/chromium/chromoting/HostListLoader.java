// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.util.Log;

import org.chromium.chromoting.jni.JniInterface;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.Locale;
import java.util.Scanner;

/** Helper for fetching the host list. */
public class HostListLoader {
    public enum Error {
        AUTH_FAILED,
        NETWORK_ERROR,
        SERVICE_UNAVAILABLE,
        UNEXPECTED_RESPONSE,
        UNKNOWN,
    }

    /** Callback for receiving the host list, or getting notified of an error. */
    public interface Callback {
        void onHostListReceived(HostInfo[] hosts);
        void onError(Error error);
    }

    /** Path from which to download a user's host list JSON object. */
    private static final String HOST_LIST_PATH =
            "https://www.googleapis.com/chromoting/v1/@me/hosts?key=";

    /** Callback handler to be used for network operations. */
    private Handler mNetworkThread;

    /** Handler for main thread. */
    private Handler mMainThread;

    public HostListLoader() {
        // Thread responsible for downloading the host list.

        mMainThread = new Handler(Looper.getMainLooper());
    }

    private void initNetworkThread() {
        if (mNetworkThread == null) {
            HandlerThread thread = new HandlerThread("network");
            thread.start();
            mNetworkThread = new Handler(thread.getLooper());
        }
    }

    /**
      * Causes the host list to be fetched on a background thread. This should be called on the
      * main thread, and callbacks will also be invoked on the main thread. On success,
      * callback.onHostListReceived() will be called, otherwise callback.onError() will be called
      * with an error-code describing the failure.
      */
    public void retrieveHostList(String authToken, Callback callback) {
        initNetworkThread();
        final String authTokenFinal = authToken;
        final Callback callbackFinal = callback;
        mNetworkThread.post(new Runnable() {
            @Override
            public void run() {
                doRetrieveHostList(authTokenFinal, callbackFinal);
            }
        });
    }

    private void doRetrieveHostList(String authToken, Callback callback) {
        HttpURLConnection link = null;
        String response = null;
        try {
            link = (HttpURLConnection)
                    new URL(HOST_LIST_PATH + JniInterface.nativeGetApiKey()).openConnection();
            link.addRequestProperty("client_id", JniInterface.nativeGetClientId());
            link.addRequestProperty("client_secret", JniInterface.nativeGetClientSecret());
            link.setRequestProperty("Authorization", "OAuth " + authToken);

            // Listen for the server to respond.
            int status = link.getResponseCode();
            switch (status) {
                case HttpURLConnection.HTTP_OK:  // 200
                    break;
                case HttpURLConnection.HTTP_UNAUTHORIZED:  // 401
                    postError(callback, Error.AUTH_FAILED);
                    return;
                case HttpURLConnection.HTTP_BAD_GATEWAY:  // 502
                case HttpURLConnection.HTTP_UNAVAILABLE:  // 503
                    postError(callback, Error.SERVICE_UNAVAILABLE);
                    return;
                default:
                    postError(callback, Error.UNKNOWN);
                    return;
            }

            StringBuilder responseBuilder = new StringBuilder();
            Scanner incoming = new Scanner(link.getInputStream());
            Log.i("auth", "Successfully authenticated to directory server");
            while (incoming.hasNext()) {
                responseBuilder.append(incoming.nextLine());
            }
            response = String.valueOf(responseBuilder);
            incoming.close();
        } catch (MalformedURLException ex) {
            // This should never happen.
            throw new RuntimeException("Unexpected error while fetching host list: " + ex);
        } catch (IOException ex) {
            postError(callback, Error.NETWORK_ERROR);
            return;
        } finally {
            if (link != null) {
                link.disconnect();
            }
        }

        // Parse directory response.
        ArrayList<HostInfo> hostList = new ArrayList<HostInfo>();
        try {
            JSONObject data = new JSONObject(response).getJSONObject("data");
            if (data.has("items")) {
                JSONArray hostsJson = data.getJSONArray("items");
                Log.i("hostlist", "Received host listing from directory server");

                int index = 0;
                while (!hostsJson.isNull(index)) {
                    JSONObject hostJson = hostsJson.getJSONObject(index);
                    // If a host is only recently registered, it may be missing some of the keys
                    // below. It should still be visible in the list, even though a connection
                    // attempt will fail because of the missing keys. The failed attempt will
                    // trigger reloading of the host-list, by which time the keys will hopefully be
                    // present, and the retried connection can succeed.
                    HostInfo host = HostInfo.create(hostJson);
                    hostList.add(host);
                    ++index;
                }
            }
        } catch (JSONException ex) {
            Log.e("hostlist", "Error parsing host list response: ", ex);
            postError(callback, Error.UNEXPECTED_RESPONSE);
            return;
        }

        sortHosts(hostList);

        final Callback callbackFinal = callback;
        final HostInfo[] hosts = hostList.toArray(new HostInfo[hostList.size()]);
        mMainThread.post(new Runnable() {
            @Override
            public void run() {
                callbackFinal.onHostListReceived(hosts);
            }
        });
    }

    /** Posts error to callback on main thread. */
    private void postError(Callback callback, Error error) {
        final Callback callbackFinal = callback;
        final Error errorFinal = error;
        mMainThread.post(new Runnable() {
            @Override
            public void run() {
                callbackFinal.onError(errorFinal);
            }
        });
    }

    private static void sortHosts(ArrayList<HostInfo> hosts) {
        Comparator<HostInfo> hostComparator = new Comparator<HostInfo>() {
            @Override
            public int compare(HostInfo a, HostInfo b) {
                if (a.isOnline != b.isOnline) {
                    return a.isOnline ? -1 : 1;
                }
                String aName = a.name.toUpperCase(Locale.getDefault());
                String bName = b.name.toUpperCase(Locale.getDefault());
                return aName.compareTo(bName);
            }
        };
        Collections.sort(hosts, hostComparator);
    }
}
