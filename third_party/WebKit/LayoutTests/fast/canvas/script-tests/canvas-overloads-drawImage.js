description("Test the behavior of CanvasRenderingContext2D.drawImage() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

function ExpectedNotEnoughArgumentsMessage(num) {
    return "\"TypeError: Failed to execute 'drawImage' on 'CanvasRenderingContext2D': 3 arguments required, but only " + num + " present.\"";
}

var TypeError = "TypeError: Failed to execute 'drawImage' on 'CanvasRenderingContext2D': No function was found that matched the signature provided.";

var imageElement = document.createElement("img");
shouldThrow("ctx.drawImage()", ExpectedNotEnoughArgumentsMessage(0));
shouldThrow("ctx.drawImage(imageElement)", ExpectedNotEnoughArgumentsMessage(1));
shouldThrow("ctx.drawImage(imageElement, 0)", ExpectedNotEnoughArgumentsMessage(2));
shouldBe("ctx.drawImage(imageElement, 0, 0)", "undefined");
shouldThrow("ctx.drawImage(imageElement, 0, 0, 0)", "TypeError");
shouldBe("ctx.drawImage(imageElement, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.drawImage(imageElement, 0, 0, 0, 0, 0)", "TypeError");
shouldThrow("ctx.drawImage(imageElement, 0, 0, 0, 0, 0, 0)", "TypeError");
shouldThrow("ctx.drawImage(imageElement, 0, 0, 0, 0, 0, 0, 0)", "TypeError");
shouldBe("ctx.drawImage(imageElement, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.drawImage(imageElement, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "TypeError");
shouldThrow("ctx.drawImage(imageElement, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "TypeError");

var canvasElement = document.createElement("canvas");
shouldThrow("ctx.drawImage(canvasElement)", ExpectedNotEnoughArgumentsMessage(1));
shouldThrow("ctx.drawImage(canvasElement, 0)", ExpectedNotEnoughArgumentsMessage(2));
shouldBe("ctx.drawImage(canvasElement, 0, 0)", "undefined");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0)", "TypeError");
shouldBe("ctx.drawImage(canvasElement, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0)", "TypeError");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0)", "TypeError");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0, 0)", "TypeError");
shouldBe("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "TypeError");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "TypeError");
