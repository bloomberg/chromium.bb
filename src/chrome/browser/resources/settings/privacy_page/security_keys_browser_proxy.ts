// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * Ctap2Status contains a subset of CTAP2 status codes. See
 * device::CtapDeviceResponseCode for the full list.
 */
export enum Ctap2Status {
  OK = 0x0,
  ERR_FP_DATABASE_FULL = 0x17,
  ERR_INVALID_OPTION = 0x2C,
  ERR_KEEPALIVE_CANCEL = 0x2D,
}

/**
 * Credential represents a CTAP2 resident credential enumerated from a
 * security key.
 *
 * id: (required) The hex encoding of the CBOR-serialized
 *     PublicKeyCredentialDescriptor of the credential.
 *
 * relyingPartyId: (required) The RP ID (i.e. the site that created the
 *     credential; eTLD+n)
 *
 * userName: (required) The PublicKeyCredentialUserEntity.name
 *
 * userDisplayName: (required) The PublicKeyCredentialUserEntity.display_name
 *
 * @see chrome/browser/ui/webui/settings/settings_security_key_handler.cc
 */
export type Credential = {
  id: string,
  relyingPartyId: string,
  userName: string,
  userDisplayName: string,
};

/**
 * Encapsulates information about an authenticator's biometric sensor.
 */
export type SensorInfo = {
  maxTemplateFriendlyName: number,
  maxSamplesForEnroll: number|null,
};

/**
 * SampleStatus is the result for reading an individual sample ("touch")
 * during a fingerprint enrollment. This is a subset of the
 * lastEnrollSampleStatus enum defined in the CTAP spec.
 */
export enum SampleStatus {
  OK = 0x0,
}

/**
 * SampleResponse indicates the result of an individual sample (sensor touch)
 * for an enrollment suboperation.
 *
 * @see chrome/browser/ui/webui/settings/settings_security_key_handler.cc
 */
export type SampleResponse = {
  status: SampleStatus,
  remaining: number,
};

/**
 * EnrollmentResponse is the final response to an enrollment suboperation,
 *
 * @see chrome/browser/ui/webui/settings/settings_security_key_handler.cc
 */
export type EnrollmentResponse = {
  code: Ctap2Status,
  enrollment: Enrollment|null,
};

/**
 * Enrollment represents a valid fingerprint template stored on a security
 * key, which can be used in a user verification request.
 *
 * @see chrome/browser/ui/webui/settings/settings_security_key_handler.cc
 */
export type Enrollment = {
  name: string,
  id: string,
};

/**
 * SetPINResponse represents the response to startSetPIN and setPIN requests.
 *
 * @see chrome/browser/ui/webui/settings/settings_security_key_handler.cc
 */
export type SetPINResponse = {
  done: boolean,
  error?: number,
  currentMinPinLength?: number,
  newMinPinLength?: number,
  retries?: number,
};

export interface SecurityKeysPINBrowserProxy {
  /**
   * Starts a PIN set/change operation by flashing all security keys. Resolves
   * with a pair of numbers. The first is one if the process has immediately
   * completed (i.e. failed). In this case the second is a CTAP error code.
   * Otherwise the process is ongoing and must be completed by calling
   * |setPIN|. In this case the second number is either the number of tries
   * remaining to correctly specify the current PIN, or else null to indicate
   * that no PIN is currently set.
   */
  startSetPIN(): Promise<SetPINResponse>;

  /**
   * Attempts a PIN set/change operation. Resolves with a pair of numbers
   * whose meaning is the same as with |startSetPIN|. The first number will
   * always be 1 to indicate that the process has completed and thus the
   * second will be the CTAP error code.
   */
  setPIN(oldPIN: string, newPIN: string): Promise<SetPINResponse>;

  /** Cancels all outstanding operations. */
  close(): void;
}

export interface SecurityKeysCredentialBrowserProxy {
  /**
   * Starts a credential management operation.
   *
   * Callers must listen to errors that can occur during the operation via a
   * 'security-keys-credential-management-error' WebListener. Values received
   * via this listener are localized error strings. When the
   * WebListener fires, the operation must be considered terminated.
   *
   * @return A promise that resolves when the handler is ready for
   *     the authenticator PIN to be provided.
   */
  startCredentialManagement(): Promise<Array<number>>;

