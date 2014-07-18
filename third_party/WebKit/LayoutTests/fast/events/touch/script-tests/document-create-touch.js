description("This tests support for the document.createTouch API.");

shouldBeTrue('"createTouch" in document');

var box = document.createElement("div");
box.id = "box";
box.style.width = "100px";
box.style.height = "100px";
document.body.appendChild(box);

var target = document.getElementById("box");
var touch = document.createTouch(window, target, 1, 100, 101, 102, 103, 5, 3, 10, 10);
shouldBeNonNull("touch");
shouldBe("touch.target", "box");
shouldBe("touch.identifier", "1");
shouldBe("touch.pageX", "100");
shouldBe("touch.pageY", "101");
shouldBe("touch.screenX", "102");
shouldBe("touch.screenY", "103");
shouldBe("touch.radiusX", "5");
shouldBe("touch.radiusY", "3");
shouldBe("touch.force", "10");
shouldBe("touch.webkitRadiusX", "5");
shouldBe("touch.webkitRadiusY", "3");
shouldBe("touch.webkitRotationAngle", "10");
shouldBe("touch.webkitForce", "10");

var emptyTouch = document.createTouch();
shouldBeNonNull("emptyTouch");
shouldBeNull("emptyTouch.target");
shouldBe("emptyTouch.identifier", "0");
shouldBe("emptyTouch.pageX", "0");
shouldBe("emptyTouch.pageY", "0");
shouldBe("emptyTouch.screenX", "0");
shouldBe("emptyTouch.screenY", "0");
shouldBe("emptyTouch.radiusX", "0");
shouldBe("emptyTouch.radiusY", "0");
shouldBeNaN("emptyTouch.force");
shouldBe("emptyTouch.webkitRadiusX", "0");
shouldBe("emptyTouch.webkitRadiusY", "0");
shouldBeNaN("emptyTouch.webkitRotationAngle");
shouldBeNaN("emptyTouch.webkitForce");

// Try invoking with incorrect parameter types.
var badParamsTouch = document.createTouch(function(x) { return x; }, 12, 'a', 'b', 'c', function(x) { return x; }, 104, 'a', 'b', 'c', 'd');
shouldBeNonNull("badParamsTouch");
shouldBeNull("badParamsTouch.target");
shouldBe("badParamsTouch.identifier", "0");
shouldBe("badParamsTouch.pageX", "0");
shouldBe("badParamsTouch.pageY", "0");
shouldBe("badParamsTouch.screenX", "0");
shouldBe("badParamsTouch.screenY", "104");
shouldBe("badParamsTouch.radiusX", "0");
shouldBe("badParamsTouch.radiusY", "0");
shouldBeNaN("badParamsTouch.force");
shouldBe("badParamsTouch.webkitRadiusX", "0");
shouldBe("badParamsTouch.webkitRadiusY", "0");
shouldBeNaN("badParamsTouch.webkitRotationAngle");
shouldBeNaN("badParamsTouch.webkitForce");

// Should not crash when invoked on a detached Document.
var detachedTouch;
shouldBeNonNull("detachedTouch = document.implementation.createDocument('a', 'b').createTouch()");
shouldBeNull("detachedTouch.target");
shouldBe("detachedTouch.identifier", "0");
shouldBe("detachedTouch.pageX", "0");
shouldBe("detachedTouch.pageY", "0");
shouldBe("detachedTouch.screenX", "0");
shouldBe("detachedTouch.screenY", "0");
shouldBe("detachedTouch.radiusX", "0");
shouldBe("detachedTouch.radiusY", "0");
shouldBeNaN("detachedTouch.force");
shouldBe("detachedTouch.webkitRadiusX", "0");
shouldBe("detachedTouch.webkitRadiusY", "0");
shouldBeNaN("detachedTouch.webkitRotationAngle");
shouldBeNaN("detachedTouch.webkitForce");

isSuccessfullyParsed();
