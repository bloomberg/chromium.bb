#!/usr/bin/perl -sw
##
## Crypt::Random::Provider::rand
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id: rand.pm,v 1.2 2001/06/22 18:16:29 vipul Exp $

package Crypt::Random::Provider::rand; 
use strict;
use Math::Pari qw(pari2num);

sub new { 

    my ($class, %params) = @_;
    my $self = { Source => $params{Source} || sub { return rand($_[0]) } };
    return bless $self, $class;

}


sub get_data { 

    my ($self, %params) = @_;
    $self = {} unless ref $self;

    my $size = $params{Size}; 
    my $skip = $params{Skip} || $$self{Skip};

    if ($size && ref $size eq "Math::Pari") { 
        $size = pari2num($size);
    }

    my $bytes = $params{Length} || (int($size / 8) + 1);
    my $source = $$self{Source} || sub { rand($_[0]) };
    
    my($r, $read, $rt) = ('', 0);
    while ($read < $bytes) {
        $rt = chr(int(&$source(256)));
        unless ($skip && $skip =~ /\Q$rt\E/) {
            $r .= $rt; $read++;
        }
    }

    $r;

}


sub available { 

    return 1;

}


1;
 
