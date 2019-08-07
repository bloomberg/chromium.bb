package # hide from the pauses
  namespace::clean::_PP_OSE_5_8;

use warnings;
use strict;

# This is the original implementation, which sadly is broken
# on perl 5.10+ withing string evals
sub on_scope_end (&) {
  $^H |= 0x020000;

  push @{
    $^H{'__namespace::clean__guardstack__'}
      ||= bless ([], 'namespace::clean::_PP_SG_STACK')
  }, shift;
}

package # hide from the pauses
  namespace::clean::_PP_SG_STACK;

use warnings;
use strict;

sub DESTROY { $_->() for @{$_[0]} }

1;
