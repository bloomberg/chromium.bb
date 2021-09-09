// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.mojo.bindings.Interface;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

/**
 * The RenderFrameHost Java wrapper to allow communicating with the native RenderFrameHost object.
 */
public interface RenderFrameHost {
    /**
     * Get the last committed URL of the frame.
     *
     * @return The last committed URL of the frame or null when being destroyed.
     */
    @Nullable
    GURL getLastCommittedURL();

    /**
     * Get the last committed Origin of the frame. This is not always the same as scheme/host/port
     * of getLastCommittedURL(), since it can be an "opaque" origin in such cases as, for example,
     * sandboxed frame.
     *
     * @return The last committed Origin of the frame or null when being destroyed.
     */
    @Nullable
    Origin getLastCommittedOrigin();

    /**
     * Fetch the canonical URL associated with the fame.
     *
     * @param callback The callback to be notified once the canonical URL has been fetched.
     */
    void getCanonicalUrlForSharing(Callback<GURL> callback);

    /**
     * Returns whether the feature policy allows the feature in this frame.
     *
     * @param feature A feature controlled by feature policy.
     *
     * @return Whether the feature policy allows the feature in this frame.
     */
    boolean isFeatureEnabled(@PermissionsPolicyFeature int feature);

    /**
     * Returns an interface by name to the Frame in the renderer process. This
     * provides access to interfaces implemented in the renderer to Java code in
     * the browser process.
     *
     * Callers are responsible to ensure that the renderer Frame exists before
     * trying to make a mojo connection to it. This can be done via
     * isRenderFrameCreated() if the caller is not inside the call-stack of an
     * IPC form the renderer (which would guarantee its existence at that time).
     *
     * @param pipe The message pipe to be connected to the renderer. If it fails
     * to make the connection, the pipe will be closed.
     */
    <I extends Interface, P extends Interface.Proxy> P getInterfaceToRendererFrame(
            Interface.Manager<I, P> manager);

    /**
     * Kills the renderer process when it is detected to be misbehaving and has
     * made a bad request.
     *
     * @param reason The BadMessageReason code from content::BadMessageReasons.
     */
    void terminateRendererDueToBadMessage(int reason);

    /**
     * Notifies the native RenderFrameHost about a user activation from the browser side.
     */
    void notifyUserActivation();

    /**
     * If a ModalCloseWatcher is active in this RenderFrameHost, signal it to close.
     * @return Whether a close signal was sent.
     */
    boolean signalModalCloseWatcherIfActive();

    /**
     * Returns whether we're in incognito mode.
     *
     * @return {@code true} if we're in incognito mode.
     */
    boolean isIncognito();

    /**
     * See native RenderFrameHost::IsRenderFrameCreated().
     *
     * @return {@code true} if render frame is created.
     */
    boolean isRenderFrameCreated();

    /**
     * @return Whether input events from the renderer are ignored on the browser side.
     */
    boolean areInputEventsIgnored();

    /**
     * Runs security checks associated with a Web Authentication GetAssertion request for the
     * the given relying party ID and an effective origin. If the request originated from a render
     * process, then the effective origin is the same as the last committed origin. However, if the
     * request originated from an internal request from the browser process (e.g. Payments
     * Autofill), then the relying party ID would not match the renderer's origin, and will
     * therefore have to provide its own effective origin. The return value is a code corresponding
     * to the AuthenticatorStatus mojo enum.
     *
     * @return Status code indicating the result of the GetAssertion request security checks.
     */
    int performGetAssertionWebAuthSecurityChecks(String relyingPartyId, Origin effectiveOrigin);

    /**
     * Runs security checks associated with a Web Authentication MakeCredential request for the
     * the given relying party ID and an effective origin. See
     * performGetAssertionWebAuthSecurityChecks for more on |effectiveOrigin|. The return value is a
     * code corresponding to the AuthenticatorStatus mojo enum.
     *
     * @return Status code indicating the result of the MakeCredential request security checks.
     */
    int performMakeCredentialWebAuthSecurityChecks(String relyingPartyId, Origin effectiveOrigin);

    /**
     * @return An identifier for this RenderFrameHost.
     */
    GlobalFrameRoutingId getGlobalFrameRoutingId();
}
