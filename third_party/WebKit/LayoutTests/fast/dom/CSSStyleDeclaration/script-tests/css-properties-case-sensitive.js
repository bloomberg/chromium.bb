description(
'This test checks that access to CSS properties via JavaScript properties on DOM elements is case sensitive.'
);

var element = document.createElement('a');
element.style.zIndex = 1;

debug('normal cases');
debug('');

shouldBe("element.style.zIndex", "'1'");
shouldBeUndefined("element.style.ZIndex");

debug('');
debug('"css" prefix');
debug('');

shouldBe("element.style.cssZIndex", "'1'");
shouldBe("element.style.CssZIndex", "'1'");
shouldBeUndefined("element.style.CsszIndex");
shouldBeUndefined("element.style.csszIndex");
