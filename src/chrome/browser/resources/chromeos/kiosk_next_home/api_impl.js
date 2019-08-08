// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Kiosk Next Home API implementation.
 */

/**
 * Gets the app type from the mojo representation of an app.
 * @param {!chromeos.kioskNextHome.mojom.App} mojoApp
 * @return {!kioskNextHome.AppType}
 */
function getAppType(mojoApp) {
  switch (mojoApp.type) {
    case apps.mojom.AppType.kArc:
      return kioskNextHome.AppType.ARC;
    case apps.mojom.AppType.kBuiltIn:
    case apps.mojom.AppType.kExtension:
    case apps.mojom.AppType.kWeb:
      return kioskNextHome.AppType.CHROME;
    default:
      return kioskNextHome.AppType.UNKNOWN;
  }
}

/**
 * Gets the app readiness from the mojo representation of an app.
 * @param {!chromeos.kioskNextHome.mojom.App} mojoApp
 * @return {!kioskNextHome.AppReadiness}
 */
function getReadiness(mojoApp) {
  switch (mojoApp.readiness) {
    case apps.mojom.Readiness.kReady:
      return kioskNextHome.AppReadiness.READY;
    case apps.mojom.Readiness.kDisabledByPolicy:
      return kioskNextHome.AppReadiness.DISABLED;
    case apps.mojom.Readiness.kUninstalledByUser:
      return kioskNextHome.AppReadiness.UNINSTALLED;
    default:
      return kioskNextHome.AppReadiness.UNKNOWN;
  }
}

/**
 * Builds an app from its mojo representation coming from the AppController.
 * @param {!chromeos.kioskNextHome.mojom.App} mojoApp
 * @return {!kioskNextHome.App} A bridge representation of an app.
 */
function buildApp(mojoApp) {
  return {
    appId: mojoApp.appId,
    type: getAppType(mojoApp),
    displayName: mojoApp.displayName,
    packageName: mojoApp.androidPackageName,
    readiness: getReadiness(mojoApp),
    // We append the intended size of the icon in density-independent
    // pixels, in this case 128x128 dips.
    thumbnailImage: 'chrome://app-icon/' + mojoApp.appId + '/128',
  };
}

/** @implements {kioskNextHome.Bridge} */
class KioskNextHomeBridge {
  constructor() {
    /** @private @const {!Array<!kioskNextHome.Listener>} */
    this.listeners_ = [];
    /** @private @const */
    this.identityAccessorProxy_ = new identity.mojom.IdentityAccessorProxy();
    /** @private @const */
    this.identityControllerProxy_ =
        new chromeos.kioskNextHome.mojom.IdentityControllerProxy();
    /** @private @const */
    this.appControllerProxy_ =
        new chromeos.kioskNextHome.mojom.AppControllerProxy();
    /** @private @const */
    this.appControllerClientCallbackRouter_ =
        new chromeos.kioskNextHome.mojom.AppControllerClientCallbackRouter();

    const kioskNextHomeInterfaceBrokerProxy =
        chromeos.kioskNextHome.mojom.KioskNextHomeInterfaceBroker.getProxy();
    kioskNextHomeInterfaceBrokerProxy.getIdentityAccessor(
        this.identityAccessorProxy_.$.createRequest());
    kioskNextHomeInterfaceBrokerProxy.getIdentityController(
        this.identityControllerProxy_.$.createRequest());
    kioskNextHomeInterfaceBrokerProxy.getAppController(
        this.appControllerProxy_.$.createRequest());

    // Attaching app listeners.
    this.appControllerClientCallbackRouter_.onAppChanged.addListener(
        mojoApp => {
          const bridgeApp = buildApp(mojoApp);
          for (const listener of this.listeners_) {
            listener.onAppChanged(
                /** @type{!kioskNextHome.App} */ (
                    Object.assign({}, bridgeApp)));
          }
        });
    this.appControllerProxy_.setClient(
        this.appControllerClientCallbackRouter_.createProxy());

    // Attaching network status listeners.
    window.addEventListener(
        'online',
        () => this.notifyNetworkStateChange(kioskNextHome.NetworkState.ONLINE));
    window.addEventListener(
        'offline',
        () =>
            this.notifyNetworkStateChange(kioskNextHome.NetworkState.OFFLINE));
  }

  /** @override */
  addListener(listener) {
    this.listeners_.push(listener);
  }

  /** @override */
  getUserGivenName() {
    return this.identityControllerProxy_.getUserInfo().then(
        result => result.userInfo.givenName);
  }

  /** @override */
  getUserDisplayName() {
    return this.identityControllerProxy_.getUserInfo().then(
        result => result.userInfo.displayName);
  }

  /** @override */
  getAccountId() {
    return this.identityAccessorProxy_.getPrimaryAccountWhenAvailable().then(
        account => account.accountInfo.gaia);
  }

  /** @override */
  getAccessToken(scopes) {
    return this.identityAccessorProxy_.getPrimaryAccountWhenAvailable()
        .then(account => {
          return this.identityAccessorProxy_.getAccessToken(
              account.accountInfo.accountId, {'scopes': scopes},
              'kiosk_next_home');
        })
        .then(tokenInfo => {
          if (tokenInfo.token) {
            return tokenInfo.token;
          }
          throw 'Unable to get access token.';
        });
  }

  /** @override */
  getAndroidId() {
    return this.appControllerProxy_.getArcAndroidId().then(response => {
      if (response.success) {
        return response.androidId;
      }
      throw 'Unable to get Android id.';
    });
  }

  /** @override */
  getApps() {
    return this.appControllerProxy_.getApps().then(response => {
      return response.apps.map(buildApp);
    });
  }

  /** @override */
  launchApp(appId) {
    this.appControllerProxy_.launchApp(appId);
  }

  /** @override */
  launchIntent(intent) {
    return this.appControllerProxy_.launchIntent(intent).then(result => {
      if (!result.launched) {
        throw result.errorMessage;
      }
    });
  }

  /** @override */
  uninstallApp(appId) {
    this.appControllerProxy_.uninstallApp(appId);
  }

  /** @override */
  getNetworkState() {
    return navigator.onLine ? kioskNextHome.NetworkState.ONLINE :
                              kioskNextHome.NetworkState.OFFLINE;
  }

  /**
   * Notifies listeners about changes in network connection state.
   * @param {kioskNextHome.NetworkState} networkState Indicates current network
   *     state.
   */
  notifyNetworkStateChange(networkState) {
    for (const listener of this.listeners_) {
      listener.onNetworkStateChanged(networkState);
    }
  }
}

/**
 * Provides bridge implementation.
 * @return {!kioskNextHome.Bridge} Bridge instance that can be used to interact
 *     with Chrome OS.
 */
kioskNextHome.getChromeOsBridge = function() {
  return new KioskNextHomeBridge();
};
