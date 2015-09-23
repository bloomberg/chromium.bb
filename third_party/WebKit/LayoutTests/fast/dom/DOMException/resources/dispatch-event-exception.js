description("Tests the properties of the exception thrown by dispatchEvent.")

var e;
try {
    document.dispatchEvent(null);
    // raises a InvalidStateError
} catch (err) {
    e = err;
}

shouldBeEqualToString("e.toString()", "InvalidStateError: Failed to execute 'dispatchEvent' on 'EventTarget': The event provided is null.");
shouldBeEqualToString("Object.prototype.toString.call(e)", "[object DOMException]");
shouldBeEqualToString("Object.prototype.toString.call(e.__proto__)", "[object DOMExceptionPrototype]");
shouldBeEqualToString("e.constructor.toString()", "function DOMException() { [native code] }");
shouldBe("e.constructor", "window.DOMException");

