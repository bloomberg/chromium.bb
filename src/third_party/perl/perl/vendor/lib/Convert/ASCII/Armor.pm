#!/usr/bin/perl -sw
##
##
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id: Armor.pm,v 1.1 2001/03/19 23:15:09 vipul Exp $

package Convert::ASCII::Armor; 
use lib "../../../lib";
use Convert::ASCII::Armour; 
use vars qw(@ISA);
@ISA = qw(Convert::ASCII::Armour);

1;

=head1 NAME

Convert::ASCII::Armor - Convert binary octets into ASCII armoured messages.

=head1 SYNOPSIS

See SYNOPSIS in Convert::ASCII::Armour.

=head1 DESCRIPTION

Empty subclass of Convert::ASCII::Armour for American English speakers.

=head1 AUTHOR

Vipul Ved Prakash, E<lt>mail@vipul.netE<gt>

=cut


