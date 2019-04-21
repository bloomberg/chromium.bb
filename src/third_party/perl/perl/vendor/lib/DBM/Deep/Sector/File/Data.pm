package DBM::Deep::Sector::File::Data;

use 5.008_004;

use strict;
use warnings FATAL => 'all';

use base qw( DBM::Deep::Sector::File );

# This is in bytes
sub size { $_[0]{engine}->data_sector_size }
sub free_meth { return '_add_free_data_sector' }

1;
__END__
