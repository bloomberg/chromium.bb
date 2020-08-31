# Web Bluetooth Service in Content

This directory contains the implementation of the [Web Bluetooth specification]
using the Bluetooth abstraction module implemented in `//device/bluetooth`. See
the [Bluetooth Abstraction README] for more details on the cross-platform
implementation of Bluetooth in Chromium.

This service is exposed to the Web through the Blink Bluetooth module, which
accesses the Web Bluetooth Service through Mojo IPC. For more details, see the
[Web Bluetooth Blink Module README].

[Web Bluetooth specification]: https://webbluetoothcg.github.io/web-bluetooth/
[Bluetooth Abstraction README]: ../../../device/bluetooth/README.md
[Web Bluetooth Blink Module README]:
../../../third_party/blink/renderer/modules/bluetooth/README.md

## Web Bluetooth Permissions

The legacy permissions system is implemented by `bluetooth_allowed_devices.h`,
which is created per origin.

The new permissions system is implemented by providing an implementation for
the `//content/public/browser/bluetooth_delegate.h` interface. In Chrome, the
implementation of this interface is provided by
`//chrome/browser/chrome_bluetooth_delegate.h` which forwards permission
queries to `//chrome/browser/bluetooth/bluetooth_chooser_context.h`. This
class uses `//components/permissions/chooser_context_base.h` as the base.
This base class is also in use by other device APIs, like WebUSB. The new
permission system enables Web Bluetooth permissions to be persistent and to
be exposed in the settings UI for users to manage more easily. For more
details on the new permissions system, see the [Web Bluetooth Persistent
Permissions] design document.

[Web Bluetooth Persistent Permissions]:
https://docs.google.com/document/d/1h3uAVXJARHrNWaNACUPiQhLt7XI-fFFQoARSs1WgMDM/edit?usp=sharing

## Testing

The Web Bluetooth Service is primarily tested using Blink Web Tests and Web
Platform Tests. These tests are found in
`//third_party/blink/web_tests/bluetooth` and
`//third_party/blink/web_tests/external/wpt/bluetooth`. There is currently an
ongoing effort to refactor the Web Bluetooth tests using the legacy
[BluetoothFakeAdapter] test infrastructure to use the new [FakeBluetooth]
Test API. For more details, see the [Web Bluetooth Web Tests README] and the
[Web Bluetooth Web Platform Tests README].

TODO(https://crbug.com/509038): Update this document when the remaining tests
have been submitted to W3C Web Platform Tests.

The tests are run using `content_shell`, which fakes the Bluetooth related UI
and the new permissions system. For more details, see the following files in
`//content/shell/browser/web_test`:
* [fake_bluetooth_chooser.h]
* [fake_bluetooth_delegate.h]
* [fake_bluetooth_scanning_prompt.h]
* [web_test_bluetooth_adapter_provider.h][BluetoothFakeAdapter]
* [web_test_first_device_bluetooth_chooser.h]

[BluetoothFakeAdapter]:
../../shell/browser/web_test/web_test_bluetooth_adapter_provider.h
[FakeBluetooth]:
../../../device/bluetooth/test/fake_bluetooth.h
[Web Bluetooth Web Tests README]:
../../../third_party/blink/web_tests/bluetooth/README.md
[Web Bluetooth Web Platform Tests README]:
../../../third_party/blink/web_tests/external/wpt/bluetooth/README.md
[fake_bluetooth_chooser.h]:
../../shell/browser/web_test/fake_bluetooth_chooser.h
[fake_bluetooth_delegate.h]:
../../shell/browser/web_test/fake_bluetooth_delegate.h
[fake_bluetooth_scanning_prompt.h]:
../../shell/browser/web_test/fake_bluetooth_scanning_prompt.h
[web_test_first_device_bluetooth_chooser.h]:
../../shell/browser/web_test/web_test_first_device_bluetooth_chooser.h

# Resources and Documentation

Mailing list: web-bluetooth@chromium.org

Bug tracker: [Blink>Bluetooth]

* [Web Bluetooth specification]
* [Bluetooth Abstraction README]
* [Web Bluetooth Blink Module README]
* [BluetoothFakeAdapter]
* [FakeBluetooth]

[Blink>Bluetooth]: https://bugs.chromium.org/p/chromium/issues/list?q=component%3ABlink%3EBluetooth&can=2

## Design Documents

* [Class Diagram of Web Bluetooth through Bluetooth Android]
* [Web Bluetooth Testing]
* [Web Bluetooth Test Scanning]
* [Web Bluetooth Persistent Permissions]

[Class Diagram of Web Bluetooth through Bluetooth Android]:
https://sites.google.com/a/chromium.org/dev/developers/design-documents/bluetooth-design-docs/web-bluetooth-through-bluetooth-android-class-diagram
[Web Bluetooth Testing]:
https://docs.google.com/document/d/1Nhv_oVDCodd1pEH_jj9k8gF4rPGb_84VYaZ9IG8M_WY/edit?usp=sharing
[Web Bluetooth Test Scanning]:
https://docs.google.com/document/d/1XFl_4ZAgO8ddM6U53A9AfUuZeWgJnlYD5wtbXqEpzeg/edit?usp=sharing
