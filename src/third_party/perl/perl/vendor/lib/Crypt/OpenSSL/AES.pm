package Crypt::OpenSSL::AES;

# Copyright (C) 2006 DelTel, Inc.
#
# This library is free software; you can redistribute it and/or modify
# it under the same terms as Perl itself, either Perl version 5.8.5 or,
# at your option, any later version of Perl 5 you may have available.

use 5.006002;
use strict;
use warnings;

require Exporter;

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use Crypt::OpenSSL::AES ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
	
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
	
);

our $VERSION = '0.05';

require XSLoader;
XSLoader::load('Crypt::OpenSSL::AES', $VERSION);

# Preloaded methods go here.

1;
__END__

=head1 NAME

Crypt::OpenSSL::AES - A Perl wrapper around OpenSSL's AES library

=head1 SYNOPSIS

     use Crypt::OpenSSL::AES;

     my $cipher = new Crypt::OpenSSL::AES($key);

     $encrypted = $cipher->encrypt($plaintext)
     $decrypted = $cipher->decrypt($encrypted)

=head1 DESCRIPTION

This module implements a wrapper around OpenSSL.  Specifically, it
wraps the methods related to the US Government's Advanced
Encryption Standard (the Rijndael algorithm).

This module is compatible with Crypt::CBC (and likely other modules
that utilize a block cipher to make a stream cipher).

This module is an alternative to the implementation provided by 
Crypt::Rijndael which implements AES itself. In contrast, this module
is simply a wrapper around the OpenSSL library.

=over 4

=item $cipher->encrypt($data)

Encrypt data. The size of C<$data> must be exactly C<blocksize> in
length (16 bytes), otherwise this function will croak.

You should use Crypt::CBC or something similar to encrypt/decrypt data
of arbitrary lengths.

=item $cipher->decrypt($data)

Decrypts C<$data>. The size of C<$data> must be exactly C<blocksize> in
length (16 bytes), otherwise this function will croak.

You should use Crypt::CBC or something similar to encrypt/decrypt data
of arbitrary lengths.

=item keysize

This method is used by Crypt::CBC to verify the key length.
This module actually supports key lengths of 16, 24, and 32 bytes,
but this method always returns 32 for Crypt::CBC's sake.

=item blocksize

This method is used by Crypt::CBC to check the block size.
The blocksize for AES is always 16 bytes. 

=back

=head2 USE WITH CRYPT::CBC

	use Crypt::CBC

	$cipher = Crypt::CBC->new(
		-key    => $key,
		-cipher => "Crypt::OpenSSL::AES"
	);             

	$encrypted = $cipher->encrypt($plaintext)
	$decrypted = $cipher->decrypt($encrypted)


=head1 SEE ALSO

L<Crypt::CBC>

http://www.openssl.org/

http://en.wikipedia.org/wiki/Advanced_Encryption_Standard

http://www.csrc.nist.gov/encryption/aes/

=head1 BUGS

Need more (and better) test cases.

=head1 AUTHOR

Tolga Tarhan, E<lt>cpan at ttar dot orgE<gt>

The US Government's Advanced Encryption Standard is the Rijndael
Algorithm and was developed by Vincent Rijmen and Joan Daemen.

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006 DelTel, Inc.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.5 or,
at your option, any later version of Perl 5 you may have available.

=cut
