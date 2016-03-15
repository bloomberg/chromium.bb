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
