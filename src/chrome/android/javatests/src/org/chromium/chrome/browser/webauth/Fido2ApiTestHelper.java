// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import static java.nio.charset.StandardCharsets.UTF_8;

import android.content.Intent;
import android.os.Parcel;
import android.os.SystemClock;
import android.util.Base64;

import androidx.annotation.Nullable;

import com.google.common.io.BaseEncoding;

import org.junit.Assert;

import org.chromium.base.test.util.JniMocker;
import org.chromium.blink.mojom.AuthenticatorAttachment;
import org.chromium.blink.mojom.AuthenticatorSelectionCriteria;
import org.chromium.blink.mojom.CableAuthentication;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.PaymentCredentialInstrument;
import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.blink.mojom.PrfValues;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialDescriptor;
import org.chromium.blink.mojom.PublicKeyCredentialParameters;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRpEntity;
import org.chromium.blink.mojom.PublicKeyCredentialType;
import org.chromium.blink.mojom.PublicKeyCredentialUserEntity;
import org.chromium.blink.mojom.UvmEntry;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.PaymentFeatureListJni;
import org.chromium.components.webauthn.Fido2Api;
import org.chromium.content.browser.ClientDataJsonImpl;
import org.chromium.content.browser.ClientDataJsonImplJni;
import org.chromium.mojo_base.mojom.TimeDelta;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.url.internal.mojom.Origin;
import org.chromium.url.mojom.Url;

import java.nio.ByteBuffer;
import java.util.concurrent.TimeUnit;

/* NO_BUILDER:
 *
 * Several comments below describe how binary blobs in this file were produced. However, they use
 * Builder classes that no longer appear to exist. Because of this, it's difficult to update these
 * blobs.
 *
 * The blobs are in Android's Parcel format. This format defines a tag-value object that represents
 * a (tag, bytestring) pair. A tag-value is serialized as a little-endian, uint32 where the bottom
 * 16 bits are the tag, and the top 16 bits are the length of the value. If the length is 0xffff,
 * then a second uint32 is used to store the actual length. (This two-word format appears to be
 * always used in practice, even when the length would fit in 16 bits.) The bytestring contains are
 * then the |length| following bytes.
 *
 * A Parcel consists of a tag-value object with tag 0x4f45 and whose value is the rest of the
 * Parcel data. That value contains a series of tag-values that define the members of the
 * destination object. Unknown tags are skipped over.
 *
 * The semantics of the values of the inner values are tag-specific. In the case of byte[] objects,
 * the value is, itself, a tag-value object. Since this has its own length, there can be padding at
 * the end and they seem to be padded with zeros to the nearest four-byte boundary.
 */

/**
 * A Helper class for testing Fido2ApiHandlerInternal.
 */
public class Fido2ApiTestHelper {
    // Test data.
    private static final String FILLER_ERROR_MSG = "Error Error";
    private static final int OBJECT_MAGIC = 20293;

