package Spreadsheet::WriteExcel::BIFFwriter;

###############################################################################
#
# BIFFwriter - An abstract base class for Excel workbooks and worksheets.
#
#
# Used in conjunction with Spreadsheet::WriteExcel
#
# Copyright 2000-2010, John McNamara, jmcnamara@cpan.org
#
# Documentation after __END__
#

use Exporter;
use strict;







use vars qw($VERSION @ISA);
@ISA = qw(Exporter);

$VERSION = '2.40';

###############################################################################
#
# Class data.
#
my $byte_order   = '';
my $BIFF_version = 0x0600;


###############################################################################
#
# new()
#
# Constructor
#
sub new {

    my $class  = $_[0];

    my $self   = {
                    _byte_order      => '',
                    _data            => '',
                    _datasize        => 0,
                    _limit           => 8224,
                    _ignore_continue => 0,
                 };

    bless $self, $class;
    $self->_set_byte_order();
    return $self;
}


###############################################################################
#
# _set_byte_order()
#
# Determine the byte order and store it as class data to avoid
# recalculating it for each call to new().
#
sub _set_byte_order {

    my $self    = shift;

    if ($byte_order eq ''){
        # Check if "pack" gives the required IEEE 64bit float
        my $teststr = pack "d", 1.2345;
        my @hexdata =(0x8D, 0x97, 0x6E, 0x12, 0x83, 0xC0, 0xF3, 0x3F);
        my $number  = pack "C8", @hexdata;

        if ($number eq $teststr) {
            $byte_order = 0;    # Little Endian
        }
        elsif ($number eq reverse($teststr)){
            $byte_order = 1;    # Big Endian
        }
        else {
            # Give up. I'll fix this in a later version.
            croak ( "Required floating point format not supported "  .
                    "on this platform. See the portability section " .
                    "of the documentation."
            );
        }
    }
    $self->{_byte_order} = $byte_order;
}


###############################################################################
#
# _prepend($data)
#
# General storage function
#
sub _prepend {

    my $self    = shift;
    my $data    = join('', @_);

    $data = $self->_add_continue($data) if length($data) > $self->{_limit};

    $self->{_data}      = $data . $self->{_data};
    $self->{_datasize} += length($data);

    return $data;
}


###############################################################################
#
# _append($data)
#
# General storage function
#
sub _append {

    my $self    = shift;
    my $data    = join('', @_);

    $data = $self->_add_continue($data) if length($data) > $self->{_limit};

    $self->{_data}      = $self->{_data} . $data;
    $self->{_datasize} += length($data);

    return $data;
}


###############################################################################
#
# _store_bof($type)
#
# $type = 0x0005, Workbook
# $type = 0x0010, Worksheet
# $type = 0x0020, Chart
#
# Writes Excel BOF record to indicate the beginning of a stream or
# sub-stream in the BIFF file.
#
sub _store_bof {

    my $self    = shift;
    my $record  = 0x0809;        # Record identifier
    my $length  = 0x0010;        # Number of bytes to follow

    my $version = $BIFF_version;
    my $type    = $_[0];

    # According to the SDK $build and $year should be set to zero.
    # However, this throws a warning in Excel 5. So, use these
    # magic numbers.
    my $build   = 0x0DBB;
    my $year    = 0x07CC;

    my $bfh     = 0x00000041;
    my $sfo     = 0x00000006;

    my $header  = pack("vv",   $record, $length);
    my $data    = pack("vvvvVV", $version, $type, $build, $year, $bfh, $sfo);

    $self->_prepend($header, $data);
}


###############################################################################
#
# _store_eof()
#
# Writes Excel EOF record to indicate the end of a BIFF stream.
#
sub _store_eof {

    my $self      = shift;
    my $record    = 0x000A; # Record identifier
    my $length    = 0x0000; # Number of bytes to follow

    my $header    = pack("vv", $record, $length);

    $self->_append($header);
}


###############################################################################
#
# _add_continue()
#
# Excel limits the size of BIFF records. In Excel 5 the limit is 2084 bytes. In
# Excel 97 the limit is 8228 bytes. Records that are longer than these limits
# must be split up into CONTINUE blocks.
#
# This function take a long BIFF record and inserts CONTINUE records as
# necessary.
#
# Some records have their own specialised Continue blocks so there is also an
# option to bypass this function.
#
sub _add_continue {

    my $self        = shift;
    my $data        = $_[0];
    my $limit       = $self->{_limit};
    my $record      = 0x003C; # Record identifier
    my $header;
    my $tmp;

    # Skip this if another method handles the continue blocks.
    return $data if $self->{_ignore_continue};

    # The first 2080/8224 bytes remain intact. However, we have to change
    # the length field of the record.
    #
    $tmp = substr($data, 0, $limit, "");
    substr($tmp, 2, 2, pack("v", $limit-4));

    # Strip out chunks of 2080/8224 bytes +4 for the header.
    while (length($data) > $limit) {
        $header  = pack("vv", $record, $limit);
        $tmp    .= $header;
        $tmp    .= substr($data, 0, $limit, "");
    }

    # Mop up the last of the data
    $header  = pack("vv", $record, length($data));
    $tmp    .= $header;
    $tmp    .= $data;

    return $tmp ;
}


###############################################################################
#
# _add_mso_generic()
#
# Create a mso structure that is part of an Escher drawing object. These are
# are used for images, comments and filters. This generic method is used by
# other methods to create specific mso records.
#
# Returns the packed record.
#
sub _add_mso_generic {

    my $self        = shift;
    my $type        = $_[0];
    my $version     = $_[1];
    my $instance    = $_[2];
    my $data        = $_[3];
    my $length      = defined $_[4] ? $_[4] : length($data);

    # The header contains version and instance info packed into 2 bytes.
    my $header      = $version | ($instance << 4);

    my $record      = pack "vvV", $header, $type, $length;
       $record     .= $data;

    return $record;
}


###############################################################################
#
# For debugging
#
sub _hexout {

    my $self = shift;

    print +(caller(1))[3], "\n";

    my $data = join '', @_;

    my @bytes = unpack("H*", $data) =~ /../g;

    while (@bytes > 16) {
        print join " ", splice @bytes, 0, 16;
        print "\n";
    }
    print join " ", @bytes, "\n\n";
}



1;


__END__

=encoding latin1

=head1 NAME

BIFFwriter - An abstract base class for Excel workbooks and worksheets.

=head1 SYNOPSIS

See the documentation for Spreadsheet::WriteExcel

=head1 DESCRIPTION

This module is used in conjunction with Spreadsheet::WriteExcel.

=head1 AUTHOR

John McNamara jmcnamara@cpan.org

=head1 COPYRIGHT

Copyright MM-MMX, John McNamara.

All Rights Reserved. This module is free software. It may be used, redistributed and/or modified under the same terms as Perl itself.
