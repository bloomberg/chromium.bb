package IO::Compress::Xz ;

use strict ;
use warnings;
use bytes;
require Exporter ;

use IO::Compress::Base 2.052 ;
use IO::Compress::Base::Common  2.052 qw(createSelfTiedObject);
use IO::Compress::Adapter::Xz 2.052 ;
use Compress::Raw::Lzma  2.052 ;


our ($VERSION, @ISA, @EXPORT_OK, %EXPORT_TAGS, $XzError);

$VERSION = '2.052';
$XzError = '';

@ISA    = qw(Exporter IO::Compress::Base);
@EXPORT_OK = qw( $XzError xz ) ;
%EXPORT_TAGS = %IO::Compress::Base::EXPORT_TAGS ;

push @{ $EXPORT_TAGS{constants} }, @Compress::Raw::Lzma::EXPORT;
$EXPORT_TAGS{all} = $EXPORT_TAGS{constants} ;

push @{ $EXPORT_TAGS{all} }, @EXPORT_OK ;
Exporter::export_ok_tags('all');


sub new
{
    my $class = shift ;

    my $obj = createSelfTiedObject($class, \$XzError);
    return $obj->_create(undef, @_);
}

sub xz
{
    my $obj = createSelfTiedObject(undef, \$XzError);
    $obj->_def(@_);
}


sub mkHeader 
{
    my $self = shift ;
    return '';

}

sub getExtraParams
{
    my $self = shift ;

    use IO::Compress::Base::Common  2.052 qw(:Parse);
    use Compress::Raw::Lzma 2.052 qw(LZMA_PRESET_DEFAULT LZMA_CHECK_CRC32) ;
    
    return (
        'Preset'        => [1, 1, Parse_unsigned, LZMA_PRESET_DEFAULT],
        'Extreme'       => [1, 1, Parse_boolean, 0],
        'Check'         => [1, 1, Parse_unsigned, LZMA_CHECK_CRC32],
        );
}



sub ckParams
{
    my $self = shift ;
    my $got = shift;

    # TODO - validate the parameters
    
    return 1 ;
}


sub mkComp
{
    my $self = shift ;
    my $got = shift ;

    my ($obj, $errstr, $errno) 
        = IO::Compress::Adapter::Xz::mkCompObject($got->value('Preset'),
                                                  $got->value('Extreme'),
                                                  $got->value('Check')
                                                 );

    return $self->saveErrorString(undef, $errstr, $errno)
        if ! defined $obj;
    
    return $obj;
}


sub mkTrailer
{
    my $self = shift ;
    return '';
}

sub mkFinalTrailer
{
    return '';
}

#sub newHeader
#{
#    my $self = shift ;
#    return '';
#}

sub getInverseClass
{
    return ('IO::Uncompress::UnXz');
}

sub getFileInfo
{
    my $self = shift ;
    my $params = shift;
    my $file = shift ;
    
}

1;

__END__

=head1 NAME

IO::Compress::Xz - Write xz files/buffers
 
 

=head1 SYNOPSIS

    use IO::Compress::Xz qw(xz $XzError) ;

    my $status = xz $input => $output [,OPTS] 
        or die "xz failed: $XzError\n";

    my $z = new IO::Compress::Xz $output [,OPTS]
        or die "xz failed: $XzError\n";

    $z->print($string);
    $z->printf($format, $string);
    $z->write($string);
    $z->syswrite($string [, $length, $offset]);
    $z->flush();
    $z->tell();
    $z->eof();
    $z->seek($position, $whence);
    $z->binmode();
    $z->fileno();
    $z->opened();
    $z->autoflush();
    $z->input_line_number();
    $z->newStream( [OPTS] );
    
    $z->close() ;

    $XzError ;

    # IO::File mode

    print $z $string;
    printf $z $format, $string;
    tell $z
    eof $z
    seek $z, $position, $whence
    binmode $z
    fileno $z
    close $z ;
    

