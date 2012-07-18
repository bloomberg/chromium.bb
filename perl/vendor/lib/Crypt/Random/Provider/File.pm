package Crypt::Random::Provider::File; 
use strict;
use Carp;
use Math::Pari qw(pari2num);
use Fcntl;

sub _defaultsource { 
    return;
}


sub new { 

    my ($class, %args) = @_;
    my $self = { Source => $args{File} || $args{Device} || $args{Filename} || $class->_defaultsource() };
    return bless $self, $class;

}


sub get_data { 

    my ($self, %params) = @_;
    $self = {} unless ref $self;

    my $size = $params{Size}; 
    my $skip = $params{Skip} || $$self{Skip} || '';
    my $q_skip = quotemeta($skip);

    if ($size && ref $size eq "Math::Pari") { 
        $size = pari2num($size);
    }

    my $bytes = $params{Length} || (int($size / 8) + 1);

    sysopen RANDOM, $$self{Source}, O_RDONLY;

    my($r, $read, $rt) = ('', 0);
    while ($read < $bytes) {
        my $howmany = sysread  RANDOM, $rt, $bytes - $read;
        next unless $howmany;
        if ($howmany == -1) { 
            croak "Error while reading from $$self{Source}. $!"
        }
        $rt =~ s/[$q_skip]//g if $skip;
        $r .= $rt; 
        $read = length $r;
    }

    $r;

}


sub available { 
    my ($class) = @_;
    return -e $class->_defaultsource();
}


1;

