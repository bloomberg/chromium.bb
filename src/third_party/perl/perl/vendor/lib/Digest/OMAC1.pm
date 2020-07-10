#!/usr/bin/perl

package Digest::OMAC1;

use strict;
#use warnings;

use base qw(Digest::CMAC);

__PACKAGE__;

__END__

=pod

=head1 NAME

Digest::OMAC1 - An alias for L<Digest::CMAC>

=head1 SYNOPSIS

	use Digest::OMAC1;

	my $d = Digest::OMAC1->new( $key, $cipher );

=head1 DESCRIPTIOn

This module has an identical interface to L<Digest::CMAC>. NIST dubbed OMAC 1
"CMAC" when they reccomended it, much like L<Crypt::Rijndael> is known as AES.
