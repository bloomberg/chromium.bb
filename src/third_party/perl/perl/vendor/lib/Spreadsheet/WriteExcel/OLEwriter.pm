package Spreadsheet::WriteExcel::OLEwriter;

###############################################################################
#
# OLEwriter - A writer class to store BIFF data in a OLE compound storage file.
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
use Carp;
use FileHandle;





use vars qw($VERSION @ISA);
@ISA = qw(Exporter);

$VERSION = '2.40';

###############################################################################
#
# new()
#
# Constructor
#
sub new {

    my $class  = shift;
    my $self   = {
                    _olefilename   => $_[0],
                    _filehandle    => "",
                    _fileclosed    => 0,
                    _internal_fh   => 0,
                    _biff_only     => 0,
                    _size_allowed  => 0,
                    _biffsize      => 0,
                    _booksize      => 0,
                    _big_blocks    => 0,
                    _list_blocks   => 0,
                    _root_start    => 0,
                    _block_count   => 4,
                 };

    bless $self, $class;
    $self->_initialize();
    return $self;
}


###############################################################################
#
# _initialize()
#
# Create a new filehandle or use the provided filehandle.
#
sub _initialize {

    my $self    = shift;
    my $olefile = $self->{_olefilename};
    my $fh;

    # If the filename is a reference it is assumed that it is a valid
    # filehandle, if not we create a filehandle.
    #
    if (ref($olefile)) {
        $fh = $olefile;
    }
    else{

        # Create a new file, open for writing
        $fh = FileHandle->new("> $olefile");

        # Workbook.pm also checks this but something may have happened since
        # then.
        if (not defined $fh) {
            croak "Can't open $olefile. It may be in use or protected.\n";
        }

        # binmode file whether platform requires it or not
        binmode($fh);

        $self->{_internal_fh} = 1;
    }

    # Store filehandle
    $self->{_filehandle} = $fh;
}


###############################################################################
#
# set_size($biffsize)
#
# Set the size of the data to be written to the OLE stream
#
#   $big_blocks = (109 depot block x (128 -1 marker word)
#                 - (1 x end words)) = 13842
#   $maxsize    = $big_blocks * 512 bytes = 7087104
#
sub set_size {

    my $self    = shift;
    my $maxsize = 7_087_104; # Use Spreadsheet::WriteExcel::Big to exceed this

    if ($_[0] > $maxsize) {
        return $self->{_size_allowed} = 0;
    }

    $self->{_biffsize} = $_[0];

    # Set the min file size to 4k to avoid having to use small blocks
    if ($_[0] > 4096) {
        $self->{_booksize} = $_[0];
    }
    else {
        $self->{_booksize} = 4096;
    }

    return $self->{_size_allowed} = 1;

}


###############################################################################
#
# _calculate_sizes()
#
# Calculate various sizes needed for the OLE stream
#
sub _calculate_sizes {

    my $self     = shift;
    my $datasize = $self->{_booksize};

    if ($datasize % 512 == 0) {
        $self->{_big_blocks} = $datasize/512;
    }
    else {
        $self->{_big_blocks} = int($datasize/512) +1;
    }
    # There are 127 list blocks and 1 marker blocks for each big block
    # depot + 1 end of chain block
    $self->{_list_blocks} = int(($self->{_big_blocks})/127) +1;
    $self->{_root_start}  = $self->{_big_blocks};
}


###############################################################################
#
# close()
#
# Write root entry, big block list and close the filehandle.
# This routine is used to explicitly close the open filehandle without
# having to wait for DESTROY.
#
sub close {

    my $self = shift;

    return if not $self->{_size_allowed};

    $self->_write_padding()          if not $self->{_biff_only};
    $self->_write_property_storage() if not $self->{_biff_only};
    $self->_write_big_block_depot()  if not $self->{_biff_only};

    my $close = 1; # Default to no error for external filehandles.

    # Close the filehandle if it was created internally.
    $close = CORE::close($self->{_filehandle}) if $self->{_internal_fh};

    $self->{_fileclosed} = 1;

    return $close;
}


###############################################################################
#
# DESTROY()
#
# Close the filehandle if it hasn't already been explicitly closed.
#
sub DESTROY {

    my $self = shift;

    local ($@, $!, $^E, $?);

    $self->close() unless $self->{_fileclosed};
}


###############################################################################
#
# write($data)
#
# Write BIFF data to OLE file.
#
sub write {

    my $self = shift;

    # Protect print() from -l on the command line.
    local $\ = undef;
    print {$self->{_filehandle}} $_[0];
}


###############################################################################
#
# write_header()
#
# Write OLE header block.
#
sub write_header {

    my $self            = shift;

    return if $self->{_biff_only};
    $self->_calculate_sizes();

    my $root_start      = $self->{_root_start};
    my $num_lists       = $self->{_list_blocks};

    my $id              = pack("NN",   0xD0CF11E0, 0xA1B11AE1);
    my $unknown1        = pack("VVVV", 0x00, 0x00, 0x00, 0x00);
    my $unknown2        = pack("vv",   0x3E, 0x03);
    my $unknown3        = pack("v",    -2);
    my $unknown4        = pack("v",    0x09);
    my $unknown5        = pack("VVV",  0x06, 0x00, 0x00);
    my $num_bbd_blocks  = pack("V",    $num_lists);
    my $root_startblock = pack("V",    $root_start);
    my $unknown6        = pack("VV",   0x00, 0x1000);
    my $sbd_startblock  = pack("V",    -2);
    my $unknown7        = pack("VVV",  0x00, -2 ,0x00);
    my $unused          = pack("V",    -1);

    # Protect print() from -l on the command line.
    local $\ = undef;

    print {$self->{_filehandle}}  $id;
    print {$self->{_filehandle}}  $unknown1;
    print {$self->{_filehandle}}  $unknown2;
    print {$self->{_filehandle}}  $unknown3;
    print {$self->{_filehandle}}  $unknown4;
    print {$self->{_filehandle}}  $unknown5;
    print {$self->{_filehandle}}  $num_bbd_blocks;
    print {$self->{_filehandle}}  $root_startblock;
    print {$self->{_filehandle}}  $unknown6;
    print {$self->{_filehandle}}  $sbd_startblock;
    print {$self->{_filehandle}}  $unknown7;

    for (1..$num_lists) {
        $root_start++;
        print {$self->{_filehandle}}  pack("V", $root_start);
    }

    for ($num_lists..108) {
        print {$self->{_filehandle}}  $unused;
    }
}


