description("Test that calling setAttributeNS() with a prefixed qualifiedName and null NS throws NAMESPACE_ERR.");

shouldThrow("document.createElement('test').setAttributeNS(null, 'foo:bar', 'baz')", '"NamespaceError: Failed to execute \'setAttributeNS\' on \'Element\': \'\' is an invalid namespace for attributes."');