  /**
   * Provides a PIN for a credential management operation. The
   * startCredentialManagement() promise must have resolved before this method
   * may be called.
   * @return A promise that resolves with null if the PIN
   *     was correct or the number of retries remaining otherwise.
   */
  providePIN(pin: string): Promise<number|null>;

  /**
   * Enumerates credentials on the authenticator. A correct PIN must have
   * previously been supplied via providePIN() before this
   * method may be called.
   */
  enumerateCredentials(): Promise<Array<Credential>>;

  /**
   * Deletes the credentials with the given IDs from the security key.
   * @return A localized response message to display to
   *     the user (on either success or error)
   */
  deleteCredentials(ids: Array<string>): Promise<string>;

  /** Cancels all outstanding operations. */
  close(): void;
}

export interface SecurityKeysResetBrowserProxy {
  /**
   * Starts a reset operation by flashing all security keys and sending a
   * reset command to the one that the user activates. Resolves with a CTAP
   * error code.
   */
  reset(): Promise<number>;

  /**
   * Waits for a reset operation to complete. Resolves with a CTAP error code.
   */
  completeReset(): Promise<number>;

  /** Cancels all outstanding operations. */
  close(): void;
}

export interface SecurityKeysBioEnrollProxy {
  /**
   * Starts a biometric enrollment operation.
   *
   * Callers must listen to errors that can occur during this operation via a
   * 'security-keys-bio-enrollment-error' WebUIListener. Values received via
   * this listener are localized error strings. The WebListener may fire at
   * any point during the operation (enrolling, deleting, etc) and when it
   * fires, the operation must be considered terminated.
   *
   * @return Resolves when the handler is ready for the
   *     authentcation PIN to be provided.
   */
  startBioEnroll(): Promise<Array<number>>;

  /**
   * Provides a PIN for a biometric enrollment operation. The startBioEnroll()
   * Promise must have resolved before this method may be called.
   *
   * @return Resolves with null if the PIN was correct, or
   *     with the number of retries remaining otherwise.
   */
  providePIN(pin: string): Promise<number|null>;

  /**
   * Obtains the |SensorInfo| for the authenticator. A correct PIN must have
   * previously been supplied via providePIN() before this method may be called.
   */
  getSensorInfo(): Promise<SensorInfo>;

  /**
   * Enumerates enrollments on the authenticator. A correct PIN must have
   * previously been supplied via providePIN() before this method may be called.
   */
  enumerateEnrollments(): Promise<Array<Enrollment>>;

  /**
   * Move the operation into enrolling mode, which instructs the authenticator
   * to start sampling for touches.
   *
   * Callers must listen to status updates that will occur during this
   * suboperation via a 'security-keys-bio-enroll-status' WebListener. Values
   * received via this listener are DictionaryValues with two elements (see
   * below). When the WebListener fires, the authenticator has either timed
   * out waiting for a touch, or has successfully processed a touch. Any
   * errors will fire the 'security-keys-bio-enrollment-error' WebListener.
   *
   * @return Resolves when the enrollment operation is finished successfully.
   */
  startEnrolling(): Promise<EnrollmentResponse>;

  /**
   * Cancel an ongoing enrollment suboperation. This can safely be called at
   * any time and only has an impact when the authenticator is currently
   * sampling.
   */
  cancelEnrollment(): void;

  /**
   * Deletes the enrollment with the given ID.
   *
   * @return The remaining enrollments.
   */
  deleteEnrollment(id: string): Promise<Array<Enrollment>>;

  /**
   * Renames the enrollment with the given ID.
   *
   * @return The updated list of enrollments.
   */
  renameEnrollment(id: string, name: string): Promise<Array<Enrollment>>;

  /** Cancels all outstanding operations. */
  close(): void;
}

