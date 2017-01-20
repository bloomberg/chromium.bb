importScripts("../../resources/testharness.js");

onmessage = function(e) {
  var detector;
  switch (e.data.detectorType) {
    case "Face": detector = new FaceDetector(); break;
    case "Barcode": detector = new BarcodeDetector(); break;
    case "Text": detector = new TextDetector(); break;
  }

  var imageBitmap = e.data.bitmap;
  detector.detect(imageBitmap)
      .then(detectionResult => {
        assert_equals(detectionResult.length, e.data.expectedLength,
                      "Number of " + e.data.detectorType);
        postMessage("PASS");
      })
      .catch(error => {
        assert_unreached("Error during detect(img): " + error);
      });
}
