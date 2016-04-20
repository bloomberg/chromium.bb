// Run all the code in a local scope.
(function(globalObject) {

// Save the list of property names of the global object before loading other scripts.
var propertyNamesInGlobal = globalObject.propertyNamesInGlobal || Object.getOwnPropertyNames(globalObject);

if (self.importScripts) {
    importScripts('../../resources/js-test.js');

    if (!self.postMessage) {
        // Shared worker.  Make postMessage send to the newest client, which in
        // our tests is the only client.

        // Store messages for sending until we have somewhere to send them.
        self.postMessage = function(message) {
            if (typeof self.pendingMessages === "undefined")
                self.pendingMessages = [];
            self.pendingMessages.push(message);
        };
        self.onconnect = function(event) {
            self.postMessage = function(message) {
                event.ports[0].postMessage(message);
            };
            // Offload any stored messages now that someone has connected to us.
            if (typeof self.pendingMessages === "undefined")
                return;
            while (self.pendingMessages.length)
                event.ports[0].postMessage(self.pendingMessages.shift());
        };
    }
}

// List of builtin JS constructors; Blink is not controlling what properties these
// objects have, so exercising them in a Blink test doesn't make sense.
//
// If new builtins are added, please update this list along with the one in
// LayoutTests/http/tests/serviceworker/webexposed/resources/global-interface-listing-worker.js
var jsBuiltins = new Set([
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
    'Proxy',
    'RangeError',
    'ReferenceError',
    'Reflect',
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

function collectPropertyInfo(object, propertyKey, output) {
    var propertyString = wellKnownSymbols.get(propertyKey) || propertyKey.toString();
    var keywords = Object.prototype.hasOwnProperty.call(object, 'prototype') ? 'static ' : '';
    var descriptor = Object.getOwnPropertyDescriptor(object, propertyKey);
    if ('value' in descriptor) {
        var type = typeof descriptor.value === 'function' ? 'method' : 'attribute';
        output.push('    ' + keywords + type + ' ' + propertyString);
    } else {
        if (descriptor.get)
            output.push('    ' + keywords + 'getter ' + propertyString);
        if (descriptor.set)
            output.push('    ' + keywords + 'setter ' + propertyString);
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
       return propertyKeys = Object.keys(object).
           concat(ownEnumerableSymbols(object)).
           filter(function(name) {
               return !protoProperties.has(name);
           });
   }
   return Object.getOwnPropertyNames(object).concat(Object.getOwnPropertySymbols(object));
}

// FIXME: List interfaces with NoInterfaceObject specified in their IDL file.
debug('[INTERFACES]');
var interfaceNames = Object.getOwnPropertyNames(this).filter(isWebIDLConstructor);
interfaceNames.sort();
interfaceNames.forEach(function(interfaceName) {
    var inheritsFrom = this[interfaceName].__proto__.name;
    if (inheritsFrom)
        debug('interface ' + interfaceName + ' : ' + inheritsFrom);
    else
        debug('interface ' + interfaceName);
    // List static properties then prototype properties.
    [this[interfaceName], this[interfaceName].prototype].forEach(function(object) {
        var propertyKeys = collectPropertyKeys(object);
        var propertyStrings = [];
        propertyKeys.forEach(function(propertyKey) {
            collectPropertyInfo(object, propertyKey, propertyStrings);
        });
        propertyStrings.sort().forEach(debug);
    });
});

debug('[GLOBAL OBJECT]');
var propertyStrings = [];
var memberNames = propertyNamesInGlobal.filter(function(propertyKey) {
    return !jsBuiltins.has(propertyKey) && !isWebIDLConstructor(propertyKey);
});
memberNames.forEach(function(propertyKey) {
    collectPropertyInfo(globalObject, propertyKey, propertyStrings);
});
propertyStrings.sort().forEach(debug);

if (isWorker())
    finishJSTest();

})(this); // Run all the code in a local scope.
