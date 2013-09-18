description("Test the behavior of CanvasRenderingContext2D.setShadow() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

function ExpectedNotEnoughArgumentsMessage(num) {
    return "\"TypeError: Failed to execute 'setShadow' on 'CanvasRenderingContext2D': 3 arguments required, but only " + num + " present.\"";
}

var TypeError = "TypeError: Type error";

shouldThrow("ctx.setShadow()", ExpectedNotEnoughArgumentsMessage(0));
shouldThrow("ctx.setShadow(0)", ExpectedNotEnoughArgumentsMessage(1));
shouldThrow("ctx.setShadow(0, 0)", ExpectedNotEnoughArgumentsMessage(2));
shouldBe("ctx.setShadow(0, 0, 0)", "undefined");
shouldBe("ctx.setShadow(0, 0, 0, 0)", "undefined");
shouldBe("ctx.setShadow(0, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.setShadow(0, 0, 0, 0, 0, 0)", "TypeError");
shouldBe("ctx.setShadow(0, 0, 0, 0, 'red')", "undefined");
shouldThrow("ctx.setShadow(0, 0, 0, 0, 'red', 0)", "TypeError");
shouldBe("ctx.setShadow(0, 0, 0, 0, 'red', 0, 0)", "undefined");
shouldThrow("ctx.setShadow(0, 0, 0, 0, 0, 0)", "TypeError");
shouldBe("ctx.setShadow(0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.setShadow(0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.setShadow(0, 0, 0, 0, 0, 0, 0, 0, 0)", "TypeError");
