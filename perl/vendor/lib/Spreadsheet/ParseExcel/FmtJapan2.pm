package Spreadsheet::ParseExcel::FmtJapan2;

###############################################################################
#
# Spreadsheet::ParseExcel::FmtJapan2 - A class for Cell formats.
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

use Jcode;
use Unicode::Map;
use base 'Spreadsheet::ParseExcel::FmtJapan';
our $VERSION = '0.65';

#------------------------------------------------------------------------------
# new (for Spreadsheet::ParseExcel::FmtJapan2)
#------------------------------------------------------------------------------
sub new {
    my ( $sPkg, %hKey ) = @_;
    my $oMap = Unicode::Map->new('CP932Excel');
    die "NO MAP FILE CP932Excel!!"
      unless ( -r Unicode::Map->mapping("CP932Excel") );

    my $oThis = {
        Code    => $hKey{Code},
        _UniMap => $oMap,
    };
    bless $oThis;
    $oThis->SUPER::new(%hKey);
    return $oThis;
}

#------------------------------------------------------------------------------
# TextFmt (for Spreadsheet::ParseExcel::FmtJapan2)
#------------------------------------------------------------------------------
sub TextFmt {
    my ( $oThis, $sTxt, $sCode ) = @_;

    #    $sCode = 'sjis' if((! defined($sCode)) || ($sCode eq '_native_'));
    if ( $oThis->{Code} ) {
        if ( !defined($sCode) ) {
            $sTxt =~ s/(.)/\x00$1/sg;
            $sTxt = $oThis->{_UniMap}->from_unicode($sTxt);
        }
        elsif ( $sCode eq 'ucs2' ) {
            $sTxt = $oThis->{_UniMap}->from_unicode($sTxt);
        }
        return Jcode::convert( $sTxt, $oThis->{Code}, 'sjis' );
    }
    else {
        return $sTxt;
    }
}
1;

__END__

=pod

=head1 NAME

Spreadsheet::ParseExcel::FmtJapan2 - A class for Cell formats.

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