export class SecurityKeysPINBrowserProxyImpl implements
    SecurityKeysPINBrowserProxy {
  startSetPIN() {
    return sendWithPromise('securityKeyStartSetPIN');
  }

  setPIN(oldPIN: string, newPIN: string) {
    return sendWithPromise('securityKeySetPIN', oldPIN, newPIN);
  }

  close() {
    return chrome.send('securityKeyPINClose');
  }

  static getInstance(): SecurityKeysPINBrowserProxy {
    return pinBrowserProxyInstance ||
        (pinBrowserProxyInstance = new SecurityKeysPINBrowserProxyImpl());
  }

  static setInstance(obj: SecurityKeysPINBrowserProxy) {
    pinBrowserProxyInstance = obj;
  }
}

let pinBrowserProxyInstance: SecurityKeysPINBrowserProxy|null = null;

export class SecurityKeysCredentialBrowserProxyImpl implements
    SecurityKeysCredentialBrowserProxy {
  startCredentialManagement() {
    return sendWithPromise('securityKeyCredentialManagementStart');
  }

  providePIN(pin: string) {
    return sendWithPromise('securityKeyCredentialManagementPIN', pin);
  }

  enumerateCredentials() {
    return sendWithPromise('securityKeyCredentialManagementEnumerate');
  }

  deleteCredentials(ids: Array<string>) {
    return sendWithPromise('securityKeyCredentialManagementDelete', ids);
  }

  close() {
    return chrome.send('securityKeyCredentialManagementClose');
  }

  static getInstance(): SecurityKeysCredentialBrowserProxy {
    return credentialBrowserProxyInstance ||
        (credentialBrowserProxyInstance =
             new SecurityKeysCredentialBrowserProxyImpl());
  }

  static setInstance(obj: SecurityKeysCredentialBrowserProxy) {
    credentialBrowserProxyInstance = obj;
  }
}

let credentialBrowserProxyInstance: SecurityKeysCredentialBrowserProxy|null =
    null;

export class SecurityKeysResetBrowserProxyImpl implements
    SecurityKeysResetBrowserProxy {
  reset() {
    return sendWithPromise('securityKeyReset');
  }

  completeReset() {
    return sendWithPromise('securityKeyCompleteReset');
  }

  close() {
    return chrome.send('securityKeyResetClose');
  }

  static getInstance(): SecurityKeysResetBrowserProxy {
    return resetBrowserProxyInstance ||
        (resetBrowserProxyInstance = new SecurityKeysResetBrowserProxyImpl());
  }

  static setInstance(obj: SecurityKeysResetBrowserProxy) {
    resetBrowserProxyInstance = obj;
  }
}

let resetBrowserProxyInstance: SecurityKeysResetBrowserProxy|null = null;

export class SecurityKeysBioEnrollProxyImpl implements
    SecurityKeysBioEnrollProxy {
  startBioEnroll() {
    return sendWithPromise('securityKeyBioEnrollStart');
  }

  providePIN(pin: string) {
    return sendWithPromise('securityKeyBioEnrollProvidePIN', pin);
  }

  getSensorInfo() {
    return sendWithPromise('securityKeyBioEnrollGetSensorInfo');
  }

  enumerateEnrollments() {
    return sendWithPromise('securityKeyBioEnrollEnumerate');
  }

  startEnrolling() {
    return sendWithPromise('securityKeyBioEnrollStartEnrolling');
  }

  cancelEnrollment() {
    return chrome.send('securityKeyBioEnrollCancel');
  }

  deleteEnrollment(id: string) {
    return sendWithPromise('securityKeyBioEnrollDelete', id);
  }

  renameEnrollment(id: string, name: string) {
    return sendWithPromise('securityKeyBioEnrollRename', id, name);
  }

  close() {
    return chrome.send('securityKeyBioEnrollClose');
  }

  static getInstance(): SecurityKeysBioEnrollProxy {
    return bioEnrollProxyInstance ||
        (bioEnrollProxyInstance = new SecurityKeysBioEnrollProxyImpl());
  }

  static setInstance(obj: SecurityKeysBioEnrollProxy) {
    bioEnrollProxyInstance = obj;
  }
}

let bioEnrollProxyInstance: SecurityKeysBioEnrollProxy|null = null;
