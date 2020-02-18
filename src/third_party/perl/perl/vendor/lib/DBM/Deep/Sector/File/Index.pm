package DBM::Deep::Sector::File::Index;

use strict;
use warnings FATAL => 'all';

use base qw( DBM::Deep::Sector::File );

my $STALE_SIZE = 2;

# Please refer to the pack() documentation for further information
my %StP = (
    1 => 'C', # Unsigned char value (no order needed as it's just one byte)
    2 => 'n', # Unsigned short in "network" (big-endian) order
    4 => 'N', # Unsigned long in "network" (big-endian) order
    8 => 'Q', # Usigned quad (no order specified, presumably machine-dependent)
);

sub _init {
    my $self = shift;

    my $engine = $self->engine;

    unless ( $self->offset ) {
        my $leftover = $self->size - $self->base_size;

        $self->{offset} = $engine->_request_index_sector( $self->size );
        $engine->storage->print_at( $self->offset, $engine->SIG_INDEX ); # Sector type
        # Skip staleness counter
        $engine->storage->print_at( $self->offset + $self->base_size,
            chr(0) x $leftover, # Zero-fill the rest
        );
    }

    return $self;
}

#XXX Change here
sub size {
    my $self = shift;
    unless ( $self->{size} ) {
        my $e = $self->engine;
        $self->{size} = $self->base_size + $e->byte_size * $e->hash_chars;
    }
    return $self->{size};
}

sub free_meth { return '_add_free_index_sector' }

sub free {
    my $self = shift;
    my $e = $self->engine;

    for my $i ( 0 .. $e->hash_chars - 1 ) {
        my $l = $self->get_entry( $i ) or next;
        $e->load_sector( $l )->free;
    }

    $self->SUPER::free();
}

sub _loc_for {
    my $self = shift;
    my ($idx) = @_;
    return $self->offset + $self->base_size + $idx * $self->engine->byte_size;
}

sub get_entry {
    my $self = shift;
    my ($idx) = @_;

    my $e = $self->engine;

    DBM::Deep->_throw_error( "get_entry: Out of range ($idx)" )
        if $idx < 0 || $idx >= $e->hash_chars;

    return unpack(
        $StP{$e->byte_size},
        $e->storage->read_at( $self->_loc_for( $idx ), $e->byte_size ),
    );
}

sub set_entry {
    my $self = shift;
    my ($idx, $loc) = @_;

    my $e = $self->engine;

    DBM::Deep->_throw_error( "set_entry: Out of range ($idx)" )
        if $idx < 0 || $idx >= $e->hash_chars;

    $self->engine->storage->print_at(
        $self->_loc_for( $idx ),
        pack( $StP{$e->byte_size}, $loc ),
    );
}

1;
__END__
