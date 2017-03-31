'use strict';

function fakeUsbDevices() {
  return define('Fake USB Devices', [
    'device/usb/public/interfaces/device.mojom',
  ], device => Promise.resolve([
    {
      guid: 'CD9FA048-FC9B-7A71-DBFC-FD44B78D6397',
      usb_version_major: 2,
      usb_version_minor: 0,
      usb_version_subminor: 0,
      class_code: 7,
      subclass_code: 1,
      protocol_code: 2,
      vendor_id: 0x18d1,
      product_id: 0xf00d,
      device_version_major: 1,
      device_version_minor: 2,
      device_version_subminor: 3,
      manufacturer_name: 'Google, Inc.',
      product_name: 'The amazing imaginary printer',
      serial_number: '4',
      active_configuration: 0,
      configurations: [
        {
          configuration_value: 1,
          configuration_name: 'Printer Mode',
          interfaces: [
            {
              interface_number: 0,
              alternates: [
                {
                  alternate_setting: 0,
                  class_code: 0xff,
                  subclass_code: 0x01,
                  protocol_code: 0x01,
                  interface_name: 'Control',
                  endpoints: [
                    {
                      endpoint_number: 1,
                      direction: device.TransferDirection.INBOUND,
                      type: device.EndpointType.INTERRUPT,
                      packet_size: 8
                    }
                  ]
                }
              ]
            },
            {
              interface_number: 1,
              alternates: [
                {
                  alternate_setting: 0,
                  class_code: 0xff,
                  subclass_code: 0x02,
                  protocol_code: 0x01,
                  interface_name: 'Data',
                  endpoints: [
                    {
                      endpoint_number: 2,
                      direction: device.TransferDirection.INBOUND,
                      type: device.EndpointType.BULK,
                      packet_size: 1024
                    },
                    {
                      endpoint_number: 2,
                      direction: device.TransferDirection.OUTBOUND,
                      type: device.EndpointType.BULK,
                      packet_size: 1024
                    }
                  ]
                }
              ]
            }
          ]
        },
        {
          configuration_value: 2,
          configuration_name: 'Fighting Robot Mode',
          interfaces: [
            {
              interface_number: 0,
              alternates: [
                {
                  alternate_setting: 0,
                  class_code: 0xff,
                  subclass_code: 0x42,
                  protocol_code: 0x01,
                  interface_name: 'Disabled',
                  endpoints: []
                },
                {
                  alternate_setting: 1,
                  class_code: 0xff,
                  subclass_code: 0x42,
                  protocol_code: 0x01,
                  interface_name: 'Activate!',
                  endpoints: [
                    {
                      endpoint_number: 1,
                      direction: device.TransferDirection.INBOUND,
                      type: device.EndpointType.ISOCHRONOUS,
                      packet_size: 1024
                    },
                    {
                      endpoint_number: 1,
                      direction: device.TransferDirection.OUTBOUND,
                      type: device.EndpointType.ISOCHRONOUS,
                      packet_size: 1024
                    }
                  ]
                }
              ]
            },
          ]
        }
      ],
      webusb_allowed_origins: { origins: [], configurations: [] },
    }
  ]));
}

let fakeDeviceInit = {
  usbVersionMajor: 2,
  usbVersionMinor: 0,
  usbVersionSubminor: 0,
  deviceClass: 7,
  deviceSubclass: 1,
  deviceProtocol: 2,
  vendorId: 0x18d1,
  productId: 0xf00d,
  deviceVersionMajor: 1,
  deviceVersionMinor: 2,
  deviceVersionSubminor: 3,
  manufacturerName: 'Google, Inc.',
  productName: 'The amazing imaginary printer',
  serialNumber: '4',
  activeConfigurationValue: 0,
  configurations: [{
    configurationValue: 1,
    configurationName: 'Printer Mode',
    interfaces: [{
      interfaceNumber: 0,
      alternates: [{
        alternateSetting: 0,
        interfaceClass: 0xff,
        interfaceSubclass: 0x01,
        interfaceProtocol: 0x01,
        interfaceName: 'Control',
        endpoints: [{
          endpointNumber: 1,
          direction: 'in',
          type: 'interrupt',
          packetSize: 8
        }]
      }]
    }, {
      interfaceNumber: 1,
      alternates: [{
        alternateSetting: 0,
        interfaceClass: 0xff,
        interfaceSubclass: 0x02,
        interfaceProtocol: 0x01,
        interfaceName: 'Data',
        endpoints: [{
          endpointNumber: 2,
          direction: 'in',
          type: 'bulk',
          packetSize: 1024
        }, {
          endpointNumber: 2,
          direction: 'out',
          type: 'bulk',
          packetSize: 1024
        }]
      }]
    }]
  }, {
    configurationValue: 2,
    configurationName: 'Fighting Robot Mode',
    interfaces: [{
      interfaceNumber: 0,
      alternates: [{
        alternateSetting: 0,
        interfaceClass: 0xff,
        interfaceSubclass: 0x42,
        interfaceProtocol: 0x01,
        interfaceName: 'Disabled',
        endpoints: []
      }, {
        alternateSetting: 1,
        interfaceClass: 0xff,
        interfaceSubclass: 0x42,
        interfaceProtocol: 0x01,
        interfaceName: 'Activate!',
        endpoints: [{
          endpointNumber: 1,
          direction: 'in',
          type: 'isochronous',
          packetSize: 1024
        }, {
          endpointNumber: 1,
          direction: 'out',
          type: 'isochronous',
          packetSize: 1024
        }]
      }]
    }]
  }]
};
