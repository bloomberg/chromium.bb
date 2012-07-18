# $Id: Buffer.pm,v 1.9 2001/07/28 06:36:50 btrott Exp $

package Data::Buffer;
use strict;

use vars qw( $VERSION );
$VERSION = '0.04';

sub new {
    my $class = shift;
    my %arg = @_;
    bless { buf => "", offset => 0, template => "" }, $class;
}

sub new_with_init {
    my $class = shift;
    my $buf = $class->new;
    $buf->append($_) for @_;
    $buf;
}

sub extract {
    my $buf = shift;
    my($nbytes) = @_;
    my $new = ref($buf)->new;
    $new->append( $buf->get_bytes($nbytes) );
    $new;
}

sub empty {
    my $buf = shift;
    $buf->{buf} = "";
    $buf->{offset} = 0;
    $buf->{template} = "";
}

sub set_offset { $_[0]->{offset} = $_[1] }
sub reset_offset { $_[0]->set_offset(0) }

sub insert_template {
    my $buf = shift;
    $buf->bytes(0, 0, $buf->{template} . chr(0));
}

sub append {
    my $buf = shift;
    $buf->{buf} .= $_[0];
}

sub bytes {
    my $buf = shift;
    my($off, $len, $rep) = @_;
    $off ||= 0;
    $len = length $buf->{buf} unless defined $len;
    return defined $rep ?
        substr($buf->{buf}, $off, $len, $rep) :
        substr($buf->{buf}, $off, $len);
}

sub length { length $_[0]->{buf} }
sub offset { $_[0]->{offset} }
sub template { $_[0]->{template} }

sub dump {
    my $buf = shift;
    my @r;
    for my $c (split //, $buf->bytes(@_)) {
        push @r, sprintf "%02x", ord $c;
        push @r, "\n" unless @r % 24;
    }
    join ' ', @r
}

sub get_all {
    my $buf = shift;
    my($tmpl, $data) = $buf->{buf} =~ /^([NYaCn\d]+)\0(.+)$/s or
        die "Buffer $buf does not appear to contain a template";
    my $b = __PACKAGE__->new;
    $b->append($data);
    my @tmpl = split //, $tmpl;
    my @data;
    while (@tmpl) {
        my $el = shift @tmpl;
        if ($el eq "N") {
            next if $tmpl[0] eq "Y";  ## Peek ahead: is it a string?
            push @data, $b->get_int32;
        }
        elsif ($el eq "n") {
            push @data, $b->get_int16;
        }
        elsif ($el eq "C") {
            push @data, $b->get_int8;
        }
        elsif ($el eq "a") {
            my $len = shift @tmpl;
            push @data, $b->get_char for 1..$len;
        }
        elsif ($el eq "Y") {
            push @data, $b->get_str;
        }
        else {
            die "Unrecognized template token: $el";
        }
    }
    @data;
}

sub get_int8 {
    my $buf = shift;
    my $off = defined $_[0] ? shift : $buf->{offset};
    $buf->{offset} += 1;
    unpack "C", $buf->bytes($off, 1);
}

sub put_int8 {
    my $buf = shift;
    $buf->{buf} .= pack "C", $_[0];
    $buf->{template} .= "C";
}

sub get_int16 {
    my $buf = shift;
    my $off = defined $_[0] ? shift : $buf->{offset};
    $buf->{offset} += 2;
    unpack "n", $buf->bytes($off, 2);
}

sub put_int16 {
    my $buf = shift;
    $buf->{buf} .= pack "n", $_[0];
    $buf->{template} .= "n";
}

sub get_int32 {
    my $buf = shift;
    my $off = defined $_[0] ? shift : $buf->{offset};
    $buf->{offset} += 4;
    unpack "N", $buf->bytes($off, 4);
}

sub put_int32 {
    my $buf = shift;
    $buf->{buf} .= pack "N", $_[0];
    $buf->{template} .= "N";
}

sub get_char {
    my $buf = shift;
    my $off = defined $_[0] ? shift : $buf->{offset};
    $buf->{offset}++;
    $buf->bytes($off, 1);
}

sub put_char {
    my $buf = shift;
    $buf->{buf} .= $_[0];
    $buf->{template} .= "a" . CORE::length($_[0]);
}

