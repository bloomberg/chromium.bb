/*
 * geolocation-mock contains a mock implementation of Geolocation and
 * PermissionService.
 */

"use strict";

class GeolocationMock {
  constructor() {
    this.geolocationInterceptor_ = new MojoInterfaceInterceptor(
        device.mojom.Geolocation.name);
    this.geolocationInterceptor_.oninterfacerequest =
        e => this.connectGeolocation_(e.handle);
    this.geolocationInterceptor_.start();

    this.permissionInterceptor_ = new MojoInterfaceInterceptor(
        blink.mojom.PermissionService.name);
    this.permissionInterceptor_.oninterfacerequest =
        e => this.connectPermission_(e.handle);
    this.permissionInterceptor_.start();

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
     * @type {!blink.mojom.PermissionStatus}
     */
    this.permissionStatus_ = blink.mojom.PermissionStatus.ASK;
    this.rejectPermissionConnections_ = false;
    this.rejectGeolocationConnections_ = false;

    this.geolocationBindingSet_ = new mojo.BindingSet(
        device.mojom.Geolocation);
    this.permissionBindingSet_ = new mojo.BindingSet(
        blink.mojom.PermissionService);
  }

  connectGeolocation_(handle) {
    if (this.rejectGeolocationConnections_) {
      handle.close();
      return;
    }
    this.geolocationBindingSet_.addBinding(this, handle);
  }

  connectPermission_(handle) {
    if (this.rejectPermissionConnections_) {
      handle.close();
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
    this.geoposition_ = new device.mojom.Geoposition();
    this.geoposition_.latitude = latitude;
    this.geoposition_.longitude = longitude;
    this.geoposition_.accuracy = accuracy;
    this.geoposition_.altitude = altitude;
    this.geoposition_.altitudeAccuracy = altitudeAccuracy;
    this.geoposition_.heading = heading;
    this.geoposition_.speed = speed;
    this.geoposition_.timestamp = new Date().getTime() / 1000;
    this.geoposition_.errorMessage = '';
    this.geoposition_.valid = true;
  }

  /**
   * Sets the error message to return to the next queryNextPosition() call. If
   * any queryNextPosition() requests are outstanding, they will all receive
   * the error set by this call.
   */
  setGeolocationPositionUnavailableError(message) {
    this.geoposition_ = new device.mojom.Geoposition();
    this.geoposition_.valid = false;
    this.geoposition_.errorMessage = message;
    this.geoposition_.errorCode =
        device.mojom.Geoposition.ErrorCode.POSITION_UNAVAILABLE;
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
    if (permissionDescriptor.name != blink.mojom.PermissionName.GEOLOCATION)
      return Promise.resolve(blink.mojom.PermissionStatus.DENIED);

    return new Promise(resolve => {
      if (this.pendingPermissionRequest_)
        this.pendingPermissionRequest_(blink.mojom.PermissionStatus.ASK);
      this.pendingPermissionRequest_ = resolve;
      this.runPermissionCallback_();
    });
  }

  runPermissionCallback_() {
    if (this.permissionStatus_ == blink.mojom.PermissionStatus.ASK ||
        !this.pendingPermissionRequest_)
      return;

    this.pendingPermissionRequest_({status: this.permissionStatus_});
    this.permissionStatus_ = blink.mojom.PermissionStatus.ASK;
    this.pendingPermissionRequest_ = null;
  }

  /**
   * Sets whether the next geolocation permission request should be allowed.
   */
  setGeolocationPermission(allowed) {
    this.permissionStatus_ = allowed ?
        blink.mojom.PermissionStatus.GRANTED :
        blink.mojom.PermissionStatus.DENIED;
    this.runPermissionCallback_();
  }
}

let geolocationMock = new GeolocationMock();
