=head1 NAME

Crypt::RC6 - Crypt::CBC compliant RC6 block cipher encryption module

=head1 SYNOPSIS

    use Crypt::RC6;
    
    my $cipher = new Crypt::RC6 $key;

    my $ciphertext = $cipher->encrypt($plaintext);
    my $plaintext = $cipher->decrypt($ciphertext);

=head1 DESCRIPTION

From THE RC6 BLOCK CIPHER, by Rivest, Robshaw, Sidney, and Yin...

"RC6 is an evolutionary improvement of RC5, designed to meet the requirements
of the Advanced Encryption Standard (AES). Like RC5, RC6 makes essential use
of data-dependent rotations. New features of RC6 include the use of four
working registers instead of two, and the inclusion of integer multiplication
as an additional primitive operation. The use of multiplication greatly
increases the diffusion achieved per round, allowing for greater security,
fewer rounds, and increased throughput."

This implementation requires the use of a 16-, 24-, or 32-byte key and 16-byte
blocks for encryption/decryption. Twenty rounds are performed.

=over 4

=cut

package Crypt::RC6;

require DynaLoader;

$VERSION = 1.0;
@ISA = qw/DynaLoader/;

bootstrap Crypt::RC6 $VERSION;

=back

=head1 SEE ALSO

http://www.rsa.com/rsalabs/rc6/

=head1 AUTHOR

John Hughes (jhughes@frostburg.edu)

I am indebted to Marc Lehmann, the author of the C<Crypt::Twofish2>
module, as I used his code as a guide.

=cut

1;
