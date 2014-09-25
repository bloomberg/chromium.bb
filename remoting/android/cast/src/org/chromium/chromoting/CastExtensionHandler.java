// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.support.v4.view.MenuItemCompat;
import android.support.v7.app.MediaRouteActionProvider;
import android.support.v7.media.MediaRouteSelector;
import android.support.v7.media.MediaRouter;
import android.support.v7.media.MediaRouter.RouteInfo;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.Toast;

import com.google.android.gms.cast.Cast;
import com.google.android.gms.cast.Cast.Listener;
import com.google.android.gms.cast.CastDevice;
import com.google.android.gms.cast.CastMediaControlIntent;
import com.google.android.gms.cast.CastStatusCodes;
import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.common.api.GoogleApiClient.ConnectionCallbacks;
import com.google.android.gms.common.api.GoogleApiClient.OnConnectionFailedListener;
import com.google.android.gms.common.api.ResultCallback;
import com.google.android.gms.common.api.Status;

import org.chromium.chromoting.jni.JniInterface;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * A handler that interacts with the Cast Extension of the Chromoting host using extension messages.
 * It uses the Cast Android Sender API to start our registered Cast Receiver App on a nearby Cast
 * device, if the user chooses to do so.
 */
public class CastExtensionHandler implements ClientExtension, ActivityLifecycleListener {

    /** Extension messages of this type will be handled by the CastExtensionHandler. */
    public static final String EXTENSION_MSG_TYPE = "cast_message";

    /** Tag used for logging. */
    private static final String TAG = "CastExtensionHandler";

    /** Application Id of the Cast Receiver App that will be run on the Cast device. */
    private static final String RECEIVER_APP_ID = "8A1211E3";

    /**
     * Custom namespace that will be used to communicate with the Cast device.
     * TODO(aiguha): Use com.google.chromeremotedesktop for official builds.
     */
    private static final String CHROMOTOCAST_NAMESPACE = "urn:x-cast:com.chromoting.cast.all";

    /** Context that wil be used to initialize the MediaRouter and the GoogleApiClient. */
    private Context mContext = null;

    /** True if the application has been launched on the Cast device. */
    private boolean mApplicationStarted;

    /** True if the client is temporarily in a disconnected state. */
    private boolean mWaitingForReconnect;

    /** Object that allows routing of media to external devices including Google Cast devices. */
    private MediaRouter mMediaRouter;

    /** Describes the capabilities of routes that the application might want to use. */
    private MediaRouteSelector mMediaRouteSelector;

    /** Cast device selected by the user. */
    private CastDevice mSelectedDevice;

    /** Object to receive callbacks about media routing changes. */
    private MediaRouter.Callback mMediaRouterCallback;

    /** Listener for events related to the connected Cast device.*/
    private Listener mCastClientListener;

    /** Object that handles Google Play Services integration. */
    private GoogleApiClient mApiClient;

    /** Callback objects for connection changes with Google Play Services. */
    private ConnectionCallbacks mConnectionCallbacks;
    private OnConnectionFailedListener mConnectionFailedListener;

    /** Channel for receiving messages from the Cast device. */
    private ChromotocastChannel mChromotocastChannel;

    /** Current session ID, if there is one. */
    private String mSessionId;

    /** Queue of messages that are yet to be delivered to the Receiver App. */
    private List<String> mChromotocastMessageQueue;

    /** Current status of the application, if any. */
    private String mApplicationStatus;

    /**
     * A callback class for receiving events about media routing.
     */
    private class CustomMediaRouterCallback extends MediaRouter.Callback {
        @Override
        public void onRouteSelected(MediaRouter router, RouteInfo info) {
            mSelectedDevice = CastDevice.getFromBundle(info.getExtras());
            connectApiClient();
        }

        @Override
        public void onRouteUnselected(MediaRouter router, RouteInfo info) {
            tearDown();
            mSelectedDevice = null;
        }
    }

    /**
     * A callback class for receiving the result of launching an application on the user-selected
     * Google Cast device.
     */
    private class ApplicationConnectionResultCallback implements
            ResultCallback<Cast.ApplicationConnectionResult> {
        @Override
        public void onResult(Cast.ApplicationConnectionResult result) {
            Status status = result.getStatus();
            if (!status.isSuccess()) {
                tearDown();
                return;
            }

            mSessionId = result.getSessionId();
            mApplicationStatus = result.getApplicationStatus();
            mApplicationStarted = result.getWasLaunched();
            mChromotocastChannel = new ChromotocastChannel();

            try {
                Cast.CastApi.setMessageReceivedCallbacks(mApiClient,
                        mChromotocastChannel.getNamespace(), mChromotocastChannel);
                sendPendingMessagesToCastDevice();
            } catch (IOException e) {
                showToast(R.string.connection_to_cast_failed, Toast.LENGTH_SHORT);
                tearDown();
            } catch (IllegalStateException e) {
                showToast(R.string.connection_to_cast_failed, Toast.LENGTH_SHORT);
                tearDown();
            }
        }
    }

