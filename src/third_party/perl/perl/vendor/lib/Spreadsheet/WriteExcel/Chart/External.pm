package Spreadsheet::WriteExcel::Chart::External;

###############################################################################
#
# External - A writer class for Excel external charts.
#
# Used in conjunction with Spreadsheet::WriteExcel
#
# perltidy with options: -mbl=2 -pt=0 -nola
#
# Copyright 2000-2010, John McNamara, jmcnamara@cpan.org
#
# Documentation after __END__
#

require Exporter;

use strict;
use Spreadsheet::WriteExcel::Chart;


use vars qw($VERSION @ISA);
@ISA = qw(Spreadsheet::WriteExcel::Chart Exporter);

$VERSION = '2.40';

###############################################################################
#
# new()
#
#
sub new {

    my $class             = shift;
    my $external_filename = shift;
    my $self              = Spreadsheet::WriteExcel::Chart->new( @_ );

    $self->{_filename}     = $external_filename;
    $self->{_external_bin} = 1;

    bless $self, $class;
    $self->_initialize();    # Requires overridden initialize().
    return $self;
}

###############################################################################
#
# _initialize()
#
# Read all the data into memory for the external binary style chart.
#
#
sub _initialize {

    my $self = shift;

    my $filename   = $self->{_filename};
    my $filehandle = FileHandle->new( $filename )
      or die "Couldn't open $filename in add_chart_ext(): $!.\n";

    binmode( $filehandle );

    $self->{_filehandle}    = $filehandle;
    $self->{_datasize}      = -s $filehandle;
    $self->{_using_tmpfile} = 0;

    # Read the entire external chart binary into the data buffer.
    # This will be retrieved by _get_data() when the chart is closed().
    read( $self->{_filehandle}, $self->{_data}, $self->{_datasize} );
}


###############################################################################
#
# _close()
#
# We don't need to create or store Chart data structures when using an
# external binary, so we have a default close method.
#
sub _close {

    my $self = shift;

    return undef;
}

1;


__END__


=head1 NAME

External - A writer class for Excel external charts.

=head1 SYNOPSIS

This module is used to include external charts in Spreadsheet::WriteExcel.

=head1 DESCRIPTION

This module is used to include external charts in L<Spreadsheet::WriteExcel>. It is an internal module and isn't used directly by the end user.

It is semi-deprecated in favour of using "native" charts. See L<Spreadsheet::WriteExcel::Chart>.

For information on how to used external charts see the C<external_charts.txt>  (or C<.pod>) in the C<external_charts> directory of the distro.


=head1 AUTHOR

John McNamara jmcnamara@cpan.org

=head1 COPYRIGHT

Copyright MM-MMX, John McNamara.

All Rights Reserved. This module is free software. It may be used, redistributed and/or modified under the same terms as Perl itself.

