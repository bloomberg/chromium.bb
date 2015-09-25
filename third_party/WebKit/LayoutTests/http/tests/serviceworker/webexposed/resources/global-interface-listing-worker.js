/* Adopted from LayoutTests/webexposed/resources/global-interface-listing.js */

// Run all the code in a local scope.
(function(global_object) {

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
    'Infinity',
    'Int16Array',
    'Int32Array',
    'Int8Array',
    'Intl',
    'JSON',
    'Map',
    'Math',
    'NaN',
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
    'URIError',
    'Uint16Array',
    'Uint32Array',
    'Uint8Array',
    'Uint8ClampedArray',
    'WeakMap',
    'WeakSet',
    'decodeURI',
    'decodeURIComponent',
    'encodeURI',
    'encodeURIComponent',
    'escape',
    'eval',
    'isFinite',
    'isNaN',
    'parseFloat',
    'parseInt',
    'undefined',
    'unescape',
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

function collect_property_info(object, property_name, output) {
  var descriptor = Object.getOwnPropertyDescriptor(object, property_name);
  if ('value' in descriptor) {
    var type;
    if (typeof descriptor.value === 'function') {
      type = 'method';
    } else {
      type = 'attribute';
    }
    output.push('    ' + type + ' ' + property_name);
  } else {
    if (descriptor.get)
      output.push('    getter ' + property_name);
    if (descriptor.set)
      output.push('    setter ' + property_name);
  }
}

var interface_names = Object.getOwnPropertyNames(global_object).filter(is_web_idl_constructor);
interface_names.sort();
interface_names.forEach(function(interface_name) {
    globals.push('interface ' + interface_name);
    var property_strings = [];
    var prototype = this[interface_name].prototype;
    Object.getOwnPropertyNames(prototype).forEach(function(property_name) {
      collect_property_info(prototype, property_name, property_strings);
    });
    globals.push.apply(globals, property_strings.sort());
  });

globals.push('global object');
var property_strings = [];
var member_names = Object.getOwnPropertyNames(global_object).filter(function(property_name) {
  return !js_builtins.has(property_name) && !is_web_idl_constructor(property_name);
});
member_names.forEach(function(property_name) {
  collect_property_info(global_object, property_name, property_strings);
});
globals.push.apply(globals, property_strings.sort());

self.addEventListener('message', function(event) {
    event.ports[0].postMessage({ result: globals });
  });

})(this); // Run all the code in a local scope.
