package Spreadsheet::ParseExcel::Dump;

###############################################################################
#
# Spreadsheet::ParseExcel::Dump - A class for dumping Excel records.
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

my %NameTbl = (

    #P291
    0x0A => 'EOF',
    0x0C => 'CALCCOUNT',
    0x0D => 'CALCMODE',
    0x0E => 'PRECISION',
    0x0F => 'REFMODE',
    0x10 => 'DELTA',
    0x11 => 'ITERATION',
    0x12 => 'PROTECT',
    0x13 => 'PASSWORD',
    0x14 => 'HEADER',

    0x15 => 'FOOTER',
    0x16 => 'EXTERNCOUNT',
    0x17 => 'EXTERNSHEET',
    0x19 => 'WINDOWPROTECT',
    0x1A => 'VERTICALPAGEBREAKS',
    0x1B => 'HORIZONTALPAGEBREAKS',
    0x1C => 'NOTE',
    0x1D => 'SELECTION',
    0x22 => '1904',
    0x26 => 'LEFTMARGIN',

    0x27 => 'RIGHTMARGIN',
    0x28 => 'TOPMARGIN',
    0x29 => 'BOTTOMMARGIN',
    0x2A => 'PRINTHEADERS',
    0x2B => 'PRINTGRIDLINES',
    0x2F => 'FILEPASS',
    0x3C => 'COUNTINUE',
    0x3D => 'WINDOW1',
    0x40 => 'BACKUP',
    0x41 => 'PANE',

    0x42 => 'CODEPAGE',
    0x4D => 'PLS',
    0x50 => 'DCON',
    0x51 => 'DCONREF',

    #P292
    0x52 => 'DCONNAME',
    0x55 => 'DEFCOLWIDTH',
    0x59 => 'XCT',
    0x5A => 'CRN',
    0x5B => 'FILESHARING',
    0x5C => 'WRITEACCES',
    0x5D => 'OBJ',
    0x5E => 'UNCALCED',
    0x5F => 'SAVERECALC',
    0x60 => 'TEMPLATE',

    0x63 => 'OBJPROTECT',
    0x7D => 'COLINFO',
    0x7E => 'RK',
    0x7F => 'IMDATA',
    0x80 => 'GUTS',
    0x81 => 'WSBOOL',
    0x82 => 'GRIDSET',
    0x83 => 'HCENTER',
    0x84 => 'VCENTER',
    0x85 => 'BOUNDSHEET',

    0x86 => 'WRITEPROT',
    0x87 => 'ADDIN',
    0x88 => 'EDG',
    0x89 => 'PUB',
    0x8C => 'COUNTRY',
    0x8D => 'HIDEOBJ',
    0x90 => 'SORT',
    0x91 => 'SUB',
    0x92 => 'PALETTE',
    0x94 => 'LHRECORD',

    0x95 => 'LHNGRAPH',
    0x96 => 'SOUND',
    0x98 => 'LPR',
    0x99 => 'STANDARDWIDTH',
    0x9A => 'FNGROUPNAME',
    0x9B => 'FILTERMODE',
    0x9C => 'FNGROUPCOUNT',

    #P293
    0x9D => 'AUTOFILTERINFO',
    0x9E => 'AUTOFILTER',
    0xA0 => 'SCL',
    0xA1 => 'SETUP',
    0xA9 => 'COORDLIST',
    0xAB => 'GCW',
    0xAE => 'SCENMAN',
    0xAF => 'SCENARIO',
    0xB0 => 'SXVIEW',
    0xB1 => 'SXVD',

    0xB2 => 'SXV',
    0xB4 => 'SXIVD',
    0xB5 => 'SXLI',
    0xB6 => 'SXPI',
    0xB8 => 'DOCROUTE',
    0xB9 => 'RECIPNAME',
    0xBC => 'SHRFMLA',
    0xBD => 'MULRK',
    0xBE => 'MULBLANK',
    0xBF => 'TOOLBARHDR',
    0xC0 => 'TOOLBAREND',
    0xC1 => 'MMS',

    0xC2 => 'ADDMENU',
    0xC3 => 'DELMENU',
    0xC5 => 'SXDI',
    0xC6 => 'SXDB',
    0xCD => 'SXSTRING',
    0xD0 => 'SXTBL',
    0xD1 => 'SXTBRGIITM',
    0xD2 => 'SXTBPG',
    0xD3 => 'OBPROJ',
    0xD5 => 'SXISDTM',

    0xD6 => 'RSTRING',
    0xD7 => 'DBCELL',
    0xDA => 'BOOKBOOL',
    0xDC => 'PARAMQRY',
    0xDC => 'SXEXT',
    0xDD => 'SCENPROTECT',
    0xDE => 'OLESIZE',

    #P294
    0xDF => 'UDDESC',
    0xE0 => 'XF',
    0xE1 => 'INTERFACEHDR',
    0xE2 => 'INTERFACEEND',
    0xE3 => 'SXVS',
    0xEA => 'TABIDCONF',
    0xEB => 'MSODRAWINGGROUP',
    0xEC => 'MSODRAWING',
    0xED => 'MSODRAWINGSELECTION',
    0xEF => 'PHONETICINFO',
    0xF0 => 'SXRULE',

    0xF1 => 'SXEXT',
    0xF2 => 'SXFILT',
    0xF6 => 'SXNAME',
    0xF7 => 'SXSELECT',
    0xF8 => 'SXPAIR',
    0xF9 => 'SXFMLA',
    0xFB => 'SXFORMAT',
    0xFC => 'SST',
    0xFD => 'LABELSST',
    0xFF => 'EXTSST',

    0x100 => 'SXVDEX',
    0x103 => 'SXFORMULA',
    0x122 => 'SXDBEX',
    0x13D => 'TABID',
    0x160 => 'USESELFS',
    0x161 => 'DSF',
    0x162 => 'XL5MODIFY',
    0x1A5 => 'FILESHARING2',
    0x1A9 => 'USERBVIEW',
    0x1AA => 'USERVIEWBEGIN',

    0x1AB => 'USERSVIEWEND',
    0x1AD => 'QSI',
    0x1AE => 'SUPBOOK',
    0x1AF => 'PROT4REV',
    0x1B0 => 'CONDFMT',
    0x1B1 => 'CF',
    0x1B2 => 'DVAL',

    #P295
    0x1B5 => 'DCONBIN',
    0x1B6 => 'TXO',
    0x1B7 => 'REFRESHALL',
    0x1B8 => 'HLINK',
    0x1BA => 'CODENAME',
    0x1BB => 'SXFDBTYPE',
    0x1BC => 'PROT4REVPASS',
    0x1BE => 'DV',
    0x200 => 'DIMENSIONS',
    0x201 => 'BLANK',

    0x202 => 'Integer',            #Not Documented
    0x203 => 'NUMBER',
    0x204 => 'LABEL',
    0x205 => 'BOOLERR',
    0x207 => 'STRING',
    0x208 => 'ROW',
    0x20B => 'INDEX',
    0x218 => 'NAME',
    0x221 => 'ARRAY',
    0x223 => 'EXTERNNAME',
    0x225 => 'DEFAULTROWHEIGHT',

    0x231 => 'FONT',
    0x236 => 'TABLE',
    0x23E => 'WINDOW2',
    0x293 => 'STYLE',
    0x406 => 'FORMULA',
    0x41E => 'FORMAT',

    0x18 => 'NAME',

    0x06 => 'FORMULA',

    0x09  => 'BOF(BIFF2)',
    0x209 => 'BOF(BIFF3)',
    0x409 => 'BOF(BIFF4)',
    0x809 => 'BOF(BIFF5-7)',

    0x31 => 'FONT', 0x27E => 'RK',

    #Chart/Graph
    0x1001 => 'UNITS',
    0x1002 => 'CHART',
    0x1003 => 'SERISES',
    0x1006 => 'DATAFORMAT',
    0x1007 => 'LINEFORMAT',
    0x1009 => 'MAKERFORMAT',
    0x100A => 'AREAFORMAT',
    0x100B => 'PIEFORMAT',
    0x100C => 'ATTACHEDLABEL',
    0x100D => 'SERIESTEXT',
    0x1014 => 'CHARTFORMAT',
    0x1015 => 'LEGEND',
    0x1016 => 'SERIESLIST',
    0x1017 => 'BAR',
    0x1018 => 'LINE',
    0x1019 => 'PIE',
    0x101A => 'AREA',
    0x101B => 'SCATTER',
    0x101C => 'CHARTLINE',
    0x101D => 'AXIS',
    0x101E => 'TICK',
    0x101F => 'VALUERANGE',
    0x1020 => 'CATSERRANGE',
    0x1021 => 'AXISLINEFORMAT',
    0x1022 => 'CHARTFORMATLINK',
    0x1024 => 'DEFAULTTEXT',
    0x1025 => 'TEXT',
    0x1026 => 'FONTX',
    0x1027 => 'OBJECTLINK',
    0x1032 => 'FRAME',
    0x1033 => 'BEGIN',
    0x1034 => 'END',
    0x1035 => 'PLOTAREA',
    0x103A => '3D',
    0x103C => 'PICF',
    0x103D => 'DROPBAR',
    0x103E => 'RADAR',
    0x103F => 'SURFACE',
    0x1040 => 'RADARAREA',
    0x1041 => 'AXISPARENT',
    0x1043 => 'LEGENDXN',
    0x1044 => 'SHTPROPS',
    0x1045 => 'SERTOCRT',
    0x1046 => 'AXESUSED',
    0x1048 => 'SBASEREF',
    0x104A => 'SERPARENT',
    0x104B => 'SERAUXTREND',
    0x104E => 'IFMT',
    0x104F => 'POS',
    0x1050 => 'ALRUNS',
    0x1051 => 'AI',
    0x105B => 'SERAUXERRBAR',
    0x105D => 'SERFMT',
    0x1060 => 'FBI',
    0x1061 => 'BOPPOP',
    0x1062 => 'AXCEXT',
    0x1063 => 'DAT',
    0x1064 => 'PLOTGROWTH',
    0x1065 => 'SINDEX',
    0x1066 => 'GELFRAME',
    0x1067 => 'BPOPPOPCUSTOM',
);

#------------------------------------------------------------------------------
# subDUMP (for Spreadsheet::ParseExcel)
#------------------------------------------------------------------------------
sub subDUMP {
    my ( $oBook, $bOp, $bLen, $sWk ) = @_;
    printf "%04X:%-23s (Len:%3d) : %s\n",
      $bOp, OpName($bOp), $bLen, unpack( "H40", $sWk );
}

#------------------------------------------------------------------------------
# Spreadsheet::ParseExcel->OpName
#------------------------------------------------------------------------------
sub OpName {
    my ($bOp) = @_;
    return ( defined $NameTbl{$bOp} ) ? $NameTbl{$bOp} : 'undef';
}

1;

__END__

=pod

=head1 NAME

Spreadsheet::ParseExcel::Dump - A class for dumping Excel records.

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