    /**
     * A callback class for receiving events about client connections and disconnections from
     * Google Play Services.
     */
    private class ConnectionCallbacks implements GoogleApiClient.ConnectionCallbacks {
        @Override
        public void onConnected(Bundle connectionHint) {
            if (mWaitingForReconnect) {
                mWaitingForReconnect = false;
                reconnectChannels();
                return;
            }
            Cast.CastApi.launchApplication(mApiClient, RECEIVER_APP_ID, false).setResultCallback(
                    new ApplicationConnectionResultCallback());
        }

        @Override
        public void onConnectionSuspended(int cause) {
            mWaitingForReconnect = true;
        }
    }

    /**
     * A listener for failures to connect with Google Play Services.
     */
    private class ConnectionFailedListener implements GoogleApiClient.OnConnectionFailedListener {
        @Override
        public void onConnectionFailed(ConnectionResult result) {
            Log.e(TAG, String.format("Google Play Service connection failed: %s", result));

            tearDown();
        }

    }

    /**
     * A channel for communication with the Cast device on the CHROMOTOCAST_NAMESPACE.
     */
    private class ChromotocastChannel implements Cast.MessageReceivedCallback {

        /**
         * Returns the namespace associated with this channel.
         */
        public String getNamespace() {
            return CHROMOTOCAST_NAMESPACE;
        }

        @Override
        public void onMessageReceived(CastDevice castDevice, String namespace, String message) {
            if (namespace.equals(CHROMOTOCAST_NAMESPACE)) {
                sendMessageToHost(message);
            }
        }
    }

    /**
     * A listener for changes when connected to a Google Cast device.
     */
    private class CastClientListener extends Cast.Listener {
        @Override
        public void onApplicationStatusChanged() {
            try {
                if (mApiClient != null) {
                    mApplicationStatus = Cast.CastApi.getApplicationStatus(mApiClient);
                }
            } catch (IllegalStateException e) {
                showToast(R.string.connection_to_cast_failed, Toast.LENGTH_SHORT);
                tearDown();
            }
        }

        @Override
        public void onVolumeChanged() {}  // Changes in volume do not affect us.

        @Override
        public void onApplicationDisconnected(int errorCode) {
            if (errorCode != CastStatusCodes.SUCCESS) {
                Log.e(TAG, String.format("Application disconnected with: %d", errorCode));
            }
            tearDown();
        }
    }

    /**
     * Constructs a CastExtensionHandler with an empty message queue.
     */
    public CastExtensionHandler() {
        mChromotocastMessageQueue = new ArrayList<String>();
    }

    //
    // ClientExtension implementation.
    //

    @Override
    public String getCapability() {
        return Capabilities.CAST_CAPABILITY;
    }

    @Override
    public boolean onExtensionMessage(String type, String data) {
        if (type.equals(EXTENSION_MSG_TYPE)) {
            mChromotocastMessageQueue.add(data);
            if (mApplicationStarted) {
                sendPendingMessagesToCastDevice();
            }
            return true;
        }
        return false;
    }

    @Override
    public ActivityLifecycleListener onActivityAcceptingListener(Activity activity) {
        return this;
    }

    //
    // ActivityLifecycleListener implementation.
    //

