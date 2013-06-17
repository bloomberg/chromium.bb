description("Test the handling of invalid arguments in canvas getImageData().");

ctx = document.createElement('canvas').getContext('2d');

shouldThrow("ctx.getImageData(NaN, 10, 10, 10)", '"NotSupportedError: The implementation did not support the requested type of object or operation."');
shouldThrow("ctx.getImageData(10, NaN, 10, 10)", '"NotSupportedError: The implementation did not support the requested type of object or operation."');
shouldThrow("ctx.getImageData(10, 10, NaN, 10)", '"NotSupportedError: The implementation did not support the requested type of object or operation."');
shouldThrow("ctx.getImageData(10, 10, 10, NaN)", '"NotSupportedError: The implementation did not support the requested type of object or operation."');
shouldThrow("ctx.getImageData(Infinity, 10, 10, 10)", '"NotSupportedError: The implementation did not support the requested type of object or operation."');
shouldThrow("ctx.getImageData(10, Infinity, 10, 10)", '"NotSupportedError: The implementation did not support the requested type of object or operation."');
shouldThrow("ctx.getImageData(10, 10, Infinity, 10)", '"NotSupportedError: The implementation did not support the requested type of object or operation."');
shouldThrow("ctx.getImageData(10, 10, 10, Infinity)", '"NotSupportedError: The implementation did not support the requested type of object or operation."');
shouldThrow("ctx.getImageData(undefined, 10, 10, 10)", '"NotSupportedError: The implementation did not support the requested type of object or operation."');
shouldThrow("ctx.getImageData(10, undefined, 10, 10)", '"NotSupportedError: The implementation did not support the requested type of object or operation."');
shouldThrow("ctx.getImageData(10, 10, undefined, 10)", '"NotSupportedError: The implementation did not support the requested type of object or operation."');
shouldThrow("ctx.getImageData(10, 10, 10, undefined)", '"NotSupportedError: The implementation did not support the requested type of object or operation."');
shouldThrow("ctx.getImageData(10, 10, 0, 10)", '"IndexSizeError: Index or size was negative, or greater than the allowed value."');
shouldThrow("ctx.getImageData(10, 10, 10, 0)", '"IndexSizeError: Index or size was negative, or greater than the allowed value."');