sub get_bytes {
    my $buf = shift;
    my($nbytes) = @_;
    my $d = $buf->bytes($buf->{offset}, $nbytes);
    $buf->{offset} += $nbytes;
    $d;
}

sub put_bytes {
    my $buf = shift;
    my($str, $nbytes) = @_;
    $buf->{buf} .= $nbytes ? substr($str, 0, $nbytes) : $str;
    $buf->{template} .= "a" . ($nbytes ? $nbytes : CORE::length($str));
}

*put_chars = \&put_char;

sub get_str {
    my $buf = shift;
    my $off = defined $_[0] ? shift : $buf->{offset};
    my $len = $buf->get_int32;
    $buf->{offset} += $len;
    $buf->bytes($off+4, $len);
}

sub put_str {
    my $buf = shift;
    my $str = shift;
    $str = "" unless defined $str;
    $buf->put_int32(CORE::length($str));
    $buf->{buf} .= $str;
    $buf->{template} .= "Y";
}

1;
__END__

=head1 NAME

Data::Buffer - Read/write buffer class

=head1 SYNOPSIS

    use Data::Buffer;
    my $buffer = Data::Buffer->new;

    ## Add a 32-bit integer.
    $buffer->put_int32(10932930);

    ## Get it back.
    my $int = $buffer->get_int32;

=head1 DESCRIPTION

I<Data::Buffer> implements a low-level binary buffer in which
you can get and put integers, strings, and other data.
Internally the implementation is based on C<pack> and C<unpack>,
such that I<Data::Buffer> is really a layer on top of those
built-in functions.

All of the I<get_*> and I<put_*> methods respect the
internal offset state in the buffer object. This means that
you should read data out of the buffer in the same order that
you put it in. For example:

    $buf->put_int16(24);
    $buf->put_int32(1233455);
    $buf->put_int16(99);

    $buf->get_int16;   # 24
    $buf->get_int32;   # 1233455
    $buf->get_int16;   # 99

Of course, this assumes that you I<know> the order of the data
items in the buffer. If your setup is such that your sending
and receiving processes won't necessarily know what's inside
the buffers they receive, take a look at the I<TEMPLATE USAGE>
section.

=head1 USAGE

=head2 Data::Buffer->new

Creates a new buffer object and returns it. The buffer is
initially empty.

This method takes no arguments.

=head2 Data::Buffer->new_with_init(@strs)

Creates a new buffer object and appends to it each of the
octet strings in I<@strs>.

Returns the new buffer object.

=head2 $buffer->get_int8

Returns the next 8-bit integer from the buffer (which
is really just the ASCII code for the next character/byte
in the buffer).

=head2 $buffer->put_int8

Appends an 8-bit integer to the buffer (which is really
just the character corresponding to that integer, in
ASCII).

=head2 $buffer->get_int16

Returns the next 16-bit integer from the buffer.

=head2 $buffer->put_int16($integer)

Appends a 16-bit integer to the buffer.

=head2 $buffer->get_int32

Returns the next 32-bit integer from the buffer.

=head2 $buffer->put_int32($integer)

Appends a 32-bit integer to the buffer.

=head2 $buffer->get_char

More appropriately called I<get_byte>, perhaps, this
returns the next byte from the buffer.

=head2 $buffer->put_char($bytes)

Appends a byte (or a sequence of bytes) to the buffer.
There is no restriction on the length of the byte
string I<$bytes>; if it makes you uncomfortable to call
I<put_char> to put multiple bytes, you can instead
call this method as I<put_chars>. It's the same thing.

=head2 $buffer->get_bytes($n)

Grabs I<$n> bytes from the buffer, where I<$n> is a positive
integer. Increments the internal offset state by I<$n>.

=head2 $buffer->put_bytes($bytes [, $n ])

Appends a sequence of bytes to the buffer; if I<$n> is
unspecified, appends the entire length of I<$bytes>.
Otherwise appends only the first I<$n> bytes of I<$bytes>.

=head2 $buffer->get_str

Returns the next "string" from the buffer. A string here
is represented as the length of the string (a 32-bit
integer) followed by the string itself.

=head2 $buffer->put_str($string)

Appends a string (32-bit integer length and the string
itself) to the buffer.

