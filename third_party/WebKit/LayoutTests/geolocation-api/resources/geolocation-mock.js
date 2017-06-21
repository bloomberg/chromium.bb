/*
 * geolocation-mock contains a mock implementation of GeolocationService and
 * PermissionService.
 */

"use strict";

let geolocationServiceMock = loadMojoModules(
    'geolocationServiceMock',
    ['device/geolocation/public/interfaces/geolocation.mojom',
     'device/geolocation/public/interfaces/geoposition.mojom',
     'third_party/WebKit/public/platform/modules/permissions/permission.mojom',
     'third_party/WebKit/public/platform/modules/permissions/permission_status.mojom',
     'mojo/public/js/bindings',
    ]).then(mojo => {
  let [geolocation, geoposition, permission, permissionStatus, bindings] =
      mojo.modules;

  class GeolocationServiceMock {
    constructor(interfaceProvider) {
      interfaceProvider.addInterfaceOverrideForTesting(
          geolocation.GeolocationService.name,
          handle => this.connectGeolocation_(handle));

      interfaceProvider.addInterfaceOverrideForTesting(
          permission.PermissionService.name,
          handle => this.connectPermission_(handle));

      this.interfaceProvider_ = interfaceProvider;

      /**
       * The next geoposition to return in response to a queryNextPosition()
       * call.
      */
      this.geoposition_ = null;

      /**
       * A pending request for permission awaiting a decision to be set via a
       * setGeolocationPermission call.
       *
       * @type {?Function}
       */
      this.pendingPermissionRequest_ = null;

      /**
       * The status to respond to permission requests with. If set to ASK, then
       * permission requests will block until setGeolocationPermission is called
       * to allow or deny permission requests.
       *
       * @type {!permissionStatus.PermissionStatus}
       */
      this.permissionStatus_ = permissionStatus.PermissionStatus.ASK;
      this.rejectPermissionConnections_ = false;
      this.rejectGeolocationConnections_ = false;

      this.geolocationBindingSet_ = new bindings.BindingSet(
          geolocation.GeolocationService);
      this.permissionBindingSet_ = new bindings.BindingSet(
          permission.PermissionService);
    }

    connectGeolocation_(handle) {
      if (this.rejectGeolocationConnections_) {
        mojo.core.close(handle);
        return;
      }
      this.geolocationBindingSet_.addBinding(this, handle);
    }

    connectPermission_(handle) {
      if (this.rejectPermissionConnections_) {
        mojo.core.close(handle);
        return;
      }
      this.permissionBindingSet_.addBinding(this, handle);
    }

    setHighAccuracy(highAccuracy) {
      // FIXME: We need to add some tests regarding "high accuracy" mode.
      // See https://bugs.webkit.org/show_bug.cgi?id=49438
    }

    /**
     * A mock implementation of GeolocationService.queryNextPosition(). This
     * returns the position set by a call to setGeolocationPosition() or
     * setGeolocationPositionUnavailableError().
     */
    queryNextPosition() {
      if (!this.geoposition_) {
        this.setGeolocationPositionUnavailableError(
            'Test error: position not set before call to queryNextPosition()');
      }
      let geoposition = this.geoposition_;
      this.geoposition_ = null;
      return Promise.resolve({geoposition});
    }

    /**
     * Sets the position to return to the next queryNextPosition() call. If any
     * queryNextPosition() requests are outstanding, they will all receive the
     * position set by this call.
     */
    setGeolocationPosition(latitude, longitude, accuracy, altitude,
                           altitudeAccuracy, heading, speed) {
      this.geoposition_ = new geoposition.Geoposition();
      this.geoposition_.latitude = latitude;
      this.geoposition_.longitude = longitude;
      this.geoposition_.accuracy = accuracy;
      this.geoposition_.altitude = altitude;
      this.geoposition_.altitude_accuracy = altitudeAccuracy;
      this.geoposition_.heading = heading;
      this.geoposition_.speed = speed;
      this.geoposition_.timestamp = new Date().getTime() / 1000;
      this.geoposition_.error_message = '';
      this.geoposition_.valid = true;
    }

    /**
     * Sets the error message to return to the next queryNextPosition() call. If
     * any queryNextPosition() requests are outstanding, they will all receive
     * the error set by this call.
     */
    setGeolocationPositionUnavailableError(message) {
      this.geoposition_ = new geoposition.Geoposition();
      this.geoposition_.valid = false;
      this.geoposition_.error_message = message;
      this.geoposition_.error_code =
          geoposition.Geoposition.ErrorCode.POSITION_UNAVAILABLE;
    }

    /**
     * Reject any connection requests for the permission service. This will
     * trigger a connection error in the client.
     */
    rejectPermissionConnections() {
      this.rejectPermissionConnections_ = true;
    }

    /**
     * Reject any connection requests for the geolocation service. This will
     * trigger a connection error in the client.
     */
    rejectGeolocationConnections() {
      this.rejectGeolocationConnections_ = true;
    }

    /**
     * A mock implementation of PermissionService.requestPermission(). This
     * returns the result set by a call to setGeolocationPermission(), waiting
     * for a call if necessary. Any permission request that is not for
     * geolocation is always denied.
     */
    requestPermission(permissionDescriptor) {
      if (permissionDescriptor.name != permission.PermissionName.GEOLOCATION)
        return Promise.resolve(permissionStatus.PermissionStatus.DENIED);

      return new Promise(resolve => {
        if (this.pendingPermissionRequest_)
          this.pendingPermissionRequest_(permissionStatus.PermissionStatus.ASK);
        this.pendingPermissionRequest_ = resolve;
        this.runPermissionCallback_();
      });
    }

    runPermissionCallback_() {
      if (this.permissionStatus_ == permissionStatus.PermissionStatus.ASK ||
          !this.pendingPermissionRequest_)
        return;

      this.pendingPermissionRequest_({status: this.permissionStatus_});
      this.permissionStatus_ = permissionStatus.PermissionStatus.ASK ;
      this.pendingPermissionRequest_ = null;
    }

    /**
     * Sets whether the next geolocation permission request should be allowed.
     */
    setGeolocationPermission(allowed) {
      this.permissionStatus_ = allowed ?
          permissionStatus.PermissionStatus.GRANTED :
          permissionStatus.PermissionStatus.DENIED;
      this.runPermissionCallback_();
    }

  }
  return new GeolocationServiceMock(mojo.frameInterfaces);
});
