/* Adopted from LayoutTests/webexposed/resources/global-interface-listing.js */

var globals = [];

function is_constructor(property_name) {
  var descriptor = Object.getOwnPropertyDescriptor(this, property_name);
  if (descriptor.value === undefined ||
      descriptor.value.prototype === undefined) {
    return false;
  }
  return descriptor.writable && !descriptor.enumerable &&
         descriptor.configurable;
}

var interface_names = Object.getOwnPropertyNames(this).filter(is_constructor);
interface_names.sort();
interface_names.forEach(function(interface_name) {
    globals.push('interface ' + interface_name);
    var property_strings = [];
    var prototype = this[interface_name].prototype;
    Object.getOwnPropertyNames(prototype).forEach(function(property_name) {
        var descriptor = Object.getOwnPropertyDescriptor(
          prototype, property_name);
        if ('value' in descriptor) {
          var type;
          if (typeof descriptor.value === 'function') {
            type = 'method';
          } else {
            type = 'attribute';
          }
          property_strings.push('    ' + type + ' ' + property_name);
        } else {
          if (descriptor.get)
            property_strings.push('    getter ' + property_name);
          if (descriptor.set)
            property_strings.push('    setter ' + property_name);
        }
      });
    globals.push.apply(globals, property_strings.sort());
  });

self.addEventListener('message', function(event) {
    event.ports[0].postMessage({ result: globals });
  });
