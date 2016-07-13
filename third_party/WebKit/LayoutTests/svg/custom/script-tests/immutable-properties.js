description("Tests whether immutable properties can not be modified.");

var svgDoc = document.implementation.createDocument("http://www.w3.org/2000/svg", "svg", null);

// 'viewport' attribute is immutable (Spec: The object itself and its contents are both readonly.)
var viewport = svgDoc.documentElement.viewport;

shouldBe("viewport.x", "0");
viewport.x = 100;

shouldBe("viewport.x", "100");
shouldBe("svgDoc.documentElement.viewport.x", "0");

var successfullyParsed = true;
