package Convert::PEM;
use strict;
use 5.008_001;

use base qw( Class::ErrorHandler );

use MIME::Base64;
use Digest::MD5 qw( md5 );
use Convert::ASN1;
use Carp qw( croak );
use Convert::PEM::CBC;

use vars qw( $VERSION );
$VERSION = '0.08';

sub new {
    my $class = shift;
    my $pem = bless { }, $class;
    $pem->init(@_);
}

sub init {
    my $pem = shift;
    my %param = @_;
    unless (exists $param{ASN} && exists $param{Name}) {
        return (ref $pem)->error("init: Name and ASN are required");
    }
    else {
        $pem->{ASN} = $param{ASN};
        $pem->{Name} = $param{Name};
    }
    $pem->{Macro} = $param{Macro};
    my $asn = $pem->{_asn} = Convert::ASN1->new;
    $asn->prepare( $pem->{ASN} ) or
        return (ref $pem)->error("ASN prepare failed: $asn->{error}");
    $pem;
}

sub asn    { $_[0]->{_asn} }
sub ASN    { $_[0]->{ASN} }
sub name   { $_[0]->{Name} }

sub read {
    my $pem = shift;
    my %param = @_;

    my $blob;
    local *FH;
    my $fname = delete $param{Filename};
    open FH, $fname or
        return $pem->error("Can't open $fname: $!");
    $blob = do { local $/; <FH> };
    close FH;

    $param{Content} = $blob;
    $pem->decode(%param);
}

sub write {
    my $pem = shift;
    my %param = @_;

    my $fname = delete $param{Filename} or
        return $pem->error("write: Filename is required");
    my $blob = $pem->encode(%param);

    local *FH;
    open FH, ">$fname" or
        return $pem->error("Can't open $fname: $!");
    print FH $blob;
    close FH;
    $blob;
}

sub decode {
    my $pem = shift;
    my %param = @_;
    my $blob = $param{Content} or
        return $pem->error("'Content' is required");
    chomp $blob;

    my $dec = $pem->explode($blob) or return;
    my $name = $param{Name} || $pem->name;
    return $pem->error("Object $dec->{Object} does not match " . $name)
        unless $dec->{Object} eq $name;

    my $head = $dec->{Headers};
    my $buf = $dec->{Content};
    my %headers = map { $_->[0] => $_->[1] } @$head;
    if (%headers && $headers{'Proc-Type'} eq '4,ENCRYPTED') {
        $buf = $pem->decrypt( Ciphertext => $buf,
                              Info       => $headers{'DEK-Info'},
                              Password   => $param{Password} )
            or return;
    }

    my $asn = $pem->asn;
    if (my $macro = ($param{Macro} || $pem->{Macro})) {
        $asn = $asn->find($macro) or
            return $pem->error("Can't find Macro $macro");
    }
    my $obj = $asn->decode($buf) or
        return $pem->error("ASN decode failed: $asn->{error}");

    $obj;
}

sub encode {
    my $pem = shift;
    my %param = @_;

    my $asn = $pem->asn;
    if (my $macro = ($param{Macro} || $pem->{Macro})) {
        $asn = $asn->find($macro) or
            return $pem->error("Can't find Macro $macro");
    }
    my $buf = $asn->encode( $param{Content} ) or
        return $pem->error("ASN encode failed: $asn->{error}");

    my(@headers);
    if ($param{Password}) {
        my($info);
        ($buf, $info) = $pem->encrypt( Plaintext => $buf,
                                       Password  => $param{Password} )
            or return;
        push @headers, [ 'Proc-Type' => '4,ENCRYPTED' ];
        push @headers, [ 'DEK-Info'  => $info ];
    }

    $pem->implode( Object  => $param{Name} || $pem->name,
                   Headers => \@headers,
                   Content => $buf );
}

sub explode {
    my $pem = shift;
    my($message) = @_;

    # Canonicalize line endings into "\n".
    $message =~ s/\r\n|\n|\r/\n/g;

    my($head, $object, $headers, $content, $tail) = $message =~ 
        m:(-----BEGIN ([^\n\-]+)-----)\n(.*?\n\n)?(.+)(-----END .*?-----)$:s;
    my $buf = decode_base64($content);

    my @headers;
    if ($headers) {
        for my $h ( split /\n/, $headers ) {
            my($k, $v) = split /:\s*/, $h, 2;
            push @headers, [ $k => $v ] if $k;
        }
    }

    { Content => $buf,
      Object  => $object,
      Headers => \@headers }
}

