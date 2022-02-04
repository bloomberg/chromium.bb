// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for bluetooth_internals.html, served from
 *     chrome://bluetooth-internals/.
 */

import {assert} from 'chrome://resources/js/assert.m.js';
import {$} from 'chrome://resources/js/util.m.js';

import {DiscoverySessionRemote} from './adapter.mojom-webui.js';
import {AdapterBroker, AdapterProperty, getAdapterBroker} from './adapter_broker.js';
import {AdapterPage} from './adapter_page.js';
import {BluetoothInternalsHandler, BluetoothInternalsHandlerRemote} from './bluetooth_internals.mojom-webui.js';
import {DebugLogPage} from './debug_log_page.js';
import {DeviceInfo} from './device.mojom-webui.js';
import {DeviceCollection} from './device_collection.js';
import {DeviceDetailsPage} from './device_details_page.js';
import {DevicesPage, ScanStatus} from './devices_page.js';
import {PageManager, PageManagerObserver} from './page_manager.js';
import {Sidebar} from './sidebar.js';
import {Snackbar, SnackbarType} from './snackbar.js';


// Expose for testing.
/** @type {AdapterBroker} */
export let adapterBroker = null;

/** @type {DeviceCollection} */
export let devices = null;

/** @type {Sidebar} */
export let sidebarObj = null;

/** @type {PageManager} */
export const pageManager = PageManager.getInstance();

devices = new DeviceCollection([]);

/** @type {AdapterPage} */
let adapterPage = null;
/** @type {DevicesPage} */
let devicesPage = null;
/** @type {DebugLogPage} */
let debugLogPage = null;

/** @type {DiscoverySessionRemote} */
let discoverySession = null;

/** @type {boolean} */
let userRequestedScanStop = false;

/** @type {!BluetoothInternalsHandlerRemote} */
const bluetoothInternalsHandler = BluetoothInternalsHandler.getRemote();

/**
 * Observer for page changes. Used to update page title header.
 */
const PageObserver = class extends PageManagerObserver {
  updateHistory(path) {
    window.location.hash = '#' + path;
  }

  /**
   * Sets the page title. Called by PageManager.
   * @override
   * @param {string} title
   */
  updateTitle(title) {
    document.querySelector('.page-title').textContent = title;
  }
};

/**
 * Removes DeviceDetailsPage with matching device |address|. The associated
 * sidebar item is also removed.
 * @param {string} address
 */
function removeDeviceDetailsPage(address) {
  const id = 'devices/' + address.toLowerCase();
  sidebarObj.removeItem(id);

  const deviceDetailsPage =
      /** @type {!DeviceDetailsPage} */ (pageManager.registeredPages.get(id));
  assert(deviceDetailsPage, 'Device Details page must exist');

  deviceDetailsPage.disconnect();
  deviceDetailsPage.pageDiv.parentNode.removeChild(deviceDetailsPage.pageDiv);

  // Inform the devices page that the user is inspecting this device.
  // This will update the links in the device table.
  devicesPage.setInspecting(
      deviceDetailsPage.deviceInfo, false /* isInspecting */);

  pageManager.unregister(deviceDetailsPage);
}

/**
 * Creates a DeviceDetailsPage with the given |deviceInfo|, appends it to
 * '#page-container', and adds a sidebar item to show the new page. If a
 * page exists that matches |deviceInfo.address|, nothing is created and the
 * existing page is returned.
 * @param {!DeviceInfo} deviceInfo
 * @return {!DeviceDetailsPage}
 */
function makeDeviceDetailsPage(deviceInfo) {
  const deviceDetailsPageId = 'devices/' + deviceInfo.address.toLowerCase();
  let deviceDetailsPage =
      /** @type {?DeviceDetailsPage} */ (
          pageManager.registeredPages.get(deviceDetailsPageId));
  if (deviceDetailsPage) {
    return deviceDetailsPage;
  }

  const pageSection = document.createElement('section');
  pageSection.hidden = true;
  pageSection.id = deviceDetailsPageId;
  $('page-container').appendChild(pageSection);

  deviceDetailsPage = new DeviceDetailsPage(deviceDetailsPageId, deviceInfo);

  deviceDetailsPage.pageDiv.addEventListener('infochanged', function(event) {
    devices.addOrUpdate(event.detail.info);
  });

  deviceDetailsPage.pageDiv.addEventListener('forgetpressed', function(event) {
    pageManager.showPageByName(devicesPage.name);
    removeDeviceDetailsPage(event.detail.address);
  });

  // Inform the devices page that the user is inspecting this device.
  // This will update the links in the device table.
  devicesPage.setInspecting(deviceInfo, true /* isInspecting */);
  pageManager.register(deviceDetailsPage);

  sidebarObj.addItem({
    pageName: deviceDetailsPageId,
    text: deviceInfo.nameForDisplay,
  });

  deviceDetailsPage.connect();
  return deviceDetailsPage;
}

/**
 * Updates the DeviceDetailsPage with the matching device |address| and
 * redraws it.
 * @param {string} address
 */