=head1 DESCRIPTION

B<WARNING -- This is a Beta release>. 

=over 5

=item * DO NOT use in production code.

=item * The documentation is incomplete in places.

=item * Parts of the interface defined here are tentative.

=item * Please report any problems you find.

=back

This module provides a Perl interface that allows writing xz 
compressed data to files or buffer.

For reading xz files/buffers, see the companion module 
L<IO::Uncompress::UnXz|IO::Uncompress::UnXz>.

=head1 Functional Interface

A top-level function, C<xz>, is provided to carry out
"one-shot" compression between buffers and/or files. For finer
control over the compression process, see the L</"OO Interface">
section.

    use IO::Compress::Xz qw(xz $XzError) ;

    xz $input => $output [,OPTS] 
        or die "xz failed: $XzError\n";

The functional interface needs Perl5.005 or better.

=head2 xz $input => $output [, OPTS]

C<xz> expects at least two parameters, C<$input> and C<$output>.

=head3 The C<$input> parameter

The parameter, C<$input>, is used to define the source of
the uncompressed data. 

It can take one of the following forms:

=over 5

=item A filename

If the C<$input> parameter is a simple scalar, it is assumed to be a
filename. This file will be opened for reading and the input data
will be read from it.

=item A filehandle

If the C<$input> parameter is a filehandle, the input data will be
read from it.
The string '-' can be used as an alias for standard input.

=item A scalar reference 

If C<$input> is a scalar reference, the input data will be read
from C<$$input>.

=item An array reference 

If C<$input> is an array reference, each element in the array must be a
filename.

The input data will be read from each file in turn. 

The complete array will be walked to ensure that it only
contains valid filenames before any data is compressed.

=item An Input FileGlob string

If C<$input> is a string that is delimited by the characters "<" and ">"
C<xz> will assume that it is an I<input fileglob string>. The
input is the list of files that match the fileglob.

See L<File::GlobMapper|File::GlobMapper> for more details.

=back

If the C<$input> parameter is any other type, C<undef> will be returned.

=head3 The C<$output> parameter

The parameter C<$output> is used to control the destination of the
compressed data. This parameter can take one of these forms.

=over 5

=item A filename

If the C<$output> parameter is a simple scalar, it is assumed to be a
filename.  This file will be opened for writing and the compressed
data will be written to it.

=item A filehandle

If the C<$output> parameter is a filehandle, the compressed data
will be written to it.
The string '-' can be used as an alias for standard output.

=item A scalar reference 

If C<$output> is a scalar reference, the compressed data will be
stored in C<$$output>.

=item An Array Reference

If C<$output> is an array reference, the compressed data will be
pushed onto the array.

=item An Output FileGlob

If C<$output> is a string that is delimited by the characters "<" and ">"
C<xz> will assume that it is an I<output fileglob string>. The
output is the list of files that match the fileglob.

When C<$output> is an fileglob string, C<$input> must also be a fileglob
string. Anything else is an error.

See L<File::GlobMapper|File::GlobMapper> for more details.

=back

If the C<$output> parameter is any other type, C<undef> will be returned.

=head2 Notes

When C<$input> maps to multiple files/buffers and C<$output> is a single
file/buffer the input files/buffers will be stored
in C<$output> as a concatenated series of compressed data streams.

=head2 Optional Parameters

Unless specified below, the optional parameters for C<xz>,
C<OPTS>, are the same as those used with the OO interface defined in the
L</"Constructor Options"> section below.

=over 5

=item C<< AutoClose => 0|1 >>

This option applies to any input or output data streams to 
C<xz> that are filehandles.

If C<AutoClose> is specified, and the value is true, it will result in all
input and/or output filehandles being closed once C<xz> has
completed.

This parameter defaults to 0.

=item C<< BinModeIn => 0|1 >>

When reading from a file or filehandle, set C<binmode> before reading.

