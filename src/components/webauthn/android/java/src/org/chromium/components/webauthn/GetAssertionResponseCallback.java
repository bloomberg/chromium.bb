// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;

/**
 * Callback interface for receiving a response from a request to produce a
 * signed assertion from an authenticator.
 */
public interface GetAssertionResponseCallback {
    public void onSignResponse(Integer status, GetAssertionAuthenticatorResponse response);
}
