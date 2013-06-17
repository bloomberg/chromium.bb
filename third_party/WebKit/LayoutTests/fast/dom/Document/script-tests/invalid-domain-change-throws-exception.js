description("This test checks that a SECURITY_ERR exception is raised if an attempt is made to change document.domain to an invalid value.");

shouldThrow('document.domain = "apple.com"', '"SecurityError: An attempt was made to break through the security policy of the user agent."');
