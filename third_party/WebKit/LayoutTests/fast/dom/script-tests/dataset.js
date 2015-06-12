description("This tests element.dataset.");

function testGet(attr, expected)
{
    var d = document.createElement("div");
    d.setAttribute(attr, "value");
    return d.dataset[expected] == "value";
}

shouldBeTrue("testGet('data-foo', 'foo')");
shouldBeTrue("testGet('data-foo-bar', 'fooBar')");
shouldBeTrue("testGet('data--', '-')");
shouldBeTrue("testGet('data--foo', 'Foo')");
shouldBeTrue("testGet('data---foo', '-Foo')");
shouldBeTrue("testGet('data---foo--bar', '-Foo-Bar')");
shouldBeTrue("testGet('data---foo---bar', '-Foo--Bar')");
shouldBeTrue("testGet('data-foo-', 'foo-')");
shouldBeTrue("testGet('data-foo--', 'foo--')");
shouldBeTrue("testGet('data-Foo', 'foo')"); // HTML lowercases all attributes.
shouldBeTrue("testGet('data-', '')");
shouldBeTrue("testGet('data-\xE0', '\xE0')");
shouldBeTrue("testGet('data-1', '1')");
shouldBeTrue("testGet('data-01', '01')");
shouldBeTrue("testGet('data-zx81', 'zx81')");
shouldBeTrue("testGet('data-i4770k', 'i4770k')");
shouldBeTrue("testGet('data-r-7', 'r-7')");
shouldBeTrue("testGet('data-r-7-k', 'r-7K')");
shouldBeUndefined("document.body.dataset.nonExisting");
debug("");

function testIsUndefined(attr, prop)
{
    var d = document.createElement("div");
    d.setAttribute(attr, "value");
    return d.dataset[prop] === undefined;
}

shouldBeTrue("testIsUndefined('data-022', '22')");
shouldBeTrue("testIsUndefined('data-22', '022')");

debug("");

function matchesNothingInDataset(attr)
{
    var d = document.createElement("div");
    d.setAttribute(attr, "value");

    var count = 0;
    for (var item in d.dataset)
        count++;
    return count == 0;
}

shouldBeTrue("matchesNothingInDataset('dataFoo')");
debug("");

function testSet(prop, expected)
{
    var d = document.createElement("div");
    d.dataset[prop] = "value";
    return d.getAttribute(expected) == "value";
}

shouldBeTrue("testSet('foo', 'data-foo')");
shouldBeTrue("testSet('fooBar', 'data-foo-bar')");
shouldBeTrue("testSet('-', 'data--')");
shouldBeTrue("testSet('Foo', 'data--foo')");
shouldBeTrue("testSet('-Foo', 'data---foo')");
shouldBeTrue("testSet('', 'data-')");
shouldBeTrue("testSet('\xE0', 'data-\xE0')");
shouldBeTrue("testSet('32', 'data-32')");
shouldBeTrue("testSet('0032', 'data-0032')");
shouldBeTrue("testSet('i18n', 'data-i18n')");
shouldBeTrue("testSet('d2', 'data-d2')");
shouldBeTrue("testSet('2d', 'data-2d')");
shouldBeTrue("testSet('d-2', 'data-d-2')");
shouldBeTrue("testSet('A--S', 'data--a---s')");
debug("");

function testIsNull(prop, attr)
{
    var d = document.createElement("div");
    d.dataset[prop] = "value";
    return d.getAttribute(attr) === null;
}

shouldBeTrue("testIsNull('0123', 'data-123')");
shouldBeTrue("testIsNull('123', 'data-0123')");
debug("");

shouldThrow("testSet('-foo', 'dummy')", '"SyntaxError: Failed to set the \'-foo\' property on \'DOMStringMap\': \'-foo\' is not a valid property name."');
shouldThrow("testSet('foo\x20', 'dummy')", '"InvalidCharacterError: Failed to set the \'foo\x20\' property on \'DOMStringMap\': \'data-foo\x20\' is not a valid attribute name."');
shouldThrow("testSet('foo\uF900', 'dummy')", '"InvalidCharacterError: Failed to set the \'foo\uF900\' property on \'DOMStringMap\': \'data-foo\uF900\' is not a valid attribute name."');
debug("");

function testDelete(attr, prop)
{
    var d = document.createElement("div");
    d.setAttribute(attr, "value");
    delete d.dataset[prop];
    return d.getAttribute(attr) != "value";
}

shouldBeTrue("testDelete('data-foo', 'foo')");
shouldBeTrue("testDelete('data-foo-bar', 'fooBar')");
shouldBeTrue("testDelete('data--', '-')");
shouldBeTrue("testDelete('data--foo', 'Foo')");
shouldBeTrue("testDelete('data---foo', '-Foo')");
shouldBeTrue("testDelete('data-', '')");
shouldBeTrue("testDelete('data-\xE0', '\xE0')");
shouldBeTrue("testDelete('data-33', '33')");
shouldBeTrue("testDelete('data-00033', '00033')");
shouldBeTrue("testDelete('data-r2', 'r2')");
shouldBeTrue("testDelete('data-2r', '2r')");
shouldBeTrue("testDelete('data-r-2', 'r-2')");
shouldBeTrue("testDelete('data--r-2-', 'R-2-')");
shouldBeTrue("testDelete('data--r-2r', 'R-2r')");
shouldBeTrue("testDelete('data--r-2-----r', 'R-2----R')");

