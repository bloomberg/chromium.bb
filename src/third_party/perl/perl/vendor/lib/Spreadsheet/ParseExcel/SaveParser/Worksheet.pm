package Spreadsheet::ParseExcel::SaveParser::Worksheet;

###############################################################################
#
# Spreadsheet::ParseExcel::SaveParser::Worksheet - A class for SaveParser Worksheets.
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

#==============================================================================
# Spreadsheet::ParseExcel::SaveParser::Worksheet
#==============================================================================

use base 'Spreadsheet::ParseExcel::Worksheet';
our $VERSION = '0.65';

sub new {
    my ( $sClass, %rhIni ) = @_;
    $sClass->SUPER::new(%rhIni);    # returns object
}

#------------------------------------------------------------------------------
# AddCell (for Spreadsheet::ParseExcel::SaveParser::Worksheet)
#------------------------------------------------------------------------------
sub AddCell {
    my ( $oSelf, $iR, $iC, $sVal, $oCell, $sCode ) = @_;
    
	$oSelf->{_Book}
      ->AddCell( $oSelf->{_SheetNo}, $iR, $iC, $sVal, $oCell, $sCode );
}

#------------------------------------------------------------------------------
# Protect (for Spreadsheet::ParseExcel::SaveParser::Worksheet)
#  - Password = undef   ->  No protect
#  - Password = ''      ->  Protected. No password
#  - Password = $pwd    ->  Protected. Password = $pwd
#------------------------------------------------------------------------------
sub Protect {
    my ( $oSelf, $sPassword ) = @_;
    $oSelf->{Protect} = $sPassword;
}

1;

__END__

=pod

=head1 NAME

Spreadsheet::ParseExcel::SaveParser::Worksheet - A class for SaveParser Worksheets.

=head1 SYNOPSIS

See the documentation for Spreadsheet::ParseExcel.

=head1 DESCRIPTION

This module is used in conjunction with Spreadsheet::ParseExcel. See the documentation for Spreadsheet::ParseExcel.

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
