package DBM::Deep::Sector::DBI::Scalar;

use strict;
use warnings FATAL => 'all';

use base qw( DBM::Deep::Sector::DBI );

sub table { 'datas' }

sub _init {
    my $self = shift;

    if ( $self->offset ) {
        my ($rows) = $self->engine->storage->read_from(
            datas => $self->offset,
            qw( id data_type key value ),
        );

        $self->{$_} = $rows->[0]{$_} for qw( data_type key value );
    }

    return;
}

sub data {
    my $self = shift;
    $self->{value};
}

1;
__END__
