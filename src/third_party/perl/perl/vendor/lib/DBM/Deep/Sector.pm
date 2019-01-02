package DBM::Deep::Sector;

use 5.008_004;

use strict;
use warnings FATAL => 'all';

use Scalar::Util ();

sub new {
    my $self = bless $_[1], $_[0];
    Scalar::Util::weaken( $self->{engine} );
    $self->_init;
    return $self;
}

sub _init {}

sub clone {
    my $self = shift;
    return ref($self)->new({
        engine => $self->engine,
        type   => $self->type,
        data   => $self->data,
    });
}


sub engine { $_[0]{engine} }
sub offset { $_[0]{offset} }
sub type   { $_[0]{type}   }
sub staleness { $_[0]{staleness} }

sub load { die "load must be implemented in a child class" }

1;
__END__
