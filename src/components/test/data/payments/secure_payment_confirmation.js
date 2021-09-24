/*
 * Copyright 2020 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Creates and returns the first parameter to the PaymentRequest constructor for
 * secure payment confirmation.
 * @param {string} credentialIdentifier - An optional base64 encoded credential
 * identifier. If not specified, then 'cred' is used instead.
 * @return {Array<PaymentMethodData>} - Secure payment confirmation method data.
 */
function getTestMethodData(credentialIdentifier) {
  return [{
    supportedMethods: 'secure-payment-confirmation',
    data: {
      action: 'authenticate',
      credentialIds: [Uint8Array.from(
          (credentialIdentifier ? atob(credentialIdentifier) : 'cred'),
          (c) => c.charCodeAt(0))],
      challenge: Uint8Array.from('challenge', (c) => c.charCodeAt(0)),
      instrument: {
        displayName: 'display_name_for_instrument',
        icon: window.location.origin + '/icon.png',
      },
      timeout: 60000,
      payeeOrigin: 'https://example-payee-origin.test',
  }}];
}

/**
 * Returns the status field of the response to a secure payment confirmation
 * request.
 * @param {string} credentialIdentifier - An optional base64 encoded credential
 * identifier. If not specified, then 'cred' is used instead.
 * @return {string} - The status field or error message.
 */
async function getSecurePaymentConfirmationStatus(credentialIdentifier) { // eslint-disable-line no-unused-vars, max-len
  return getStatusForMethodData(getTestMethodData(credentialIdentifier));
}

/**
 * Checks the result of canMakePayment() (ignoring its actual result) and then
 * returns the status field of the response to a secure payment confirmation
 * request.
 * @return {string} - The status field or error message.
 */
async function getSecurePaymentConfirmationStatusAfterCanMakePayment() { // eslint-disable-line no-unused-vars, max-len
  return getStatusForMethodDataAfterCanMakePayment(
      getTestMethodData(), /* checkCanMakePaymentFirst = */true);
}

/**
 * Checks whether secure payment confirmation can make payments.
 * @return {string} - 'true', 'false', or error message on failure.
 */
async function securePaymentConfirmationCanMakePayment() { // eslint-disable-line no-unused-vars, max-len
  return canMakePaymentForMethodData(getTestMethodData());
}

/**
 * Creates a PaymentRequest for secure payment confirmation, checks
 * canMakePayment twice, and returns the second value.
 * @return {string} - 'true', 'false', or error message on failure.
 */
async function securePaymentConfirmationCanMakePaymentTwice() { // eslint-disable-line no-unused-vars, max-len
  return canMakePaymentForMethodDataTwice(getTestMethodData());
}

/**
 * Checks whether secure payment confirmation has enrolled instruments.
 * @return {string} - 'true', 'false', or error message on failure.
 */
async function securePaymentConfirmationHasEnrolledInstrument() { // eslint-disable-line no-unused-vars, max-len
  return hasEnrolledInstrumentForMethodData(getTestMethodData());
}

/**
 * Creates a secure payment confirmation credential and returns "OK" on success.
 * @param {string} icon - The URL of the icon for the credential.
 * @return {string} - Either "OK" or an error string.
 */
async function createPaymentCredential(icon) { // eslint-disable-line no-unused-vars, max-len
  try {
    // Intentionally ignore the result.
    await createAndReturnPaymentCredential(icon);
    return 'OK';
  } catch (e) {
    return e.toString();
  }
}

/**
 * Creates a secure payment confirmation credential and returns its identifier.
 * @param {string} icon - The URL of the icon for the credential.
 * @return {string} - The base64 encoded identifier of the new credential,
 * or the error message.
 */
async function createCredentialAndReturnItsIdentifier(icon) { // eslint-disable-line no-unused-vars, max-len
  try {
    const credential = await createAndReturnPaymentCredential(icon);
    return btoa(String.fromCharCode(...new Uint8Array(credential.rawId)));
  } catch (e) {
    return e.toString();
  }
}

/**
 * Creates a secure payment confirmation credential and returns its
 * clientDataJSON.type field.
 * @param {string} icon - The URL of the icon for the credential.
 * @return {string} - The clientDataJson.type field of the new credential.
 */
async function createCredentialAndReturnClientDataType(icon) { // eslint-disable-line no-unused-vars, max-len
  const credential = await createAndReturnPaymentCredential(icon);
  return JSON.parse(String.fromCharCode(...new Uint8Array(
      credential.response.clientDataJSON))).type;
}

/**
 * Creates a secure payment confirmation credential and returns its type.
 * @param {string} icon - The URL of the icon for the credential.
 * @return {string} - Either "PaymentCredential" or "PublicKeyCredential".
 */
async function createCredentialAndReturnItsType(icon) { // eslint-disable-line no-unused-vars, max-len
  const credential = await createAndReturnPaymentCredential(icon);
  return credential.constructor.name;
}

/**
 * Creates and returns a secure payment confirmation credential.
 * @param {string} icon - The URL of the icon for the credential.
 * @return {PaymentCredential} - The new credential.
 */
async function createAndReturnPaymentCredential(icon) {
  const textEncoder = new TextEncoder();
  const publicKeyRP = {
      id: 'a.com',
      name: 'Acme',
  };
  const publicKeyParameters = [{
      type: 'public-key',
      alg: -7,
  }];
  const publicKey = {
      user: {
        displayName: 'User',
        id: textEncoder.encode('user_123'),
        name: 'user@acme.com',
      },
      rp: publicKeyRP,
      challenge: textEncoder.encode('climb a mountain'),
      pubKeyCredParams: publicKeyParameters,
      extensions: {payment: {isPayment: true}},
  };
  return navigator.credentials.create({publicKey});
}

/**
 * Creates a public key credential with 'payment' extension and returns its
 * identifier in base64 encoding.
 * @return {DOMString} - The new credential's identifier in base64 encoding.
 */
async function createPublicKeyCredentialWithPaymentExtensionAndReturnItsId() { // eslint-disable-line no-unused-vars, max-len
  try {
    const textEncoder = new TextEncoder();
    const credential = await navigator.credentials.create({
      publicKey: {
        challenge: textEncoder.encode('climb a mountain'),
        rp: {
          id: 'a.com',
          name: 'Acme',
        },
        user: {
          displayName: 'User',
          id: textEncoder.encode('user_123'),
          name: 'user@acme.com',
        },
        pubKeyCredParams: [{
          alg: -7,
          type: 'public-key',
        }],
        timeout: 60000,
        attestation: 'direct',
        extensions: {payment: {isPayment: true}},
      },
    });
    return btoa(String.fromCharCode(...new Uint8Array(credential.rawId)));
  } catch (e) {
    return e.toString();
  }
}

/**
 * Attempts to create a payment credential that is missing the RP ID.
 * @param {string} icon - The URL of the icon for the credential.
 * @return {PaymentCredential} - The new credential.
 */
async function createCredentialWithNoRpId(icon) { // eslint-disable-line no-unused-vars, max-len
  const textEncoder = new TextEncoder();
  const publicKeyRP = {
      // id omitted
      name: 'Acme',
  };
  const publicKeyParameters = [{
      type: 'public-key',
      alg: -7,
  }];
  const publicKey = {
      user: {
        displayName: 'User',
        id: textEncoder.encode('user_123'),
        name: 'user@acme.com',
      },
      rp: publicKeyRP,
      challenge: textEncoder.encode('climb a mountain'),
      pubKeyCredParams: publicKeyParameters,
      extensions: {payment: {isPayment: true}},
  };
  return navigator.credentials.create({publicKey});
}
