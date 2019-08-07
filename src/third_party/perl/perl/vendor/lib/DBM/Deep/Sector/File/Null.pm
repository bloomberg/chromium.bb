package DBM::Deep::Sector::File::Null;

use 5.008_004;

use strict;
use warnings FATAL => 'all';

use base qw( DBM::Deep::Sector::File::Data );

my $STALE_SIZE = 2;

# Please refer to the pack() documentation for further information
my %StP = (
    1 => 'C', # Unsigned char value (no order needed as it's just one byte)
    2 => 'n', # Unsigned short in "network" (big-endian) order
    4 => 'N', # Unsigned long in "network" (big-endian) order
    8 => 'Q', # Usigned quad (no order specified, presumably machine-dependent)
);

sub type { $_[0]{engine}->SIG_NULL }
sub data_length { 0 }
sub data { return }

sub _init {
    my $self = shift;

    my $engine = $self->engine;

    unless ( $self->offset ) {
        my $leftover = $self->size - $self->base_size - 1 * $engine->byte_size - 1;

        $self->{offset} = $engine->_request_data_sector( $self->size );
        $engine->storage->print_at( $self->offset, $self->type ); # Sector type
        # Skip staleness counter
        $engine->storage->print_at( $self->offset + $self->base_size,
            pack( $StP{$engine->byte_size}, 0 ),  # Chain loc
            pack( $StP{1}, $self->data_length ),  # Data length
            chr(0) x $leftover,                   # Zero-fill the rest
        );

        return;
    }
}

1;
__END__