    /**
     * This byte array was produced by
     * com.google.android.gms.fido.fido2.api.common.AuthenticatorAttestationResponse with test data,
     * i.e.:
     * AuthenticatorAttestationResponse response =
     *         new AuthenticatorAttestationResponse.Builder()
     *                 .setAttestationObject(TEST_ATTESTATION_OBJECT)
     *                 .setClientDataJSON(TEST_CLIENT_DATA_JSON)
     *                 .setKeyHandle(TEST_KEY_HANDLE)
     *                 .build().serializeToBytes();
     *
     * NOTE: See NO_BUILDER comment, above. Additionally this byte array was modified by prepending
     * an object header and tag with value four so that it's compatible with
     * FIDO2_KEY_CREDENTIAL_EXTRA.
     */
    private static final byte[] TEST_AUTHENTICATOR_ATTESTATION_RESPONSE = new byte[] {69, 79, -1,
            -1, 60, 1, 0, 0, 4, 0, -1, -1, 52, 1, 0, 0, 69, 79, -1, -1, 44, 1, 0, 0, 2, 0, -1, -1,
            36, 0, 0, 0, 32, 0, 0, 0, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5,
            6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 9, 3, 0, -1, -1, 8, 0, 0, 0, 3, 0, 0, 0, 4, 5, 6, 0, 4, 0,
            -1, -1, -24, 0, 0, 0, -30, 0, 0, 0, -93, 99, 102, 109, 116, 100, 110, 111, 110, 101,
            103, 97, 116, 116, 83, 116, 109, 116, -96, 104, 97, 117, 116, 104, 68, 97, 116, 97, 88,
            -60, 38, -67, 114, 120, -66, 70, 55, 97, -15, -6, -95, -79, 10, -76, -60, -8, 38, 112,
            38, -100, 65, 12, 114, 106, 31, -42, -32, 88, 85, -31, -101, 70, 65, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 124, 80, -60, -114, 69, -117, 44, -120,
            122, -62, 63, 104, 18, -66, 2, -3, -56, 35, -24, 66, -4, 74, 48, -128, -52, 80, -100,
            46, 97, 93, -25, -21, -53, 40, 123, 90, -107, -20, 111, -4, 15, 64, 122, 15, -84, -21,
            -33, -15, 26, 11, 35, 36, -49, 116, 52, -74, 107, 63, 113, -59, 125, -27, -120, -63,
            -91, 1, 2, 3, 38, 32, 1, 33, 88, 32, -75, -80, 118, 102, -14, 124, -108, -9, -27, -91,
            59, -48, -92, -102, -38, -44, 92, 95, 14, -62, 41, -117, -70, 101, 9, 64, 35, 31, -20,
            79, -71, -71, 34, 88, 32, -24, -33, 64, 97, -31, -34, 96, -83, -119, -25, 21, -14, -56,
            -70, -37, -116, -21, -33, -128, -66, 61, 41, 107, 16, -25, 120, 106, -113, 54, -62,
            -102, 42, 0, 0};

    /**
     * This byte array was produced by
     * com.google.android.gms.fido.fido2.api.common.AuthenticatorAssertionResponse with test data,
     * i.e.:
     * AuthenticatorAssertionResponse.Builder()
     *         .setAuthenticatorData(TEST_AUTHENTICATOR_DATA)
     *         .setSignature(TEST_SIGNATURE)
     *         .setClientDataJSON(TEST_CLIENT_DATA_JSON)
     *         .setKeyHandle(TEST_KEY_HANDLE)
     *         .build().serializeToBytes();
     *
     * NOTE: See NO_BUILDER comment, above. Additionally this byte array was modified by prepending
     * an object header and tag with value five so that it's compatible with
     * FIDO2_KEY_CREDENTIAL_EXTRA.
     */
    private static final byte[] TEST_AUTHENTICATOR_ASSERTION_RESPONSE =
            new byte[] {69, 79, -1, -1, 108, 0, 0, 0, 5, 0, -1, -1, 100, 0, 0, 0, 69, 79, -1, -1,
                    92, 0, 0, 0, 2, 0, -1, -1, 36, 0, 0, 0, 32, 0, 0, 0, 5, 6, 7, 8, 5, 6, 7, 8, 5,
                    6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 9, 3, 0, -1,
                    -1, 8, 0, 0, 0, 3, 0, 0, 0, 4, 5, 6, 0, 4, 0, -1, -1, 8, 0, 0, 0, 3, 0, 0, 0, 7,
                    8, 9, 0, 5, 0, -1, -1, 8, 0, 0, 0, 3, 0, 0, 0, 10, 11, 12, 0};

