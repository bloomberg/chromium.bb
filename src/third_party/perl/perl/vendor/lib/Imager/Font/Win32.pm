package Imager::Font::Win32;
use strict;
use vars qw(@ISA);
@ISA = qw(Imager::Font::W32);

require Imager::Font::W32;

1;

__END__

=head1 NAME

=for stopwords GDI

Imager::Font::Win32 - uses Win32 GDI services for text output

=head1 SYNOPSIS

  my $font = Imager::Font->new(face=>"Arial");

=head1 DESCRIPTION

This module is obsolete.

=cut
