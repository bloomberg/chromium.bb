description("Test the behavior of CanvasRenderingContext2D.setStrokeColor() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

shouldThrow("ctx.setStrokeColor()");
shouldBe("ctx.setStrokeColor('red')", "undefined");
shouldBe("ctx.setStrokeColor(0)", "undefined");
shouldBe("ctx.setStrokeColor(0, 0)", "undefined");
shouldThrow("ctx.setStrokeColor(0, 0, 0)");
shouldBe("ctx.setStrokeColor(0, 0, 0, 0)", "undefined");
shouldBe("ctx.setStrokeColor(0, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.setStrokeColor(0, 0, 0, 0, 0, 0)");
