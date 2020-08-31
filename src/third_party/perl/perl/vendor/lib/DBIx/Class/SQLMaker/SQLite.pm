package # Hide from PAUSE
  DBIx::Class::SQLMaker::SQLite;

use warnings;
use strict;

use base qw( DBIx::Class::SQLMaker );

#
# SQLite does not understand SELECT ... FOR UPDATE
# Disable it here
sub _lock_select () { '' };

1;