// The element.dataset deleter is only applied to properties
// that are present; check that any underlying native property
// is deleted instead.
function testNativeDelete(prop, isConfigurable)
{
    var d = document.createElement("div");
    Object.defineProperty(d.dataset, prop, {configurable: isConfigurable, value: "native_value"});
    delete d.dataset[prop];
    return isConfigurable ? !(prop in d.dataset) : (d.dataset[prop] === "native_value");
}

// TODO(jochen): Reenable this once it behaves correctly
//shouldBeTrue("testNativeDelete('-r-2-', false)");
shouldBeTrue("testNativeDelete('foo', true)");

debug("");

shouldBeFalse("testDelete('dummy', '-foo')");
debug("");

function testForIn(array)
{
    var d = document.createElement("div");
    for (var i = 0; i < array.length; ++i) {
        d.setAttribute(array[i], "value");
    }

    var count = 0;
    for (var item in d.dataset)
        count++;

    return count;
}

shouldBe("testForIn(['data-foo', 'data-bar', 'data-baz'])", "3");
shouldBe("testForIn(['data-foo', 'data-bar', 'dataFoo'])", "2");
shouldBe("testForIn(['data-foo', 'data-bar', 'style'])", "2");
shouldBe("testForIn(['data-foo', 'data-bar', 'data-'])", "3");
shouldBe("testForIn(['data-foo', 'data-bar', 'data-43'])", "3");
shouldBe("testForIn(['data-foo', 'data-oric1', 'data-bar'])", "3");
shouldBe("testForIn(['data-foo', 'data-oric-1', 'data-bar'])", "3");
shouldBe("testForIn(['data-foo', 'data-oric-1x', 'data-bar'])", "3");


debug("");
debug("Property override:");
var div = document.createElement("div");
// If the Object prototype already has "foo", dataset doesn't create the
// corresponding attribute for "foo".
shouldBe("Object.prototype.foo = 'on Object'; div.dataset.foo", "'on Object'");
shouldBe("div.dataset['foo'] = 'on dataset'; div.dataset.foo", "'on dataset'");
shouldBeTrue("div.hasAttribute('data-foo')");
shouldBe("div.setAttribute('data-foo', 'attr'); div.dataset.foo", "'attr'");
debug("Update the JavaScript property:");
shouldBe("div.dataset.foo = 'updated'; div.dataset.foo", "'updated'");
shouldBe("div.getAttribute('data-foo')", "'updated'");
// "Bar" can't be represented as a data- attribute.
shouldBe("div.dataset.Bar = 'on dataset'; div.dataset.Bar", "'on dataset'");
shouldBeFalse("div.hasAttribute('data-Bar')");
debug("Make the JavaScript property empty:");
shouldBe("div.dataset.foo = ''; div.dataset.foo", "''");
shouldBe("div.getAttribute('data-foo')", "''");
debug("Remove the attribute:");
shouldBe("div.removeAttribute('data-foo'); div.dataset.foo", "'on Object'");
debug("Remove the JavaScript property:");
shouldBe("div.setAttribute('data-foo', 'attr'); delete div.dataset.foo; div.dataset.foo", "'on Object'");
shouldBeFalse("div.hasAttribute('foo')");
shouldBeUndefined("delete div.dataset.Bar; div.dataset.Bar");

shouldBe("Object.prototype[11] = 'on Object'; div.dataset[11]", "'on Object'");
shouldBe("div.dataset['11'] = 'on dataset'; div.dataset[11]", "'on dataset'");
shouldBeTrue("div.hasAttribute('data-11')");
shouldBe("div.setAttribute('data-11', 'attr'); div.dataset[11]", "'attr'");
debug("Update the JavaScript property:");
shouldBe("div.dataset[11] = 'updated'; div.dataset[11]", "'updated'");
shouldBe("div.getAttribute('data-11')", "'updated'");

shouldBe("Object.prototype['a500'] = 'on Object'; div.dataset['a500']", "'on Object'");
shouldBe("div.dataset['a500'] = 'on dataset'; div.dataset['a500']", "'on dataset'");
shouldBeTrue("div.hasAttribute('data-a500')");
shouldBe("div.setAttribute('data-a500', 'attr'); div.dataset['a500']", "'attr'");
debug("Update the JavaScript property:");
shouldBe("div.dataset['a500'] = 'updated'; div.dataset['a500']", "'updated'");
shouldBe("div.getAttribute('data-a500')", "'updated'");

shouldBe("Object.prototype['a-500k'] = 'on Object'; div.dataset['a-500k']", "'on Object'");
shouldBe("div.dataset['a-500k'] = 'on dataset'; div.dataset['a-500k']", "'on dataset'");
shouldBeTrue("div.hasAttribute('data-a-500k')");
shouldBe("div.setAttribute('data-a-500k', 'attr'); div.dataset['a-500k']", "'attr'");
debug("Update the JavaScript property:");
shouldBe("div.dataset['a-500k'] = 'updated'; div.dataset['a-500k']", "'updated'");
shouldBe("div.getAttribute('data-a-500k')", "'updated'");

debug("");
debug("Set null:");

var d = document.createElement("div");
d.dataset.foo = null;
shouldBe("d.dataset.foo", "'null'");

debug("");
