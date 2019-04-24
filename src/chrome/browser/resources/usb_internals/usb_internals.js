// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for usb_internals.html, served from chrome://usb-internals/.
 */
cr.define('usb_internals', function() {
  class UsbInternals {
    constructor() {
      const pageHandler = mojom.UsbInternalsPageHandler.getProxy();

      // Connection to the UsbInternalsPageHandler instance running in the
      // browser process.
      /** @private {device.mojom.UsbDeviceManagerProxy} */
      this.usbManager_ = new device.mojom.UsbDeviceManagerProxy;
      pageHandler.bindUsbDeviceManagerInterface(
          this.usbManager_.$.createRequest());

      /** @private {device.mojom.UsbDeviceManagerTestProxy} */
      this.usbManagerTest_ = new device.mojom.UsbDeviceManagerTestProxy;
      pageHandler.bindTestInterface(this.usbManagerTest_.$.createRequest());

      cr.ui.decorate('tabbox', cr.ui.TabBox);

      this.refreshDeviceList();

      $('add-test-device-form').addEventListener('submit', (event) => {
        this.addTestDevice(event);
      });
      this.refreshTestDeviceList();
    }

    async refreshDeviceList() {
      const response = await this.usbManager_.getDevices();
      devices_page.setDevices(response.results);
    }

    async refreshTestDeviceList() {
      const response = await this.usbManagerTest_.getTestDevices();

      const tableBody = $('test-device-list');
      tableBody.innerHTML = '';

      const rowTemplate = document.querySelector('#test-device-row');
      const td = rowTemplate.content.querySelectorAll('td');

      for (const device of response.devices) {
        td[0].textContent = device.name;
        td[1].textContent = device.serialNumber;
        td[2].textContent = device.landingPage.url;

        const clone = document.importNode(rowTemplate.content, true);

        const removeButton = clone.querySelector('button');
        removeButton.addEventListener('click', async () => {
          await this.usbManagerTest_.removeDeviceForTesting(device.guid);
          this.refreshTestDeviceList();
        });

        tableBody.appendChild(clone);
      }
    }

    async addTestDevice(event) {
      event.preventDefault();

      const response = await this.usbManagerTest_.addDeviceForTesting(
          $('test-device-name').value, $('test-device-serial').value,
          $('test-device-landing-page').value);
      if (response.success) {
        this.refreshTestDeviceList();
      }

      $('add-test-device-result').textContent = response.message;
      $('add-test-device-result').className =
          response.success ? 'action-success' : 'action-failure';
    }
  }

  return {
    UsbInternals,
  };
});

document.addEventListener('DOMContentLoaded', () => {
  new usb_internals.UsbInternals();
});
