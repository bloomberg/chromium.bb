=head1 NAME

Crypt::Serpent - Crypt::CBC compliant Serpent block cipher encryption module

=head1 SYNOPSIS

    use Crypt::Serpent;
    
    my $cipher = new Crypt::Serpent $key;

    my $ciphertext = $cipher->encrypt($plaintext);
    my $plaintext = $cipher->decrypt($ciphertext);

=head1 DESCRIPTION

From the Tropical Software Serpent page...

"Serpent was designed by Ross Anderson, Eli Biham and Lars Knudsen as a
candidate for the Advanced Encryption Standard. It has been selected as
one of the five finalists in the AES competition. Serpent is faster than
DES and more secure than Triple DES. It provides users with a very high
level of assurance that no shortcut attack will be found. To achieve this,
the algorithm's designers limited themselves to well understood cryptography
mechanisms, so that they could rely on the wide experience and proven
techniques of block cipher cryptanalysis. The algorithm uses twice as many
rounds as are necessary to block all currently known shortcut attacks. This
means that Serpent should be safe against as yet unknown attacks that may be
capable of breaking the standard 16 rounds used in many types of encryption
today. However, the fact that Serpent uses so many rounds means that it is
the slowest of the five AES finalists. But this shouldn't be an issue because
it still outperforms Triple DES. The algorithm's designers maintain that
Serpent has a service life of at least a century."

"Serpent is a 128-bit block cipher, meaning that data is encrypted and
decrypted in 128-bit chunks. The key length can vary, but for the purposes
of the AES it is defined to be either 128, 192, or 256 bits. This block size
and variable key length is standard among all AES candidates and was one of
the major design requirements specified by NIST. The Serpent algorithm uses
32 rounds, or iterations of the main algorithm."

=over 4

=cut

package Crypt::Serpent;

require DynaLoader;

$VERSION = 1.01;
@ISA = qw/DynaLoader/;

bootstrap Crypt::Serpent $VERSION;

=back

=head1 SEE ALSO

http://www.tropsoft.com/strongenc/serpent.htm

=head1 AUTHOR

John Hughes (jhughes@frostburg.edu)

=cut

1;
