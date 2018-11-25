// * |globalObject| should be the global (usually |this|).
// * |propertyNamesInGlobal| should be a list of properties captured before
//   other scripts are loaded, using: Object.getOwnPropertyNames(this);
// * |platformSpecific| determines the platform-filtering of interfaces and
//   properties. Only platform-specific interfaces/properties will be tested if
//   set to true, and only all-platform interfaces/properties will be used
//   if set to false.
// * |outputFunc| is called back with each line of output.

function globalInterfaceListing(globalObject, propertyNamesInGlobal, platformSpecific, outputFunc) {

// List of builtin JS constructors; Blink is not controlling what properties these
// objects have, so exercising them in a Blink test doesn't make sense.
//
// If new builtins are added, please update this list along with the one in
// LayoutTests/http/tests/worklet/webexposed/resources/global-interface-listing-worklet.js
var jsBuiltins = new Set([
    'Array',
    'ArrayBuffer',
    'Atomics',
    'BigInt',
    'BigInt64Array',
    'BigUint64Array',
    'Boolean',
    'DataView',
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
    'Proxy',
    'RangeError',
    'ReferenceError',
    'Reflect',
    'RegExp',
    'Set',
    'SharedArrayBuffer',
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
    'WebAssembly',
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

function isWebIDLConstructor(propertyKey) {
    if (jsBuiltins.has(propertyKey))
        return false;
    var descriptor = Object.getOwnPropertyDescriptor(this, propertyKey);
    if (descriptor.value == undefined || descriptor.value.prototype == undefined)
        return false;
    return descriptor.writable && !descriptor.enumerable && descriptor.configurable;
}

var wellKnownSymbols = new Map([
    [Symbol.hasInstance, "@@hasInstance"],
    [Symbol.isConcatSpreadable, "@@isConcatSpreadable"],
    [Symbol.iterator, "@@iterator"],
    [Symbol.match, "@@match"],
    [Symbol.replace, "@@replace"],
    [Symbol.search, "@@search"],
    [Symbol.species, "@@species"],
    [Symbol.split, "@@split"],
    [Symbol.toPrimitive, "@@toPrimitive"],
    [Symbol.toStringTag, "@@toStringTag"],
    [Symbol.unscopables, "@@unscopables"]
]);

// List of all platform-specific interfaces. Please update this list when adding
// a new platform-specific interface. This enables us to keep the churn on the
// platform-specific expectations files to a bare minimum to make updates in the
// common (platform-neutral) case as simple as possible.
var platformSpecificInterfaces = new Set([
    'Bluetooth',
    'BluetoothCharacteristicProperties',
    'BluetoothDevice',
    'BluetoothRemoteGATTCharacteristic',
    'BluetoothRemoteGATTDescriptor',
    'BluetoothRemoteGATTServer',
    'BluetoothRemoteGATTService',
    'BluetoothUUID',
]);

// List of all platform-specific properties on interfaces that appear on all
// platforms. Please update this list when adding a new platform-specific
// property to a platform-neutral interface.
var platformSpecificProperties = {
    Navigator: new Set([
        'getter bluetooth',
    ]),
    Notification: new Set([
        'getter image',
    ]),
};

// List of all platform-specific global properties. Please update this list when
// adding a new platform-specific global property.
var platformSpecificGlobalProperties = new Set([
]);

function filterPlatformSpecificInterface(interfaceName) {
    return platformSpecificProperties.hasOwnProperty(interfaceName) ||
        platformSpecificInterfaces.has(interfaceName) == platformSpecific;
}

function filterPlatformSpecificProperty(interfaceName, property) {
    return (platformSpecificInterfaces.has(interfaceName) ||
            (platformSpecificProperties.hasOwnProperty(interfaceName) &&
             platformSpecificProperties[interfaceName].has(property))) ==
        platformSpecific;
}

function filterPlatformSpecificGlobalProperty(property) {
    return platformSpecificGlobalProperties.has(property) == platformSpecific;
}

function collectPropertyInfo(object, propertyKey, output) {
    var propertyString = wellKnownSymbols.get(propertyKey) || propertyKey.toString();
    var keywords = Object.prototype.hasOwnProperty.call(object, 'prototype') ? 'static ' : '';
    var descriptor = Object.getOwnPropertyDescriptor(object, propertyKey);
    if ('value' in descriptor) {
        var type = typeof descriptor.value === 'function' ? 'method' : 'attribute';
        output.push(keywords + type + ' ' + propertyString);
    } else {
        if (descriptor.get)
            output.push(keywords + 'getter ' + propertyString);
        if (descriptor.set)
            output.push(keywords + 'setter ' + propertyString);
    }
}

function ownEnumerableSymbols(object) {
    return Object.getOwnPropertySymbols(object).
        filter(function(name) {
            return Object.getOwnPropertyDescriptor(object, name).enumerable;
        });
}

function collectPropertyKeys(object) {
   if (Object.prototype.hasOwnProperty.call(object, 'prototype')) {
       // Skip properties that aren't static (e.g. consts), or are inherited.
       // TODO(caitp): Don't exclude non-enumerable properties
       var protoProperties = new Set(Object.keys(object.prototype).concat(
                                     Object.keys(object.__proto__),
                                     ownEnumerableSymbols(object.prototype),
                                     ownEnumerableSymbols(object.__proto__)));
       return Object.keys(object).
           concat(ownEnumerableSymbols(object)).
           filter(function(name) {
               return !protoProperties.has(name);
           });
   }
   return Object.getOwnPropertyNames(object).concat(Object.getOwnPropertySymbols(object));
}

function outputProperty(property) {
    outputFunc('    ' + property);
}

// FIXME: List interfaces with NoInterfaceObject specified in their IDL file.
outputFunc('[INTERFACES]');
var interfaceNames = Object.getOwnPropertyNames(this)
                           .filter(isWebIDLConstructor)
                           .filter(filterPlatformSpecificInterface);
interfaceNames.sort();
interfaceNames.forEach(function(interfaceName) {
    var inheritsFrom = this[interfaceName].__proto__.name;
    if (inheritsFrom)
        outputFunc('interface ' + interfaceName + ' : ' + inheritsFrom);
    else
        outputFunc('interface ' + interfaceName);
    // List static properties then prototype properties.
    [this[interfaceName], this[interfaceName].prototype].forEach(function(object) {
        var propertyKeys = collectPropertyKeys(object);
        var propertyStrings = [];
        propertyKeys.forEach(function(propertyKey) {
            collectPropertyInfo(object, propertyKey, propertyStrings);
        });

        propertyStrings.filter((property) => filterPlatformSpecificProperty(interfaceName, property))
            .sort().forEach(outputProperty);
    });
});

outputFunc('[GLOBAL OBJECT]');
var propertyStrings = [];
var memberNames = propertyNamesInGlobal.filter(function(propertyKey) {
    return !jsBuiltins.has(propertyKey) && !isWebIDLConstructor(propertyKey);
});
memberNames.forEach(function(propertyKey) {
    collectPropertyInfo(globalObject, propertyKey, propertyStrings);
});
propertyStrings.sort().filter(filterPlatformSpecificGlobalProperty).forEach(outputProperty);
}
