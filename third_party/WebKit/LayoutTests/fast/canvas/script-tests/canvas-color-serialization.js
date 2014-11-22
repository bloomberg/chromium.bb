description("Test that getting color properties from a CanvasRenderingContext2D returns properly formatted values.");

ctx = document.createElement('canvas').getContext('2d');

function trySettingStrokeStyle(value) {
    ctx.strokeStyle = '#666';
    ctx.strokeStyle = value;
    return ctx.strokeStyle;
}

function trySettingFillStyle(value) {
    ctx.fillStyle = '#666';
    ctx.fillStyle = value;
    return ctx.fillStyle;
}

function trySettingShadowColor(value) {
    ctx.shadowColor = '#666';
    ctx.shadowColor = value;
    return ctx.shadowColor;
}

function trySettingColor(value, expected) {
    shouldBe("trySettingStrokeStyle(" + value + ")", expected);
    shouldBe("trySettingFillStyle(" + value + ")", expected);
    shouldBe("trySettingShadowColor(" + value + ")", expected);
}

function checkDefaultValue(value) {
    return value;
}

shouldBe("checkDefaultValue(ctx.strokeStyle)", "'#000000'");
shouldBe("checkDefaultValue(ctx.fillStyle)", "'#000000'");
shouldBe("checkDefaultValue(ctx.shadowColor)", "'rgba(0, 0, 0, 0)'");

trySettingColor("'transparent'", "'rgba(0, 0, 0, 0)'");
trySettingColor("'red'", "'#ff0000'");
trySettingColor("'white'", "'#ffffff'");
trySettingColor("''", "'#666666'");
trySettingColor("'RGBA(0, 0, 0, 0)'", "'rgba(0, 0, 0, 0)'");
trySettingColor("'rgba(0,255,0,1.0)'", "'#00ff00'");
trySettingColor("'rgba(1,2,3,0.4)'", "'rgba(1, 2, 3, 0.4)'");
trySettingColor("'RgB(1,2,3)'", "'#010203'");
trySettingColor("'rGbA(1,2,3,0)'", "'rgba(1, 2, 3, 0)'");
trySettingColor("true", "'#666666'");
trySettingColor("false", "'#666666'");
trySettingColor("0", "'#666666'");
trySettingColor("1", "'#666666'");
trySettingColor("-1", "'#666666'");
trySettingColor("NaN", "'#666666'");
trySettingColor("Infinity", "'#666666'");
trySettingColor("null", "'#666666'");

