# Web Bluetooth Blink Module

`Source/modules/bluetooth` implements the renderer process details and bindings
for the [Web Bluetooth specification]. It uses the Web Bluetooth Service
[mojom] to communicate with the [Web Bluetooth Service].

[Web Bluetooth specification]: https://webbluetoothcg.github.io/web-bluetooth/
[mojom]: ../../../public/platform/modules/bluetooth/web_bluetooth.mojom
[Web Bluetooth Service]: /content/browser/bluetooth/


## LE only Scanning

There isn't much support for GATT over BR/EDR from neither platforms nor
devices so performing a Dual scan will find devices that the API is not
able to interact with. To avoid wasting power and confusing users with
devices they are not able to interact with, navigator.bluetooth.requestDevice
performs an LE-only Scan.


## Testing

Bluetooth layout tests in `LayoutTests/bluetooth/` rely on
fake Bluetooth implementation classes constructed in
`content/shell/browser/layout_test/layout_test_bluetooth_adapter_provider`.
These tests span JavaScript binding to the `device/bluetooth` API layer.

See also the [Web Bluetooth Fuzzer](testing/clusterfuzz/README.md).


## Design Documents

See: [Class Diagram of Web Bluetooth through Bluetooth Android][Class]

[Class]: https://sites.google.com/a/chromium.org/dev/developers/design-documents/bluetooth-design-docs/web-bluetooth-through-bluetooth-android-class-diagram

