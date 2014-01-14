description("Test how Node.prefix setter raises NAMESPACE_ERR.");

shouldThrow("document.createElementNS(null, 'attr').prefix = 'abc'");
shouldThrow("document.createElementNS('foo', 'bar').prefix = 'xml'");