    /** Initializes the MediaRouter and related objects using the provided activity Context. */
    @Override
    public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
        if (activity == null) {
            return;
        }
        mContext = activity;
        mMediaRouter = MediaRouter.getInstance(activity);
        mMediaRouteSelector = new MediaRouteSelector.Builder()
                .addControlCategory(CastMediaControlIntent.categoryForCast(RECEIVER_APP_ID))
                .build();
        mMediaRouterCallback = new CustomMediaRouterCallback();
    }

    @Override
    public void onActivityDestroyed(Activity activity) {
        tearDown();
    }

    @Override
    public void onActivityPaused(Activity activity) {
        removeMediaRouterCallback();
    }

    @Override
    public void onActivityResumed(Activity activity) {
        addMediaRouterCallback();
    }

    @Override
    public void onActivitySaveInstanceState (Activity activity, Bundle outState) {}

    @Override
    public void onActivityStarted(Activity activity) {
        addMediaRouterCallback();
    }

    @Override
    public void onActivityStopped(Activity activity) {
        removeMediaRouterCallback();
    }

    @Override
    public boolean onActivityCreatedOptionsMenu(Activity activity, Menu menu) {
        // Find the cast button in the menu.
        MenuItem mediaRouteMenuItem = menu.findItem(R.id.media_route_menu_item);
        if (mediaRouteMenuItem == null) {
            return false;
        }

        // Setup a MediaRouteActionProvider using the button.
        MediaRouteActionProvider mediaRouteActionProvider =
                (MediaRouteActionProvider) MenuItemCompat.getActionProvider(mediaRouteMenuItem);
        mediaRouteActionProvider.setRouteSelector(mMediaRouteSelector);

        return true;
    }

    @Override
    public boolean onActivityOptionsItemSelected(Activity activity, MenuItem item) {
        if (item.getItemId() == R.id.actionbar_disconnect) {
            removeMediaRouterCallback();
            showToast(R.string.connection_to_cast_closed, Toast.LENGTH_SHORT);
            tearDown();
            return true;
        }
        return false;
    }

    //
    // Extension Message Handling logic
    //

    /** Sends a message to the Chromoting host. */
    private void sendMessageToHost(String data) {
        JniInterface.sendExtensionMessage(EXTENSION_MSG_TYPE, data);
    }

    /** Sends any messages in the message queue to the Cast device. */
    private void sendPendingMessagesToCastDevice() {
        for (String msg : mChromotocastMessageQueue) {
            sendMessageToCastDevice(msg);
        }
        mChromotocastMessageQueue.clear();
    }

    //
    // Cast Sender API logic
    //

    /**
     * Initializes and connects to Google Play Services.
     */
    private void connectApiClient() {
        if (mContext == null) {
            return;
        }
        mCastClientListener = new CastClientListener();
        mConnectionCallbacks = new ConnectionCallbacks();
        mConnectionFailedListener = new ConnectionFailedListener();

        Cast.CastOptions.Builder apiOptionsBuilder = Cast.CastOptions
                .builder(mSelectedDevice, mCastClientListener)
                .setVerboseLoggingEnabled(true);

        mApiClient = new GoogleApiClient.Builder(mContext)
                .addApi(Cast.API, apiOptionsBuilder.build())
                .addConnectionCallbacks(mConnectionCallbacks)
                .addOnConnectionFailedListener(mConnectionFailedListener)
                .build();
        mApiClient.connect();
    }

    /**
     * Adds the callback object to the MediaRouter. Called when the owning activity starts/resumes.
     */
    private void addMediaRouterCallback() {
        if (mMediaRouter != null && mMediaRouteSelector != null && mMediaRouterCallback != null) {
            mMediaRouter.addCallback(mMediaRouteSelector, mMediaRouterCallback,
                    MediaRouter.CALLBACK_FLAG_PERFORM_ACTIVE_SCAN);
        }
    }

    /**
     * Removes the callback object from the MediaRouter. Called when the owning activity
     * stops/pauses.
     */
    private void removeMediaRouterCallback() {
        if (mMediaRouter != null && mMediaRouterCallback != null) {
            mMediaRouter.removeCallback(mMediaRouterCallback);
        }
    }

    /**
     * Sends a message to the Cast device on the CHROMOTOCAST_NAMESPACE.
     */
    private void sendMessageToCastDevice(String message) {
        if (mApiClient == null || mChromotocastChannel == null) {
            return;
        }
        Cast.CastApi.sendMessage(mApiClient, mChromotocastChannel.getNamespace(), message)
                .setResultCallback(new ResultCallback<Status>() {
                    @Override
                    public void onResult(Status result) {
                        if (!result.isSuccess()) {
                            Log.e(TAG, "Failed to send message to cast device.");
                        }
                    }
                });

    }

    /**
     * Restablishes the chromotocast message channel, so we can continue communicating with the
     * Google Cast device. This must be called when resuming a connection.
     */
    private void reconnectChannels() {
        if (mApiClient == null && mChromotocastChannel == null) {
            return;
        }
        try {
            Cast.CastApi.setMessageReceivedCallbacks(
                    mApiClient, mChromotocastChannel.getNamespace(), mChromotocastChannel);
            sendPendingMessagesToCastDevice();
        } catch (IOException e) {
            showToast(R.string.connection_to_cast_failed, Toast.LENGTH_SHORT);
        } catch (IllegalStateException e) {
            showToast(R.string.connection_to_cast_failed, Toast.LENGTH_SHORT);
        }
    }

    /**
     * Stops the running application on the Google Cast device and performs the required tearDown
     * sequence.
     */
    private void tearDown() {
        if (mApiClient != null && mApplicationStarted && mApiClient.isConnected()) {
            Cast.CastApi.stopApplication(mApiClient, mSessionId);
            if (mChromotocastChannel != null) {
                try {
                    Cast.CastApi.removeMessageReceivedCallbacks(
                            mApiClient, mChromotocastChannel.getNamespace());
                } catch (IOException e) {
                    Log.e(TAG, "Failed to remove chromotocast channel.");
                }
            }
            mApiClient.disconnect();
        }
        mChromotocastChannel = null;
        mApplicationStarted = false;
        mApiClient = null;
        mSelectedDevice = null;
        mWaitingForReconnect  = false;
        mSessionId = null;
    }

    /**
     * Makes a toast using the given message and duration.
     */
    private void showToast(int messageId, int duration) {
        if (mContext != null) {
            Toast.makeText(mContext, mContext.getString(messageId), duration).show();
        }
    }
}
