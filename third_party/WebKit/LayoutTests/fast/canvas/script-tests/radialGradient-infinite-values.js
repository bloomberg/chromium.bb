description(
'This test checks createRadialGradient with infinite values'
);

var ctx = document.createElement('canvas').getContext('2d');

shouldThrow("ctx.createRadialGradient(0, 0, 100, 0, 0, NaN)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, 0, 0, Infinity)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, 0, 0, -Infinity)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, 0, NaN, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, 0, Infinity, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, 0, -Infinity, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, NaN, 0, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, Infinity, 0, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createRadialGradient(0, 0, 100, -Infinity, 0, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createRadialGradient(0, 0, NaN, 0, 0, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createRadialGradient(0, 0, Infinity, 0, 0, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createRadialGradient(0, 0, -Infinity, 0, 0, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createRadialGradient(0, NaN, 100, 0, 0, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createRadialGradient(0, Infinity, 100, 0, 0, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createRadialGradient(0, -Infinity, 100, 0, 0, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createRadialGradient(NaN, 0, 100, 0, 0, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createRadialGradient(Infinity, 0, 100, 0, 0, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createRadialGradient(-Infinity, 0, 100, 0, 0, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
