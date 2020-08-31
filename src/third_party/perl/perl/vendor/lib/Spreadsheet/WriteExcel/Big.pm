package Spreadsheet::WriteExcel::Big;

###############################################################################
#
# WriteExcel::Big
#
# Spreadsheet::WriteExcel - Write formatted text and numbers to a
# cross-platform Excel binary file.
#
# Copyright 2000-2010, John McNamara.
#
#

require Exporter;

use strict;
use Spreadsheet::WriteExcel::Workbook;






use vars qw($VERSION @ISA);
@ISA = qw(Spreadsheet::WriteExcel::Workbook Exporter);

$VERSION = '2.40';

###############################################################################
#
# new()
#
# Constructor. Thin wrapper for a Workbook object.
#
# This module is no longer required directly and its use is deprecated. See
# the Pod documentation below.
#
sub new {

    my $class = shift;
    my $self  = Spreadsheet::WriteExcel::Workbook->new(@_);

    # Check for file creation failures before re-blessing
    bless  $self, $class if defined $self;

    return $self;
}


1;


__END__

=encoding latin1

=head1 NAME


Big - A class for creating Excel files > 7MB.


=head1 SYNOPSIS

Use of this module is deprecated. See below.


=head1 DESCRIPTION

The module was a sub-class of Spreadsheet::WriteExcel used for creating Excel files greater than 7MB. However, it is no longer required and is now deprecated.

As of version 2.17 Spreadsheet::WriteExcel can create files larger than 7MB directly if OLE::Storage_Lite is installed.

This module only exists for backwards compatibility. If your programs use ::Big you should convert them to use Spreadsheet::WritExcel directly.


=head1 REQUIREMENTS

L<OLE::Storage_Lite>.


=head1 AUTHOR


John McNamara jmcnamara@cpan.org


=head1 COPYRIGHT


Copyright MM-MMX, John McNamara.


All Rights Reserved. This module is free software. It may be used, redistributed and/or modified under the same terms as Perl itself.
