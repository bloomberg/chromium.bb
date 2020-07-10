package     # hide from PAUSE
    DBIx::Class::Admin::Descriptive;

use warnings;
use strict;

use base 'Getopt::Long::Descriptive';

require DBIx::Class::Admin::Usage;
sub usage_class { 'DBIx::Class::Admin::Usage'; }

1;
