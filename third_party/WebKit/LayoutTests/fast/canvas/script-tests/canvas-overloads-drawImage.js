description("Test the behavior of CanvasRenderingContext2D.drawImage() when called with different numbers of arguments.");

var ctx = document.createElement('canvas').getContext('2d');

var imageElement = document.createElement("img");
shouldThrow("ctx.drawImage()");
shouldThrow("ctx.drawImage(imageElement)");
shouldThrow("ctx.drawImage(imageElement, 0)");
shouldBe("ctx.drawImage(imageElement, 0, 0)", "undefined");
shouldThrow("ctx.drawImage(imageElement, 0, 0, 0)");
shouldBe("ctx.drawImage(imageElement, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.drawImage(imageElement, 0, 0, 0, 0, 0)");
shouldThrow("ctx.drawImage(imageElement, 0, 0, 0, 0, 0, 0)");
shouldThrow("ctx.drawImage(imageElement, 0, 0, 0, 0, 0, 0, 0)");
shouldBe("ctx.drawImage(imageElement, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImage(imageElement, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImage(imageElement, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");

var canvasElement = document.createElement("canvas");
shouldThrow("ctx.drawImage(canvasElement)");
shouldThrow("ctx.drawImage(canvasElement, 0)");
shouldBe("ctx.drawImage(canvasElement, 0, 0)", "undefined");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0)");
shouldBe("ctx.drawImage(canvasElement, 0, 0, 0, 0)", "undefined");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0)");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0)");
shouldThrow("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0, 0)");
shouldBe("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
shouldBe("ctx.drawImage(canvasElement, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)", "undefined");
