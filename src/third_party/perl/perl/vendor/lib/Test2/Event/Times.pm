package Test2::Event::Times;
use strict;
use warnings;

BEGIN { require Test2::Event; our @ISA = qw(Test2::Event) }
use Test2::Util::HashBase qw/-start -stop -user -sys -cuser -csys/;

use Test2::Util::Times qw/render_bench/;

our $VERSION = '0.000122';

sub summary {
    my $self = shift;
    return render_bench(@{$self}{(START, STOP, USER, SYS, CUSER, CSYS)});
}

sub facet_data {
    my $self = shift;

    my $data = $self->SUPER::facet_data();

    $data->{times} = {
        START() => $self->{+START},
        STOP()  => $self->{+STOP},
        USER()  => $self->{+USER},
        SYS()   => $self->{+SYS},
        CUSER() => $self->{+CUSER},
        CSYS()  => $self->{+CSYS},
    };

    return $data;
}

1;