Defaults to 0.

=item C<< Append => 0|1 >>

The behaviour of this option is dependent on the type of output data
stream.

=over 5

=item * A Buffer

If C<Append> is enabled, all compressed data will be append to the end of
the output buffer. Otherwise the output buffer will be cleared before any
compressed data is written to it.

=item * A Filename

If C<Append> is enabled, the file will be opened in append mode. Otherwise
the contents of the file, if any, will be truncated before any compressed
data is written to it.

=item * A Filehandle

If C<Append> is enabled, the filehandle will be positioned to the end of
the file via a call to C<seek> before any compressed data is
written to it.  Otherwise the file pointer will not be moved.

=back

When C<Append> is specified, and set to true, it will I<append> all compressed 
data to the output data stream.

So when the output is a filehandle it will carry out a seek to the eof
before writing any compressed data. If the output is a filename, it will be opened for
appending. If the output is a buffer, all compressed data will be
appended to the existing buffer.

Conversely when C<Append> is not specified, or it is present and is set to
false, it will operate as follows.

When the output is a filename, it will truncate the contents of the file
before writing any compressed data. If the output is a filehandle
its position will not be changed. If the output is a buffer, it will be
wiped before any compressed data is output.

Defaults to 0.

=back

=head2 Examples

To read the contents of the file C<file1.txt> and write the compressed
data to the file C<file1.txt.xz>.

    use strict ;
    use warnings ;
    use IO::Compress::Xz qw(xz $XzError) ;

    my $input = "file1.txt";
    xz $input => "$input.xz"
        or die "xz failed: $XzError\n";

To read from an existing Perl filehandle, C<$input>, and write the
compressed data to a buffer, C<$buffer>.

    use strict ;
    use warnings ;
    use IO::Compress::Xz qw(xz $XzError) ;
    use IO::File ;

    my $input = new IO::File "<file1.txt"
        or die "Cannot open 'file1.txt': $!\n" ;
    my $buffer ;
    xz $input => \$buffer 
        or die "xz failed: $XzError\n";

To compress all files in the directory "/my/home" that match "*.txt"
and store the compressed data in the same directory

    use strict ;
    use warnings ;
    use IO::Compress::Xz qw(xz $XzError) ;

    xz '</my/home/*.txt>' => '<*.xz>'
        or die "xz failed: $XzError\n";

and if you want to compress each file one at a time, this will do the trick

    use strict ;
    use warnings ;
    use IO::Compress::Xz qw(xz $XzError) ;

    for my $input ( glob "/my/home/*.txt" )
    {
        my $output = "$input.xz" ;
        xz $input => $output 
            or die "Error compressing '$input': $XzError\n";
    }

=head1 OO Interface

=head2 Constructor

The format of the constructor for C<IO::Compress::Xz> is shown below

    my $z = new IO::Compress::Xz $output [,OPTS]
        or die "IO::Compress::Xz failed: $XzError\n";

It returns an C<IO::Compress::Xz> object on success and undef on failure. 
The variable C<$XzError> will contain an error message on failure.

If you are running Perl 5.005 or better the object, C<$z>, returned from 
IO::Compress::Xz can be used exactly like an L<IO::File|IO::File> filehandle. 
This means that all normal output file operations can be carried out 
with C<$z>. 
For example, to write to a compressed file/buffer you can use either of 
these forms

    $z->print("hello world\n");
    print $z "hello world\n";

The mandatory parameter C<$output> is used to control the destination
of the compressed data. This parameter can take one of these forms.

=over 5

=item A filename

If the C<$output> parameter is a simple scalar, it is assumed to be a
filename. This file will be opened for writing and the compressed data
will be written to it.

=item A filehandle

If the C<$output> parameter is a filehandle, the compressed data will be
written to it.
The string '-' can be used as an alias for standard output.

=item A scalar reference 

