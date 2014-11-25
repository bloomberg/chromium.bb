function loadJson(path) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", path, false);
    xhr.send(null);
    return JSON.parse(xhr.responseText);
}

function ColorTest(testPath, isSingleByte) {
    this.testData = loadJson(testPath);
    this.expectedValuesAreFloats = !isSingleByte;
    this.parentColor = "rgb(45, 23, 27)";
    description("Color test for " + testPath);
}

ColorTest.prototype.parseColor = function(cssText) {
    var failSentinal = "rgb(12, 34, 223)"; // Random value not in our test data.
    this.testElement.setAttribute('style', "color: " + failSentinal + "; color: " + cssText);
    var result = window.getComputedStyle(this.testElement, null)['color'];
    if (result == failSentinal)
        return "parse error";
    if (result == this.parentColor)
        return "currentColor";
    return result;
}

ColorTest.prototype.expectedString = function(expectedValue) {
    if (expectedValue == null)
        return "parse error"
    if (expectedValue.length != 4)
        return expectedValue;
    var rgbaValues = expectedValue;
    if (this.expectedValuesAreFloats) {
        // FIXME: We should not have to Math.floor these values!
        rgbaValues = rgbaValues.map(function(decimal) { return Math.floor(decimal * 255); });
    }
    var rgbString = rgbaValues.slice(0, 3).join(", ");
    if (expectedValue[3] != 1) {
        // For whatever reason, the alpha from blink is a float!?
        return "rgba(" + rgbString + ", " + expectedValue[3] + ")";
    }
    return "rgb(" + rgbString + ")";
}

// Mozilla has something like this built in, this is just a hack.
function escapeString(string) {
    return (string + '').replace(/[\\"']/g, '\\$&').replace(/\u0000/g, '\\0').replace(/\n/g, '\\n');
}

ColorTest.prototype.run = function() {
    setPrintTestResultsLazily();

    // This is a hack to make getComputedstyle work.
    this.parentElement = document.createElement("foo");
    this.testElement = document.createElement("bar");
    this.parentElement.appendChild(this.testElement);
    this.parentElement.style.color = this.parentColor;
    document.documentElement.appendChild(this.parentElement);
    window.colorTest = this; // Make shouldBe easier to read.

    for (var i = 0; i < this.testData.length; i += 2) {
        var testCSS = this.testData[i];
        var expectedColor = this.testData[i + 1];
        shouldBeEqualToString("colorTest.parseColor(\"" +  escapeString(testCSS) + "\")", this.expectedString(expectedColor));
    }
    this.testElement.parentNode.removeChild(this.testElement);
}
