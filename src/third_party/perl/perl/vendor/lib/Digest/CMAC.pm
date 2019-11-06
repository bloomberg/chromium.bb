package Digest::CMAC;

use base qw(Digest::OMAC::Base);

use strict;
#use warnings;
use Carp;
use MIME::Base64;

use vars qw($VERSION);

$VERSION = '0.04';

sub _lu2 {
	my ( $self, $blocksize,  $L, $Lu ) = @_;
	$self->_lu( $blocksize, $Lu );
}

1;
__END__

=head1 NAME

Digest::CMAC - The One-key CBC MAC message authentication code.

=head1 SYNOPSIS

  use Digest::CMAC;
  my $omac1 = Digest::CMAC->new($key);
  
  $omac1->add($data);
  
  my $binary_tag = $omac1->digest;
  my $hex_tag    = $omac1->hexdigest;
  my $base64_tag = $omac1->b64digest;

=head1 DESCRIPTION

This module implements OMAC1 blockcipher-based message authentication code for perl. For OMAC1/OMAC. Check http://www.nuee.nagoya-u.ac.jp/labs/tiwata/omac/omac.html. Here is an excerpt of that page

=over 4

OMAC is a blockcipher-based message authentication code designed and analyzed by me and Kaoru Kurosawa.

OMAC is a simple variant of the CBC MAC (Cipher Block Chaining Message Authentication Code). OMAC stands for One-Key CBC MAC.

OMAC allows and is secure for messages of any bit length (while the CBC MAC is only secure on messages of one fixed length, and the length must be a multiple of the block length). Also, the efficiency of OMAC is highly optimized. It is almost as efficient as the CBC MAC.

"NIST Special Publication 800-38B Recommendation for Block Cipher Modes of Operation: the CMAC Mode for Authentication" has been finalized on May 18, 2005. This Recommendation specifies CMAC, which is equivalent to OMAC (OMAC1).

=back 4

Like many block-cipher's Crypt:: modules like L<Crypt::Rijndael>, and L<MIME::Base64>.

=head1 METHODS

=over 4

=item new

  my $omac1 = Digest::CMAC->new($key [, $cipher]);

This creates a new Digest::CMAC object, using $key.

$cipher is 'Crypt::Rijndael'(default), 'Crypt::Misty1', Crypt::Blowfish', or whatever blockcipher you like. $key is fixed length string that blockcipher demands. 

=item add

  $omac1->add($message,...);

The $message provided as argument are appended to the message we calculate the MAC. The return value is the $cmac object itself;

=item reset

  $omac1->reset;

This is just an alias for $cmac->new;

=item digest

  my $digest = $omac1->digest;

Return the binary authentication code for the message. The returned string will be blockcipher's block size.

=item hexdigest

  my $digest = $omac1->hexdigest;

Same as $cmac->digest, but will return the digest in hexadecimal form.

=item b64digest

Same as $omac1->digest, but will return the digest as a base64 encoded string.

=back

=head1 SEE ALSO

L<Crypt::Rijndael>,
http://www.nuee.nagoya-u.ac.jp/labs/tiwata/omac/omac.html,
http://www.csrc.nist.gov/publications/nistpubs/800-38B/SP_800-38B.pdf

=head1 AUTHOR

OMAC designed and analyzed by
Tetsu Iwata and Kaoru Kurosawa

"Crypt::CMAC" was written by
Hiroyuki OYAMA <oyama@module.jp>

OMAC2 support added by Yuval Kogman

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006 by Hiroyuki OYAMA, 2007 by Hiroyuki OYAMA, Yuval Kogman

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.6 or,
at your option, any later version of Perl 5 you may have available.

=cut
