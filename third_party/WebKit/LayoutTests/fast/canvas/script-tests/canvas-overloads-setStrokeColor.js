description("Test the behavior of CanvasRenderingContext2D.setStrokeColor() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

function ExpectedNotEnoughArgumentsMessage(num) {
    return "\"TypeError: Failed to execute 'setStrokeColor' on 'CanvasRenderingContext2D': 1 argument required, but only " + num + " present.\"";
}

var TypeError = "TypeError: Type error";

shouldThrow("ctx.setStrokeColor()", ExpectedNotEnoughArgumentsMessage(0));
shouldBe("ctx.setStrokeColor('red')", "undefined");
shouldBe("ctx.setStrokeColor(0)", "undefined");
shouldBe("ctx.setStrokeColor(0, 0)", "undefined");
shouldThrow("ctx.setStrokeColor(0, 0, 0)", "TypeError");
shouldBe("ctx.setStrokeColor(0, 0, 0, 0)", "undefined");
shouldBe("ctx.setStrokeColor(0, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.setStrokeColor(0, 0, 0, 0, 0, 0)", "TypeError");