    /**
     * This byte array is produced by
     * com.google.android.gms.fido.fido2.api.common.PublicKeyCredential with test data,
     * i.e.:
     * AuthenticatorAssertionResponse response =
     *         new AuthenticatorAssertionResponse.Builder()
     *                 .setAuthenticatorData(TEST_AUTHENTICATOR_DATA)
     *                 .setSignature(TEST_SIGNATURE)
     *                 .setClientDataJSON(TEST_CLIENT_DATA_JSON)
     *                 .setKeyHandle(TEST_KEY_HANDLE)
     *                 .build();
     * UvmEntry uvmEntry0 =
     *         new UvmEntry.Builder()
     *                 .setUserVerificationMethod(TEST_USER_VERIFICATION_METHOD[0])
     *                 .setKeyProtectionType(TEST_KEY_PROTECTION_TYPE[0])
     *                 .setMatcherProtectionType(TEST_MATCHER_PROTECTION_TYPE[0])
     *                 .build();
     * UvmEntry uvmEntry1 =
     *         new UvmEntry.Builder()
     *                 .setUserVerificationMethod(TEST_USER_VERIFICATION_METHOD[1])
     *                 .setKeyProtectionType(TEST_KEY_PROTECTION_TYPE[1])
     *                 .setMatcherProtectionType(TEST_MATCHER_PROTECTION_TYPE[1])
     *                 .build();
     * UvmEntries uvmEntries =
     *         new UvmEntries.Builder().addUvmEntry(uvmEntry0).addUvmEntry(uvmEntry1).build();
     * AuthenticationExtensionsClientOutputs
     *         authenticationExtensionsClientOutputs =
     *                 new AuthenticationExtensionsClientOutputs.Builder()
     *                         .setUserVerificationMethodEntries(uvmEntries)
     *                         .build();
     * PublicKeyCredential publicKeyCredential =
     *         new PublicKeyCredential.Builder()
     *                 .setResponse(response)
     *                 .setAuthenticationExtensionsClientOutputs(
     *                         authenticationExtensionsClientOutputs)
     *                 .build().serializeToBytes();
     *
     * NOTE: See NO_BUILDER comment, above.
     */
    private static final byte[] TEST_ASSERTION_PUBLIC_KEY_CREDENTIAL_WITH_UVM = new byte[] {69, 79,
            -1, -1, 4, 1, 0, 0, 2, 0, -1, -1, 28, 0, 0, 0, 10, 0, 0, 0, 112, 0, 117, 0, 98, 0, 108,
            0, 105, 0, 99, 0, 45, 0, 107, 0, 101, 0, 121, 0, 0, 0, 0, 0, 5, 0, -1, -1, 100, 0, 0, 0,
            69, 79, -1, -1, 92, 0, 0, 0, 2, 0, -1, -1, 36, 0, 0, 0, 32, 0, 0, 0, 5, 6, 7, 8, 5, 6,
            7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 9, 3, 0, -1,
            -1, 8, 0, 0, 0, 3, 0, 0, 0, 4, 5, 6, 0, 4, 0, -1, -1, 8, 0, 0, 0, 3, 0, 0, 0, 7, 8, 9,
            0, 5, 0, -1, -1, 8, 0, 0, 0, 3, 0, 0, 0, 10, 11, 12, 0, 7, 0, -1, -1, 108, 0, 0, 0, 69,
            79, -1, -1, 100, 0, 0, 0, 1, 0, -1, -1, 92, 0, 0, 0, 69, 79, -1, -1, 84, 0, 0, 0, 1, 0,
            -1, -1, 76, 0, 0, 0, 2, 0, 0, 0, 32, 0, 0, 0, 69, 79, -1, -1, 24, 0, 0, 0, 1, 0, 4, 0,
            2, 0, 0, 0, 2, 0, 4, 0, 2, 0, 0, 0, 3, 0, 4, 0, 4, 0, 0, 0, 32, 0, 0, 0, 69, 79, -1, -1,
            24, 0, 0, 0, 1, 0, 4, 0, 0, 2, 0, 0, 2, 0, 4, 0, 1, 0, 0, 0, 3, 0, 4, 0, 1, 0, 0, 0};

