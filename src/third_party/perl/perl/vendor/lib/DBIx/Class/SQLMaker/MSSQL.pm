package # Hide from PAUSE
  DBIx::Class::SQLMaker::MSSQL;

use warnings;
use strict;

use base qw( DBIx::Class::SQLMaker );

#
# MSSQL does not support ... OVER() ... RNO limits
#
sub _rno_default_order {
  return \ '(SELECT(1))';
}

1;
