/* Adopted from LayoutTests/webexposed/resources/global-interface-listing.js */

var globals = [];

// List of builtin JS constructors; Blink is not controlling what properties these
// objects have, so exercising them in a Blink test doesn't make sense.
//
// This list should be kept in sync with the one at LayoutTests/webexposed/resources/global-interface-listing.js
var js_builtins = new Set([
    'Array',
    'ArrayBuffer',
    'Boolean',
    'Date',
    'Error',
    'EvalError',
    'Float32Array',
    'Float64Array',
    'Function',
    'Int16Array',
    'Int32Array',
    'Int8Array',
    'Map',
    'Number',
    'Object',
    'Promise',
    'RangeError',
    'ReferenceError',
    'RegExp',
    'Set',
    'String',
    'Symbol',
    'SyntaxError',
    'TypeError',
    'Uint16Array',
    'Uint32Array',
    'Uint8Array',
    'Uint8ClampedArray',
    'URIError',
    'WeakMap',
    'WeakSet',
]);

function is_web_idl_constructor(property_name) {
  if (js_builtins.has(property_name))
    return false;
  var descriptor = Object.getOwnPropertyDescriptor(this, property_name);
  if (descriptor.value === undefined ||
      descriptor.value.prototype === undefined) {
    return false;
  }
  return descriptor.writable && !descriptor.enumerable &&
         descriptor.configurable;
}

var interface_names = Object.getOwnPropertyNames(this).filter(is_web_idl_constructor);
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
