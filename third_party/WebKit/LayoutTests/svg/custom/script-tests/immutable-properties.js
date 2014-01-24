description("Tests whether immutable properties can not be modified.");

var svgDoc = document.implementation.createDocument("http://www.w3.org/2000/svg", "svg", null);

// 'viewport' attribute is immutable (Spec: The object itself and its contents are both readonly.)
var viewport = svgDoc.documentElement.viewport;

shouldBe("viewport.x", "0");
viewport.x = 100;

shouldBe("viewport.x", "100");
shouldBe("svgDoc.documentElement.viewport.x", "0");

// Every attribute of SVGZoomEvent is immutable (Spec: The object itself and its contents are both readonly.)
var zoomEvent = svgDoc.createEvent("SVGZoomEvents");

// 'zoomRectScreen' property
var zoomRectScreen = zoomEvent.zoomRectScreen;

shouldBe("zoomRectScreen.x", "0");
shouldThrow("zoomRectScreen.x = 100;");

// 'previousScale' property
shouldBe("zoomEvent.previousScale", "0")
zoomEvent.previousScale = 200;
shouldBe("zoomEvent.previousScale", "0")

// 'previousTranslate' property
var previousTranslate = zoomEvent.previousTranslate;

shouldBe("previousTranslate.x", "0");
shouldThrow("previousTranslate.x = 300;");

shouldBe("zoomEvent.previousTranslate.x", "0");

// 'newScale' property
shouldBe("zoomEvent.newScale", "0");
shouldThrow("zoomEvent.newScale = 200;");
shouldBe("zoomEvent.newScale", "0");

// 'newTranslate' property
var newTranslate = zoomEvent.newTranslate;

shouldBe("newTranslate.x", "0");
shouldThrow("newTranslate.x = 300;");
shouldBe("newTranslate.x", "0");

var successfullyParsed = true;