function updateDeviceDetailsPage(address) {
  const detailPageId = 'devices/' + address.toLowerCase();
  const page = pageManager.registeredPages.get(detailPageId);
  if (page) {
    /** @type {!DeviceDetailsPage} */ (page).redraw();
  }
}

function updateStoppedDiscoverySession() {
  devicesPage.setScanStatus(ScanStatus.OFF);
  discoverySession = null;
}

function setupAdapterSystem(response) {
  adapterBroker.addEventListener('adapterchanged', function(event) {
    adapterPage.adapterFieldSet.value[event.detail.property] =
        event.detail.value;
    adapterPage.redraw();

    if (event.detail.property == AdapterProperty.DISCOVERING &&
        !event.detail.value && !userRequestedScanStop && discoverySession) {
      updateStoppedDiscoverySession();
      Snackbar.show(
          'Discovery session ended unexpectedly', SnackbarType.WARNING);
    }
  });

  adapterPage.setAdapterInfo(response.info);

  adapterPage.pageDiv.addEventListener('refreshpressed', function() {
    adapterBroker.getInfo().then(function(response) {
      if (response && response.info) {
        adapterPage.setAdapterInfo(response.info);
      } else {
        console.error('Failed to fetch adapter info.');
      }
    });
  });
}

function setupDeviceSystem(response) {
  // Hook up device collection events.
  adapterBroker.addEventListener('deviceadded', function(event) {
    devices.addOrUpdate(event.detail.deviceInfo);
    updateDeviceDetailsPage(event.detail.deviceInfo.address);
  });
  adapterBroker.addEventListener('devicechanged', function(event) {
    devices.addOrUpdate(event.detail.deviceInfo);
    updateDeviceDetailsPage(event.detail.deviceInfo.address);
  });
  adapterBroker.addEventListener('deviceremoved', function(event) {
    devices.remove(event.detail.deviceInfo);
    updateDeviceDetailsPage(event.detail.deviceInfo.address);
  });

  response.devices.forEach(devices.addOrUpdate, devices /* this */);

  devicesPage.setDevices(devices);

  devicesPage.pageDiv.addEventListener('inspectpressed', function(event) {
    const detailsPage =
        makeDeviceDetailsPage(devices.getByAddress(event.detail.address));
    pageManager.showPageByName(detailsPage.name);
  });

  devicesPage.pageDiv.addEventListener('forgetpressed', function(event) {
    pageManager.showPageByName(devicesPage.name);
    removeDeviceDetailsPage(event.detail.address);
  });

  devicesPage.pageDiv.addEventListener('scanpressed', function(event) {
    if (discoverySession) {
      userRequestedScanStop = true;
      devicesPage.setScanStatus(ScanStatus.STOPPING);

      discoverySession.stop().then(function(response) {
        if (response.success) {
          updateStoppedDiscoverySession();
          userRequestedScanStop = false;
          return;
        }

        devicesPage.setScanStatus(ScanStatus.ON);
        Snackbar.show('Failed to stop discovery session', SnackbarType.ERROR);
        userRequestedScanStop = false;
      });

      return;
    }

    devicesPage.setScanStatus(ScanStatus.STARTING);
    adapterBroker.startDiscoverySession()
        .then(function(session) {
          discoverySession = assert(session);

          discoverySession.onConnectionError.addListener(() => {
            updateStoppedDiscoverySession();
            Snackbar.show('Discovery session ended', SnackbarType.WARNING);
          });

          devicesPage.setScanStatus(ScanStatus.ON);
        })
        .catch(function(error) {
          devicesPage.setScanStatus(ScanStatus.OFF);
          Snackbar.show(
              'Failed to start discovery session', SnackbarType.ERROR);
          console.error(error);
        });
  });
}

function setupPages() {
  sidebarObj = new Sidebar(/** @type {!HTMLElement} */ ($('sidebar')));
  $('menu-btn').addEventListener('click', function() {
    sidebarObj.open();
  });
  pageManager.addObserver(sidebarObj);
  pageManager.addObserver(new PageObserver());

  devicesPage = new DevicesPage();
  pageManager.register(devicesPage);
  adapterPage = new AdapterPage();
  pageManager.register(adapterPage);
  debugLogPage = new DebugLogPage(bluetoothInternalsHandler);
  pageManager.register(debugLogPage);

  // Set up hash-based navigation.
  window.addEventListener('hashchange', function() {
    // If a user navigates and the page doesn't exist, do nothing.
    const pageName = window.location.hash.substr(1);
    if ($(pageName)) {
      pageManager.showPageByName(pageName);
    }
  });

  if (!window.location.hash) {
    pageManager.showPageByName(adapterPage.name);
    return;
  }

  // Only the root pages are available on page load.
  pageManager.showPageByName(window.location.hash.split('/')[0].substr(1));
}

export function initializeViews() {
  setupPages();
  return getAdapterBroker()
      .then(function(broker) {
        adapterBroker = broker;
      })
      .then(function() {
        return adapterBroker.getInfo();
      })
      .then(setupAdapterSystem)
      .then(function() {
        return adapterBroker.getDevices();
      })
      .then(setupDeviceSystem)
      .catch(function(error) {
        Snackbar.show(error.message, SnackbarType.ERROR);
        console.error(error);
      });
}
