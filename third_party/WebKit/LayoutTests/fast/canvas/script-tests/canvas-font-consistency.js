descriptionQuiet("Series of tests to ensure that context.font is consistent.");

var canvas = document.createElement('canvas');
var ctx = canvas.getContext('2d');
var fontNames = ["bold 13px Arial",
                 "italic 13px Arial",
                 "small-caps 13px Arial"];

for (var i = 0; i < fontNames.length; i++) {
    var fontName = fontNames[i];
    debug('Testing font "' + fontName + '"');
    ctx.font = fontName;
    shouldBe("ctx.font", "fontName");
}