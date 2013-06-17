description("Tests the properties of the exception thrown by dispatchEvent.")

var e;
try {
    document.dispatchEvent(null);
    // raises a InvalidStateError
} catch (err) {
    e = err;
}

shouldBeEqualToString("e.toString()", "InvalidStateError: An attempt was made to use an object that is not, or is no longer, usable.");
shouldBeEqualToString("Object.prototype.toString.call(e)", "[object DOMException]");
shouldBeEqualToString("Object.prototype.toString.call(e.__proto__)", "[object DOMExceptionPrototype]");
shouldBeEqualToString("e.constructor.toString()", "function DOMException() { [native code] }");
shouldBe("e.constructor", "window.DOMException");

