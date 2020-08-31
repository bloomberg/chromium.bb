package Crypt::DSA::GMP::Signature;
use strict;
use warnings;

BEGIN {
  $Crypt::DSA::GMP::Signature::AUTHORITY = 'cpan:DANAJ';
  $Crypt::DSA::GMP::Signature::VERSION = '0.01';
}

use Carp qw( croak );

sub new {
    my ($class, %param) = @_;
    my $sig = bless { }, $class;
    if ($param{Content}) {
        return $sig->deserialize(%param);
    }
    return $sig;
}

BEGIN {
    no strict 'refs';  ## no critic (ProhibitNoStrict)
    for my $meth (qw( r s )) {
        # Values are stored as Math::BigInt objects
        *$meth = sub {
            my($key, $value) = @_;
            if (defined $value) {
              my $str;
              if (ref($value) eq 'Math::BigInt')  { $key->{$meth} = $value; }
              elsif (ref($value) eq 'Math::Pari') { $str = Math::Pari::pari2pv($value); }
              elsif (ref $value)                  { $str = "$value"; }
              elsif ($value =~ /^0x/)             { $key->{$meth} = Math::BigInt->new($value); }
              else                                { $str = $value; }
              $key->{$meth} = Math::BigInt->new("$str")
                  if defined $str && $str =~ /^\d+$/;
            } elsif (@_ > 1 && !defined $value) {
              delete $key->{$meth};
            }
            $key->{$meth};
        };
    }
}

sub _asn {
    require Convert::ASN1;
    my $asn = Convert::ASN1->new;
    $asn->prepare('SEQUENCE { r INTEGER, s INTEGER }') or croak $asn->{error};
    $asn;
}

sub deserialize {
    my ($sig, %param) = @_;
    my $asn = __PACKAGE__->_asn;
    my $ref = $asn->decode($param{Content});
    if (!$ref) {
      require MIME::Base64;
      my $base64_content = do {
        no warnings;
        MIME::Base64::decode_base64($param{Content});
      };
      $ref = $asn->decode($base64_content);
    }
    croak "Invalid Content" unless $ref;
    $sig->s($ref->{s});
    $sig->r($ref->{r});
    $sig;
}

sub serialize {
    my ($sig, %param) = @_;
    my $asn = __PACKAGE__->_asn;
    my $buf = $asn->encode({ s => $sig->s, r => $sig->r })
        or croak $asn->{error};
    $buf;
}

1;
__END__

=for stopwords deserialize

=head1 NAME

Crypt::DSA::GMP::Signature - DSA signature object

=head1 SYNOPSIS

    use Crypt::DSA::GMP::Signature;
    my $sig = Crypt::DSA::GMP::Signature->new;

    $sig->r($r);
    $sig->s($s);

=head1 DESCRIPTION

L<Crypt::DSA::GMP::Signature> represents a DSA signature. It has two
methods, L</r> and L</s>, which are the L<Math::BigInt> representations
of the two pieces of the DSA signature.

=head1 USAGE

=head2 Crypt::DSA::GMP::Signature->new( %options )

Creates a new signature object, and optionally initializes it with the
information in I<%options>, which can contain:

=over 4

=item * Content

An ASN.1-encoded string representing the DSA signature. In ASN.1 notation,
this looks like:

    SEQUENCE {
        r INTEGER,
        s INTEGER
    }

If I<Content> is provided, I<new> will automatically call the L</deserialize>
method to parse the content, and set the L</r> and L</s> methods on the
resulting L<Crypt::DSA::GMP::Signature> object.

=back

=head1 METHODS

=head2 serialize

Serializes the signature object I<$sig> into the format described above:
an ASN.1-encoded representation of the signature, using the ASN.1 syntax
above.

=head2 deserialize

Deserializes the ASN.1-encoded representation into a signature object.

=head2 r

One half of the DSA signature for a message.
This is a L<Math::BigInt> object.

=head2 s

One half of the DSA signature for a message.
This is a L<Math::BigInt> object.

=head1 AUTHOR & COPYRIGHTS

See L<Crypt::DSA::GMP> for author, copyright, and license information.

=cut
