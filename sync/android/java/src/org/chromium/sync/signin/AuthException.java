// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.signin;

import android.content.Intent;

/*
 * AuthException abstracts away authenticator specific exceptions behind a single interface.
 * It is used for passing information that is useful for better handling of errors.
 */
public class AuthException extends Exception {
    private final boolean mIsTransientError;
    private final Intent mRecoveryIntent;

    /*
     * A simple constructor that stores all the error handling information and makes it available to
     * the handler.
     * @param isTransientError Whether the error is transient and we can retry.
     * @param recoveryIntent An intent that can be used to recover from this error.
     * Thus, a user recoverable error is not transient, since it requires explicit user handling
     * before retry.
     */
    public AuthException(boolean isTransientError, Intent recoveryIntent, Exception exception) {
        super(exception);
        assert !isTransientError || recoveryIntent == null;
        mIsTransientError = isTransientError;
        mRecoveryIntent = recoveryIntent;
    }

    /*
     * @return Whether the error is transient and we can retry.
     */
    public boolean isTransientError() {
        return mIsTransientError;
    }

    /*
     * @return An intent that can be used to recover from this error if it is recoverabale by user
     * intervention, else {@code null}.
     */
    public Intent getRecoveryIntent() {
        return mRecoveryIntent;
    }
}
