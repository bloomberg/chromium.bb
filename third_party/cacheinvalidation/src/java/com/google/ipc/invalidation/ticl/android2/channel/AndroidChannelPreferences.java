/*
 * Copyright 2011 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.google.ipc.invalidation.ticl.android2.channel;

import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;
import com.google.ipc.invalidation.ticl.android2.channel.AndroidChannelConstants.C2dmConstants;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Base64;


/** Accessor class for shared preference entries used by the channel. */
 public class AndroidChannelPreferences {
  /** Name of the preferences in which channel preferences are stored. */
  private static final String PREFERENCES_NAME = "com.google.ipc.invalidation.gcmchannel";

  /**
   * Preferences entry used to buffer the last message sent by the Ticl in the case where a GCM
   * registration id is not currently available.
   */
  private static final String BUFFERED_MSG_PREF = "buffered-msg";

  private static final Logger logger = AndroidLogger.forTag("ChannelPrefs");

  /** Sets the token echoed on subsequent HTTP requests. */
  static void setEchoToken(Context context, String token) {
    SharedPreferences.Editor editor = getPreferences(context).edit();

    // This might fail, but at worst it just means we lose an echo token; the channel
    // needs to be able to handle that anyway since it can never assume an echo token
    // makes it to the client (since the channel can drop messages).
    editor.putString(C2dmConstants.ECHO_PARAM, token);
    if (!editor.commit()) {
      logger.warning("Failed writing shared preferences for: setEchoToken");
    }
  }

  /** Returns the echo token that should be included on HTTP requests. */
  
  public static String getEchoToken(Context context) {
    return getPreferences(context).getString(C2dmConstants.ECHO_PARAM, null);
  }

  /** Buffers the last message sent by the Ticl. Overwrites any previously buffered message. */
  static void bufferMessage(Context context, byte[] message) {
    SharedPreferences.Editor editor = getPreferences(context).edit();
    String encodedMessage =
        Base64.encodeToString(message, Base64.URL_SAFE | Base64.NO_WRAP  | Base64.NO_PADDING);
    editor.putString(BUFFERED_MSG_PREF, encodedMessage);

    // This might fail, but at worst we'll just drop a message, which the Ticl must be prepared to
    // handle.
    if (!editor.commit()) {
      logger.warning("Failed writing shared preferences for: bufferMessage");
    }
  }

  /**
   * Removes and returns the buffered Ticl message, if any. If no message was buffered, returns
   * {@code null}.
   */
  static byte[] takeBufferedMessage(Context context) {
    SharedPreferences preferences = getPreferences(context);
    String message = preferences.getString(BUFFERED_MSG_PREF, null);
    if (message == null) {
      // No message was buffered.
      return null;
    }
    // There is a message to return. Remove the stored value from the preferences.
    SharedPreferences.Editor editor = preferences.edit();
    editor.remove(BUFFERED_MSG_PREF);

    // If this fails, we might send the same message twice, which is fine.
    if (!editor.commit()) {
      logger.warning("Failed writing shared preferences for: takeBufferedMessage");
    }

    // Return the decoded message.
    return Base64.decode(message, Base64.URL_SAFE);
  }

  /** Returns whether a message has been buffered, for tests. */
  public static boolean hasBufferedMessageForTest(Context context) {
    return getPreferences(context).contains(BUFFERED_MSG_PREF);
  }

  /** Returns a new {@link SharedPreferences} instance to access the channel preferences. */
  private static SharedPreferences getPreferences(Context context) {
    return context.getSharedPreferences(PREFERENCES_NAME, Context.MODE_PRIVATE);
  }
}
