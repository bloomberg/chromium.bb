'use strict';

(() => {
  let dependencies = [
    "file:///gen/layout_test_data/mojo/public/js/mojo_bindings.js",
    "file:///gen/device/usb/public/interfaces/device.mojom.js",
    "file:///gen/device/usb/public/interfaces/device_manager.mojom.js",
    "file:///gen/device/usb/public/interfaces/chooser_service.mojom.js",
    "resources/webusb-test-impl.js",
  ];
  for (let dep of dependencies)
    document.write("<script src='" + dep + "'></script>");
})();
