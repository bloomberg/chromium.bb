This directory contains the test cases for longtask API v2. For now, the
longtask API V2 are still working in progress and not ready to be web-exposed
 yet. Some of them, if appropriate to be web-platform-tests, will be upstreamed
to web-platform-tests after longtask v2 are fully implemented.

The test cases in this directory requires enabling of runtime feature flag
LongTaskV2. The test cases in this directory are expected to fail since 
LongTaskV2 are not supposed to be enabled for this folder. The 
fail-expectatation exists for the compatibility with the 'mojo-loading' virtual 
tests.
