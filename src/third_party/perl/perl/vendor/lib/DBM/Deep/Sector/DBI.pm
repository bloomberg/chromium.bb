package DBM::Deep::Sector::DBI;

use 5.008_004;

use strict;
use warnings FATAL => 'all';

use base qw( DBM::Deep::Sector );

use DBM::Deep::Sector::DBI::Reference ();
use DBM::Deep::Sector::DBI::Scalar ();

sub free {
    my $self = shift;

    $self->engine->storage->delete_from(
        $self->table, $self->offset,
    );
}

sub reload {
    my $self = shift;
    $self->_init;
}

sub load {
    my $self = shift;
    my ($engine, $offset, $type) = @_;

    if ( !defined $type || $type eq 'refs' ) {
        return DBM::Deep::Sector::DBI::Reference->new({
            engine => $engine,
            offset => $offset,
        });
    }
    elsif ( $type eq 'datas' ) {
        my $sector = DBM::Deep::Sector::DBI::Scalar->new({
            engine => $engine,
            offset => $offset,
        });

        if ( $sector->{data_type} eq 'R' ) {
            return $self->load(
                $engine, $sector->{value}, 'refs',
            );
        }

        return $sector;
    }

    DBM::Deep->_throw_error( "'$offset': Don't know what to do with type '$type'" );
}

1;
__END__
