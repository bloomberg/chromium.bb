// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.caf.remoting;

import com.google.android.gms.cast.MediaInfo;
import com.google.android.gms.cast.MediaStatus;
import com.google.android.gms.cast.framework.media.RemoteMediaClient;
import com.google.android.gms.common.api.Result;

import org.chromium.base.Log;
import org.chromium.chrome.browser.media.router.FlingingController;
import org.chromium.chrome.browser.media.router.MediaController;
import org.chromium.chrome.browser.media.router.MediaStatusBridge;
import org.chromium.chrome.browser.media.router.MediaStatusObserver;

/** Adapter class for bridging {@link RemoteMediaClient} and {@link FlingController}. */
public class FlingingControllerAdapter implements FlingingController, MediaController {
    private static final String TAG = "FlingCtrlAdptr";

    private final StreamPositionExtrapolator mStreamPositionExtrapolator;
    private final RemotingSessionController mSessionController;
    private final String mMediaUrl;
    private MediaStatusObserver mMediaStatusObserver;
    private boolean mLoaded;

    FlingingControllerAdapter(RemotingSessionController sessionController, String mediaUrl) {
        mSessionController = sessionController;
        mMediaUrl = mediaUrl;
        mStreamPositionExtrapolator = new StreamPositionExtrapolator();
    }

    ////////////////////////////////////////////
    // FlingingController implementation begin
    ////////////////////////////////////////////

    @Override
    public MediaController getMediaController() {
        return this;
    }

    @Override
    public void setMediaStatusObserver(MediaStatusObserver observer) {
        assert mMediaStatusObserver == null;
        mMediaStatusObserver = observer;
    }

    @Override
    public void clearMediaStatusObserver() {
        assert mMediaStatusObserver != null;
        mMediaStatusObserver = null;
    }

    @Override
    public long getApproximateCurrentTime() {
        return mStreamPositionExtrapolator.getPosition();
    }

    public long getDuration() {
        return mStreamPositionExtrapolator.getDuration();
    }

    ////////////////////////////////////////////
    // FlingingController implementation end
    ////////////////////////////////////////////

    /** Starts loading the media URL, from the given position. */
    public void load(long position) {
        if (!mSessionController.isConnected()) return;

        mLoaded = true;

        MediaInfo mediaInfo = new MediaInfo.Builder(mMediaUrl)
                                      .setContentType("*/*")
                                      .setStreamType(MediaInfo.STREAM_TYPE_BUFFERED)
                                      .build();
        mSessionController.getRemoteMediaClient().load(mediaInfo, /* autoplay= */ true, position);
    }

    ////////////////////////////////////////////
    // MediaController implementation begin
    ////////////////////////////////////////////

    @Override
    public void play() {
        if (!mSessionController.isConnected()) return;

        if (!mLoaded) {
            load(/* position= */ 0);
            return;
        }

        mSessionController.getRemoteMediaClient().play().setResultCallback(
                this::onMediaCommandResult);
    }

    @Override
    public void pause() {
        if (!mSessionController.isConnected()) return;
        mSessionController.getRemoteMediaClient().pause().setResultCallback(
                this::onMediaCommandResult);
    }

    @Override
    public void setMute(boolean mute) {
        if (!mSessionController.isConnected()) return;
        mSessionController.getRemoteMediaClient().setStreamMute(mute).setResultCallback(
                this::onMediaCommandResult);
    }

    @Override
    public void setVolume(double volume) {
        if (!mSessionController.isConnected()) return;
        mSessionController.getRemoteMediaClient().setStreamVolume(volume).setResultCallback(
                this::onMediaCommandResult);
    }

    @Override
    public void seek(long position) {
        if (!mSessionController.isConnected()) return;

        if (!mLoaded) {
            load(position);
            return;
        }

        mSessionController.safelySeek(position).setResultCallback(this::onMediaCommandResult);
        mStreamPositionExtrapolator.onSeek(position);
    }

    ////////////////////////////////////////////
    // MediaController implementation end
    ////////////////////////////////////////////

    public void onStatusUpdated() {
        if (mMediaStatusObserver == null) return;

        RemoteMediaClient remoteMediaClient = mSessionController.getRemoteMediaClient();

        MediaStatus mediaStatus = remoteMediaClient.getMediaStatus();
        if (mediaStatus != null) {
            if (mediaStatus.getPlayerState() == MediaStatus.PLAYER_STATE_IDLE
                    && mediaStatus.getIdleReason() == MediaStatus.IDLE_REASON_FINISHED) {
                mLoaded = false;
                mStreamPositionExtrapolator.onFinish();
            } else {
                mStreamPositionExtrapolator.update(remoteMediaClient.getStreamDuration(),
                        remoteMediaClient.getApproximateStreamPosition(),
                        remoteMediaClient.isPlaying(), mediaStatus.getPlaybackRate());
            }

            mMediaStatusObserver.onMediaStatusUpdate(new MediaStatusBridge(mediaStatus));

        } else {
            mLoaded = false;
            mStreamPositionExtrapolator.clear();
        }
    }

    private void onMediaCommandResult(Result result) {
        // When multiple API calls are made in quick succession, "Results have already been set"
        // IllegalStateExceptions might be thrown from GMS code. We prefer to catch the exception
        // and noop it, than to crash. This might lead to some API calls never getting their
        // onResult() called, so we should not rely on onResult() being called for every API call.
        // See https://crbug.com/853923.
        if (!result.getStatus().isSuccess()) {
            Log.e(TAG, "Error when sending command. Status code: %d",
                    result.getStatus().getStatusCode());
        }
    }
}
