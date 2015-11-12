//  TODO(chongz): Remove this file and import from w3c tests after approved.
//  crbug.com/552530

// Check a Touch object's atttributes for existence and correct type
// TA: 1.1.2, 1.1.3
function check_Touch_object(t) {
    test(function() {
        assert_equals(Object.prototype.toString.call(t), "[object Touch]", "touch is of type Touch");
    }, "touch point is a Touch object");
    [
        ["long", "identifier"],
        ["EventTarget", "target"],
        ["long", "screenX"],
        ["long", "screenY"],
        ["long", "clientX"],
        ["long", "clientY"],
        ["long", "pageX"],
        ["long", "pageY"],
        ["long", "radiusX"],
        ["long", "radiusY"],
        ["long", "rotationAngle"],
        ["long", "force"],
    ].forEach(function(attr) {
        var type = attr[0];
        var name = attr[1];

        // existence check
        test(function() {
            assert_true(name in t, name + " attribute in Touch object");
        }, "Touch." + name + " attribute exists");

        // type check
        switch (type) {
            case "long":
                test(function() {
                    assert_equals(typeof t[name], "number", name + " attribute of type long");
                }, "Touch." + name + " attribute is of type number (long)");
                break;
            case "EventTarget":
                // An event target is some type of Element
                test(function() {
                    assert_true(t[name] instanceof Element, "EventTarget must be an Element.");
                }, "Touch." + name + " attribute is of type Element");
                break;
            default:
                break;
        }
    });
}

// Check a TouchList object's attributes and methods for existence and proper type
// Also make sure all of the members of the list are Touch objects
// TA: 1.2.1, 1.2.2, 1.2.5, 1.2.6
function check_TouchList_object(tl) {
    test(function() {
        assert_equals(Object.prototype.toString.call(tl), "[object TouchList]", "touch list is of type TouchList");
    }, "touch list is a TouchList object");
    [
        ["unsigned long", "length"],
        ["function", "item"],
    ].forEach(function(attr) {
        var type = attr[0];
        var name = attr[1];

        // existence check
        test(function() {
            assert_true(name in tl, name + " attribute in TouchList");
        }, "TouchList." + name + " attribute exists");

        // type check
        switch (type) {
            case "unsigned long":
                test(function() {
                    assert_equals(typeof tl[name], "number", name + " attribute of type long");
                }, "TouchList." + name + " attribute is of type number (unsigned long)");
                break;
            case "function":
                test(function() {
                    assert_equals(typeof tl[name], "function", name + " attribute of type function");
                }, "TouchList." + name + " attribute is of type function");
                break;
            default:
                break;
        }
    });
    // Each member of tl should be a proper Touch object
    for (var i = 0; i < tl.length; i++) {
        check_Touch_object(tl.item(i));
    }
    // TouchList.item(x) should return null if x is >= TouchList.length
    test(function() {
        var t = tl.item(tl.length);
        assert_equals(t, null, "TouchList.item(TouchList.length) must return null");
    }, "TouchList.item returns null if the index is >= the length of the list");
}

// Check a TouchEvent event's attributes for existence and proper type
// Also check that each of the event's TouchList objects are valid
// TA: 1.{3,4,5}.1.1, 1.{3,4,5}.1.2
function check_TouchEvent(ev) {
    test(function() {
        assert_true(ev instanceof TouchEvent, "event is a TouchEvent event");
    }, ev.type + " event is a TouchEvent event");
    [
        ["TouchList", "touches"],
        ["TouchList", "targetTouches"],
        ["TouchList", "changedTouches"],
        ["boolean", "altKey"],
        ["boolean", "metaKey"],
        ["boolean", "ctrlKey"],
        ["boolean", "shiftKey"],
    ].forEach(function(attr) {
        var type = attr[0];
        var name = attr[1];
        // existence check
        test(function() {
            assert_true(name in ev, name + " attribute in " + ev.type + " event");
        }, ev.type + "." + name + " attribute exists");
        // type check
        switch (type) {
            case "boolean":
                test(function() {
                    assert_equals(typeof ev[name], "boolean", name + " attribute of type boolean");
                }, ev.type + "." + name + " attribute is of type boolean");
                break;
            case "TouchList":
                test(function() {
                    assert_equals(Object.prototype.toString.call(ev[name]), "[object TouchList]", name + " attribute of type TouchList");
                }, ev.type + "." + name + " attribute is of type TouchList");
                break;
            default:
                break;
        }
    });
}
