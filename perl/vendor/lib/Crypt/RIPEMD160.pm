package Crypt::RIPEMD160;

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);

require Exporter;
require DynaLoader;
require AutoLoader;
@ISA = qw(Exporter AutoLoader DynaLoader);

# Items to export into callers namespace by default
@EXPORT = qw();

@EXPORT_OK = qw();

$VERSION = '0.06';

bootstrap Crypt::RIPEMD160 $VERSION;

#package RIPEMD160; # Package-Definition like in Crypt::IDEA

#use strict;
use Carp;

sub addfile
{
    no strict 'refs';
    my ($self, $handle) = @_;
    my ($package, $file, $line) = caller;
    my ($data);

    if (!ref($handle)) {
	$handle = $package . "::" . $handle unless ($handle =~ /(\:\:|\')/);
    }
    while (read($handle, $data, 8192)) {
	$self->add($data);
    }
}

sub hexdigest
{
    my ($self) = shift;
    my ($tmp);

    $tmp = unpack("H*", ($self->digest()));
    return(substr($tmp, 0,8) . " " . substr($tmp, 8,8) . " " .
	   substr($tmp,16,8) . " " . substr($tmp,24,8) . " " .
	   substr($tmp,32,8));
}

sub hash
{
    my($self, $data) = @_;

    if (ref($self)) {
	$self->reset();
    } else {
	$self = new Crypt::RIPEMD160;
    }
    $self->add($data);
    $self->digest();
}

sub hexhash
{
    my($self, $data) = @_;

    if (ref($self)) {
	$self->reset();
    } else {
	$self = new Crypt::RIPEMD160;
    }
    $self->add($data);
    $self->hexdigest();
}

1;
__END__
# Below is the stub of documentation for your module. You better edit it!

=head1 NAME

Crypt::RIPEMD160 - Perl extension for the RIPEMD-160 Hash function

=head1 SYNOPSIS

    use Crypt::RIPEMD160;
    
    $context = new Crypt::RIPEMD160;
    $context->reset();
    
    $context->add(LIST);
    $context->addfile(HANDLE);
    
    $digest = $context->digest();
    $string = $context->hexdigest();

    $digest = Crypt::RIPEMD160->hash(SCALAR);
    $string = Crypt::RIPEMD160->hexhash(SCALAR);

=head1 DESCRIPTION

The B<Crypt::RIPEMD160> module allows you to use the RIPEMD160
Message Digest algorithm from within Perl programs.

The module is based on the implementation from Antoon Bosselaers from
Katholieke Universiteit Leuven. 

A new RIPEMD160 context object is created with the B<new> operation.
Multiple simultaneous digest contexts can be maintained, if desired.
The context is updated with the B<add> operation which adds the
strings contained in the I<LIST> parameter. Note, however, that
C<add('foo', 'bar')>, C<add('foo')> followed by C<add('bar')> and
C<add('foobar')> should all give the same result.

The final message digest value is returned by the B<digest> operation
as a 20-byte binary string. This operation delivers the result of
B<add> operations since the last B<new> or B<reset> operation. Note
that the B<digest> operation is effectively a destructive, read-once
operation. Once it has been performed, the context must be B<reset>
before being used to calculate another digest value.

Several convenience functions are also provided. The B<addfile>
operation takes an open file-handle and reads it until end-of file in
8192 byte blocks adding the contents to the context. The file-handle
can either be specified by name or passed as a type-glob reference, as
shown in the examples below. The B<hexdigest> operation calls
B<digest> and returns the result as a printable string of hexdecimal
digits. This is exactly the same operation as performed by the
B<unpack> operation in the examples below.

The B<hash> operation can act as either a static member function (ie
you invoke it on the RIPEMD160 class as in the synopsis above) or as a
normal virtual function. In both cases it performs the complete RIPEMD160
cycle (reset, add, digest) on the supplied scalar value. This is
convenient for handling small quantities of data. When invoked on the
class a temporary context is created. When invoked through an already
created context object, this context is used. The latter form is
slightly more efficient. The B<hexhash> operation is analogous to
B<hexdigest>.

=head1 EXAMPLES

    use Crypt::RIPEMD160;
    
    $ripemd160 = new Crypt::RIPEMD160;
    $ripemd160->add('foo', 'bar');
    $ripemd160->add('baz');
    $digest = $ripemd160->digest();
    
    print("Digest is " . unpack("H*", $digest) . "\n");

The above example would print out the message

    Digest is f137cb536c05ec2bc97e73327937b6e81d3a4cc9

provided that the implementation is working correctly.

Remembering the Perl motto ("There's more than one way to do it"), the
following should all give the same result:

    use Crypt::RIPEMD160;
    $ripemd160 = new Crypt::RIPEMD160;

    die "Can't open /etc/passwd ($!)\n" unless open(P, "/etc/passwd");

    seek(P, 0, 0);
    $ripemd160->reset;
    $ripemd160->addfile(P);
    $d = $ripemd160->hexdigest;
    print "addfile (handle name) = $d\n";

    seek(P, 0, 0);
    $ripemd160->reset;
    $ripemd160->addfile(\*P);
    $d = $ripemd160->hexdigest;
    print "addfile (type-glob reference) = $d\n";

    seek(P, 0, 0);
    $ripemd160->reset;
    while (<P>)
    {
        $ripemd160->add($_);
    }
    $d = $ripemd160->hexdigest;
    print "Line at a time = $d\n";

    seek(P, 0, 0);
    $ripemd160->reset;
    $ripemd160->add(<P>);
    $d = $ripemd160->hexdigest;
    print "All lines at once = $d\n";

    seek(P, 0, 0);
    $ripemd160->reset;
    while (read(P, $data, (rand % 128) + 1))
    {
        $ripemd160->add($data);
    }
    $d = $ripemd160->hexdigest;
    print "Random chunks = $d\n";

    seek(P, 0, 0);
    $ripemd160->reset;
    undef $/;
    $data = <P>;
    $d = $ripemd160->hexhash($data);
    print "Single string = $d\n";

    close(P);

=head1 NOTE

The RIPEMD160 extension may be redistributed under the same terms as Perl.
The RIPEMD160 algorithm is published in "Fast Software Encryption, LNCS 1039,
D. Gollmann (Ed.), pp.71-82".

The basic C code implementing the algorithm is covered by the
following copyright:

=over 1

C<
/********************************************************************\
 *
 *      FILE:     rmd160.c
 *
 *      CONTENTS: A sample C-implementation of the RIPEMD-160
 *                hash-function.
 *      TARGET:   any computer with an ANSI C compiler
 *
 *      AUTHOR:   Antoon Bosselaers, ESAT-COSIC
 *      DATE:     1 March 1996
 *      VERSION:  1.0
 *
 *      Copyright (c) Katholieke Universiteit Leuven
 *      1996, All Rights Reserved
 *
\********************************************************************/
>

=back

=head1 RIPEMD-160 test suite results (ASCII):

C<
* message: "" (empty string)
  hashcode: 9c1185a5c5e9fc54612808977ee8f548b2258d31
* message: "a"
  hashcode: 0bdc9d2d256b3ee9daae347be6f4dc835a467ffe
* message: "abc"
  hashcode: 8eb208f7e05d987a9b044a8e98c6b087f15a0bfc
* message: "message digest"
  hashcode: 5d0689ef49d2fae572b881b123a85ffa21595f36
* message: "abcdefghijklmnopqrstuvwxyz"
  hashcode: f71c27109c692c1b56bbdceb5b9d2865b3708dbc
* message: "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
  hashcode: 12a053384a9c0c88e405a06c27dcf49ada62eb2b
* message: "A...Za...z0...9"
  hashcode: b0e20b6e3116640286ed3a87a5713079b21f5189
* message: 8 times "1234567890"
  hashcode: 9b752e45573d4b39f4dbd3323cab82bf63326bfb
* message: 1 million times "a"
  hashcode: 52783243c1697bdbe16d37f97f68f08325dc1528
>

This copyright does not prohibit distribution of any version of Perl
containing this extension under the terms of the GNU or Artistic
licences.

=head1 AUTHOR

The RIPEMD-160 interface was written by Christian H. Geuer-Pollmann (CHGEUER)
(C<geuer-pollmann@nue.et-inf.uni.siegen.de>) and Ken Neighbors (C<ken@nsds.com>).

=head1 SEE ALSO

MD5(3pm) and SHA(1).

=cut