    private static final byte[] TEST_KEY_HANDLE = BaseEncoding.base16().decode(
            "0506070805060708050607080506070805060708050607080506070805060709");
    private static final String TEST_ENCODED_KEY_HANDLE = Base64.encodeToString(
            TEST_KEY_HANDLE, Base64.URL_SAFE | Base64.NO_PADDING | Base64.NO_WRAP);
    private static final byte[] TEST_ATTESTATION_OBJECT = new byte[] {-93, 99, 102, 109, 116, 100,
            110, 111, 110, 101, 103, 97, 116, 116, 83, 116, 109, 116, -96, 104, 97, 117, 116, 104,
            68, 97, 116, 97, 88, -60, 38, -67, 114, 120, -66, 70, 55, 97, -15, -6, -95, -79, 10,
            -76, -60, -8, 38, 112, 38, -100, 65, 12, 114, 106, 31, -42, -32, 88, 85, -31, -101, 70,
            65, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 124, 80, -60,
            -114, 69, -117, 44, -120, 122, -62, 63, 104, 18, -66, 2, -3, -56, 35, -24, 66, -4, 74,
            48, -128, -52, 80, -100, 46, 97, 93, -25, -21, -53, 40, 123, 90, -107, -20, 111, -4, 15,
            64, 122, 15, -84, -21, -33, -15, 26, 11, 35, 36, -49, 116, 52, -74, 107, 63, 113, -59,
            125, -27, -120, -63, -91, 1, 2, 3, 38, 32, 1, 33, 88, 32, -75, -80, 118, 102, -14, 124,
            -108, -9, -27, -91, 59, -48, -92, -102, -38, -44, 92, 95, 14, -62, 41, -117, -70, 101,
            9, 64, 35, 31, -20, 79, -71, -71, 34, 88, 32, -24, -33, 64, 97, -31, -34, 96, -83, -119,
            -25, 21, -14, -56, -70, -37, -116, -21, -33, -128, -66, 61, 41, 107, 16, -25, 120, 106,
            -113, 54, -62, -102, 42};

    // TEST_DISCOVERABLE_CREDENTIAL_ASSERTION is the payload of an Intent response for an assertion
    // with an empty allowList. This was captured from Play Services.
    private static final byte[] TEST_DISCOVERABLE_CREDENTIAL_ASSERTION = new byte[] {69, 79, -1, -1,
            100, 1, 0, 0, 1, 0, -1, -1, 52, 0, 0, 0, 22, 0, 0, 0, 99, 0, 72, 0, 116, 0, 85, 0, 70,
            0, 86, 0, 112, 0, 82, 0, 88, 0, 99, 0, 73, 0, 50, 0, 109, 0, 67, 0, 49, 0, 76, 0, 119,
            0, 106, 0, 95, 0, 85, 0, 72, 0, 65, 0, 0, 0, 0, 0, 2, 0, -1, -1, 28, 0, 0, 0, 10, 0, 0,
            0, 112, 0, 117, 0, 98, 0, 108, 0, 105, 0, 99, 0, 45, 0, 107, 0, 101, 0, 121, 0, 0, 0, 0,
            0, 3, 0, -1, -1, 20, 0, 0, 0, 16, 0, 0, 0, 112, 123, 84, 21, 90, 81, 93, -62, 54, -104,
            45, 75, -62, 63, -44, 28, 5, 0, -1, -1, -32, 0, 0, 0, 69, 79, -1, -1, -40, 0, 0, 0, 2,
            0, -1, -1, 20, 0, 0, 0, 16, 0, 0, 0, 112, 123, 84, 21, 90, 81, 93, -62, 54, -104, 45,
            75, -62, 63, -44, 28, 3, 0, -1, -1, 16, 0, 0, 0, 9, 0, 0, 0, 60, 105, 110, 118, 97, 108,
            105, 100, 62, 0, 0, 0, 4, 0, -1, -1, 44, 0, 0, 0, 37, 0, 0, 0, -28, 83, 41, -48, 58, 32,
            104, -47, -54, -9, -9, -69, 10, -23, 84, -26, -80, -26, 37, -105, 69, -13, 47, 72, 41,
            -9, 80, -16, 80, 17, -7, -62, 5, 0, 0, 0, 0, 0, 0, 0, 5, 0, -1, -1, 76, 0, 0, 0, 70, 0,
            0, 0, 48, 68, 2, 32, 86, -36, 80, 3, 65, -90, 66, 76, 100, -126, -34, 82, -22, 96, 3,
            71, 3, 68, 57, 62, -4, -80, 34, 119, 97, 30, 100, -65, -36, 95, -68, 19, 2, 32, 91, 52,
            -110, -105, -104, 105, 94, 41, 63, -97, -86, 15, 35, 117, 61, 73, 119, -113, 95, 69, 44,
            -13, -22, 61, 32, 79, 53, 106, 127, -67, 32, 4, 0, 0, 6, 0, -1, -1, 20, 0, 0, 0, 15, 0,
            0, 0, 98, 111, 98, 64, 101, 120, 97, 109, 112, 108, 101, 46, 99, 111, 109, 0};

