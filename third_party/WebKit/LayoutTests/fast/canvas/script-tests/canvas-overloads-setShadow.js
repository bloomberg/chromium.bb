description("Test the behavior of CanvasRenderingContext2D.setShadow() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

shouldThrow("ctx.setShadow()");
shouldThrow("ctx.setShadow(0)");
shouldThrow("ctx.setShadow(0, 0)");
shouldBe("ctx.setShadow(0, 0, 0)", "undefined");
shouldBe("ctx.setShadow(0, 0, 0, 0)", "undefined");
shouldBe("ctx.setShadow(0, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.setShadow(0, 0, 0, 0, 0, 0)");
shouldBe("ctx.setShadow(0, 0, 0, 0, 'red')", "undefined");
shouldThrow("ctx.setShadow(0, 0, 0, 0, 'red', 0)");
shouldBe("ctx.setShadow(0, 0, 0, 0, 'red', 0, 0)", "undefined");
shouldThrow("ctx.setShadow(0, 0, 0, 0, 0, 0)");
shouldBe("ctx.setShadow(0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.setShadow(0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.setShadow(0, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
