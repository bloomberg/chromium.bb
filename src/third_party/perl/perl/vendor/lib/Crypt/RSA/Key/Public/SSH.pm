#!/usr/bin/perl -sw
##
## Crypt::RSA::Key::Private::SSH
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id: SSH.pm,v 1.1 2001/05/20 23:37:48 vipul Exp $

package Crypt::RSA::Key::Public::SSH;
use strict;
use Crypt::RSA::DataFormat qw(bitsize);
use Crypt::RSA::Key::Public;
use vars qw(@ISA);
@ISA = qw(Crypt::RSA::Key::Public);

sub deserialize {
    my ($self, %params) = @_;
    my ($bitsize, $e, $n, $ident) = split /\s/, join'',@{$params{String}};
    $self->n ($n);
    $self->e ($e);
    $self->Identity ($ident);
    return $self;
}

sub serialize { 
    my ($self, %params) = @_;
    my $bitsize = bitsize ($self->n);
    my $string = join ' ', $bitsize, $self->e, $self->n, $self->Identity;
    return $string;
}

1;

=head1 NAME

Crypt::RSA::Key::Public::SSH - SSH Public Key Import

=head1 SYNOPSIS

    Crypt::RSA::Key::Public::SSH is a class derived from
    Crypt::RSA::Key::Public that provides serialize() and
    deserialze() methods for SSH 2.x keys.

=head1 AUTHOR

Vipul Ved Prakash, E<lt>mail@vipul.netE<gt>

=cut


