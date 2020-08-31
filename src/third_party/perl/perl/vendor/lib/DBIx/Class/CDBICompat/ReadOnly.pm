package # hide from PAUSE
    DBIx::Class::CDBICompat::ReadOnly;

use strict;
use warnings;

sub make_read_only {
  my $proto = shift;
  $proto->add_trigger("before_$_" => sub { shift->throw_exception("$proto is read only") })
    foreach qw/create delete update/;
  return $proto;
}

1;
