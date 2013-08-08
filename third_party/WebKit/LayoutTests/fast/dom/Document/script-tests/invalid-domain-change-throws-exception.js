description("This test checks that a SECURITY_ERR exception is raised if an attempt is made to change document.domain to an invalid value.");

shouldThrow('document.domain = "apple.com"', '"SecurityError: \'document.domain\' assignment failed: \'apple.com\' is not a suffix of \'\'."');
