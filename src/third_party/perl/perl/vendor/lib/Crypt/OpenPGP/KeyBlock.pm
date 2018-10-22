package Crypt::OpenPGP::KeyBlock;
use strict;

use Crypt::OpenPGP::PacketFactory;

sub primary_uid {
    $_[0]->{pkt}{ 'Crypt::OpenPGP::UserID' } ?
        $_[0]->{pkt}{ 'Crypt::OpenPGP::UserID' }->[0]->id : undef;
}

sub key { $_[0]->get('Crypt::OpenPGP::Certificate')->[0] }
sub subkey { $_[0]->get('Crypt::OpenPGP::Certificate')->[1] }

sub encrypting_key {
    my $kb = shift;
    my $keys = $kb->get('Crypt::OpenPGP::Certificate');
    return unless $keys && @$keys;
    for my $key (@$keys) {
        return $key if $key->can_encrypt;
    }
}

sub signing_key {
    my $kb = shift;
    my $keys = $kb->get('Crypt::OpenPGP::Certificate');
    return unless $keys && @$keys;
    for my $key (@$keys) {
        return $key if $key->can_sign;
    }
}

sub key_by_id { $_[0]->{keys_by_id}->{$_[1]} ||
                $_[0]->{keys_by_short_id}->{$_[1]} }

sub new {
    my $class = shift;
    my $kb = bless { }, $class;
    $kb->init(@_);
}

sub init {
    my $kb = shift;
    $kb->{pkt} = { };
    $kb->{order} = [ ];
    $kb->{keys_by_id} = { };
    $kb;
}

sub add {
    my $kb = shift;
    my($pkt) = @_;
    push @{ $kb->{pkt}->{ ref($pkt) } }, $pkt;
    push @{ $kb->{order} }, $pkt;
    if (ref($pkt) eq 'Crypt::OpenPGP::Certificate') {
        my $kid = $pkt->key_id;
        $kb->{keys_by_id}{ $kid } = $pkt;
        $kb->{keys_by_short_id}{ substr $kid, -4, 4 } = $pkt;
    }
}

sub get { $_[0]->{pkt}->{ $_[1] } }

sub save {
    my $kb = shift;
    Crypt::OpenPGP::PacketFactory->save( @{ $kb->{order} } );
}

sub save_armoured {
    my $kb = shift;
    require Crypt::OpenPGP::Armour;
    Crypt::OpenPGP::Armour->armour(
                Data => $kb->save,
                Object => 'PUBLIC KEY BLOCK'
        );
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::KeyBlock - Key block object

=head1 SYNOPSIS

    use Crypt::OpenPGP::KeyBlock;

    my $packet = Crypt::OpenPGP::UserID->new( Identity => 'foo' );
    my $kb = Crypt::OpenPGP::KeyBlock->new;
    $kb->add($packet);

    my $serialized = $kb->save;

=head1 DESCRIPTION

I<Crypt::OpenPGP::KeyBlock> represents a single keyblock in a keyring.
A key block is essentially just a set of associated keys containing
exactly one master key, zero or more subkeys, some user ID packets, some
signatures, etc. The key is that there is only one master key
associated with each keyblock.

=head1 USAGE

=head2 Crypt::OpenPGP::KeyBlock->new

Constructs a new key block object and returns that object.

=head2 $kb->encrypting_key

Returns the key that performs encryption in this key block. For example,
if a DSA key is the master key in a key block with an ElGamal subkey,
I<encrypting_key> returns the ElGamal subkey certificate, because DSA
keys do not perform encryption/decryption.

=head2 $kb->signing_key

Returns the key that performs signing in this key block. For example,
if a DSA key is the master key in a key block with an ElGamal subkey,
I<encrypting_key> returns the DSA master key certificate, because DSA
supports signing/verification.

=head2 $kb->add($packet)

Adds the packet I<$packet> to the key block. If the packet is a PGP
certificate (a I<Crypt::OpenPGP::Certificate> object), the certificate
is also added to the internal key-management mechanism.

=head2 $kb->save

Serializes each of the packets contained in the I<KeyBlock> object,
in order, and returns the serialized data. This output can then be
fed to I<Crypt::OpenPGP::Armour> for ASCII-armouring, for example,
or can be written out to a keyring file.

=head2 $kb->save_armoured

Saves an armoured version of the keyblock (this is useful for exporting
public keys).

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
