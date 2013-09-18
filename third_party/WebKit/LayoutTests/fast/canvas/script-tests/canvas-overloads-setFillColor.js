description("Test the behavior of CanvasRenderingContext2D.setFillColor() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

function ExpectedNotEnoughArgumentsMessage(num) {
    return "\"TypeError: Failed to execute 'setFillColor' on 'CanvasRenderingContext2D': 1 argument required, but only " + num + " present.\"";
}

var TypeError = "TypeError: Type error";

shouldThrow("ctx.setFillColor()", ExpectedNotEnoughArgumentsMessage(0));
shouldBe("ctx.setFillColor('red')", "undefined");
shouldBe("ctx.setFillColor(0)", "undefined");
shouldBe("ctx.setFillColor(0, 0)", "undefined");
shouldThrow("ctx.setFillColor(0, 0, 0)", "TypeError");
shouldBe("ctx.setFillColor(0, 0, 0, 0)", "undefined");
shouldBe("ctx.setFillColor(0, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.setFillColor(0, 0, 0, 0, 0, 0)", "TypeError");
