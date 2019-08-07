// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DevicesPage, served from
 *     chrome://usb-internals/.
 */

cr.define('devices_page', function() {
  const UsbDeviceProxy = device.mojom.UsbDeviceProxy;

  /**
   * Page that contains a tab header and a tab panel displaying devices table.
   */
  class DevicesPage {
    /**
     * @param {!device.mojom.UsbDeviceManagerProxy} usbManager
     */
    constructor(usbManager) {
      /** @private {device.mojom.UsbDeviceManagerProxy} */
      this.usbManager_ = usbManager;

      this.renderDeviceList_();
    }

    /**
     * Sets the device collection for the page's device table.
     * @private
     */
    async renderDeviceList_() {
      const response = await this.usbManager_.getDevices();

      /** @type {!Array<!device.mojom.UsbDeviceInfo>} */
      const devices = response.results;

      const tableBody = $('device-list');
      tableBody.innerHTML = '';

      const rowTemplate = document.querySelector('#device-row');

      for (const device of devices) {
        const clone = document.importNode(rowTemplate.content, true);

        const td = clone.querySelectorAll('td');

        td[0].textContent = device.busNumber;
        td[1].textContent = device.portNumber;
        td[2].textContent = toHex(device.vendorId);
        td[3].textContent = toHex(device.productId);
        if (device.manufacturerName) {
          td[4].textContent = decodeString16(device.manufacturerName.data);
        }
        if (device.productName) {
          td[5].textContent = decodeString16(device.productName.data);
        }
        if (device.serialNumber) {
          td[6].textContent = decodeString16(device.serialNumber.data);
        }

        const inspectButton = clone.querySelector('button');
        inspectButton.addEventListener('click', () => {
          this.switchToTab_(device);
        });

        tableBody.appendChild(clone);
      }
    }

    /**
     * Switches to the device's tab, creating one if necessary.
     * @param {!device.mojom.UsbDeviceInfo} device
     * @private
     */
    switchToTab_(device) {
      const tabId = device.guid;

      if (null == $(tabId)) {
        const devicePage = new DevicePage(this.usbManager_, device);
      }
      $(tabId).selected = true;
    }
  }

  /**
   * Page that contains a tree view displaying devices detail and can get
   * descriptors.
   */
  class DevicePage {
    /**
     * @param {!device.mojom.UsbDeviceManagerProxy} usbManager
     * @param {!device.mojom.UsbDeviceInfo} device
     */
    constructor(usbManager, device) {
      /** @private {device.mojom.UsbDeviceManagerProxy} */
      this.usbManager_ = usbManager;

      this.renderTab(device);
    }

    /**
     * Renders a tab to display a tree view showing device's detail information.
     * @param {!device.mojom.UsbDeviceInfo} device
     * @private
     */
    renderTab(device) {
      const tabs = document.querySelector('tabs');

      const tabTemplate = document.querySelector('#tab-template');
      const tabClone = document.importNode(tabTemplate.content, true);

      const tab = tabClone.querySelector('tab');
      if (device.productName) {
        tab.textContent = decodeString16(device.productName.data);
      } else {
        const vendorId = toHex(device.vendorId).slice(2);
        const productId = toHex(device.productId).slice(2);
        tab.textContent = `${vendorId}:${productId}`;
      }
      tab.id = device.guid;

      tabs.appendChild(tabClone);
      cr.ui.decorate('tab', cr.ui.Tab);

      const tabPanels = document.querySelector('tabpanels');

      const tabPanelTemplate = document.querySelector('#tabpanel-template');
      const tabPanelClone = document.importNode(tabPanelTemplate.content, true);

      /**
       * Root of the WebContents tree of current device.
       * @type {cr.ui.Tree|null}
       */
      const treeViewRoot = tabPanelClone.querySelector('#tree-view');
      cr.ui.decorate(treeViewRoot, cr.ui.Tree);
      treeViewRoot.detail = {payload: {}, children: {}};
      // Clear the tree first before populating it with the new content.
      treeViewRoot.innerText = '';
      this.renderDeviceTree_(device, treeViewRoot);

      const usbDeviceProxy = new UsbDeviceProxy;
      this.usbManager_.getDevice(device.guid, usbDeviceProxy.$.createRequest());

      const getStringDescriptorButton =
          tabPanelClone.querySelector('#string-descriptor-button');
      const stringDescriptorElement =
          tabPanelClone.querySelector('.string-descriptor-panel');
      const stringDescriptorPanel = new descriptor_panel.DescriptorPanel(
          usbDeviceProxy, stringDescriptorElement);
      stringDescriptorPanel.initialStringDescriptorPanel(tab.id);
      getStringDescriptorButton.addEventListener('click', () => {
        stringDescriptorElement.hidden = !stringDescriptorElement.hidden;

        // Clear the panel before rendering new data.
        stringDescriptorPanel.clearView();

        if (!stringDescriptorElement.hidden) {
          stringDescriptorPanel.getAllLanguageCodes();
        }
      });

      const getDeviceDescriptorButton =
          tabPanelClone.querySelector('#device-descriptor-button');
      const deviceDescriptorElement =
          tabPanelClone.querySelector('.device-descriptor-panel');
      const deviceDescriptorPanel = new descriptor_panel.DescriptorPanel(
          usbDeviceProxy, deviceDescriptorElement, stringDescriptorPanel);
      getDeviceDescriptorButton.addEventListener('click', () => {
        deviceDescriptorElement.hidden = !deviceDescriptorElement.hidden;

        // Clear the panel before rendering new data.
        deviceDescriptorPanel.clearView();

        if (!deviceDescriptorElement.hidden) {
          deviceDescriptorPanel.renderDeviceDescriptor();
        }
      });

      const getConfigurationDescriptorButton =
          tabPanelClone.querySelector('#configuration-descriptor-button');
      const configurationDescriptorElement =
          tabPanelClone.querySelector('.configuration-descriptor-panel');
      const configurationDescriptorPanel = new descriptor_panel.DescriptorPanel(
          usbDeviceProxy, configurationDescriptorElement,
          stringDescriptorPanel);
      getConfigurationDescriptorButton.addEventListener('click', () => {
        configurationDescriptorElement.hidden =
            !configurationDescriptorElement.hidden;

        // Clear the panel before rendering new data.
        configurationDescriptorPanel.clearView();

        if (!configurationDescriptorElement.hidden) {
          configurationDescriptorPanel.renderConfigurationDescriptor();
        }
      });

      tabPanels.appendChild(tabPanelClone);
      cr.ui.decorate('tabpanel', cr.ui.TabPanel);
    }

    /**
     * Renders a tree to display the device's detail information.
     * @param {!device.mojom.UsbDeviceInfo} device
     * @param {!cr.ui.Tree} root
     * @private
     */
    renderDeviceTree_(device, root) {
      root.add(customTreeItem(`USB Version: ${device.usbVersionMajor}.${
          device.usbVersionMinor}.${device.usbVersionSubminor}`));

      root.add(customTreeItem(`Class Code: ${device.classCode}`));

      root.add(customTreeItem(`Subclass Code: ${device.subclassCode}`));

      root.add(customTreeItem(`Protocol Code: ${device.protocolCode}`));

      root.add(customTreeItem(`Port Number: ${device.portNumber}`));

      root.add(customTreeItem(`Vendor Id: ${toHex(device.vendorId)}`));

      root.add(customTreeItem(`Product Id: ${toHex(device.productId)}`));

      root.add(customTreeItem(`Device Version: ${device.deviceVersionMajor}.${
          device.deviceVersionMinor}.${device.deviceVersionSubminor}`));

      root.add(customTreeItem(`Manufacturer Name: ${
          decodeString16(device.manufacturerName.data)}`));

      root.add(customTreeItem(
          `Product Name: ${decodeString16(device.productName.data)}`));

      root.add(customTreeItem(
          `Serial Number: ${decodeString16(device.serialNumber.data)}`));

      root.add(customTreeItem(
          `WebUSB Landing Page: ${device.webusbLandingPage.url}`));

      root.add(customTreeItem(
          `Active Configuration: ${device.activeConfiguration}`));

      const configurationsArray = device.configurations;
      this.renderConfigurationTreeItem_(configurationsArray, root);
    }

    /**
     * Renders a tree item to display the device's configurations information.
     * @param {!Array<!device.mojom.UsbConfigurationInfo>} configurationsArray
     * @param {!cr.ui.TreeItem} root
     * @private
     */
    renderConfigurationTreeItem_(configurationsArray, root) {
      for (const configuration of configurationsArray) {
        const configurationItem =
            customTreeItem(`Configuration ${configuration.configurationValue}`);

        if (configuration.configurationName) {
          configurationItem.add(customTreeItem(`Configuration Name: ${
              decodeString16(configuration.configurationName.data)}`));
        }

        const interfacesArray = configuration.interfaces;
        this.renderInterfacesTreeItem_(interfacesArray, configurationItem);

        root.add(configurationItem);
      }
    }

    /**
     * Renders a tree item to display the device's interfaces information.
     * @param {!Array<!device.mojom.UsbInterfaceInfo>} interfacesArray
     * @param {!cr.ui.TreeItem} root
     * @private
     */
    renderInterfacesTreeItem_(interfacesArray, root) {
      for (const currentInterface of interfacesArray) {
        const interfaceItem =
            customTreeItem(`Interface ${currentInterface.interfaceNumber}`);

        const alternatesArray = currentInterface.alternates;
        this.renderAlternatesTreeItem_(alternatesArray, interfaceItem);

        root.add(interfaceItem);
      }
    }

    /**
     * Renders a tree item to display the device's alternate interfaces
     * information.
     * @param {!Array<!device.mojom.UsbAlternateInterfaceInfo>} alternatesArray
     * @param {!cr.ui.TreeItem} root
     * @private
     */
    renderAlternatesTreeItem_(alternatesArray, root) {
      for (const alternate of alternatesArray) {
        const alternateItem =
            customTreeItem(`Alternate ${alternate.alternateSetting}`);

        alternateItem.add(customTreeItem(`Class Code: ${alternate.classCode}`));

        alternateItem.add(
            customTreeItem(`Subclass Code: ${alternate.subclassCode}`));

        alternateItem.add(
            customTreeItem(`Protocol Code: ${alternate.protocolCode}`));

        if (alternate.interfaceName) {
          alternateItem.add(customTreeItem(`Interface Name: ${
              decodeString16(alternate.interfaceName.data)}`));
        }

        const endpointsArray = alternate.endpoints;
        this.renderEndpointsTreeItem_(endpointsArray, alternateItem);

        root.add(alternateItem);
      }
    }

    /**
     * Renders a tree item to display the device's endpoints information.
     * @param {!Array<!device.mojom.UsbEndpointInfo>} endpointsArray
     * @param {!cr.ui.TreeItem} root
     * @private
     */
    renderEndpointsTreeItem_(endpointsArray, root) {
      for (const endpoint of endpointsArray) {
        let itemLabel = 'Endpoint ';

        itemLabel += endpoint.endpointNumber;

        switch (endpoint.direction) {
          case device.mojom.UsbTransferDirection.INBOUND:
            itemLabel += ' (INBOUND)';
            break;
          case device.mojom.UsbTransferDirection.OUTBOUND:
            itemLabel += ' (OUTBOUND)';
            break;
        }

        const endpointItem = customTreeItem(itemLabel);

        let usbTransferType = '';
        switch (endpoint.type) {
          case device.mojom.UsbTransferType.CONTROL:
            usbTransferType = 'CONTROL';
            break;
          case device.mojom.UsbTransferType.ISOCHRONOUS:
            usbTransferType = 'ISOCHRONOUS';
            break;
          case device.mojom.UsbTransferType.BULK:
            usbTransferType = 'BULK';
            break;
          case device.mojom.UsbTransferType.INTERRUPT:
            usbTransferType = 'INTERRUPT';
            break;
        }

        endpointItem.add(
            customTreeItem(`USB Transfer Type: ${usbTransferType}`));

        endpointItem.add(customTreeItem(`Packet Size: ${endpoint.packetSize}`));

        root.add(endpointItem);
      }
    }
  }

  /**
   * Parses utf16 coded string.
   * @param {!mojoBase.mojom.String16} arr
   * @return {string}
   */
  function decodeString16(arr) {
    return arr.map(ch => String.fromCodePoint(ch)).join('');
  }

  /**
   * Parses the decimal number to hex format.
   * @param {number} num
   * @return {string}
   */
  function toHex(num) {
    return `0x${num.toString(16).padStart(4, '0').slice(-4).toUpperCase()}`;
  }

  /**
   * Renders a customized TreeItem with the given content and class name.
   * @param {string} itemLabel
   * @return {!cr.ui.TreeItem}
   * @private
   */
  function customTreeItem(itemLabel) {
    return item = new cr.ui.TreeItem({
      label: itemLabel,
      icon: '',
    });
  }

  return {
    DevicePage,
    DevicesPage,
  };
});