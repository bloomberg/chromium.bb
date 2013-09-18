description("Test the behavior of CanvasRenderingContext2D.strokeText() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

function ExpectedMessage(num) {
    return "\"TypeError: Failed to execute 'strokeText' on 'CanvasRenderingContext2D': 3 arguments required, but only " + num + " present.\"";
}

shouldThrow("ctx.strokeText()", ExpectedMessage(0));
shouldThrow("ctx.strokeText('moo')", ExpectedMessage(1));
shouldThrow("ctx.strokeText('moo',0)", ExpectedMessage(2));
shouldBe("ctx.strokeText('moo',0,0)", "undefined");
shouldBe("ctx.strokeText('moo',0,0,0)", "undefined");
shouldBe("ctx.strokeText('moo',0,0,0,0)", "undefined");
