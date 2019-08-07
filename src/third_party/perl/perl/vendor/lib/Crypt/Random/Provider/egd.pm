#!/usr/bin/perl -sw
##
##
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id: egd.pm,v 1.5 2001/07/12 15:59:48 vipul Exp $

package Crypt::Random::Provider::egd;
use strict;

use IO::Socket;
use Carp;
use Math::Pari qw(pari2num);


sub _defaultsource { 

    my $source;
    for my $d (qw( /var/run/egd-pool /dev/egd-pool /etc/entropy )) {
        if (IO::Socket::UNIX->new(Peer => $d)) { $source = $d; last }
    }
    return $source;

}


sub new { 

    my ($class, %args) = @_;
    my $self = { Source => $args{Source} || $args{Device} || $args{Filename} };
    $$self{Source} = $class->_defaultsource() unless $$self{Source};
    croak "egd entropy pool file not found.\n" unless $$self{Source};
    return bless $self, $class;

}


sub get_data {
   
    my ( $self, %params ) = @_;
    my $class = ref $self || $self;
    $self = {} unless ref $self;
 
    my $bytes    = $params{Length} ||
                   (int( pari2num($params{ Size }) / 8) + 1);
    my $dev      = $params{Source} || $$self{Source};
    my $skip     = $params{Skip};

    croak "$dev doesn't exist.  aborting." unless $dev && -e $dev;

    my $s = IO::Socket::UNIX->new(Peer => $dev);
    croak "couldn't talk to egd. $!" unless $s;

    my($r, $read) = ('', 0);
    while ($read < $bytes) {
        my $msg = pack "CC", 0x01, 1;
        $s->syswrite($msg, length $msg);
        my $rt;
        my $nread = $s->sysread($rt, 1);
        croak "read from entropy socket failed" unless $nread == 1;
        my $count = unpack("C", $rt);
        $nread = $s->sysread($rt, $count);
        croak "couldn't get all the requested entropy.  aborting."
            unless $nread == $count;
        unless ($skip && $skip =~ /\Q$rt\E/) {
            if ($params{Verbosity}) { print '.' unless $read % 2 }
            $r .= $rt;
            $read++;
        }
    }

    $r;
}


sub available { 
   
    my $class = shift; 
    return 1 if $class->_defaultsource();
    return;

}


1;


