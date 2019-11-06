package # hide from PAUSE
    DBIx::Class::CDBICompat::Pager;

use strict;

# even though fatalization has been proven over and over to be a universally
# bad idea, this line has been part of the code from the beginning
# leaving the compat layer as-is, something may in fact depend on that
use warnings FATAL => 'all';

*pager = \&page;

sub page {
  my $class = shift;

  my $rs = $class->search(@_);
  unless ($rs->{attrs}{page}) {
    $rs = $rs->page(1);
  }
  return ( $rs->pager, $rs );
}

1;
