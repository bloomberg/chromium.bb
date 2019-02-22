package # hide from the pauses
  namespace::clean::_PP_OSE;

use warnings;
use strict;

use Tie::Hash;
use Hash::Util::FieldHash 'fieldhash';

# Here we rely on a combination of several behaviors:
#
# * %^H is deallocated on scope exit, so any references to it disappear
# * A lost weakref in a fieldhash causes the corresponding key to be deleted
# * Deletion of a key on a tied hash triggers DELETE
#
# Therefore the DELETE of a tied fieldhash containing a %^H reference will
# be the hook to fire all our callbacks.

fieldhash my %hh;
{
  package # hide from pause too
    namespace::clean::_TieHintHashFieldHash;
  use base 'Tie::StdHash';
  sub DELETE {
    my $ret = shift->SUPER::DELETE(@_);
    $_->() for @$ret;
    $ret;
  }
}

sub on_scope_end (&) {
  $^H |= 0x020000;

  tie(%hh, 'namespace::clean::_TieHintHashFieldHash')
    unless tied %hh;

  push @{ $hh{\%^H} ||= [] }, shift;
}

1;
