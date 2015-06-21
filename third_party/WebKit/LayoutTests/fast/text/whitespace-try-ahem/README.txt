Temporary for debugging crbug.com/392046.
Tests are copied from ../whitespace. They are flaky on Windows (more) and
Mac (less) perhaps because of Ahem font loading.

Try to use @font-face instead of the orignal pre-registered 'Ahem' font family.
