description("createDocument tests modeled after mozilla's testing");

function ExpectedNotEnoughArgumentsMessage(num) {
    return "\"TypeError: Failed to execute 'createDocumentType' on 'DOMImplementation': 3 arguments required, but only " + num + " present.\"";
}

shouldThrow("document.implementation.createDocumentType('foo')", ExpectedNotEnoughArgumentsMessage(1));
shouldThrow("document.implementation.createDocumentType('foo', null)", ExpectedNotEnoughArgumentsMessage(2));
shouldThrow("document.implementation.createDocumentType(undefined, undefined)", ExpectedNotEnoughArgumentsMessage(2));
shouldThrow("document.implementation.createDocumentType(null, undefined)", ExpectedNotEnoughArgumentsMessage(2));
shouldThrow("document.implementation.createDocumentType(undefined, null)", ExpectedNotEnoughArgumentsMessage(2));
shouldNotThrow("document.implementation.createDocumentType(undefined, undefined, null)");
shouldThrow("document.implementation.createDocumentType(null, null)", ExpectedNotEnoughArgumentsMessage(2));
shouldThrow("document.implementation.createDocumentType(null, '')", ExpectedNotEnoughArgumentsMessage(2));
shouldThrow("document.implementation.createDocumentType('', null)", ExpectedNotEnoughArgumentsMessage(2));
shouldThrow("document.implementation.createDocumentType('', '')", ExpectedNotEnoughArgumentsMessage(2));
shouldThrow("document.implementation.createDocumentType('a:', null, null)", "'NamespaceError: An attempt was made to create or change an object in a way which is incorrect with regard to namespaces.'");
shouldThrow("document.implementation.createDocumentType(':foo', null, null)", "'NamespaceError: An attempt was made to create or change an object in a way which is incorrect with regard to namespaces.'");
shouldThrow("document.implementation.createDocumentType(':', null, null)", "'NamespaceError: An attempt was made to create or change an object in a way which is incorrect with regard to namespaces.'");
shouldNotThrow("document.implementation.createDocumentType('foo', null, null)");
shouldNotThrow("document.implementation.createDocumentType('foo:bar', null, null)");
shouldThrow("document.implementation.createDocumentType('foo::bar', null, null)", "'NamespaceError: An attempt was made to create or change an object in a way which is incorrect with regard to namespaces.'");
shouldThrow("document.implementation.createDocumentType('\t:bar', null, null)", "'InvalidCharacterError: The string contains invalid characters.'");
shouldThrow("document.implementation.createDocumentType('foo:\t', null, null)", "'InvalidCharacterError: The string contains invalid characters.'");
shouldThrow("document.implementation.createDocumentType('foo :bar', null, null)", "'InvalidCharacterError: The string contains invalid characters.'");
shouldThrow("document.implementation.createDocumentType('foo: bar', null, null)", "'InvalidCharacterError: The string contains invalid characters.'");
shouldThrow("document.implementation.createDocumentType('a:b:c', null, null)", "'NamespaceError: An attempt was made to create or change an object in a way which is incorrect with regard to namespaces.'");

