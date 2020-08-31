package Crypt::DSA::Key::PEM;

use strict;
use Carp qw( croak );
use Convert::PEM;
use Crypt::DSA::Key;

use vars qw{$VERSION @ISA};
BEGIN {
    $VERSION = '1.17';
    @ISA     = 'Crypt::DSA::Key';
}

sub deserialize {
    my $key = shift;
    my %param = @_;
    $param{Content} =~ /DSA PRIVATE KEY/ ?
        $key->_deserialize_privkey(%param) :
        $key->_deserialize_pubkey(%param);
}

sub _deserialize_privkey {
    my $key = shift;
    my %param = @_;

    my $pem = $key->_pem;
    my $pkey = $pem->decode( Content  => $param{Content},
                             Password => $param{Password},
                             Macro    => 'DSAPrivateKey' );
    return unless $pkey;

    for my $m (qw( p q g pub_key priv_key )) {
        $key->$m( $pkey->{$m} );
    }
    $key;
}

sub _deserialize_pubkey {
    my $key = shift;
    my %param = @_;

    my $pem = $key->_pem;
    my $pkey = $pem->decode( Content  => $param{Content},
                             Password => $param{Password},
                             Macro    => 'DSAPublicKey',
                             Name     => 'PUBLIC KEY' );
    return unless $pkey;

    my $asn = $pem->asn->find('DSAPubKeyInner');
    my $num = $asn->decode($pkey->{pub_key}[0]) or croak $asn->{error};

    for my $m (qw( p q g )) {
        $key->$m( $pkey->{inner}{DSAParams}{$m} );
    }
    $key->pub_key($num);

    $key;
}

sub serialize {
    my $key = shift;
    ## If this is a private key (has the private key portion), serialize
    ## it as a private key; otherwise use a public key ASN.1 object.
    $key->priv_key ? $key->_serialize_privkey(@_) : $key->_serialize_pubkey(@_);
}

sub _serialize_privkey {
    my $key = shift;
    my %param = @_;

    my $pkey = { version => 0 };
    for my $m (qw( p q g pub_key priv_key )) {
        $pkey->{$m} = $key->$m();
    }

    my $pem = $key->_pem;
    my $buf = $pem->encode(
            Content  => $pkey,
            Password => $param{Password},
            Name     => 'DSA PRIVATE KEY',
            Macro    => 'DSAPrivateKey',
        ) or croak $pem->errstr;
    $buf;
}

sub _serialize_pubkey {
    my $key = shift;
    my %param = @_;
    my $pem = $key->_pem;
    my $asn = $pem->asn->find('DSAPubKeyInner');
    ## Force stringification.
    my $str = $asn->encode($key->pub_key . '') or croak $asn->{error};
    my $pkey = {
        inner => {
            objId => '1.2.840.10040.4.1',
            DSAParams => {
                p => $key->p,
                q => $key->q,
                g => $key->g
            },
        },
        pub_key => $str
    };
    my $buf = $pem->encode(
            Content  => $pkey,
            Password => $param{Password},
            Name     => 'PUBLIC KEY',
            Macro    => 'DSAPublicKey',
        ) or return $key->error($pem->errstr);
    $buf;
}

sub _pem {
    my $key = shift;
    unless (defined $key->{__pem}) {
        my $pem = Convert::PEM->new(
            Name => "DSA PRIVATE KEY",
            ASN  => qq(
                DSAPrivateKey ::= SEQUENCE {
                    version INTEGER,
                    p INTEGER,
                    q INTEGER,
                    g INTEGER,
                    pub_key INTEGER,
                    priv_key INTEGER
                }

                DSAPublicKey ::= SEQUENCE {
                    inner SEQUENCE {
                        objId OBJECT IDENTIFIER,
                        DSAParams SEQUENCE {
                            p INTEGER,
                            q INTEGER,
                            g INTEGER
                        }
                    }
                    pub_key BIT STRING
                }

                DSAPubKeyInner ::= INTEGER
        ));
        $key->{__pem} = $pem;
    }
    $key->{__pem};
}

1;
__END__

=head1 NAME

Crypt::DSA::Key::PEM - Read/write DSA PEM files

=head1 SYNOPSIS

    use Crypt::DSA::Key;
    my $key = Crypt::DSA::Key->new( Type => 'PEM', ...);
    $key->write( Type => 'PEM', ...);

=head1 DESCRIPTION

I<Crypt::DSA::Key::PEM> provides an interface to reading and
writing DSA PEM files, using I<Convert::PEM>. The files are
ASN.1-encoded and optionally encrypted.

You shouldn't use this module directly. As the SYNOPSIS above
suggests, this module should be considered a plugin for
I<Crypt::DSA::Key>, and all access to PEM files (reading DSA
keys from disk, etc.) should be done through that module.

Read the I<Crypt::DSA::Key> documentation for more details.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::DSA manpage for author, copyright,
and license information.

=cut
