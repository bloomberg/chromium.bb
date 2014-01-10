description("Test the handling of invalid arguments in canvas getImageData().");

ctx = document.createElement('canvas').getContext('2d');

shouldThrow("ctx.getImageData(NaN, 10, 10, 10)");
shouldThrow("ctx.getImageData(10, NaN, 10, 10)");
shouldThrow("ctx.getImageData(10, 10, NaN, 10)");
shouldThrow("ctx.getImageData(10, 10, 10, NaN)");
shouldThrow("ctx.getImageData(Infinity, 10, 10, 10)");
shouldThrow("ctx.getImageData(10, Infinity, 10, 10)");
shouldThrow("ctx.getImageData(10, 10, Infinity, 10)");
shouldThrow("ctx.getImageData(10, 10, 10, Infinity)");
shouldThrow("ctx.getImageData(undefined, 10, 10, 10)");
shouldThrow("ctx.getImageData(10, undefined, 10, 10)");
shouldThrow("ctx.getImageData(10, 10, undefined, 10)");
shouldThrow("ctx.getImageData(10, 10, 10, undefined)");
shouldThrow("ctx.getImageData(10, 10, 0, 10)");
shouldThrow("ctx.getImageData(10, 10, 10, 0)");