    // TEST_USER_HANDLE is the user ID contained within `TEST_DISCOVERABLE_CREDENTIAL_ASSERTION`.
    public static final byte[] TEST_USER_HANDLE = "bob@example.com".getBytes(UTF_8);

    private static final byte[] TEST_CLIENT_DATA_JSON = new byte[] {4, 5, 6};
    private static final byte[] TEST_AUTHENTICATOR_DATA = new byte[] {7, 8, 9};
    private static final byte[] TEST_SIGNATURE = new byte[] {10, 11, 12};
    private static final long TIMEOUT_SAFETY_MARGIN_MS = scaleTimeout(TimeUnit.SECONDS.toMillis(1));
    private static final long TIMEOUT_MS = scaleTimeout(TimeUnit.SECONDS.toMillis(1));
    private static final int[] TEST_USER_VERIFICATION_METHOD = new int[] {0x00000002, 0x00000200};
    private static final short[] TEST_KEY_PROTECTION_TYPE = new short[] {0x0002, 0x0001};
    private static final short[] TEST_MATCHER_PROTECTION_TYPE = new short[] {0x0004, 0x0001};
    private static Url createUrl(String s) {
        Url url = new Url();
        url.url = s;
        return url;
    }

    /**
     * Builds a test intent to be returned by a successful call to makeCredential.
     * @return Intent containing the response from the Fido2 API.
     */
    public static Intent createSuccessfulMakeCredentialIntent() {
        Intent intent = new Intent();
        intent.putExtra(Fido2Api.CREDENTIAL_EXTRA, TEST_AUTHENTICATOR_ATTESTATION_RESPONSE);
        return intent;
    }

    /**
     * Construct default options for a makeCredential request.
     * @return Options for the Fido2 API.
     * @throws Exception
     */
    public static PublicKeyCredentialCreationOptions createDefaultMakeCredentialOptions()
            throws Exception {
        PublicKeyCredentialCreationOptions options = new PublicKeyCredentialCreationOptions();
        options.challenge = "climb a mountain".getBytes("UTF8");

        options.relyingParty = new PublicKeyCredentialRpEntity();
        options.relyingParty.id = "subdomain.example.test";
        options.relyingParty.name = "Acme";
        options.relyingParty.icon = createUrl("https://icon.example.test");

        options.user = new PublicKeyCredentialUserEntity();
        options.user.id = "1098237235409872".getBytes("UTF8");
        options.user.name = "avery.a.jones@example.com";
        options.user.displayName = "Avery A. Jones";
        options.user.icon = createUrl("https://usericon.example.test");

        options.timeout = new TimeDelta();
        options.timeout.microseconds = TimeUnit.MILLISECONDS.toMicros(TIMEOUT_MS);

        PublicKeyCredentialParameters parameters = new PublicKeyCredentialParameters();
        parameters.algorithmIdentifier = -7;
        parameters.type = PublicKeyCredentialType.PUBLIC_KEY;
        options.publicKeyParameters = new PublicKeyCredentialParameters[] {parameters};

        PublicKeyCredentialDescriptor descriptor = new PublicKeyCredentialDescriptor();
        descriptor.type = 0;
        descriptor.id = new byte[] {8, 7, 6};
        descriptor.transports = new int[] {0};
        options.excludeCredentials = new PublicKeyCredentialDescriptor[] {descriptor};

        options.authenticatorSelection = new AuthenticatorSelectionCriteria();
        /* TODO add UserVerificationRequirement and ResidentKeyRequirement when the FIDO2 API
         * supports it */
        options.authenticatorSelection.authenticatorAttachment =
                AuthenticatorAttachment.CROSS_PLATFORM;
        return options;
    }

    /**
     * Verifies that the returned response matches expected values.
     * @param response The response from the Fido2 API.
     */
    public static void validateMakeCredentialResponse(
            MakeCredentialAuthenticatorResponse response) {
        Assert.assertArrayEquals(response.attestationObject, TEST_ATTESTATION_OBJECT);
        Assert.assertArrayEquals(response.info.rawId, TEST_KEY_HANDLE);
        Assert.assertEquals(response.info.id, TEST_ENCODED_KEY_HANDLE);
        Assert.assertArrayEquals(response.info.clientDataJson, TEST_CLIENT_DATA_JSON);
    }

