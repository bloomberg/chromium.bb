(function() {

if (!window.internals) {
    testFailed('Test must be run with --expose-internals-for-testing flag.');
    return;
}

// FIXME(https://crbug.com/43394):
// These properties should live in the element prototypes instead of on the individual instances.
function listElementProperties(type) {
    debug('[' + type.toUpperCase() + ' NAMESPACE ELEMENT PROPERTIES]');
    var namespace = internals[type + 'Namespace']();
    debug('namespace ' + namespace);
    var tags = internals[type + 'Tags']();
    var tagProperties = {};
    var isCommonProperty = null; // Will be a map containing the intersection of properties across all elements as keys.
    tags.forEach(function(tag) {
        var element = document.createElement(tag, namespace);
        // We don't read out the property descriptors here to avoid the test timing out.
        var properties = Object.getOwnPropertyNames(element).sort();
        tagProperties[tag] = properties;
        if (isCommonProperty === null) {
            isCommonProperty = {};
            properties.forEach(function(property) {
                isCommonProperty[property] = true;
            });
        } else {
            var hasProperty = {};
            properties.forEach(function(property) {
                hasProperty[property] = true;
            });
            Object.getOwnPropertyNames(isCommonProperty).forEach(function(property) {
                if (!hasProperty[property])
                    delete isCommonProperty[property];
            });
        }
    });
    debug(escapeHTML('<common>'));
    Object.getOwnPropertyNames(isCommonProperty).sort().forEach(function(property) {
        debug('    property ' + property);
    });
    tags.forEach(function(tag) {
        debug(type + ' element ' + tag);
        tagProperties[tag].forEach(function(property) {
            if (!isCommonProperty[property])
                debug('    property ' + property);
        });
    });
}

listElementProperties('html');
listElementProperties('svg');

})();
