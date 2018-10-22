# This code based slightly on the Systemics Crypt::CFB.
# Parts Copyright (C) 1995, 1996 Systemics Ltd (http://www.systemics.com/)
# All rights reserved.

package Crypt::OpenPGP::CFB;
use strict;

sub new {
    my $class = shift;
    my $c = bless { }, $class;
    $c->init(@_);
}

sub init {
    my $c = shift;
    my($cipher, $iv) = @_;
    $c->{cipher} = $cipher;
    $c->{blocksize} = $cipher->blocksize;
    $c->{iv} = $iv || "\0" x $c->{blocksize};
    $c;
}

sub sync { $_[0]->{unused} = '' }

sub encrypt {
    my $c = shift;
    my($data) = @_;
    my $ret = '';
    my $iv = $c->{iv};
    my $out = $c->{unused} || '';
    my $size = length $out;
    while ($data) {
        unless ($size) {
            $out = $c->{cipher}->encrypt($iv);
            $size = $c->{blocksize};
        }
        my $in = substr $data, 0, $size, '';
        $size -= (my $got = length $in);
        $iv .= ($in ^= substr $out, 0, $got, '');
        substr $iv, 0, $got, '';
        $ret .= $in;
    }
    $c->{unused} = $out;
    $c->{iv} = $iv;
    $ret;
}

sub decrypt {
    my $c = shift;
    my($data) = @_;
    my $ret = '';
    my $iv = $c->{iv};
    my $out = $c->{unused} || '';
    my $size = length $out;
    while ($data) {
        unless ($size) {
            $out = $c->{cipher}->encrypt($iv);
            $size = $c->{blocksize};
        }
        my $in = substr $data, 0, $size, '';
        $size -= (my $got = length $in);
        substr $iv .= $in, 0, $got, '';
        $ret .= ($in ^= substr $out, 0, $got, '');
    }
    $c->{unused} = $out;
    $c->{iv} = $iv;
    $ret;
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::CFB - PGP Cipher Feedback Mode

=head1 SYNOPSIS

    use Crypt::OpenPGP::CFB;

    my $key = 'foo bar';
    my $cipher = Crypt::Blowfish->new( $key );   # for example
    my $cfb = Crypt::OpenPGP::CFB->new( $cipher );

    my $plaintext = 'this is secret!';
    my $ct = $cfb->encrypt( $plaintext );

    my $pt = $cfb->decrypt( $ct );

=head1 DESCRIPTION

I<Crypt::OpenPGP::CFB> implements the variant of Cipher Feedback mode
that PGP uses in its encryption and decryption. The key difference
with PGP CFB is that the CFB state is resynchronized at each
encryption/decryption. This applies both when encrypting secret key
data and in symmetric encryption of standard encrypted data. More
differences are described in the OpenPGP RFC, in section 12.8
(OpenPGP CFB mode).

Typically you should never need to directly use I<Crypt::OpenPGP::CFB>;
I<Crypt::OpenPGP::Cipher> objects wrap around an instance of this
class and provide a uniform interface to symmetric ciphers. See
the documentation for that module for usage details.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
