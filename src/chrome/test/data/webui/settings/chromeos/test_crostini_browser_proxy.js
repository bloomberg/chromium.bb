// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {TestBrowserProxy} from '../../test_browser_proxy.js';
// clang-format on

/** @implements {settings.CrostiniBrowserProxy} */
/* #export */ class TestCrostiniBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'requestCrostiniInstallerView',
      'requestRemoveCrostini',
      'exportCrostiniContainer',
      'importCrostiniContainer',
      'requestCrostiniContainerUpgradeView',
      'requestCrostiniUpgraderDialogStatus',
      'requestCrostiniContainerUpgradeAvailable',
      'addCrostiniPortForward',
      'getCrostiniDiskInfo',
      'resizeCrostiniDisk',
      'addCrostiniPortForward',
      'removeCrostiniPortForward',
      'removeAllCrostiniPortForwards',
      'activateCrostiniPortForward',
      'deactivateCrostiniPortForward',
      'getCrostiniActivePorts',
      'checkCrostiniIsRunning',
      'shutdownCrostini',
      'setCrostiniMicSharingEnabled',
      'getCrostiniMicSharingEnabled',
      'requestCrostiniInstallerStatus',
      'requestArcAdbSideloadStatus',
      'getCanChangeArcAdbSideloading',
      'createContainer',
      'deleteContainer',
      'requestContainerInfo',
      'setContainerBadgeColor',
      'stopContainer',
    ]);
    this.crostiniMicSharingEnabled = false;
    this.crostiniIsRunning = true;
    this.methodCalls_ = {};
    this.portOperationSuccess = true;
    this.containerInfo = [];
  }

  getNewPromiseFor(name) {
    if (name in this.methodCalls_) {
      return new Promise((resolve, reject) => {
        this.methodCalls_[name].push({name, resolve, reject});
      });
    } else {
      return new Promise((resolve, reject) => {
        this.methodCalls_[name] = [{name, resolve, reject}];
      });
    }
  }

  async resolvePromises(name, ...args) {
    for (const o of this.methodCalls_[name]) {
      await o.resolve(...args);
    }
    this.methodCalls_[name] = [];
  }

  async rejectPromises(name, ...args) {
    for (const o of this.methodCalls_[name]) {
      await o.reject(...args);
    }
    this.methodCalls_[name] = [];
  }

  /** @override */
  requestCrostiniInstallerView() {
    this.methodCalled('requestCrostiniInstallerView');
  }

  /** override */
  requestRemoveCrostini() {
    this.methodCalled('requestRemoveCrostini');
  }

  /**override */
  requestArcAdbSideloadStatus() {
    this.methodCalled('requestArcAdbSideloadStatus');
  }

  /** override */
  getCanChangeArcAdbSideloading() {
    this.methodCalled('getCanChangeArcAdbSideloading');
  }

  /** @override */
  requestCrostiniInstallerStatus() {
    this.methodCalled('requestCrostiniInstallerStatus');
    cr.webUIListenerCallback('crostini-installer-status-changed', false);
  }

  /** @override */
  requestCrostiniExportImportOperationStatus() {
    cr.webUIListenerCallback(
        'crostini-export-import-operation-status-changed', false);
  }

  /** override */
  exportCrostiniContainer() {
    this.methodCalled('exportCrostiniContainer');
  }

  /** override */
  importCrostiniContainer() {
    this.methodCalled('importCrostiniContainer');
  }

  /** @override */
  requestCrostiniContainerUpgradeView() {
    this.methodCalled('requestCrostiniContainerUpgradeView');
  }

  /** @override */
  requestCrostiniUpgraderDialogStatus() {
    cr.webUIListenerCallback('crostini-upgrader-status-changed', false);
  }

  /** @override */
  requestCrostiniContainerUpgradeAvailable() {
    cr.webUIListenerCallback(
        'crostini-container-upgrade-available-changed', true);
  }

  /** @override */
  addCrostiniPortForward(
      vmName, containerName, portNumber, protocolIndex, label) {
    this.methodCalled(
        'addCrostiniPortForward', vmName, containerName, portNumber,
        protocolIndex, label);
    return Promise.resolve(this.portOperationSuccess);
  }

  /** @override */
  removeCrostiniPortForward(vmName, containerName, portNumber, protocolIndex) {
    this.methodCalled(
        'removeCrostiniPortForward', vmName, containerName, portNumber,
        protocolIndex);
    return Promise.resolve(this.portOperationSuccess);
  }

  /** @override */
  activateCrostiniPortForward(
      vmName, containerName, portNumber, protocolIndex) {
    this.methodCalled(
        'activateCrostiniPortForward', vmName, containerName, portNumber,
        protocolIndex);
    return Promise.resolve(this.portOperationSuccess);
  }

  /** @override */
  removeAllCrostiniPortForwards(vmName, containerName) {
    this.methodCalled('removeAllCrostiniPortForwards');
  }

  /** @override */
  getCrostiniDiskInfo(vmName, requestFullInfo) {
    this.methodCalled('getCrostiniDiskInfo', vmName, requestFullInfo);
    return this.getNewPromiseFor('getCrostiniDiskInfo');
  }

  /** @override */
  resizeCrostiniDisk(vmName, newSizeBytes) {
    this.methodCalled('resizeCrostiniDisk', vmName, newSizeBytes);
    return this.getNewPromiseFor('resizeCrostiniDisk');
  }

  /** @override */
  deactivateCrostiniPortForward(
      vmName, containerName, portNumber, protocolIndex) {
    this.methodCalled(
        'deactivateCrostiniPortForward', vmName, containerName, portNumber,
        protocolIndex);
    return Promise.resolve(this.portOperationSuccess);
  }

  /** @override */
  getCrostiniActivePorts() {
    this.methodCalled('getCrostiniActivePorts');
    return Promise.resolve(new Array());
  }

  /** @override */
  checkCrostiniIsRunning() {
    this.methodCalled('checkCrostiniIsRunning');
    return Promise.resolve(this.crostiniIsRunning);
  }

  /** @override */
  shutdownCrostini() {
    this.methodCalled('shutdownCrostini');
    this.crostiniIsRunning = false;
  }

  /** @override */
  setCrostiniMicSharingEnabled(enabled) {
    this.methodCalled('setCrostiniMicSharingEnabled');
    this.crostiniMicSharingEnabled = enabled;
  }

  /** @override */
  getCrostiniMicSharingEnabled() {
    this.methodCalled('getCrostiniMicSharingEnabled');
    return Promise.resolve(this.CrostiniMicSharingEnabled);
  }

  /** @override */
  createContainer(containerId, imageServer, imageAlias) {
    this.methodCalled('createContainer');
  }

  /** @override */
  deleteContainer(containerId) {
    this.methodCalled('deleteContainer');
  }

  /** @override */
  requestContainerInfo() {
    this.methodCalled('requestContainerInfo');
    cr.webUIListenerCallback('crostini-container-info', this.containerInfo);
  }

  /** @override */
  setContainerBadgeColor(containerId, badge_color) {
    this.methodCalled('setContainerBadgeColor');
  }

  /** @override */
  stopContainer(containerId) {
    this.methodCalled('stopContainer');
  }
}
