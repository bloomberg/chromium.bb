// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Intent;
import android.graphics.PointF;
import android.os.Bundle;
import android.speech.RecognitionListener;
import android.speech.RecognizerIntent;
import android.speech.SpeechRecognizer;

import com.google.vrtoolkit.cardboard.CardboardActivity;
import com.google.vrtoolkit.cardboard.CardboardView;

import org.chromium.chromoting.jni.JniInterface;

import java.util.ArrayList;

/**
 * Virtual desktop activity for Cardboard.
 */
public class CardboardDesktopActivity extends CardboardActivity {
    // Flag to indicate whether the current activity is going to switch to normal
    // desktop activity.
    private boolean mSwitchToDesktopActivity;

    private CardboardDesktopRenderer mRenderer;
    private SpeechRecognizer mSpeechRecognizer;

    // Flag to indicate whether the speech recognizer is listening or not.
    private boolean mIsListening;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.cardboard_desktop);
        mSwitchToDesktopActivity = false;
        CardboardView cardboardView = (CardboardView) findViewById(R.id.cardboard_view);
        mRenderer = new CardboardDesktopRenderer(this);
        mIsListening = false;

        // Associate a CardboardView.StereoRenderer with cardboardView.
        cardboardView.setRenderer(mRenderer);

        // Associate the cardboardView with this activity.
        setCardboardView(cardboardView);
    }

    @Override
    public void onCardboardTrigger() {
        if (mRenderer.isLookingAtDesktop()) {
            PointF coordinates = mRenderer.getMouseCoordinates();
            JniInterface.sendMouseEvent((int) coordinates.x, (int) coordinates.y,
                    TouchInputHandler.BUTTON_LEFT, true);
            JniInterface.sendMouseEvent((int) coordinates.x, (int) coordinates.y,
                    TouchInputHandler.BUTTON_LEFT, false);
        } else if (mRenderer.isLookingLeftOfDesktop()) {
            // TODO(shichengfeng): Give a more polished UI (including menu icons
            // and visual indicator during when speech recognizer is listening).
            mSwitchToDesktopActivity = true;
            finish();
        } else if (mRenderer.isLookingRightOfDesktop()) {
            listenForVoiceInput();
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
        JniInterface.enableVideoChannel(true);
        mRenderer.attachRedrawCallback();
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (!mSwitchToDesktopActivity) {
            JniInterface.enableVideoChannel(false);
        }
        if (mSpeechRecognizer != null) {
            mSpeechRecognizer.stopListening();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        JniInterface.enableVideoChannel(true);
    }

    @Override
    protected void onStop() {
        super.onStop();
        if (mSwitchToDesktopActivity) {
            mSwitchToDesktopActivity = false;
        } else {
            JniInterface.enableVideoChannel(false);
        }
        if (mSpeechRecognizer != null) {
            mSpeechRecognizer.stopListening();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (mSpeechRecognizer != null) {
            mSpeechRecognizer.cancel();
            mSpeechRecognizer.destroy();
        }
    }

    private void listenForVoiceInput() {
        if (mIsListening) {
            return;
        }

        if (mSpeechRecognizer == null) {
            if (SpeechRecognizer.isRecognitionAvailable(this)) {
                mSpeechRecognizer = SpeechRecognizer.createSpeechRecognizer(this);
                mSpeechRecognizer.setRecognitionListener(new VoiceInputRecognitionListener());
            } else {
                return;
            }
        }

        mIsListening = true;

        Intent intent = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);

        // LANGUAGE_MODEL_FREE_FORM is used to improve dictation accuracy
        // for the voice keyboard.
        intent.putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL,
                RecognizerIntent.LANGUAGE_MODEL_FREE_FORM);

        mSpeechRecognizer.startListening(intent);
    }

    private class VoiceInputRecognitionListener implements RecognitionListener {
        public void onReadyForSpeech(Bundle params) {
        }

        public void onBeginningOfSpeech() {
        }

        public void onRmsChanged(float rmsdB){
        }

        public void onBufferReceived(byte[] buffer) {
        }

        public void onEndOfSpeech() {
            mIsListening = false;
        }

        public void onError(int error) {
            mIsListening = false;
        }

        public void onResults(Bundle results) {
            // TODO(shichengfeng): If necessary, provide a list of choices for user to pick.
            ArrayList<String> data =
                    results.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION);
            if (!data.isEmpty()) {
                JniInterface.sendTextEvent(data.get(0));
            }
        }

        public void onPartialResults(Bundle partialResults) {
        }

        public void onEvent(int eventType, Bundle params) {
        }
    }
}
