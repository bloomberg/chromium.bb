description("createDocument tests modeled after mozilla's testing");

shouldThrow("document.implementation.createDocumentType('foo')", "'TypeError: Not enough arguments'");
shouldThrow("document.implementation.createDocumentType('foo', null)", "'TypeError: Not enough arguments'");
shouldThrow("document.implementation.createDocumentType(undefined, undefined)", "'TypeError: Not enough arguments'");
shouldThrow("document.implementation.createDocumentType(null, undefined)", "'TypeError: Not enough arguments'");
shouldThrow("document.implementation.createDocumentType(undefined, null)", "'TypeError: Not enough arguments'");
shouldNotThrow("document.implementation.createDocumentType(undefined, undefined, null)");
shouldThrow("document.implementation.createDocumentType(null, null)", "'TypeError: Not enough arguments'");
shouldThrow("document.implementation.createDocumentType(null, '')", "'TypeError: Not enough arguments'");
shouldThrow("document.implementation.createDocumentType('', null)", "'TypeError: Not enough arguments'");
shouldThrow("document.implementation.createDocumentType('', '')", "'TypeError: Not enough arguments'");
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

