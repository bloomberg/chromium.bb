function collectProperties()
{
    // Collect properties of the top-level window, since touching the properties
    // of a DOMWindow affects its internal C++ state.
    collectPropertiesHelper(window, []);

    propertiesToVerify.sort(function (a, b)
    {
        if (a.property < b.property)
            return -1
        if (a.property > b.property)
            return 1;
        return 0;
    });
}

function emitExpectedResult(path, expected)
{
    if (path[0] == 'internals' // Skip internals properties, since they aren't web accessible.
        || path[0] == 'propertiesToVerify' // Skip the list we're building...
        || path[0] == 'clientInformation') // Just an alias for navigator.
        return;
    // FIXME: Skip MemoryInfo for now, since it's not implemented as a DOMWindowProperty, and has
    // no way of knowing when it's detached. Eventually this should have the same behavior.
    if (path.length >= 2 && (path[0] == 'console' || path[0] == 'performance') && path[1] == 'memory')
        return;
    // Skip things that are assumed to be constants.
    if (path[path.length - 1].toUpperCase() == path[path.length - 1])
        return;

    // Various special cases...
    var propertyPath = path.join('.');
    switch (propertyPath) {
    case "location.href":
        expected = "'about:blank'";
        break;
    case "location.origin":
        expected = "'null'";
        break;
    case "location.pathname":
        expected = "'blank'";
        break;
    case "location.protocol":
        expected = "'about:'";
        break;
    case "navigator.appCodeName":
    case "navigator.appName":
    case "navigator.language":
    case "navigator.onLine":
    case "navigator.platform":
    case "navigator.product":
    case "navigator.productSub":
    case "navigator.vendor":
        expected = "window." + propertyPath;
        break;
    }

    insertExpectedResult(path, expected);
}

function collectPropertiesHelper(object, path)
{
    if (path.length > 20)
        throw 'Error: probably looping';
    for (var property in object) {
        if (!object[property])
            continue;
        path.push(property);
        var type = typeof(object[property]);
        if (type == "object") {
            // Skip some traversing through types that will end up in cycles...
            if (!object[property].Window
                && !(object[property] instanceof Node)
                && !(object[property] instanceof MimeTypeArray)
                && !(object[property] instanceof PluginArray)) {
                collectPropertiesHelper(object[property], path);
            }
        } else if (type == "string") {
            emitExpectedResult(path, "''");
        } else if (type == "number") {
            emitExpectedResult(path, "0");
        } else if (type == "boolean") {
            emitExpectedResult(path, "false");
        }
        path.pop();
    }
}

