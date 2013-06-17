description("Test the argument bounds of canvas createImageData.");

var canvas = document.getElementById('canvas');
var ctx = canvas.getContext('2d');

shouldThrow("ctx.createImageData(Infinity, Infinity)", '"NotSupportedError: The implementation did not support the requested type of object or operation."');
shouldThrow("ctx.createImageData(Infinity, 10)", '"NotSupportedError: The implementation did not support the requested type of object or operation."');
shouldThrow("ctx.createImageData(-Infinity, 10)", '"NotSupportedError: The implementation did not support the requested type of object or operation."');
shouldThrow("ctx.createImageData(10, Infinity)", '"NotSupportedError: The implementation did not support the requested type of object or operation."');
shouldThrow("ctx.createImageData(10, -Infinity)", '"NotSupportedError: The implementation did not support the requested type of object or operation."');
shouldThrow("ctx.createImageData(NaN, 10)", '"NotSupportedError: The implementation did not support the requested type of object or operation."');
shouldThrow("ctx.createImageData(10, NaN)", '"NotSupportedError: The implementation did not support the requested type of object or operation."');
