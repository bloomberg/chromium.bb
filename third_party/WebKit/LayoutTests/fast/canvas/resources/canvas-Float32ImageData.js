function setF16Color(float32ImageData, i, f16Color) {
  var s = i * 4;
  float32ImageData[s] = f16Color[0];
  float32ImageData[s + 1] = f16Color[1];
  float32ImageData[s + 2] = f16Color[2];
  float32ImageData[s + 3] = f16Color[3];
}

function getF16Color(float32ImageData, i) {
  var result = [];
  var s = i * 4;
  for (var j = 0; j < 4; j++) {
    result[j] = float32ImageData[s + j];
  }
  return result;
}

function compareF16Colors(f16Color1, f16Color2) {
  for (var j = 0; j < 4; j++)
    if (f16Color1[j] != f16Color2[j])
      return false;
  return true;
}

test(function() {

  float32ImageData = new Float32ImageData(100, 50);
  assert_equals(float32ImageData.width, 100,
      "The width of the float32ImageData should be the width we passed to the constructor.");
  assert_equals(float32ImageData.height, 50,
      "The height of the float32ImageData should be the height we passed to the constructor.");

  var f16ColorData = getF16Color(float32ImageData.data, 4);
  assert_equals(f16ColorData[0], 0,
      "The default ImageData color is transparent black.");
  assert_equals(f16ColorData[1], 0,
      "The default ImageData color is transparent black.");
  assert_equals(f16ColorData[2], 0,
      "The default ImageData color is transparent black.");
  assert_equals(f16ColorData[3], 0,
      "The default ImageData color is transparent black.");

  var testColor = new Float32Array([0x00003c00, 0x00003c01, 0x00003c02, 0x00003c03]);
  setF16Color(float32ImageData.data, 4, testColor);
  f16ColorData = getF16Color(float32ImageData.data, 4);
  assert_equals(f16ColorData[0], 0x00003c00,
      "The red component of f16 color is the value we set.");
  assert_equals(f16ColorData[1], 0x00003c01,
      "The green component of f16 color is the value we set.");
  assert_equals(f16ColorData[2], 0x00003c02,
      "The blue component of f16 color is the value we set.");
  assert_equals(f16ColorData[3], 0x00003c03,
      "The alpha component of f16 color is the value we set.");

  assert_throws(null, function() {new Float32ImageData(10);},
      "Float32ImageData constructor requires both width and height.");
  assert_throws("IndexSizeError", function() {new Float32ImageData(0,10);},
      "Float32ImageData width must be greater than zero.");
  assert_throws("IndexSizeError", function() {new Float32ImageData(10,0);},
      "Float32ImageData height must be greater than zero.");
  assert_throws("IndexSizeError", function() {new Float32ImageData('width', 'height');},
      "Float32ImageData width and height must be numbers.");
  assert_throws("IndexSizeError", function() {new Float32ImageData(1 << 31, 1 << 31);},
      "Float32ImageData width multiplied by height must be less than 2^30 to avoid buffer size overflow.");

  assert_throws(null, function() {new Float32ImageData(new Float32Array(0));},
      "The Float32Array passed to the constructor cannot have a length of zero.");
  assert_throws("IndexSizeError", function() {new Float32ImageData(new Float32Array(27), 2);},
      "The size of Float32Array passed to the constructor must be divisible by 4 * width.");
  assert_throws("IndexSizeError", function() {new Float32ImageData(new Float32Array(28), 7, 0);},
      "The size of Float32Array passed to the constructor must be equal to 4 * width * height.");
  assert_throws(null, function() {new Float32ImageData(self, 4, 4);},
      "The object passed to the constructor as data array should be of type Float32Array.");
  assert_throws(null, function() {new Float32ImageData(null, 4, 4);},
      "The object passed to the constructor as data array should be of type Float32Array.");
  assert_throws("IndexSizeError", function() {new Float32ImageData(float32ImageData.data, 0);},
      "The size of Float32Array passed to the constructor must be divisible by 4 * width.");
  assert_throws("IndexSizeError", function() {new Float32ImageData(float32ImageData.data, 13);},
      "The size of Float32Array passed to the constructor must be divisible by 4 * width.");
  assert_throws("IndexSizeError", function() {new Float32ImageData(float32ImageData.data, 1 << 31);},
      "The size of Float32Array passed to the constructor must be divisible by 4 * width.");
  assert_throws("IndexSizeError", function() {new Float32ImageData(float32ImageData.data, 'biggish');},
      "The width parameter must be a number.");
  assert_throws("IndexSizeError", function() {new Float32ImageData(float32ImageData.data, 1 << 24, 1 << 31);},
      "Float32ImageData width multiplied by height must be less than 2^30 to avoid buffer size overflow.");
  assert_throws("IndexSizeError", function() {new Float32ImageData(float32ImageData.data, 1 << 31, 1 << 24);},
      "Float32ImageData width multiplied by height must be less than 2^30 to avoid buffer size overflow.");
  assert_equals((new Float32ImageData(new Float32Array(28), 7)).height, 1,
      "The height must be equal to size of the data array divided by 4 * width.");

  float32ImageDataFromData = new Float32ImageData(float32ImageData.data, 100);
  assert_equals(float32ImageDataFromData.width, 100,
      "The width of the float32ImageDataFromData should be the same as that of float32ImageData.");
  assert_equals(float32ImageDataFromData.height, 50,
      "The height of the float32ImageDataFromData should be the same as that of float32ImageData.");
  assert_true(float32ImageDataFromData.data == float32ImageData.data,
      "The pixel data buffer of float32ImageDataFromData should be the same as that of float32ImageData.");
  assert_true(compareF16Colors(getF16Color(float32ImageDataFromData.data, 10),
                               getF16Color(float32ImageData.data, 10)),
      "The color of a pixel from float32ImageDataFromData should be the same as that of float32ImageData.");
  setF16Color(float32ImageData.data, 10, testColor);
  assert_true(compareF16Colors(getF16Color(float32ImageDataFromData.data, 10),
                               getF16Color(float32ImageData.data, 10)),
      "Setting the color of a pixel from float32ImageData must cascade to the same pixel of float32ImageDataFromData.");

  var data = new Float32Array(400);
  data[22] = 129;
  float32ImageDataFromData = new Float32ImageData(data, 20, 5);
  assert_equals(float32ImageDataFromData.width, 20,
      "The width of the float32ImageData should be the width we passed to the constructor.");
  assert_equals(float32ImageDataFromData.height, 5,
      "The height of the Float32ImageData must be equal to size of the Float32Array divided by 4 * width.");
  assert_true(float32ImageDataFromData.data == data,
      "The pixel data buffer of float32ImageDataFromData should be the same buffer passed to the constructor.");
  assert_true(compareF16Colors(getF16Color(float32ImageDataFromData.data, 2), getF16Color(data, 2)),
      "The pixel data of float32ImageDataFromData should be the same as that of the buffer passed to the constructor.");
  setF16Color(float32ImageDataFromData.data, 2, testColor);
  assert_true(compareF16Colors(getF16Color(float32ImageDataFromData.data, 2), getF16Color(data, 2)),
      "Setting the color of a pixel from float32ImageDataFromData must cascade to the same pixel of the buffer passed to the constructor.");

}, 'This test examines the correct behavior of Float32ImageData API.');
