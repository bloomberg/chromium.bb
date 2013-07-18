// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.test.util;

import android.accounts.Account;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.widget.TextView;

/**
 * A dummy implementation of the Android {@link GrantCredentialsPermissionActivity} that is used
 * when displaying the Allow/Deny dialog when an application asks for an auth token
 * for a given auth token type and that app has never gotten the permission.
 *
 * Currently this activity just starts up, broadcasts an intent, and finishes.
 *
 * This class is used by {@link MockAccountManager}.
 */
public class MockGrantCredentialsPermissionActivity extends Activity {

    private static final String TAG = "MockGrantCredentialsPermissionActivity";

    static final String ACCOUNT = "account";

    static final String AUTH_TOKEN_TYPE = "authTokenType";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        TextView textView = new TextView(this);
        Account account = (Account) getIntent().getParcelableExtra(ACCOUNT);
        String authTokenType = getIntent().getStringExtra(AUTH_TOKEN_TYPE);
        String accountName = account == null ? null : account.name;
        String message = "account = " + accountName + ", authTokenType = " + authTokenType;
        textView.setText(message);
        setContentView(textView);
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Send out the broadcast after the Activity has completely started up.
        Handler handler = new Handler(Looper.getMainLooper());
        handler.post(new Runnable() {
            @Override
            public void run() {
                Log.d(TAG, "Broadcasting " + MockAccountManager.MUTEX_WAIT_ACTION);
                sendBroadcast(new Intent(MockAccountManager.MUTEX_WAIT_ACTION));
                finish();
            }
        });
    }
}
