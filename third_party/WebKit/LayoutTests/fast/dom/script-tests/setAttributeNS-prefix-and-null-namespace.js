description("Test that calling setAttributeNS() with a prefixed qualifiedName and null NS throws NAMESPACE_ERR.");

shouldThrow("document.createElement('test').setAttributeNS(null, 'foo:bar', 'baz')", "'NamespaceError: An attempt was made to create or change an object in a way which is incorrect with regard to namespaces.'");
