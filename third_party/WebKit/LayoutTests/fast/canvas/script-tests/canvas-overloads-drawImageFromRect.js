description("Test the behavior of CanvasRenderingContext2D.drawImageFromRect() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

function ExpectedNotEnoughArgumentsMessage(num) {
    return "\"TypeError: Failed to execute 'drawImageFromRect' on 'CanvasRenderingContext2D': 1 argument required, but only " + num + " present.\"";
}

var imageElement = document.createElement("img");
shouldThrow("ctx.drawImageFromRect()", ExpectedNotEnoughArgumentsMessage(0));
shouldBe("ctx.drawImageFromRect(imageElement)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImageFromRect(imageElement, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