If C<$output> is a scalar reference, the compressed data will be stored
in C<$$output>.

=back

If the C<$output> parameter is any other type, C<IO::Compress::Xz>::new will
return undef.

=head2 Constructor Options

C<OPTS> is any combination of the following options:

=over 5

=item C<< AutoClose => 0|1 >>

This option is only valid when the C<$output> parameter is a filehandle. If
specified, and the value is true, it will result in the C<$output> being
closed once either the C<close> method is called or the C<IO::Compress::Xz>
object is destroyed.

This parameter defaults to 0.

=item C<< Append => 0|1 >>

Opens C<$output> in append mode. 

The behaviour of this option is dependent on the type of C<$output>.

=over 5

=item * A Buffer

If C<$output> is a buffer and C<Append> is enabled, all compressed data
will be append to the end of C<$output>. Otherwise C<$output> will be
cleared before any data is written to it.

=item * A Filename

If C<$output> is a filename and C<Append> is enabled, the file will be
opened in append mode. Otherwise the contents of the file, if any, will be
truncated before any compressed data is written to it.

=item * A Filehandle

If C<$output> is a filehandle, the file pointer will be positioned to the
end of the file via a call to C<seek> before any compressed data is written
to it.  Otherwise the file pointer will not be moved.

=back

This parameter defaults to 0.

=item C<< Preset => $preset >>

Used to choose the compression preset.

Valid values are 0-9 and C<LZMA_PRESET_DEFAULT>.

0 is the fastest compression with the lowest memory usage and the lowest
compression.

9 is the slowest compession with the highest memory usage but with the best
compression.

Defaults to C<LZMA_PRESET_DEFAULT> (6).

=item C<< Extreme => 0|1 >>

Makes the compression a lot slower, but a small compression gain.

Defaults to 0.

=item C<< Check => $check >>

Used to specify the integrrity check used in the xz data stream.
Valid values are C<LZMA_CHECK_NONE>, C<LZMA_CHECK_CRC32>,
C<LZMA_CHECK_CRC64>, C<LZMA_CHECK_SHA256>.

Defaults to C<LZMA_CHECK_CRC32>.

=item C<< Strict => 0|1 >>

This is a placeholder option.

=back

=head2 Examples

TODO

=head1 Methods 

=head2 print

Usage is

    $z->print($data)
    print $z $data

Compresses and outputs the contents of the C<$data> parameter. This
has the same behaviour as the C<print> built-in.

Returns true if successful.

=head2 printf

Usage is

    $z->printf($format, $data)
    printf $z $format, $data

Compresses and outputs the contents of the C<$data> parameter.

Returns true if successful.

=head2 syswrite

Usage is

    $z->syswrite $data
    $z->syswrite $data, $length
    $z->syswrite $data, $length, $offset

Compresses and outputs the contents of the C<$data> parameter.

Returns the number of uncompressed bytes written, or C<undef> if
unsuccessful.

=head2 write

Usage is

    $z->write $data
    $z->write $data, $length
    $z->write $data, $length, $offset

Compresses and outputs the contents of the C<$data> parameter.

Returns the number of uncompressed bytes written, or C<undef> if
unsuccessful.

=head2 flush

Usage is

    $z->flush;

Flushes any pending compressed data to the output file/buffer.

Returns true on success.

=head2 tell

Usage is

    $z->tell()
    tell $z

Returns the uncompressed file offset.

=head2 eof

Usage is

    $z->eof();
    eof($z);

Returns true if the C<close> method has been called.

=head2 seek

    $z->seek($position, $whence);
    seek($z, $position, $whence);

Provides a sub-set of the C<seek> functionality, with the restriction
that it is only legal to seek forward in the output file/buffer.
It is a fatal error to attempt to seek backward.

Empty parts of the file/buffer will have NULL (0x00) bytes written to them.

The C<$whence> parameter takes one the usual values, namely SEEK_SET,
SEEK_CUR or SEEK_END.

