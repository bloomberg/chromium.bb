description("Tests the acceptable types for arguments to navigator.getUserMedia methods.");

function test(expression, expressionShouldThrow, expectedException) {
    if (expressionShouldThrow) {
        if (expectedException)
            shouldThrow(expression, '"' + expectedException + '"');
        else
            shouldThrow(expression, '"TypeError: Failed to execute \'webkitGetUserMedia\' on \'Navigator\': 2 arguments required, but only 1 present."');
    } else {
        shouldNotThrow(expression);
    }
}

var notSupportedError = 'NotSupportedError: The implementation did not support the requested type of object or operation.';
var typeErrorArg2 = 'TypeError: Failed to execute \'webkitGetUserMedia\' on \'Navigator\': The callback provided as parameter 2 is not a function.';
var typeErrorArg3 = 'TypeError: Failed to execute \'webkitGetUserMedia\' on \'Navigator\': The callback provided as parameter 3 is not a function.';
var typeNotAnObjectError = new TypeError('Not an object.');

var emptyFunction = function() {};

// No arguments
test('navigator.webkitGetUserMedia()', true, 'TypeError: Failed to execute \'webkitGetUserMedia\' on \'Navigator\': 2 arguments required, but only 0 present.');

// 1 Argument (getUserMedia requires at least 2 arguments).
test('navigator.webkitGetUserMedia(undefined)', true);
test('navigator.webkitGetUserMedia(null)', true);
test('navigator.webkitGetUserMedia({ })', true);
test('navigator.webkitGetUserMedia({video: true})', true);
test('navigator.webkitGetUserMedia(true)', true);
test('navigator.webkitGetUserMedia(42)', true);
test('navigator.webkitGetUserMedia(Infinity)', true);
test('navigator.webkitGetUserMedia(-Infinity)', true);
test('navigator.webkitGetUserMedia(emptyFunction)', true);

// 2 Arguments.
test('navigator.webkitGetUserMedia({video: true}, emptyFunction)', false);
test('navigator.webkitGetUserMedia(undefined, emptyFunction)', true, notSupportedError);
test('navigator.webkitGetUserMedia(null, emptyFunction)', true, notSupportedError);
test('navigator.webkitGetUserMedia({ }, emptyFunction)', true, notSupportedError);
test('navigator.webkitGetUserMedia(true, emptyFunction)', true, typeNotAnObjectError);
test('navigator.webkitGetUserMedia(42, emptyFunction)', true, typeNotAnObjectError);
test('navigator.webkitGetUserMedia(Infinity, emptyFunction)', true, typeNotAnObjectError);
test('navigator.webkitGetUserMedia(-Infinity, emptyFunction)', true, typeNotAnObjectError);
test('navigator.webkitGetUserMedia(emptyFunction, emptyFunction)', true, notSupportedError);
test('navigator.webkitGetUserMedia({video: true}, "foobar")', true, typeErrorArg2);
test('navigator.webkitGetUserMedia({video: true}, undefined)', true, typeErrorArg2);
test('navigator.webkitGetUserMedia({video: true}, null)', true, typeErrorArg2);
test('navigator.webkitGetUserMedia({video: true}, {})', true, typeErrorArg2);
test('navigator.webkitGetUserMedia({video: true}, true)', true, typeErrorArg2);
test('navigator.webkitGetUserMedia({video: true}, 42)', true, typeErrorArg2);
test('navigator.webkitGetUserMedia({video: true}, Infinity)', true, typeErrorArg2);
test('navigator.webkitGetUserMedia({video: true}, -Infinity)', true, typeErrorArg2);

// 3 Arguments.
test('navigator.webkitGetUserMedia({ }, emptyFunction, emptyFunction)', true, notSupportedError);
test('navigator.webkitGetUserMedia({video: true}, emptyFunction, emptyFunction)', false);
test('navigator.webkitGetUserMedia({video: true}, emptyFunction, undefined)', false);
test('navigator.webkitGetUserMedia({audio:true, video:true}, emptyFunction, undefined)', false);
test('navigator.webkitGetUserMedia({audio:true}, emptyFunction, undefined)', false);
test('navigator.webkitGetUserMedia({video: true}, emptyFunction, "video")', true, typeErrorArg3);
test('navigator.webkitGetUserMedia({video: true}, emptyFunction, null)', false);
test('navigator.webkitGetUserMedia({video: true}, emptyFunction, {})', true, typeErrorArg3);
test('navigator.webkitGetUserMedia({video: true}, emptyFunction, true)', true, typeErrorArg3);
test('navigator.webkitGetUserMedia({video: true}, emptyFunction, 42)', true, typeErrorArg3);
test('navigator.webkitGetUserMedia({video: true}, emptyFunction, Infinity)', true, typeErrorArg3);
test('navigator.webkitGetUserMedia({video: true}, emptyFunction, -Infinity)', true, typeErrorArg3);

window.jsTestIsAsync = false;