###############################################################################
#
# _write_big_block_depot()
#
# Write big block depot.
#
sub _write_big_block_depot {

    my $self         = shift;
    my $num_blocks   = $self->{_big_blocks};
    my $num_lists    = $self->{_list_blocks};
    my $total_blocks = $num_lists *128;
    my $used_blocks  = $num_blocks + $num_lists +2;

    my $marker       = pack("V", -3);
    my $end_of_chain = pack("V", -2);
    my $unused       = pack("V", -1);


    # Protect print() from -l on the command line.
    local $\ = undef;

    for my $i (1..$num_blocks-1) {
        print {$self->{_filehandle}}  pack("V",$i);
    }

    print {$self->{_filehandle}}  $end_of_chain;
    print {$self->{_filehandle}}  $end_of_chain;

    for (1..$num_lists) {
        print {$self->{_filehandle}}  $marker;
    }

    for ($used_blocks..$total_blocks) {
        print {$self->{_filehandle}}  $unused;
    }
}


###############################################################################
#
# _write_property_storage()
#
# Write property storage. TODO: add summary sheets
#
sub _write_property_storage {

    my $self     = shift;

    my $rootsize = -2;
    my $booksize = $self->{_booksize};

    #################  name         type   dir start size
    $self->_write_pps('Root Entry', 0x05,   1,   -2, 0x00);
    $self->_write_pps('Workbook',   0x02,  -1, 0x00, $booksize);
    $self->_write_pps('',           0x00,  -1, 0x00, 0x0000);
    $self->_write_pps('',           0x00,  -1, 0x00, 0x0000);
}


###############################################################################
#
# _write_pps()
#
# Write property sheet in property storage
#
sub _write_pps {

    my $self            = shift;

    my $name            = $_[0];
    my @name            = ();
    my $length          = 0;

    if ($name ne '') {
        $name   = $_[0] . "\0";
        # Simulate a Unicode string
        @name   = map(ord, split('', $name));
        $length = length($name) * 2;
    }

    my $rawname         = pack("v*", @name);
    my $zero            = pack("C",  0);

    my $pps_sizeofname  = pack("v",  $length);    #0x40
    my $pps_type        = pack("v",  $_[1]);      #0x42
    my $pps_prev        = pack("V",  -1);         #0x44
    my $pps_next        = pack("V",  -1);         #0x48
    my $pps_dir         = pack("V",  $_[2]);      #0x4c

    my $unknown1        = pack("V",  0);

    my $pps_ts1s        = pack("V",  0);          #0x64
    my $pps_ts1d        = pack("V",  0);          #0x68
    my $pps_ts2s        = pack("V",  0);          #0x6c
    my $pps_ts2d        = pack("V",  0);          #0x70
    my $pps_sb          = pack("V",  $_[3]);      #0x74
    my $pps_size        = pack("V",  $_[4]);      #0x78


    # Protect print() from -l on the command line.
    local $\ = undef;

    print {$self->{_filehandle}}  $rawname;
    print {$self->{_filehandle}}  $zero x (64 -$length);
    print {$self->{_filehandle}}  $pps_sizeofname;
    print {$self->{_filehandle}}  $pps_type;
    print {$self->{_filehandle}}  $pps_prev;
    print {$self->{_filehandle}}  $pps_next;
    print {$self->{_filehandle}}  $pps_dir;
    print {$self->{_filehandle}}  $unknown1 x 5;
    print {$self->{_filehandle}}  $pps_ts1s;
    print {$self->{_filehandle}}  $pps_ts1d;
    print {$self->{_filehandle}}  $pps_ts2d;
    print {$self->{_filehandle}}  $pps_ts2d;
    print {$self->{_filehandle}}  $pps_sb;
    print {$self->{_filehandle}}  $pps_size;
    print {$self->{_filehandle}}  $unknown1;
}


###############################################################################
#
# _write_padding()
#
# Pad the end of the file
#
sub _write_padding {

    my $self     = shift;
    my $biffsize = $self->{_biffsize};
    my $min_size;

    if ($biffsize < 4096) {
        $min_size = 4096;
    }
    else {
        $min_size = 512;
    }

    # Protect print() from -l on the command line.
    local $\ = undef;

    if ($biffsize % $min_size != 0) {
        my $padding  = $min_size - ($biffsize % $min_size);
        print {$self->{_filehandle}}  "\0" x $padding;
    }
}


1;


__END__

=encoding latin1

=head1 NAME

OLEwriter - A writer class to store BIFF data in a OLE compound storage file.

=head1 SYNOPSIS

See the documentation for Spreadsheet::WriteExcel

=head1 DESCRIPTION

This module is used in conjunction with Spreadsheet::WriteExcel.

=head1 AUTHOR

John McNamara jmcnamara@cpan.org

=head1 COPYRIGHT

Copyright MM-MMX, John McNamara.

All Rights Reserved. This module is free software. It may be used, redistributed and/or modified under the same terms as Perl itself.
