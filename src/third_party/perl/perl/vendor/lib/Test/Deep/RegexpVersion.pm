use strict;
use warnings;

package Test::Deep::RegexpVersion;

# Older versions of Perl treated Regexp refs as opaque scalars blessed
# into the "Regexp" class. Several bits of code need this so we
# centralise the test for that kind of version.
our $OldStyle = ($] < 5.011);

1;