Returns 1 on success, 0 on failure.

=head2 binmode

Usage is

    $z->binmode
    binmode $z ;

This is a noop provided for completeness.

=head2 opened

    $z->opened()

Returns true if the object currently refers to a opened file/buffer. 

=head2 autoflush

    my $prev = $z->autoflush()
    my $prev = $z->autoflush(EXPR)

If the C<$z> object is associated with a file or a filehandle, this method
returns the current autoflush setting for the underlying filehandle. If
C<EXPR> is present, and is non-zero, it will enable flushing after every
write/print operation.

If C<$z> is associated with a buffer, this method has no effect and always
returns C<undef>.

B<Note> that the special variable C<$|> B<cannot> be used to set or
retrieve the autoflush setting.

=head2 input_line_number

    $z->input_line_number()
    $z->input_line_number(EXPR)

This method always returns C<undef> when compressing. 

=head2 fileno

    $z->fileno()
    fileno($z)

If the C<$z> object is associated with a file or a filehandle, C<fileno>
will return the underlying file descriptor. Once the C<close> method is
called C<fileno> will return C<undef>.

If the C<$z> object is associated with a buffer, this method will return
C<undef>.

=head2 close

    $z->close() ;
    close $z ;

Flushes any pending compressed data and then closes the output file/buffer. 

For most versions of Perl this method will be automatically invoked if
the IO::Compress::Xz object is destroyed (either explicitly or by the
variable with the reference to the object going out of scope). The
exceptions are Perl versions 5.005 through 5.00504 and 5.8.0. In
these cases, the C<close> method will be called automatically, but
not until global destruction of all live objects when the program is
terminating.

Therefore, if you want your scripts to be able to run on all versions
of Perl, you should call C<close> explicitly and not rely on automatic
closing.

Returns true on success, otherwise 0.

If the C<AutoClose> option has been enabled when the IO::Compress::Xz
object was created, and the object is associated with a file, the
underlying file will also be closed.

=head2 newStream([OPTS])

Usage is

    $z->newStream( [OPTS] )

Closes the current compressed data stream and starts a new one.

OPTS consists of any of the the options that are available when creating
the C<$z> object.

See the L</"Constructor Options"> section for more details.

=head1 Importing 

No symbolic constants are required by this IO::Compress::Xz at present. 

=over 5

=item :all

Imports C<xz> and C<$XzError>.
Same as doing this

    use IO::Compress::Xz qw(xz $XzError) ;

    

=back

=head1 EXAMPLES

=head1 SEE ALSO

L<Compress::Zlib>, L<IO::Compress::Gzip>, L<IO::Uncompress::Gunzip>, L<IO::Compress::Deflate>, L<IO::Uncompress::Inflate>, L<IO::Compress::RawDeflate>, L<IO::Uncompress::RawInflate>, L<IO::Compress::Bzip2>, L<IO::Uncompress::Bunzip2>, L<IO::Compress::Lzma>, L<IO::Uncompress::UnLzma>, L<IO::Uncompress::UnXz>, L<IO::Compress::Lzop>, L<IO::Uncompress::UnLzop>, L<IO::Compress::Lzf>, L<IO::Uncompress::UnLzf>, L<IO::Uncompress::AnyInflate>, L<IO::Uncompress::AnyUncompress>

L<IO::Compress::FAQ|IO::Compress::FAQ>

L<File::GlobMapper|File::GlobMapper>, L<Archive::Zip|Archive::Zip>,
L<Archive::Tar|Archive::Tar>,
L<IO::Zlib|IO::Zlib>

=head1 AUTHOR

This module was written by Paul Marquess, F<pmqs@cpan.org>. 

=head1 MODIFICATION HISTORY

See the Changes file.

=head1 COPYRIGHT AND LICENSE

Copyright (c) 2005-2012 Paul Marquess. All rights reserved.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

