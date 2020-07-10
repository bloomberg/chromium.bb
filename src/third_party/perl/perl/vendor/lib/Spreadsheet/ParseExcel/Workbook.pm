package Spreadsheet::ParseExcel::Workbook;

###############################################################################
#
# Spreadsheet::ParseExcel::Workbook - A class for Workbooks.
#
# Used in conjunction with Spreadsheet::ParseExcel.
#
# Copyright (c) 2014      Douglas Wilson
# Copyright (c) 2009-2013 John McNamara
# Copyright (c) 2006-2008 Gabor Szabo
# Copyright (c) 2000-2006 Kawai Takanori
#
# perltidy with standard settings.
#
# Documentation after __END__
#

use strict;
use warnings;

our $VERSION = '0.65';

###############################################################################
#
# new()
#
# Constructor.
#
sub new {
    my ($class) = @_;
    my $self = {};
    bless $self, $class;
}

###############################################################################
sub color_idx_to_rgb {
    my( $workbook, $iidx ) = @_;

    my $palette = $workbook->{aColor};
    return ( ( defined $palette->[$iidx] ) ? $palette->[$iidx] : $palette->[0] );
}

###############################################################################
#
# worksheet()
#
# This method returns a single Worksheet object using either its name or index.
#
sub worksheet {
    my ( $oBook, $sName ) = @_;
    my $oWkS;
    foreach $oWkS ( @{ $oBook->{Worksheet} } ) {
        return $oWkS if ( $oWkS->{Name} eq $sName );
    }
    if ( $sName =~ /^\d+$/ ) {
        return $oBook->{Worksheet}->[$sName];
    }
    return undef;
}

###############################################################################
#
# worksheets()
#
# Returns an array of Worksheet objects.
#
sub worksheets {
    my $self = shift;

    return @{ $self->{Worksheet} };
}

###############################################################################
#
# worksheet_count()
#
# Returns the number Woksheet objects in the Workbook.
#
sub worksheet_count {

    my $self = shift;

    return $self->{SheetCount};
}

###############################################################################
#
# get_filename()
#
# Returns the name of the Excel file of C<undef> if the data was read from a filehandle rather than a file.
#
sub get_filename {

    my $self = shift;

    return $self->{File};
}

###############################################################################
#
# get_print_areas()
#
# Returns an array ref of print areas.
#
# TODO. This should really be a Worksheet method.
#
sub get_print_areas {

    my $self = shift;

    return $self->{PrintArea};
}

###############################################################################
#
# get_print_titles()
#
# Returns an array ref of print title hash refs.
#
# TODO. This should really be a Worksheet method.
#
sub get_print_titles {

    my $self = shift;

    return $self->{PrintTitle};
}

###############################################################################
#
# using_1904_date()
#
# Returns true if the Excel file is using the 1904 date epoch.
#
sub using_1904_date {

    my $self = shift;

    return $self->{Flg1904};
}

###############################################################################
#
# ParseAbort()
#
# Todo
#
sub ParseAbort {
    my ( $self, $val ) = @_;
    $self->{_ParseAbort} = $val;
}

=head2 get_active_sheet()

Return the number of the active (open) worksheet (at the time the workbook
was saved.  May return undef.

=cut

sub get_active_sheet {
    my $workbook = shift;

    return $workbook->{ActiveSheet};
}

###############################################################################
#
# Parse(). Deprecated.
#
# Syntactic wrapper around Spreadsheet::ParseExcel::Parse().
# This method is *deprecated* since it doesn't conform to the current
# error handling in the S::PE Parse() method.
#
sub Parse {

    my ( $class, $source, $formatter ) = @_;
    my $excel = Spreadsheet::ParseExcel->new();
    my $workbook = $excel->Parse( $source, $formatter );
    $workbook->{_Excel} = $excel;
    return $workbook;
}

###############################################################################
#
# Mapping between legacy method names and new names.
#
{
    no warnings;    # Ignore warnings about variables used only once.
    *Worksheet = *worksheet;
}

1;

__END__

=pod

=head1 NAME

Spreadsheet::ParseExcel::Workbook - A class for Workbooks.

=head1 SYNOPSIS

See the documentation for Spreadsheet::ParseExcel.

=head1 DESCRIPTION

This module is used in conjunction with Spreadsheet::ParseExcel. See the documentation for L<Spreadsheet::ParseExcel>.


=head1 Methods

The following Workbook methods are available:

    $workbook->worksheets()
    $workbook->worksheet()
    $workbook->worksheet_count()
    $workbook->get_filename()
    $workbook->get_print_areas()
    $workbook->get_print_titles()
    $workbook->using_1904_date()


=head2 worksheets()

The C<worksheets()> method returns an array of Worksheet objects. This was most commonly used to iterate over the worksheets in a workbook:

    for my $worksheet ( $workbook->worksheets() ) {
        ...
    }


=head2 worksheet()

The C<worksheet()> method returns a single C<Worksheet> object using either its name or index:

    $worksheet = $workbook->worksheet('Sheet1');
    $worksheet = $workbook->worksheet(0);

Returns C<undef> if the sheet name or index doesn't exist.


=head2 worksheet_count()

The C<worksheet_count()> method returns the number of Woksheet objects in the Workbook.

    my $worksheet_count = $workbook->worksheet_count();


=head2 get_filename()

The C<get_filename()> method returns the name of the Excel file of C<undef> if the data was read from a filehandle rather than a file.

    my $filename = $workbook->get_filename();


=head2 get_print_areas()

The C<get_print_areas()> method returns an array ref of print areas.

    my $print_areas = $workbook->get_print_areas();

Each print area is as follows:

    [ $start_row, $start_col, $end_row, $end_col ]

Returns undef if there are no print areas.


=head2 get_print_titles()

The C<get_print_titles()> method returns an array ref of print title hash refs.

    my $print_titles = $workbook->get_print_titles();

Each print title array ref is as follows:

    {
        Row    => [ $start_row, $end_row ],
        Column => [ $start_col, $end_col ],
    }


Returns undef if there are no print titles.


=head2 using_1904_date()

The C<using_1904_date()> method returns true if the Excel file is using the 1904 date epoch instead of the 1900 epoch.

    my $using_1904_date = $workbook->using_1904_date();

 The Windows version of Excel generally uses the 1900 epoch while the Mac version of Excel generally uses the 1904 epoch.

Returns 0 if the 1900 epoch is in use.


=head1 AUTHOR

Current maintainer 0.60+: Douglas Wilson dougw@cpan.org

Maintainer 0.40-0.59: John McNamara jmcnamara@cpan.org

Maintainer 0.27-0.33: Gabor Szabo szabgab@cpan.org

Original author: Kawai Takanori kwitknr@cpan.org

=head1 COPYRIGHT

Copyright (c) 2014 Douglas Wilson

Copyright (c) 2009-2013 John McNamara

Copyright (c) 2006-2008 Gabor Szabo

Copyright (c) 2000-2006 Kawai Takanori

All rights reserved.

You may distribute under the terms of either the GNU General Public License or the Artistic License, as specified in the Perl README file.

=cut
