package # hide from pause
  DBIx::Class::ResultSet::Pager;

use warnings;
use strict;

# temporary, to load MRO::Compat, will be soon entirely rewritten anyway
use DBIx::Class::_Util;

use base 'Data::Page';
use mro 'c3';

# simple support for lazy totals
sub _total_entries_accessor {
  if (@_ == 1 and ref $_[0]->{total_entries} eq 'CODE') {
    return $_[0]->{total_entries} = $_[0]->{total_entries}->();
  }

  return shift->next::method(@_);
}

sub _skip_namespace_frames { qr/^Data::Page/ }

1;