    /**
     * Constructs default options for a getAssertion request.
     * @return Options for the Fido2 API
     * @throws Exception
     */
    public static PublicKeyCredentialRequestOptions createDefaultGetAssertionOptions()
            throws Exception {
        PublicKeyCredentialRequestOptions options = new PublicKeyCredentialRequestOptions();
        options.challenge = "climb a mountain".getBytes("UTF8");
        options.timeout = new TimeDelta();
        options.timeout.microseconds = TimeUnit.MILLISECONDS.toMicros(TIMEOUT_MS);
        options.relyingPartyId = "subdomain.example.test";

        PublicKeyCredentialDescriptor descriptor = new PublicKeyCredentialDescriptor();
        descriptor.type = 0;
        descriptor.id = new byte[] {8, 7, 6};
        descriptor.transports = new int[] {0};
        options.allowCredentials = new PublicKeyCredentialDescriptor[] {descriptor};

        options.cableAuthenticationData = new CableAuthentication[] {};
        options.prfInputs = new PrfValues[] {};
        return options;
    }

    /**
     * Builds a test intent without uvm extension to be returned by a successful call to
     * makeCredential.
     * @return Intent containing the response from the Fido2 API.
     */
    public static Intent createSuccessfulGetAssertionIntent() {
        Intent intent = new Intent();
        intent.putExtra(Fido2Api.CREDENTIAL_EXTRA, TEST_AUTHENTICATOR_ASSERTION_RESPONSE);
        return intent;
    }

    /**
     * Builds a test intent with uvm extension to be returned by a successful call to
     * makeCredential.
     * @return Intent containing the response from the Fido2 API.
     */
    public static Intent createSuccessfulGetAssertionIntentWithUvm() {
        Intent intent = new Intent();
        intent.putExtra(Fido2Api.CREDENTIAL_EXTRA, TEST_ASSERTION_PUBLIC_KEY_CREDENTIAL_WITH_UVM);
        return intent;
    }

    /**
     * Builds a test intent that contains a response to an assertion that had an empty allowList.
     */
    public static Intent createSuccessfulGetAssertionIntentWithUserId() {
        Intent intent = new Intent();
        intent.putExtra(Fido2Api.CREDENTIAL_EXTRA, TEST_DISCOVERABLE_CREDENTIAL_ASSERTION);
        return intent;
    }

    /**
     * Verifies that the returned userVerificationMethod matches expected values.
     * @param userVerificationMethods The userVerificationMethods from the Fido2 API.
     */
    public static void validateUserVerificationMethods(
            boolean echoUserVerificationMethods, UvmEntry[] userVerificationMethods) {
        if (echoUserVerificationMethods) {
            Assert.assertEquals(
                    userVerificationMethods.length, TEST_USER_VERIFICATION_METHOD.length);
            for (int i = 0; i < userVerificationMethods.length; i++) {
                Assert.assertEquals(userVerificationMethods[i].userVerificationMethod,
                        TEST_USER_VERIFICATION_METHOD[i]);
                Assert.assertEquals(
                        userVerificationMethods[i].keyProtectionType, TEST_KEY_PROTECTION_TYPE[i]);
                Assert.assertEquals(userVerificationMethods[i].matcherProtectionType,
                        TEST_MATCHER_PROTECTION_TYPE[i]);
            }
        }
    }

    /**
     * Verifies that the returned response matches expected values.
     * @param response The response from the Fido2 API.
     */
    public static void validateGetAssertionResponse(GetAssertionAuthenticatorResponse response) {
        Assert.assertArrayEquals(response.info.authenticatorData, TEST_AUTHENTICATOR_DATA);
        Assert.assertArrayEquals(response.signature, TEST_SIGNATURE);
        Assert.assertArrayEquals(response.info.rawId, TEST_KEY_HANDLE);
        Assert.assertEquals(response.info.id, TEST_ENCODED_KEY_HANDLE);
        Assert.assertArrayEquals(response.info.clientDataJson, TEST_CLIENT_DATA_JSON);
        validateUserVerificationMethods(
                response.echoUserVerificationMethods, response.userVerificationMethods);
    }

