description(
'This test checks that access to the CSS float property via JavaScript properties on DOM elements is case sensitive. The use of .CssFloat is deprecated and will be removed in M50.'
);

var element = document.createElement('a');
element.style.float = 'left';

debug('normal cases');
debug('');

shouldBe("element.style.float", "'left'");
shouldBeUndefined("element.style.Float");

debug('');
debug('"css" prefix');
debug('');

shouldBe("element.style.cssFloat", "'left'");
shouldBe("element.style.CssFloat", "'left'");
shouldBeUndefined("element.style.Cssfloat");
shouldBeUndefined("element.style.cssfloat");