=head2 $buffer->extract($n)

Extracts the next I<$n> bytes from the buffer I<$buffer>,
increments the offset state in I<$buffer>, and returns a
new buffer object containing the extracted bytes.

=head1 TEMPLATE USAGE

Generally when you use I<Data::Buffer> it's to communicate
with another process (perhaps a C program) that bundles up
its data into binary buffers. In those cases, it's very likely
that the data will be in some well-known order in the buffer:
in other words, it might be documented that a certain C program
creates a buffer containing:

=over 4

=item * an int8

=item * a string

=item * an int32

=back

In this case, you would presumably know about the order of the
data in the buffer, and you could extract it accordingly:

    $buffer->get_int8;
    $buffer->get_str;
    $buffer->get_int32;

In other cases, however, there may not be a well-defined order
of data items in the buffer. This might be the case if you're
inventing your own protocol, and you want your binary buffers
to "know" about their contents. In this case, you'll want to
use the templating features of I<Data::Buffer>.

When you use the I<put_> methods to place data in a buffer,
I<Data::Buffer> keeps track of the types of data that you're
inserting in a template description of the buffer. This template
contains all of the information necessary for a process to
receive a buffer and extract the data in the buffer without
knowledge of the order of the items.

To use this feature, simply use the I<insert_template> method
after you've filled your buffer to completion. For example:

    my $buffer = Data::Buffer->new;
    $buffer->put_str("foo");
    $buffer->put_int32(9999);
    $buffer->insert_template;

    ## Ship off the buffer to another process.

The receiving process should then invoke the I<get_all> method
on the buffer to extract all of the data:

    my $buffer = Data::Buffer->new;
    $buffer->append( $received_buffer_data );
    my @data = $buffer->get_all;

@data will now contain two elements: C<"foo"> and C<9999>.

=head1 LOW-LEVEL METHODS

=head2 $buffer->append($bytes)

Appends raw data I<$bytes> to the end of the in-memory
buffer. Generally you don't need to use this method
unless you're initializing an empty buffer, because
when you need to add data to a buffer you should
generally use one of the I<put_*> methods.

=head2 $buffer->empty

Empties out the buffer object.

=head2 $buffer->bytes([ $offset [, $length [, $replacement ]]])

Behaves exactly like the I<substr> built-in function,
except on the buffer I<$buffer>. Given no arguments,
I<bytes> returns the entire buffer; given one argument
I<$offset>, returns everything from that position to
the end of the string; given I<$offset> and I<$length>,
returns the segment of the buffer starting at I<$offset>
and consisting of I<$length> bytes; and given all three
arguments, replaces that segment with I<$replacement>.

This is a very low-level method, and you generally
won't need to use it.

Also be warned that you should not intermix use of this
method with use of the I<get_*> and I<put_*> methods;
the latter classes of methods maintain internal state
of the buffer offset where arguments will be gotten from
and put, respectively. The I<bytes> method gives no
thought to this internal offset state.

=head2 $buffer->length

Returns the length of the buffer object.

=head2 $buffer->offset

Returns the internal offset state.

If you insist on intermixing calls to I<bytes> with calls
to the I<get_*> and I<put_*> methods, you'll probably
want to use this method to get some status on that
internal offset.

=head2 $buffer->set_offset($offset)

Sets the internal offset state to I<$offset>.

=head2 $buffer->reset_offset

Sets the internal offset state to 0.

=head2 $buffer->dump(@args)

Returns a hex dump of the buffer. The dump is of the I<entire>
buffer I<$buffer>; in other words, I<dump> doesn't respect the
internal offset pointer.

I<@args> is passed directly through to the I<bytes> method,
which means that you can supply arguments to emulate support
of the internal offset:

    my $dump = $buffer->dump($buffer->offset);

=head2 $buffer->insert_padding

A helper method: pads out the buffer so that the length
of the transferred packet will be evenly divisible by
8, which is a requirement of the SSH protocol.

=head1 AUTHOR & COPYRIGHTS

Benjamin Trott, ben@rhumba.pair.com

Except where otherwise noted, Data::Buffer is Copyright 2001
Benjamin Trott. All rights reserved. Data::Buffer is free
software; you may redistribute it and/or modify it under
the same terms as Perl itself.

=cut
