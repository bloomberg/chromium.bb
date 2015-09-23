description("Test the behavior of CanvasRenderingContext2D.fillText() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

function ExpectedNotEnoughArgumentsMessage(num) {
    return "\"TypeError: Failed to execute 'fillText' on 'CanvasRenderingContext2D': 3 arguments required, but only " + num + " present.\"";
}

shouldThrow("ctx.fillText()", ExpectedNotEnoughArgumentsMessage(0));
shouldThrow("ctx.fillText('moo')", ExpectedNotEnoughArgumentsMessage(1));
shouldThrow("ctx.fillText('moo',0)", ExpectedNotEnoughArgumentsMessage(2));
shouldBe("ctx.fillText('moo',0,0)", "undefined");
shouldBe("ctx.fillText('moo',0,0,0)", "undefined");
shouldBe("ctx.fillText('moo',0,0,0,0)", "undefined");
