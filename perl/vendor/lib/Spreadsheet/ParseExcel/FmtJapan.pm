package Spreadsheet::ParseExcel::FmtJapan;
use utf8;

###############################################################################
#
# Spreadsheet::ParseExcel::FmtJapan - A class for Cell formats.
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

use Encode qw(find_encoding decode);
use base 'Spreadsheet::ParseExcel::FmtDefault';
our $VERSION = '0.65';

my %FormatTable = (
    0x00 => 'General',
    0x01 => '0',
    0x02 => '0.00',
    0x03 => '#,##0',
    0x04 => '#,##0.00',
    0x05 => '(\\#,##0_);(\\#,##0)',
    0x06 => '(\\#,##0_);[Red](\\#,##0)',
    0x07 => '(\\#,##0.00_);(\\#,##0.00_)',
    0x08 => '(\\#,##0.00_);[Red](\\#,##0.00_)',
    0x09 => '0%',
    0x0A => '0.00%',
    0x0B => '0.00E+00',
    0x0C => '# ?/?',
    0x0D => '# ??/??',

    #    0x0E => 'm/d/yy',
    0x0E => 'yyyy/m/d',
    0x0F => 'd-mmm-yy',
    0x10 => 'd-mmm',
    0x11 => 'mmm-yy',
    0x12 => 'h:mm AM/PM',
    0x13 => 'h:mm:ss AM/PM',
    0x14 => 'h:mm',
    0x15 => 'h:mm:ss',

    #    0x16 => 'm/d/yy h:mm',
    0x16 => 'yyyy/m/d h:mm',

    #0x17-0x24 -- Differs in Natinal
    0x1E => 'm/d/yy',
    0x1F => 'yyyy"年"m"月"d"日"',
    0x20 => 'h"時"mm"分"',
    0x21 => 'h"時"mm"分"ss"秒"',

    #0x17-0x24 -- Differs in Natinal
    0x25 => '(#,##0_);(#,##0)',
    0x26 => '(#,##0_);[Red](#,##0)',
    0x27 => '(#,##0.00);(#,##0.00)',
    0x28 => '(#,##0.00);[Red](#,##0.00)',
    0x29 => '_(*#,##0_);_(*(#,##0);_(*"-"_);_(@_)',
    0x2A => '_(\\*#,##0_);_(\\*(#,##0);_(*"-"_);_(@_)',
    0x2B => '_(*#,##0.00_);_(*(#,##0.00);_(*"-"??_);_(@_)',
    0x2C => '_(\\*#,##0.00_);_(\\*(#,##0.00);_(*"-"??_);_(@_)',
    0x2D => 'mm:ss',
    0x2E => '[h]:mm:ss',
    0x2F => 'mm:ss.0',
    0x30 => '##0.0E+0',
    0x31 => '@',

    0x37 => 'yyyy"年"m"月"',
    0x38 => 'm"月"d"日"',
    0x39 => 'ge.m.d',
    0x3A => 'ggge"年"m"月"d"日"',
);

#------------------------------------------------------------------------------
# new (for Spreadsheet::ParseExcel::FmtJapan)
#------------------------------------------------------------------------------
sub new {
    my ( $class, %args ) = @_;
    my $encoding = $args{Code} || $args{encoding};
    my $self = { Code => $encoding };
    if($encoding){
        $self->{encoding} = find_encoding($encoding eq 'sjis' ? 'cp932' : $encoding)
            or do{
                require Carp;
                Carp::croak(qq{Unknown encoding '$encoding'});
            };
    }
    return bless $self, $class;
}

#------------------------------------------------------------------------------
# TextFmt (for Spreadsheet::ParseExcel::FmtJapan)
#------------------------------------------------------------------------------
sub TextFmt {
    my ( $self, $text, $input_encoding ) = @_;
    if(!defined $input_encoding){
        $input_encoding = 'utf8';
    }
    elsif($input_encoding eq '_native_'){
        $input_encoding = 'cp932'; # Shift_JIS in Microsoft products
    }
    $text = decode($input_encoding, $text);
    return $self->{Code} ? $self->{encoding}->encode($text) : $text;
}
#------------------------------------------------------------------------------
# FmtStringDef (for Spreadsheet::ParseExcel::FmtJapan)
#------------------------------------------------------------------------------
sub FmtStringDef {
    my ( $self, $format_index, $book ) = @_;
    return $self->SUPER::FmtStringDef( $format_index, $book, \%FormatTable );
}

#------------------------------------------------------------------------------
# CnvNengo (for Spreadsheet::ParseExcel::FmtJapan)
#------------------------------------------------------------------------------

# Convert A.D. into Japanese Nengo (aka Gengo)

my @Nengo = (
	{
		name      => '平成', # Heisei
		abbr_name => 'H',

		base      => 1988,
		start     => 19890108,
	},
	{
		name      => '昭和', # Showa
		abbr_name => 'S',

		base      => 1925,
		start     => 19261225,
	},
	{
		name      => '大正', # Taisho
		abbr_name => 'T',

		base      => 1911,
		start     => 19120730,
	},
	{
		name      => '明治', # Meiji
		abbr_name => 'M',

		base      => 1867,
		start     => 18680908,
	},
);

# Usage: CnvNengo(name => @tm) or CnvNeng(abbr_name => @tm)
sub CnvNengo {
    my ( $kind, @tm ) = @_;
    my $year = $tm[5] + 1900;
    my $wk = ($year * 10000) + ($tm[4] * 100) + ($tm[3] * 1);
    #my $wk = sprintf( '%04d%02d%02d', $year, $tm[4], $tm[3] );
    foreach my $nengo(@Nengo){
        if( $wk >= $nengo->{start} ){
            return $nengo->{$kind} . ($year - $nengo->{base});
        }
    }
    return $year;
}

1;

__END__

=pod

=head1 NAME

Spreadsheet::ParseExcel::FmtJapan - A class for Cell formats.

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