sub implode {
    my $pem = shift;
    my %param = @_;
    my $head = "-----BEGIN $param{Object}-----"; 
    my $tail = "-----END $param{Object}-----";
    my $content = encode_base64( $param{Content}, '' );
    $content =~ s!(.{1,64})!$1\n!g;
    my $headers = join '',
                  map { "$_->[0]: $_->[1]\n" }
                  @{ $param{Headers} };
    $headers .= "\n" if $headers;
    "$head\n$headers$content$tail\n";
}

use vars qw( %CTYPES );
%CTYPES = ('DES-EDE3-CBC' => 'Crypt::DES_EDE3');

sub decrypt {
    my $pem = shift;
    my %param = @_;
    my $passphrase = $param{Password} || "";
    my($ctype, $iv) = split /,/, $param{Info};
    my $cmod = $CTYPES{$ctype} or
        return $pem->error("Unrecognized cipher: '$ctype'");
    $iv = pack "H*", $iv;
    my $cbc = Convert::PEM::CBC->new(
                   Passphrase => $passphrase,
                   Cipher     => $cmod,
                   IV         => $iv );
    my $buf = $cbc->decrypt($param{Ciphertext}) or
        return $pem->error("Decryption failed: " . $cbc->errstr);
    $buf;
}

sub encrypt {
    my $pem = shift;
    my %param = @_;
    $param{Password} or return $param{Plaintext};
    my $ctype = $param{Cipher} || 'DES-EDE3-CBC';
    my $cmod = $CTYPES{$ctype} or
        return $pem->error("Unrecognized cipher: '$ctype'");
    my $cbc = Convert::PEM::CBC->new(
                    Passphrase => $param{Password},
                    Cipher     => $cmod );
    my $iv = uc join '', unpack "H*", $cbc->iv;
    my $buf = $cbc->encrypt($param{Plaintext}) or
        return $pem->error("Encryption failed: " . $cbc->errstr);
    ($buf, "$ctype,$iv");
}

1;
__END__

=head1 NAME

Convert::PEM - Read/write encrypted ASN.1 PEM files

=head1 SYNOPSIS

    use Convert::PEM;
    my $pem = Convert::PEM->new(
                   Name => "DSA PRIVATE KEY",
                   ASN => qq(
                       DSAPrivateKey SEQUENCE {
                           version INTEGER,
                           p INTEGER,
                           q INTEGER,
                           g INTEGER,
                           pub_key INTEGER,
                           priv_key INTEGER
                       }
                  ));

    my $keyfile = 'private-key.pem';
    my $pwd = 'foobar';

    my $pkey = $pem->read(
                   Filename => $keyfile,
                   Password => $pwd
             );

    $pem->write(
                   Content  => $pkey,
                   Password => $pwd,
                   Filename => $keyfile
             );

=head1 DESCRIPTION

I<Convert::PEM> reads and writes PEM files containing ASN.1-encoded
objects. The files can optionally be encrypted using a symmetric
cipher algorithm, such as 3DES. An unencrypted PEM file might look
something like this:

    -----BEGIN DH PARAMETERS-----
    MB4CGQDUoLoCULb9LsYm5+/WN992xxbiLQlEuIsCAQM=
    -----END DH PARAMETERS-----

The string beginning C<MB4C...> is the Base64-encoded, ASN.1-encoded
"object."

An encrypted file would have headers describing the type of
encryption used, and the initialization vector:

    -----BEGIN DH PARAMETERS-----
    Proc-Type: 4,ENCRYPTED
    DEK-Info: DES-EDE3-CBC,C814158661DC1449

    AFAZFbnQNrGjZJ/ZemdVSoZa3HWujxZuvBHzHNoesxeyqqidFvnydA==
    -----END DH PARAMETERS-----

The two headers (C<Proc-Type> and C<DEK-Info>) indicate information
about the type of encryption used, and the string starting with
C<AFAZ...> is the Base64-encoded, encrypted, ASN.1-encoded
contents of this "object."

The initialization vector (C<C814158661DC1449>) is chosen randomly.

=head1 USAGE

=head2 $pem = Convert::PEM->new( %arg )

Constructs a new I<Convert::PEM> object designed to read/write an
object of a specific type (given in I<%arg>, see below). Returns the
new object on success, C<undef> on failure (see I<ERROR HANDLING> for
details).

I<%arg> can contain:

=over 4

=item * Name

The name of the object; when decoding a PEM-encoded stream, the name
in the encoding will be checked against the value of I<Name>.
Similarly, when encoding an object, the value of I<Name> will be used
as the name of the object in the PEM-encoded content. For example, given
the string C<FOO BAR>, the output from I<encode> will start with a
header like:

    -----BEGIN FOO BAR-----

I<Name> is a required argument.

=item * ASN

An ASN.1 description of the content to be either encoded or decoded.

I<ASN> is a required argument.

