package Convert::PEM::CBC;
use strict;

use Carp qw( croak );
use Digest::MD5 qw( md5 );
use base qw( Class::ErrorHandler );

sub new {
    my $class = shift;
    my $cbc = bless { }, $class;
    $cbc->init(@_);
}

sub init {
    my $cbc = shift;
    my %param = @_;
    $cbc->{iv} = exists $param{IV} ? $param{IV} :
        pack("C*", map { rand 255 } 1..8);
    croak "init: Cipher is required"
        unless my $cipher = $param{Cipher};
    if (ref($cipher)) {
        $cbc->{cipher} = $cipher;
    }
    else {
        eval "use $cipher;";
        croak "Loading '$cipher' failed: $@" if $@;
        my $key = $param{Key};
        if (!$key && exists $param{Passphrase}) {
            $key = bytes_to_key($param{Passphrase}, $cbc->{iv},
                \&md5, $cipher->keysize);
        }
        croak "init: either Key or Passphrase required"
            unless $key;
        $cbc->{cipher} = $cipher->new($key);
    }
    $cbc;
}

sub iv     { $_[0]->{iv} }

sub encrypt {
    my $cbc = shift;
    my($text) = @_;
    my $cipher = $cbc->{cipher};
    my $bs = $cipher->blocksize;
    my @blocks = $text =~ /(.{1,$bs})/ogs;
    my $last = pop @blocks if length($blocks[-1]) < $bs;
    my $iv = $cbc->{iv};
    my $buf = '';
    for my $block (@blocks) {
        $buf .= $iv = $cipher->encrypt($iv ^ $block);
    }
    $last = pack("C*", ($bs) x $bs) unless $last && length $last;
    if (length $last) {
        $last .= pack("C*", ($bs-length($last)) x ($bs-length($last)))
            if length($last) < $bs;
        $buf .= $iv = $cipher->encrypt($iv ^ $last);
    }
    $cbc->{iv} = $iv;
    $buf;
}

sub decrypt {
    my $cbc = shift;
    my($text) = @_;
    my $cipher = $cbc->{cipher};
    my $bs = $cipher->blocksize;
    my @blocks = $text =~ /(.{1,$bs})/ogs;
    my $last = length($blocks[-1]) < $bs ?
        join '', splice(@blocks, -2) : pop @blocks;
    my $iv = $cbc->{iv};
    my $buf = '';
    for my $block (@blocks) {
        $buf .= $iv ^ $cipher->decrypt($block);
        $iv = $block;
    }
    $last = pack "a$bs", $last;
    if (length($last)) {
        my $tmp = $iv ^ $cipher->decrypt($last);
        $iv = $last;
        $last = $tmp;
        my $cut = ord substr $last, -1;
        return $cbc->error("Bad key/passphrase")
            if $cut > $bs;
        substr($last, -$cut) = '';
        $buf .= $last;
    }
    $cbc->{iv} = $iv;
    $buf;
}

sub bytes_to_key {
    my($key, $salt, $md, $ks) = @_;
    my $ckey = $md->($key, $salt);
    while (length($ckey) < $ks) {
        $ckey .= $md->($ckey, $key, $salt);
    }
    substr $ckey, 0, $ks;
}

1;
__END__

=head1 NAME

Convert::PEM::CBC - Cipher Block Chaining Mode implementation

=head1 SYNOPSIS

    use Convert::PEM::CBC;
    my $cbc = Convert::PEM::CBC->new(
                         Cipher     => 'Crypt::DES_EDE3',
                         Passphrase => 'foo'
           );

    my $plaintext = 'foo bar baz';
    $cbc->encrypt($plaintext);

=head1 DESCRIPTION

I<Convert::PEM::CBC> implements the CBC (Cipher Block Chaining)
mode for encryption/decryption ciphers; the CBC is designed for
compatability with OpenSSL and may not be compatible with other
implementations (such as SSH).

=head1 USAGE

=head2 $cbc = Convert::PEM::CBC->new(%args)

Creates a new I<Convert::PEM::CBC> object and initializes it.
Returns the new object.

I<%args> can contain:

=over 4

=item * Cipher

Either the name of an encryption cipher class (eg. I<Crypt::DES>),
or an object already blessed into such a class. The class must
support the I<keysize>, I<blocksize>, I<encrypt>, and I<decrypt>
methods. If the value is a blessed object, it is assumed that the
object has already been initialized with a key.

This argument is mandatory.

=item * Passphrase

A passphrase to encrypt/decrypt the content. This is different in
implementation from a key (I<Key>), because it is assumed that a
passphrase comes directly from a user, and must be munged into the
correct form for a key. This "munging" is done by repeatedly
computing an MD5 hash of the passphrase, the IV, and the existing
hash, until the generated key is longer than the keysize for the
cipher (I<Cipher>).

Because of this "munging", this argument can be any length (even
an empty string).

If you give the I<Cipher> argument an object, this argument is
ignored. If the I<Cipher> argument is a cipher class, either this
argument or I<Key> must be provided.

=item * Key

A raw key, to be passed directly to the new cipher object. Because
this is passed directly to the cipher itself, the length of the
key must be equal to or greater than the keysize for the I<Cipher>.

As with the I<Passphrase> argument, if you give the I<Cipher>
argument an already-constructed cipher object, this argument is
ignored. If the I<Cipher> argument is a cipher class, either this
argument or I<Passphrase> must be provided.

=item * IV

The initialization vector for CBC mode.

This argument is optional; if not provided, a random IV will be
generated. Obviously, if you're decrypting data, you should provide
this argument, because your IV should match the IV used to encrypt
the data.

=back

=head2 $cbc->encrypt($plaintext)

Encrypts the plaintext I<$plaintext> using the underlying cipher
implementation in CBC mode, and returns the ciphertext.

If any errors occur, returns I<undef>, and you should check the
I<errstr> method to find out what went wrong.

=head2 $cbc->decrypt($ciphertext)

Decrypts the ciphertext I<$ciphertext> using the underlying
cipher implementation in CBC mode, and returns the plaintext.

If any errors occur, returns I<undef>, and you should check the
I<errstr> method to find out what went wrong.

=head2 $cbc->iv

Returns the current initialization vector. One use for this might be
to grab the initial value of the IV if it's created randomly (ie.
you haven't provided an I<IV> argument to I<new>):

    my $cbc = Convert::PEM::CBC->new( Cipher => $cipher );
    my $iv = $cbc->iv;   ## Generated randomly in 'new'.

I<Convert::PEM> uses this to write the IV to the PEM file when
encrypting, so that it can be known when trying to decrypt the
file.

=head2 $cbc->errstr

Returns the value of the last error that occurred. This should only
be considered meaningful when you've received I<undef> from one of
the functions above; in all other cases its relevance is undefined.

=head1 AUTHOR & COPYRIGHTS

Please see the Convert::PEM manpage for author, copyright, and
license information.

=cut
