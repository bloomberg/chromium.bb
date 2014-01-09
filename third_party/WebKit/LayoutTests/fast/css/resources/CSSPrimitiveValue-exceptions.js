description("This tests that the methods on CSSPrimitiveValue throw exceptions ");

div = document.createElement('div');
div.style.width = "10px";
div.style.height = "90%";
div.style.content = "counter(dummy, square)";
div.style.clip = "rect(0, 0, 1, 1)";
div.style.color = "rgb(0, 0, 0)";

// Test passing invalid unit to getFloatValue
shouldThrow("div.style.getPropertyCSSValue('width').getFloatValue(CSSPrimitiveValue.CSS_UNKNOWN)");
shouldThrow("div.style.getPropertyCSSValue('width').getFloatValue(CSSPrimitiveValue.CSS_STRING)");

// Test invalid unit conversions in getFloatValue
shouldThrow("div.style.getPropertyCSSValue('width').getFloatValue(CSSPrimitiveValue.CSS_HZ)");
shouldThrow("div.style.getPropertyCSSValue('width').getFloatValue(CSSPrimitiveValue.CSS_S)");
shouldThrow("div.style.getPropertyCSSValue('width').getFloatValue(CSSPrimitiveValue.CSS_RAD)");
shouldThrow("div.style.getPropertyCSSValue('width').getFloatValue(CSSPrimitiveValue.CSS_PERCENTAGE)");
shouldThrow("div.style.getPropertyCSSValue('height').getFloatValue(CSSPrimitiveValue.CSS_PX)");
shouldThrow("div.style.getPropertyCSSValue('height').getFloatValue(CSSPrimitiveValue.CSS_DEG)");

// Test calling get*Value for CSSPrimitiveValue of the wrong type
shouldBe("div.style.getPropertyCSSValue('clip').primitiveType", "CSSPrimitiveValue.CSS_RECT");
shouldThrow("div.style.getPropertyCSSValue('clip').getFloatValue(CSSPrimitiveValue.CSS_PX)");
shouldThrow("div.style.getPropertyCSSValue('clip').getStringValue()");
shouldThrow("div.style.getPropertyCSSValue('clip').getCounterValue()");
shouldThrow("div.style.getPropertyCSSValue('clip').getRGBColorValue()");

shouldBe("div.style.getPropertyCSSValue('color').primitiveType", "CSSPrimitiveValue.CSS_RGBCOLOR");
shouldThrow("div.style.getPropertyCSSValue('color').getFloatValue(CSSPrimitiveValue.CSS_PX)");
shouldThrow("div.style.getPropertyCSSValue('color').getStringValue()");
shouldThrow("div.style.getPropertyCSSValue('color').getCounterValue()");
shouldThrow("div.style.getPropertyCSSValue('color').getRectValue()");