=item * Macro

If your ASN.1 description (in the I<ASN> parameter) includes more than
one ASN.1 macro definition, you will want to use the I<Macro> parameter
to specify which definition to use when encoding/decoding objects.
For example, if your ASN.1 description looks like this:

    Foo ::= SEQUENCE {
        x INTEGER,
        bar Bar
    }

    Bar ::= INTEGER

If you want to encode/decode a C<Foo> object, you will need to tell
I<Convert::PEM> to use the C<Foo> macro definition by using the I<Macro>
parameter and setting the value to C<Foo>.

I<Macro> is an optional argument.

=back

=head2 $obj = $pem->decode(%args)

Decodes, and, optionally, decrypts a PEM file, returning the
object as decoded by I<Convert::ASN1>. The difference between this
method and I<read> is that I<read> reads the contents of a PEM file
on disk; this method expects you to pass the PEM contents as an
argument.

If an error occurs while reading the file or decrypting/decoding
the contents, the function returns I<undef>, and you should check
the error message using the I<errstr> method (below).

I<%args> can contain:

=over 4

=item * Content

The PEM contents.

=item * Password

The password with which the file contents were encrypted.

If the file is encrypted, this is a mandatory argument (well, it's
not strictly mandatory, but decryption isn't going to work without
it). Otherwise it's not necessary.

=back

=head2 $blob = $pem->encode(%args)

Constructs the contents for the PEM file from an object: ASN.1-encodes
the object, optionally encrypts those contents.

Returns I<undef> on failure (encryption failure, file-writing failure,
etc.); in this case you should check the error message using the
I<errstr> method (below). On success returns the constructed PEM string.

I<%args> can contain:

=over 4

=item * Content

A hash reference that will be passed to I<Convert::ASN1::encode>,
and which should correspond to the ASN.1 description you gave to the
I<new> method. The hash reference should have the exact same format
as that returned from the I<read> method.

This argument is mandatory.

=item * Password

A password used to encrypt the contents of the PEM file. This is an
optional argument; if not provided the contents will be unencrypted.

=back

=head2 $obj = $pem->read(%args)

Reads, decodes, and, optionally, decrypts a PEM file, returning
the object as decoded by I<Convert::ASN1>. This is implemented
as a wrapper around I<decode>, with the bonus of reading the PEM
file from disk for you.

If an error occurs while reading the file or decrypting/decoding
the contents, the function returns I<undef>, and you should check
the error message using the I<errstr> method (below).

In addition to the arguments that can be passed to the I<decode>
method (minus the I<Content> method), I<%args> can contain:

=over 4

=item * Filename

The location of the PEM file that you wish to read.

=back

=head2 $pem->write(%args)

Constructs the contents for the PEM file from an object: ASN.1-encodes
the object, optionally encrypts those contents; then writes the file
to disk. This is implemented as a wrapper around I<encode>, with the
bonus of writing the file to disk for you.

Returns I<undef> on failure (encryption failure, file-writing failure,
etc.); in this case you should check the error message using the
I<errstr> method (below). On success returns the constructed PEM string.

In addition to the arguments for I<encode>, I<%args> can contain:

=over 4

=item * Filename

The location on disk where you'd like the PEM file written.

=back

=head2 $pem->errstr

Returns the value of the last error that occurred. This should only
be considered meaningful when you've received I<undef> from one of
the functions above; in all other cases its relevance is undefined.

=head2 $pem->asn

Returns the I<Convert::ASN1> object used internally to decode and
encode ASN.1 representations. This is useful when you wish to
interact directly with that object; for example, if you need to
call I<configure> on that object to set the type of big-integer
class to be used when decoding/encoding big integers:

    $pem->asn->configure( decode => { bigint => 'Math::Pari' },
                          encode => { bigint => 'Math::Pari' } );

=head1 ERROR HANDLING

If an error occurs in any of the above methods, the method will return
C<undef>. You should then call the method I<errstr> to determine the
source of the error:

    $pem->errstr

In the case that you do not yet have a I<Convert::PEM> object (that is,
if an error occurs while creating a I<Convert::PEM> object), the error
can be obtained as a class method:

    Convert::PEM->errstr

For example, if you try to decode an encrypted object, and you do not
give a passphrase to decrypt the object:

    my $obj = $pem->read( Filename => "encrypted.pem" )
        or die "Decryption failed: ", $pem->errstr;

=head1 LICENSE

Convert::PEM is free software; you may redistribute it and/or modify
it under the same terms as Perl itself.

=head1 AUTHOR & COPYRIGHTS

Except where otherwise noted, Convert::PEM is Copyright Benjamin
Trott, cpan@stupidfool.org. All rights reserved.

=cut
