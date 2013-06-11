description("Tests the properties of the exception thrown by dispatchEvent.")

var e;
try {
    document.dispatchEvent(null);
    // raises a InvalidStateError
} catch (err) {
    e = err;
}

shouldBeEqualToString("e.toString()", "Error: InvalidStateError: DOM Exception 11");
shouldBeEqualToString("Object.prototype.toString.call(e)", "[object DOMException]");
shouldBeEqualToString("Object.prototype.toString.call(e.__proto__)", "[object DOMExceptionPrototype]");
shouldBeEqualToString("e.constructor.toString()", "function DOMException() { [native code] }");
shouldBe("e.constructor", "window.DOMException");