    /**
     * Verifies that the response did not return before timeout.
     * @param startTimeMs The start time of the operation.
     */
    public static void verifyRespondedBeforeTimeout(long startTimeMs) {
        long elapsedTime = SystemClock.elapsedRealtime() - startTimeMs;
        Assert.assertTrue(elapsedTime < TIMEOUT_MS);
    }

    private static void appendErrorResponseToParcel(
            int errorCode, @Nullable String message, Parcel parcel) {
        final int a = writeHeader(OBJECT_MAGIC, parcel);
        final int b = writeHeader(6, parcel);
        final int c = writeHeader(OBJECT_MAGIC, parcel);

        int z = writeHeader(2, parcel);
        parcel.writeInt(errorCode);
        writeLength(z, parcel);

        if (message != null) {
            z = writeHeader(3, parcel);
            parcel.writeString(message);
            writeLength(z, parcel);
        }

        writeLength(c, parcel);
        writeLength(b, parcel);
        writeLength(a, parcel);
    }

    private static int writeHeader(int tag, Parcel parcel) {
        parcel.writeInt(0xffff0000 | tag);
        return startLength(parcel);
    }

    private static int startLength(Parcel parcel) {
        int pos = parcel.dataPosition();
        parcel.writeInt(0xdddddddd);
        return pos;
    }

    private static void writeLength(int pos, Parcel parcel) {
        int totalLength = parcel.dataPosition();
        parcel.setDataPosition(pos);
        parcel.writeInt(totalLength - pos - 4);
        parcel.setDataPosition(totalLength);
    }

    /**
     * Constructs an intent that returns an error response from the Fido2 API.
     * @param errorCode Numeric values corresponding to a Fido2 error.
     * @return an Intent containing the error response.
     */
    public static Intent createErrorIntent(int errorCode, @Nullable String errorMsg) {
        Parcel parcel = Parcel.obtain();
        appendErrorResponseToParcel(errorCode, errorMsg, parcel);

        Intent intent = new Intent();
        intent.putExtra(Fido2Api.CREDENTIAL_EXTRA, parcel.marshall());
        return intent;
    }

    /** Creates a PaymentOptions object with normal values. */
    public static PaymentOptions createPaymentOptions() {
        PaymentOptions options = new PaymentOptions();
        options.total = new PaymentCurrencyAmount();
        options.total.currency = "USD";
        options.total.value = "888";
        options.instrument = new PaymentCredentialInstrument();
        options.instrument.displayName = "MaxPay";
        options.instrument.icon = new Url();
        options.instrument.icon.url = "https://www.google.com/icon.png";
        options.payeeOrigin = new Origin();
        options.payeeOrigin.scheme = "https";
        options.payeeOrigin.host = "test.example";
        options.payeeOrigin.port = 443;
        return options;
    }

    /**
     * Mocks PaymentFeatureList so that the Security Payment Confirmation flag has been enabled.
     * @param mocker The mocker used to mock the PaymentFeatureListJni.
     */
    public static void setSecurePaymentConfirmationEnabled(JniMocker mocker) {
        PaymentFeatureList.Natives paymentFeatureListJni = new PaymentFeatureList.Natives() {
            @Override
            public boolean isEnabled(String featureName) {
                return featureName.equals(PaymentFeatureList.SECURE_PAYMENT_CONFIRMATION);
            }
        };
        mocker.mock(PaymentFeatureListJni.TEST_HOOKS, paymentFeatureListJni);
    }

    /**
     * Mocks ClientDataJson so that it returns the provided result.
     * @param mocker The mocker used to mock the PaymentFeatureListJni.
     * @param mockResult The mock value for {@link ClientDataJson#buildClientDataJson} to return.
     */
    public static void mockClientDataJson(JniMocker mocker, String mockResult) {
        ClientDataJsonImpl.Natives clientDataJsonJni = new ClientDataJsonImpl.Natives() {
            @Override
            public String buildClientDataJson(int clientDataRequestType, String callerOrigin,
                    byte[] challenge, boolean isCrossOrigin, ByteBuffer optionsByteBuffer,
                    String relyingPartyId, String topOrigin) {
                return mockResult;
            }
        };
        mocker.mock(ClientDataJsonImplJni.TEST_HOOKS, clientDataJsonJni);
    }
}
