description("Test the behavior of CanvasRenderingContext2D.setFillColor() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

shouldThrow("ctx.setFillColor()");
shouldBe("ctx.setFillColor('red')", "undefined");
shouldBe("ctx.setFillColor(0)", "undefined");
shouldBe("ctx.setFillColor(0, 0)", "undefined");
shouldThrow("ctx.setFillColor(0, 0, 0)");
shouldBe("ctx.setFillColor(0, 0, 0, 0)", "undefined");
shouldBe("ctx.setFillColor(0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.setFillColor(0, 0, 0, 0, 0, 0)", "undefined");
