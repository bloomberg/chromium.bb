if (this.importScripts)
    importScripts('../../fast/js/resources/js-test-pre.js');

function isConstructor(propertyName) {
    var descriptor = Object.getOwnPropertyDescriptor(this, propertyName);
    if (descriptor.value == undefined || descriptor.value.prototype == undefined)
        return false;
    return descriptor.writable && !descriptor.enumerable && descriptor.configurable;
}

var constructorNames = [];
var propertyNames = Object.getOwnPropertyNames(this);
for (var i = 0; i < propertyNames.length; i++) {
    if (isConstructor(propertyNames[i]))
        constructorNames[constructorNames.length] = propertyNames[i];
}


constructorNames.sort();
for (var i = 0; i < constructorNames.length; i++)
    debug(constructorNames[i]);

if (isWorker())
  finishJSTest();
