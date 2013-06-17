description(
'This test checks createLinearGradient with infinite values'
);

var ctx = document.createElement('canvas').getContext('2d');

shouldThrow("ctx.createLinearGradient(0, 0, 100, NaN)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createLinearGradient(0, 0, 100, Infinity)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createLinearGradient(0, 0, 100, -Infinity)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createLinearGradient(0, 0, NaN, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createLinearGradient(0, 0, Infinity, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createLinearGradient(0, 0, -Infinity, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createLinearGradient(0, NaN, 100, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createLinearGradient(0, Infinity, 100, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createLinearGradient(0, -Infinity, 100, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createLinearGradient(NaN, 0, 100, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createLinearGradient(Infinity, 0, 100, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
shouldThrow("ctx.createLinearGradient(-Infinity, 0, 100, 100)", "'NotSupportedError: The implementation did not support the requested type of object or operation.'");
