package Spreadsheet::ParseExcel::Utility;

###############################################################################
#
# Spreadsheet::ParseExcel::Utility - Utility functions for ParseExcel.
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

require Exporter;
use vars qw(@ISA @EXPORT_OK);
@ISA       = qw(Exporter);
@EXPORT_OK = qw(ExcelFmt LocaltimeExcel ExcelLocaltime
  col2int int2col sheetRef xls2csv);

our $VERSION = '0.65';

my $qrNUMBER = qr/(^[+-]?\d+(\.\d+)?$)|(^[+-]?\d+\.?(\d*)[eE][+-](\d+))$/;

###############################################################################
#
# ExcelFmt()
#
# This function takes an Excel style number format and converts a number into
# that format. for example: 'hh:mm:ss AM/PM' + 0.01023148 = '12:14:44 AM'.
#
# It does this with a type of templating mechanism. The format string is parsed
# to identify tokens that need to be replaced and their position within the
# string is recorded. These can be thought of as placeholders. The number is
# then converted to the required formats and substituted into the placeholders.
#
# Interested parties should refer to the Excel documentation on cell formats for
# more information: http://office.microsoft.com/en-us/excel/HP051995001033.aspx
# The Microsoft documentation for the Excel Binary File Format, [MS-XLS].pdf,
# also contains a ABNF grammar for number format strings.
#
# Maintainers notes:
# ==================
#
# Note on format subsections:
# A format string can contain 4 possible sub-sections separated by semi-colons:
# Positive numbers, negative numbers, zero values, and text.
# For example: _(* #,##0_);_(* (#,##0);_(* "-"_);_(@_)
#
# Note on conditional formats.
# A number format in Excel can have a conditional expression such as:
#     [>9999999](000)000-0000;000-0000
# This is equivalent to the following in Perl:
#     $format = $number > 9999999 ? '(000)000-0000' : '000-0000';
# Nested conditionals are also possible but we don't support them.
#
# Efficiency: The excessive use of substr() isn't very efficient. However,
# it probably doesn't merit rewriting this function with a parser or regular
# expressions and \G.
#
# TODO: I think the single quote handling may not be required. Check.
#
sub ExcelFmt {

    my ( $format_str, $number, $is_1904, $number_type, $want_subformats ) = @_;

    # Return text strings without further formatting.
    return $number unless $number =~ $qrNUMBER;

    # Handle OpenOffice.org GENERAL format.
    $format_str = '@' if uc($format_str) eq "GENERAL";

    # Check for a conditional at the start of the format. See notes above.
    my $conditional;
    if ( $format_str =~ /^\[([<>=][^\]]+)\](.*)$/ ) {
        $conditional = $1;
        $format_str  = $2;
    }

    # Ignore the underscore token which is used to indicate a padding space.
    $format_str =~ s/_/ /g;

    # Split the format string into 4 possible sub-sections: positive numbers,
    # negative numbers, zero values, and text. See notes above.
    my @formats;
    my $section      = 0;
    my $double_quote = 0;
    my $single_quote = 0;

    # Initial parsing of the format string to remove escape characters. This
    # also handles quoted strings. See note about single quotes above.
  CHARACTER:
    for my $char ( split //, $format_str ) {

        if ( $double_quote or $single_quote ) {
            $formats[$section] .= $char;
            $double_quote = 0 if $char eq '"';
            $single_quote = 0;
            next CHARACTER;
        }

        if ( $char eq ';' ) {
            $section++;
            next CHARACTER;
        }
        elsif ( $char eq '"' ) {
            $double_quote = 1;
        }
        elsif ( $char eq '!' ) {
            $single_quote = 1;
        }
        elsif ( $char eq '\\' ) {
            $single_quote = 1;
        }
        elsif ( $char eq '(' ) {
            next CHARACTER;    # Ignore.
        }
        elsif ( $char eq ')' ) {
            next CHARACTER;    # Ignore.
        }

        # Convert upper case OpenOffice.org date/time formats to lowercase..
        $char = lc($char) if $char =~ /[DMYHS]/;

        $formats[$section] .= $char;
    }

    # Select the appropriate format from the 4 possible sub-sections:
    # positive numbers, negative numbers, zero values, and text.
    # We ignore the Text section since non-numeric values are returned
    # unformatted at the start of the function.
    my $format;
    $section = 0;

    if ( @formats == 1 ) {
        $section = 0;
    }
    elsif ( @formats == 2 ) {
        if ( $number < 0 ) {
            $section = 1;
        }
        else {
            $section = 0;
        }
    }
    elsif ( @formats == 3 ) {
        if ( $number == 0 ) {
            $section = 2;
        }
        elsif ( $number < 0 ) {
            $section = 1;
        }
        else {
            $section = 0;
        }
    }
    else {
        $section = 0;
    }

    # Override the previous choice if the format is conditional.
    if ($conditional) {

        # TODO. Replace string eval with a function.
        $section = eval "$number $conditional" ? 0 : 1;
    }

    # We now have the required format.
    $format = $formats[$section];

    # The format string can contain one of the following colours:
    # [Black] [Blue] [Cyan] [Green] [Magenta] [Red] [White] [Yellow]
    # or the string [ColorX] where x is a colour index from 1 to 56.
    # We don't use the colour but we return it to the caller.
    #
    my $color = '';
    if ( $format =~ s/^(\[[A-Za-z]{3,}(\d{1,2})?\])// ) {
        $color = $1;
    }

    # Remove the locale, such as [$-409], from the format string.
    my $locale = '';
    if ( $format =~ s/^(\[\$?-F?\d+\])// ) {
        $locale = $1;
    }

    # Replace currency locale, such as [$$-409], with $ in the format string.
    # See the RT#60547 test cases in 21_number_format_user.t.
    if ( $format =~ s/(\[\$([^-]+)(-\d+)?\])/$2/s ) {
        $locale = $1;
    }


    # Remove leading # from '# ?/?', '# ??/??' fraction formats.
    $format =~ s{# \?}{?}g;

    # Parse the format string and create an AoA of placeholders that contain
    # the parts of the string to be replaced. The format of the information
    # stored is: [ $token, $start_pos, $end_pos, $option_info ].
    #
    my $format_mode  = '';    # Either: '', 'number', 'date'
    my $pos          = 0;     # Character position within format string.
    my @placeholders = ();    # Arefs with parts of the format to be replaced.
    my $token        = '';    # The actual format extracted from the total str.
    my $start_pos;            # A position variable. Initial parser position.
    my $token_start = -1;     # A position variable.
    my $decimal_pos = -1;     # Position of the punctuation char "." or ",".
    my $comma_count = 0;      # Count of the commas in the format.
    my $is_fraction = 0;      # Number format is a fraction.
    my $is_currency = 0;      # Number format is a currency.
    my $is_percent  = 0;      # Number format is a percentage.
    my $is_12_hour  = 0;      # Time format is using 12 hour clock.
    my $seen_dot    = 0;      # Treat only the first "." as the decimal point.

    # Parse the format.
  PARSER:
    while ( $pos < length $format ) {
        $start_pos = $pos;
        my $char = substr( $format, $pos, 1 );

        # Ignore control format characters such as '#0+-.?eE,%'. However,
        # only ignore '.' if it is the first one encountered. RT 45502.
        if ( ( !$seen_dot && $char !~ /[#0\+\-\.\?eE\,\%]/ )
            || $char !~ /[#0\+\-\?eE\,\%]/ )
        {

            if ( $token_start != -1 ) {
                push @placeholders,
                  [
                    substr( $format, $token_start, $pos - $token_start ),
                    $decimal_pos, $pos - $token_start
                  ];
                $token_start = -1;
            }
        }

        # Processing for quoted strings within the format. See notes above.
        if ( $char eq '"' ) {
            $double_quote = $double_quote ? 0 : 1;
            $pos++;
            next PARSER;
        }
        elsif ( $char eq '!' ) {
            $single_quote = 1;
            $pos++;
            next PARSER;
        }
        elsif ( $char eq '\\' ) {
            if ( $single_quote != 1 ) {
                $single_quote = 1;
                $pos++;
                next PARSER;
            }
        }

        if (   ( defined($double_quote) and ($double_quote) )
            or ( defined($single_quote) and ($single_quote) )
            or ( $seen_dot && $char eq '.' ) )
        {
            $single_quote = 0;
            if (
                ( $format_mode ne 'date' )
                and (  ( substr( $format, $pos, 2 ) eq "\x81\xA2" )
                    || ( substr( $format, $pos, 2 ) eq "\x81\xA3" )
                    || ( substr( $format, $pos, 2 ) eq "\xA2\xA4" )
                    || ( substr( $format, $pos, 2 ) eq "\xA2\xA5" ) )
              )
            {

                # The above matches are currency symbols.
                push @placeholders,
                  [ substr( $format, $pos, 2 ), length($token), 2 ];
                $is_currency = 1;
                $pos += 2;
            }
            else {
                $pos++;
            }
        }
        elsif (
            ( $char =~ /[#0\+\.\?eE\,\%]/ )
            || (    ( $format_mode ne 'date' )
                and ( ( $char eq '-' ) || ( $char eq '(' ) || ( $char eq ')' ) )
            )
          )
        {
            $format_mode = 'number' unless $format_mode;
            if ( substr( $format, $pos, 1 ) =~ /[#0]/ ) {
                if (
                    substr( $format, $pos ) =~
                    /^([#0]+[\.]?[0#]*[eE][\+\-][0#]+)/ )
                {
                    push @placeholders, [ $1, $pos, length($1) ];
                    $pos += length($1);
                }
                else {
                    if ( $token_start == -1 ) {
                        $token_start = $pos;
                        $decimal_pos = length($token);
                    }
                }
            }
            elsif ( substr( $format, $pos, 1 ) eq '?' ) {

                # Look for a fraction format like ?/? or ??/??
                if ( $token_start != -1 ) {
                    push @placeholders,
                      [
                        substr(
                            $format, $token_start, $pos - $token_start + 1
                        ),
                        $decimal_pos,
                        $pos - $token_start + 1
                      ];
                }
                $token_start = $pos;

                # Find the end of the fraction format.
              FRACTION:
                while ( $pos < length($format) ) {
                    if ( substr( $format, $pos, 1 ) eq '/' ) {
                        $is_fraction = 1;
                    }
                    elsif ( substr( $format, $pos, 1 ) eq '?' ) {
                        $pos++;
                        next FRACTION;
                    }
                    else {
                        if ( $is_fraction
                            && ( substr( $format, $pos, 1 ) =~ /[0-9]/ ) )
                        {

                            # TODO: Could invert if() logic and remove this.
                            $pos++;
                            next FRACTION;
                        }
                        else {
                            last FRACTION;
                        }
                    }
                    $pos++;
                }
                $pos--;

                push @placeholders,
                  [
                    substr( $format, $token_start, $pos - $token_start + 1 ),
                    length($token), $pos - $token_start + 1
                  ];
                $token_start = -1;
            }
            elsif ( substr( $format, $pos, 3 ) =~ /^[eE][\+\-][0#]$/ ) {
                if ( substr( $format, $pos ) =~ /([eE][\+\-][0#]+)/ ) {
                    push @placeholders, [ $1, $pos, length($1) ];
                    $pos += length($1);
                }
                $token_start = -1;
            }
            else {
                if ( $token_start != -1 ) {
                    push @placeholders,
                      [
                        substr( $format, $token_start, $pos - $token_start ),
                        $decimal_pos, $pos - $token_start
                      ];
                    $token_start = -1;
                }
                if ( substr( $format, $pos, 1 ) =~ /[\+\-]/ ) {
                    push @placeholders,
                      [ substr( $format, $pos, 1 ), length($token), 1 ];
                    $is_currency = 1;
                }
                elsif ( substr( $format, $pos, 1 ) eq '.' ) {
                    push @placeholders,
                      [ substr( $format, $pos, 1 ), length($token), 1 ];
                    $seen_dot = 1;
                }
                elsif ( substr( $format, $pos, 1 ) eq ',' ) {
                    $comma_count++;
                    push @placeholders,
                      [ substr( $format, $pos, 1 ), length($token), 1 ];
                }
                elsif ( substr( $format, $pos, 1 ) eq '%' ) {
                    $is_percent = 1;
                }
                elsif (( substr( $format, $pos, 1 ) eq '(' )
                    || ( substr( $format, $pos, 1 ) eq ')' ) )
                {
                    push @placeholders,
                      [ substr( $format, $pos, 1 ), length($token), 1 ];
                    $is_currency = 1;
                }
            }
            $pos++;
        }
        elsif ( $char =~ /[ymdhsapg]/i ) {
            $format_mode = 'date' unless $format_mode;
            if ( substr( $format, $pos, 5 ) =~ /am\/pm/i ) {
                push @placeholders, [ 'am/pm', length($token), 5 ];
                $is_12_hour = 1;
                $pos += 5;
            }
            elsif ( substr( $format, $pos, 3 ) =~ /a\/p/i ) {
                push @placeholders, [ 'a/p', length($token), 3 ];
                $is_12_hour = 1;
                $pos += 3;
            }
            elsif ( substr( $format, $pos, 5 ) eq 'mmmmm' ) {
                push @placeholders, [ 'mmmmm', length($token), 5 ];
                $pos += 5;
            }
            elsif (( substr( $format, $pos, 4 ) eq 'mmmm' )
                || ( substr( $format, $pos, 4 ) eq 'dddd' )
                || ( substr( $format, $pos, 4 ) eq 'yyyy' )
                || ( substr( $format, $pos, 4 ) eq 'ggge' ) )
            {
                push @placeholders,
                  [ substr( $format, $pos, 4 ), length($token), 4 ];
                $pos += 4;
            }
            elsif (( substr( $format, $pos, 3 ) eq 'ddd' )
                || ( substr( $format, $pos, 3 ) eq 'mmm' )
                || ( substr( $format, $pos, 3 ) eq 'yyy' ) )
            {
                push @placeholders,
                  [ substr( $format, $pos, 3 ), length($token), 3 ];
                $pos += 3;
            }
            elsif (( substr( $format, $pos, 2 ) eq 'yy' )
                || ( substr( $format, $pos, 2 ) eq 'mm' )
                || ( substr( $format, $pos, 2 ) eq 'dd' )
                || ( substr( $format, $pos, 2 ) eq 'hh' )
                || ( substr( $format, $pos, 2 ) eq 'ss' )
                || ( substr( $format, $pos, 2 ) eq 'ge' ) )
            {
                if (
                       ( substr( $format, $pos, 2 ) eq 'mm' )
                    && (@placeholders)
                    && (   ( $placeholders[-1]->[0] eq 'h' )
                        or ( $placeholders[-1]->[0] eq 'hh' ) )
                  )
                {

                    # For this case 'm' is minutes not months.
                    push @placeholders, [ 'mm', length($token), 2, 'minutes' ];
                }
                else {
                    push @placeholders,
                      [ substr( $format, $pos, 2 ), length($token), 2 ];
                }
                if (   ( substr( $format, $pos, 2 ) eq 'ss' )
                    && ( @placeholders > 1 ) )
                {
                    if (   ( $placeholders[-2]->[0] eq 'm' )
                        || ( $placeholders[-2]->[0] eq 'mm' ) )
                    {

                        # For this case 'm' is minutes not months.
                        push( @{ $placeholders[-2] }, 'minutes' );
                    }
                }
                $pos += 2;
            }
            elsif (( substr( $format, $pos, 1 ) eq 'm' )
                || ( substr( $format, $pos, 1 ) eq 'd' )
                || ( substr( $format, $pos, 1 ) eq 'h' )
                || ( substr( $format, $pos, 1 ) eq 's' ) )
            {
                if (
                       ( substr( $format, $pos, 1 ) eq 'm' )
                    && (@placeholders)
                    && (   ( $placeholders[-1]->[0] eq 'h' )
                        or ( $placeholders[-1]->[0] eq 'hh' ) )
                  )
                {

                    # For this case 'm' is minutes not months.
                    push @placeholders, [ 'm', length($token), 1, 'minutes' ];
                }
                else {
                    push @placeholders,
                      [ substr( $format, $pos, 1 ), length($token), 1 ];
                }
                if (   ( substr( $format, $pos, 1 ) eq 's' )
                    && ( @placeholders > 1 ) )
                {
                    if (   ( $placeholders[-2]->[0] eq 'm' )
                        || ( $placeholders[-2]->[0] eq 'mm' ) )
                    {

                        # For this case 'm' is minutes not months.
                        push( @{ $placeholders[-2] }, 'minutes' );
                    }
                }
                $pos += 1;
            }
        }
        elsif ( ( substr( $format, $pos, 3 ) eq '[h]' ) ) {
            $format_mode = 'date' unless $format_mode;
            push @placeholders, [ '[h]', length($token), 3 ];
            $pos += 3;
        }
        elsif ( ( substr( $format, $pos, 4 ) eq '[mm]' ) ) {
            $format_mode = 'date' unless $format_mode;
            push @placeholders, [ '[mm]', length($token), 4 ];
            $pos += 4;
        }
        elsif ( $char eq '@' ) {
            push @placeholders, [ '@', length($token), 1 ];
            $pos++;
        }
        elsif ( $char eq '*' ) {
            push @placeholders,
              [ substr( $format, $pos, 1 ), length($token), 1 ];
        }
        else {
            $pos++;
        }
        $pos++ if ( $pos == $start_pos );    #No Format match
        $token .= substr( $format, $start_pos, $pos - $start_pos );

    }    # End of parsing.

    # Copy the located format string to a result string that we will perform
    # the substitutions on and return to the user.
    my $result = $token;

    # Add a placeholder between the decimal/comma and end of the token, if any.
    if ( $token_start != -1 ) {
        push @placeholders,
          [
            substr( $format, $token_start, $pos - $token_start + 1 ),
            $decimal_pos, $pos - $token_start + 1
          ];
    }

    #
    # In the next sections we process date, number and text formats. We take a
    # format such as yyyy/mm/dd and replace it with something like 2008/12/25.
    #
    if ( ( $format_mode eq 'date' ) && ( $number =~ $qrNUMBER ) ) {

        # The maximum allowable date in Excel is 9999-12-31T23:59:59.000 which
        # equates to 2958465.999+ in the 1900 epoch and 2957003.999+ in the
        # 1904 epoch. We use 0 as the minimum in both epochs. The 1904 system
        # actually supports negative numbers but that isn't worth the effort.
        my $min_date = 0;
        my $max_date = 2958466;
        $max_date = 2957004 if $is_1904;

        if ( $number < $min_date || $number >= $max_date ) {
            return $number;    # Return unformatted number.
        }

        # Process date formats.
        my @time = ExcelLocaltime( $number, $is_1904 );

        #    0     1     2      3     4       5      6      7
        my ( $sec, $min, $hour, $day, $month, $year, $wday, $msec ) = @time;

        $month++;              # localtime() zero indexed month.
        $year += 1900;         # localtime() year.

        my @full_month_name = qw(
          None January February March April May June July
          August September October November December
        );
        my @short_month_name = qw(
          None Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
        );
        my @full_day_name = qw(
          Sunday Monday Tuesday Wednesday Thursday Friday Saturday
        );
        my @short_day_name = qw(
          Sun Mon Tue Wed Thu Fri Sat
        );

        # Replace the placeholders in the template such as yyyy mm dd with
        # actual numbers or strings.
        my $replacement;
        for my $placeholder ( reverse @placeholders ) {

            if ( $placeholder->[-1] eq 'minutes' ) {

                # For this case 'm/mm' is minutes not months.
                if ( $placeholder->[0] eq 'mm' ) {
                    $replacement = sprintf( "%02d", $min );
                }
                else {
                    $replacement = sprintf( "%d", $min );
                }
            }
            elsif ( $placeholder->[0] eq 'yyyy' ) {

                # 4 digit Year. 2000 -> 2000.
                $replacement = sprintf( '%04d', $year );
            }
            elsif ( $placeholder->[0] eq 'yy' ) {

                # 2 digit Year. 2000 -> 00.
                $replacement = sprintf( '%02d', $year % 100 );
            }
            elsif ( $placeholder->[0] eq 'mmmmm' ) {

                # First character of the month name. 1 -> J.
                $replacement = substr( $short_month_name[$month], 0, 1 );
            }
            elsif ( $placeholder->[0] eq 'mmmm' ) {

                # Full month name. 1 -> January.
                $replacement = $full_month_name[$month];
            }
            elsif ( $placeholder->[0] eq 'mmm' ) {

                # Short month name. 1 -> Jan.
                $replacement = $short_month_name[$month];
            }
            elsif ( $placeholder->[0] eq 'mm' ) {

                # 2 digit month. 1 -> 01.
                $replacement = sprintf( '%02d', $month );
            }
            elsif ( $placeholder->[0] eq 'm' ) {

                # 1 digit month. 1 -> 1.
                $replacement = sprintf( '%d', $month );
            }
            elsif ( $placeholder->[0] eq 'dddd' ) {

                # Full day name. Wednesday (for example.)
                $replacement = $full_day_name[$wday];
            }
            elsif ( $placeholder->[0] eq 'ddd' ) {

                # Short day name. Wed (for example.)
                $replacement = $short_day_name[$wday];
            }
            elsif ( $placeholder->[0] eq 'dd' ) {

                # 2 digit day. 1 -> 01.
                $replacement = sprintf( '%02d', $day );
            }
            elsif ( $placeholder->[0] eq 'd' ) {

                # 1 digit day. 1 -> 1.
                $replacement = sprintf( '%d', $day );
            }
            elsif ( $placeholder->[0] eq 'hh' ) {

                # 2 digit hour.
                if ($is_12_hour) {
                    my $hour_tmp = $hour % 12;
                    $hour_tmp = 12 if $hour % 12 == 0;
                    $replacement = sprintf( '%d', $hour_tmp );
                }
                else {
                    $replacement = sprintf( '%02d', $hour );
                }
            }
            elsif ( $placeholder->[0] eq 'h' ) {

                # 1 digit hour.
                if ($is_12_hour) {
                    my $hour_tmp = $hour % 12;
                    $hour_tmp = 12 if $hour % 12 == 0;
                    $replacement = sprintf( '%2d', $hour_tmp );
                }
                else {
                    $replacement = sprintf( '%d', $hour );
                }
            }
            elsif ( $placeholder->[0] eq 'ss' ) {

                # 2 digit seconds.
                $replacement = sprintf( '%02d', $sec );
            }
            elsif ( $placeholder->[0] eq 's' ) {

                # 1 digit seconds.
                $replacement = sprintf( '%d', $sec );
            }
            elsif ( $placeholder->[0] eq 'am/pm' ) {

                # AM/PM.
                $replacement = ( $hour >= 12 ) ? 'PM' : 'AM';
            }
            elsif ( $placeholder->[0] eq 'a/p' ) {

                # AM/PM.
                $replacement = ( $hour >= 12 ) ? 'P' : 'A';
            }
            elsif ( $placeholder->[0] eq '.' ) {

                # Decimal point for seconds.
                $replacement = '.';
            }
            elsif ( $placeholder->[0] =~ /(^0+$)/ ) {

                # Milliseconds. For example h:ss.000.
                my $length = length($1);
                $replacement =
                  substr( sprintf( "%.${length}f", $msec / 1000 ), 2, $length );
            }
            elsif ( $placeholder->[0] eq '[h]' ) {

                # Hours modulus 24. 25 displays as 25 not as 1.
                $replacement = sprintf( '%d', int($number) * 24 + $hour );
            }
            elsif ( $placeholder->[0] eq '[mm]' ) {

                # Mins modulus 60. 72 displays as 72 not as 12.
                $replacement =
                  sprintf( '%d', ( int($number) * 24 + $hour ) * 60 + $min );
            }
            elsif ( $placeholder->[0] eq 'ge' ) {
                require Spreadsheet::ParseExcel::FmtJapan;
                # Japanese Nengo (aka Gengo) in initialism (abbr. name)
                 $replacement =
                  Spreadsheet::ParseExcel::FmtJapan::CnvNengo( abbr_name => @time );
            }
            elsif ( $placeholder->[0] eq 'ggge' ) {
                require Spreadsheet::ParseExcel::FmtJapan;
                # Japanese Nengo (aka Gengo) in Kanji (full name)
                 $replacement =
                  Spreadsheet::ParseExcel::FmtJapan::CnvNengo( name => @time );
            }
            elsif ( $placeholder->[0] eq '@' ) {

                # Text format.
                $replacement = $number;
            }
            elsif ( $placeholder->[0] eq ',' ) {
                next;
            }

            # Substitute the replacement string back into the template.
            substr( $result, $placeholder->[1], $placeholder->[2],
                $replacement );
        }
    }
    elsif ( ( $format_mode eq 'number' ) && ( $number =~ $qrNUMBER ) ) {

        # Process non date formats.
        if (@placeholders) {
            while ( $placeholders[-1]->[0] eq ',' ) {
                $comma_count--;
                substr(
                    $result,
                    $placeholders[-1]->[1],
                    $placeholders[-1]->[2], ''
                );
                $number /= 1000;
                pop @placeholders;
            }

            my $number_format = join( '', map { $_->[0] } @placeholders );
            my $number_result;
            my $str_length    = 0;
            my $engineering   = 0;
            my $is_decimal    = 0;
            my $is_integer    = 0;
            my $after_decimal = undef;

            for my $token ( split //, $number_format ) {
                if ( $token eq '.' ) {
                    $str_length++;
                    $is_decimal = 1;
                }
                elsif ( ( $token eq 'E' ) || ( $token eq 'e' ) ) {
                    $engineering = 1;
                }
                elsif ( $token eq '0' ) {
                    $str_length++;
                    $after_decimal++ if $is_decimal;
                    $is_integer = 1;
                }
                elsif ( $token eq '#' ) {
                    $after_decimal++ if $is_decimal;
                    $is_integer = 1;
                }
                elsif ( $token eq '?' ) {
                    $after_decimal++ if $is_decimal;
                }
            }

            $number *= 100.0 if $is_percent;

            my $data = ($is_currency) ? abs($number) : $number + 0;

            if ($is_fraction) {
                $number_result = sprintf( "%0${str_length}d", int($data) );
            }
            else {
                if ($is_decimal) {

                    if ( defined $after_decimal ) {
                        $number_result =
                          sprintf "%0${str_length}.${after_decimal}f", $data;
                    }
                    else {
                        $number_result = sprintf "%0${str_length}f", $data;
                    }

                    # Fix for Perl and sprintf not rounding up like Excel.
                    # http://rt.cpan.org/Public/Bug/Display.html?id=45626
                    if ( $data =~ /^${number_result}5/ ) {
                        $number_result =
                          sprintf "%0${str_length}.${after_decimal}f",
                          $data . '1';
                    }
                }
                else {
                    $number_result = sprintf( "%0${str_length}.0f", $data );
                }
            }

            $number_result = AddComma($number_result) if $comma_count > 0;

            my $number_length = length($number_result);
            my $decimal_pos   = -1;
            my $replacement;

            for ( my $i = @placeholders - 1 ; $i >= 0 ; $i-- ) {
                my $placeholder = $placeholders[$i];

                if ( $placeholder->[0] =~
                    /([#0]*)([\.]?)([0#]*)([eE])([\+\-])([0#]+)/ )
                {
                    substr( $result, $placeholder->[1], $placeholder->[2],
                        MakeE( $placeholder->[0], $number ) );
                }
                elsif ( $placeholder->[0] =~ /\// ) {
                    substr( $result, $placeholder->[1], $placeholder->[2],
                        MakeFraction( $placeholder->[0], $number, $is_integer )
                    );
                }
                elsif ( $placeholder->[0] eq '.' ) {
                    $number_length--;
                    $decimal_pos = $number_length;
                }
                elsif ( $placeholder->[0] eq '+' ) {
                    substr( $result, $placeholder->[1], $placeholder->[2],
                        ( $number > 0 )
                        ? '+'
                        : ( ( $number == 0 ) ? '+' : '-' ) );
                }
                elsif ( $placeholder->[0] eq '-' ) {
                    substr( $result, $placeholder->[1], $placeholder->[2],
                        ( $number > 0 )
                        ? ''
                        : ( ( $number == 0 ) ? '' : '-' ) );
                }
                elsif ( $placeholder->[0] eq '@' ) {
                    substr( $result, $placeholder->[1], $placeholder->[2],
                        $number );
                }
                elsif ( $placeholder->[0] eq '*' ) {
                    substr( $result, $placeholder->[1], $placeholder->[2], '' );
                }
                elsif (( $placeholder->[0] eq "\xA2\xA4" )
                    or ( $placeholder->[0] eq "\xA2\xA5" )
                    or ( $placeholder->[0] eq "\x81\xA2" )
                    or ( $placeholder->[0] eq "\x81\xA3" ) )
                {
                    substr(
                        $result,           $placeholder->[1],
                        $placeholder->[2], $placeholder->[0]
                    );
                }
                elsif (( $placeholder->[0] eq '(' )
                    or ( $placeholder->[0] eq ')' ) )
                {
                    substr(
                        $result,           $placeholder->[1],
                        $placeholder->[2], $placeholder->[0]
                    );
                }
                else {
                    if ( $number_length > 0 ) {
                        if ( $i <= 0 ) {
                            $replacement =
                              substr( $number_result, 0, $number_length );
                            $number_length = 0;
                        }
                        else {
                            my $real_part_length = length( $placeholder->[0] );
                            if ( $decimal_pos >= 0 ) {
                                my $format = $placeholder->[0];
                                $format =~ s/^#+//;
                                $real_part_length = length $format;
                                $real_part_length =
                                  ( $number_length <= $real_part_length )
                                  ? $number_length
                                  : $real_part_length;
                            }
                            else {
                                $real_part_length =
                                  ( $number_length <= $real_part_length )
                                  ? $number_length
                                  : $real_part_length;
                            }
                            $replacement =
                              substr( $number_result,
                                $number_length - $real_part_length,
                                $real_part_length );
                            $number_length -= $real_part_length;
                        }
                    }
                    else {
                        $replacement = '';
                    }
                    substr( $result, $placeholder->[1], $placeholder->[2],
                        "\x00" . $replacement );
                }
            }
            $replacement =
              ( $number_length > 0 )
              ? substr( $number_result, 0, $number_length )
              : '';
            $result =~ s/\x00/$replacement/;
            $result =~ s/\x00//g;
        }
    }
    else {

        # Process text formats
        my $is_text = 0;
        for ( my $i = @placeholders - 1 ; $i >= 0 ; $i-- ) {
            my $placeholder = $placeholders[$i];
            if ( $placeholder->[0] eq '@' ) {
                substr( $result, $placeholder->[1], $placeholder->[2],
                    $number );
                $is_text++;
            }
            else {
                substr( $result, $placeholder->[1], $placeholder->[2], '' );
            }
        }

        $result = $number unless $is_text;

    }    # End of placeholder substitutions.

    # Trim the leading and trailing whitespace from the results.
    $result =~ s/^\s+//;
    $result =~ s/\s+$//;

    # Fix for negative currency.
    $result =~ s/^\$\-/\-\$/;
    $result =~ s/^\$ \-/\-\$ /;

    # Return color and locale strings if required.
    if ($want_subformats) {
        return ( $result, $color, $locale );
    }
    else {
        return $result;
    }
}

#------------------------------------------------------------------------------
# AddComma (for Spreadsheet::ParseExcel::Utility)
#------------------------------------------------------------------------------
sub AddComma {
    my ($sNum) = @_;

    if ( $sNum =~ /^([^\d]*)(\d\d\d\d+)(\.*.*)$/ ) {
        my ( $sPre, $sObj, $sAft ) = ( $1, $2, $3 );
        for ( my $i = length($sObj) - 3 ; $i > 0 ; $i -= 3 ) {
            substr( $sObj, $i, 0, ',' );
        }
        return $sPre . $sObj . $sAft;
    }
    else {
        return $sNum;
    }
}

#------------------------------------------------------------------------------
# MakeFraction (for Spreadsheet::ParseExcel::Utility)
#------------------------------------------------------------------------------
sub MakeFraction {
    my ( $sFmt, $iData, $iFlg ) = @_;
    my $iBunbo;
    my $iShou;

    #1. Init
    # print "FLG: $iFlg\n";
    if ($iFlg) {
        $iShou = $iData - int($iData);
        return '' if ( $iShou == 0 );
    }
    else {
        $iShou = $iData;
    }
    $iShou = abs($iShou);
    my $sSWk;

    #2.Calc BUNBO
    #2.1 BUNBO defined
    if ( $sFmt =~ /\/(\d+)$/ ) {
        $iBunbo = $1;
        return sprintf( "%d/%d", $iShou * $iBunbo, $iBunbo );
    }
    else {

        #2.2 Calc BUNBO
        $sFmt =~ /\/(\?+)$/;
        my $iKeta = length($1);
        my $iSWk  = 1;
        my $sSWk  = '';
        my $iBunsi;
        for ( my $iBunbo = 2 ; $iBunbo < 10**$iKeta ; $iBunbo++ ) {
            $iBunsi = int( $iShou * $iBunbo + 0.5 );
            my $iCmp = abs( $iShou - ( $iBunsi / $iBunbo ) );
            if ( $iCmp < $iSWk ) {
                $iSWk = $iCmp;
                $sSWk = sprintf( "%d/%d", $iBunsi, $iBunbo );
                last if ( $iSWk == 0 );
            }
        }
        return $sSWk;
    }
}

#------------------------------------------------------------------------------
# MakeE (for Spreadsheet::ParseExcel::Utility)
#------------------------------------------------------------------------------
sub MakeE {
    my ( $sFmt, $iData ) = @_;

    $sFmt =~ /(([#0]*)[\.]?[#0]*)([eE])([\+\-][0#]+)/;
    my ( $sKari, $iKeta, $sE, $sSisu ) = ( $1, length($2), $3, $4 );
    $iKeta = 1 if ( $iKeta <= 0 );

    my $iLog10 = 0;
    $iLog10 = ( $iData == 0 ) ? 0 : ( log( abs($iData) ) / log(10) );
    $iLog10 = (
        int( $iLog10 / $iKeta ) +
          ( ( ( $iLog10 - int( $iLog10 / $iKeta ) ) < 0 ) ? -1 : 0 ) ) * $iKeta;

    my $sUe = ExcelFmt( $sKari, $iData * ( 10**( $iLog10 * -1 ) ), 0 );
    my $sShita = ExcelFmt( $sSisu, $iLog10, 0 );
    return $sUe . $sE . $sShita;
}

#------------------------------------------------------------------------------
# LeapYear (for Spreadsheet::ParseExcel::Utility)
#------------------------------------------------------------------------------
sub LeapYear {
    my ($iYear) = @_;
    return 1 if ( $iYear == 1900 );    #Special for Excel
    return ( ( ( $iYear % 4 ) == 0 )
          && ( ( $iYear % 100 ) || ( $iYear % 400 ) == 0 ) )
      ? 1
      : 0;
}

#------------------------------------------------------------------------------
# LocaltimeExcel (for Spreadsheet::ParseExcel::Utility)
#------------------------------------------------------------------------------
sub LocaltimeExcel {
    my ( $iSec, $iMin, $iHour, $iDay, $iMon, $iYear, $iwDay, $iMSec, $flg1904 )
      = @_;

    #0. Init
    $iMon++;
    $iYear += 1900;

    #1. Calc Time
    my $iTime;
    $iTime = $iHour;
    $iTime *= 60;
    $iTime += $iMin;
    $iTime *= 60;
    $iTime += $iSec;
    $iTime += $iMSec / 1000.0 if ( defined($iMSec) );
    $iTime /= 86400.0;    #3600*24(1day in seconds)
    my $iY;
    my $iYDays;

    #2. Calc Days
    if ($flg1904) {
        $iY = 1904;
        $iTime--;         #Start from Jan 1st
        $iYDays = 366;
    }
    else {
        $iY     = 1900;
        $iYDays = 366;    #In Excel 1900 is leap year (That's not TRUE!)
    }
    while ( $iY < $iYear ) {
        $iTime += $iYDays;
        $iY++;
        $iYDays = ( LeapYear($iY) ) ? 366 : 365;
    }
    for ( my $iM = 1 ; $iM < $iMon ; $iM++ ) {
        if (   $iM == 1
            || $iM == 3
            || $iM == 5
            || $iM == 7
            || $iM == 8
            || $iM == 10
            || $iM == 12 )
        {
            $iTime += 31;
        }
        elsif ( $iM == 4 || $iM == 6 || $iM == 9 || $iM == 11 ) {
            $iTime += 30;
        }
        elsif ( $iM == 2 ) {
            $iTime += ( LeapYear($iYear) ) ? 29 : 28;
        }
    }
    $iTime += $iDay;
    return $iTime;
}

my @month_days = qw(
  0 31 28 31 30 31 30 31 31 30 31 30 31
);

#------------------------------------------------------------------------------
# ExcelLocaltime (for Spreadsheet::ParseExcel::Utility)
#------------------------------------------------------------------------------
sub ExcelLocaltime {

    my ( $dObj, $flg1904 ) = @_;
    my ( $iSec, $iMin, $iHour, $iDay, $iMon, $iYear, $iwDay, $iMSec );
    my ( $iDt, $iTime, $iYDays, $iMD );

    $iDt   = int($dObj);
    $iTime = $dObj - $iDt;

    #1. Calc Days
    if ($flg1904) {
        $iYear = 1904;
        $iDt++;    #Start from Jan 1st
        $iYDays = 366;
        $iwDay = ( ( $iDt + 4 ) % 7 );
    }
    else {
        $iYear  = 1900;
        $iYDays = 366;    #In Excel 1900 is leap year (That's not TRUE!)
        $iwDay = ( ( $iDt + 6 ) % 7 );
    }
    while ( $iDt > $iYDays ) {
        $iDt -= $iYDays;
        $iYear++;
        $iYDays =
          (      ( ( $iYear % 4 ) == 0 )
              && ( ( $iYear % 100 ) || ( $iYear % 400 ) == 0 ) ) ? 366 : 365;
    }
    $iYear -= 1900;       # Localtime year is relative to 1900.

    for ( $iMon = 1 ; $iMon <= 12 ; $iMon++ ) {
        $iMD = $month_days[$iMon];
        $iMD++ if $iMon == 2 and $iYear % 4 == 0;

        last if ( $iDt <= $iMD );
        $iDt -= $iMD;
    }

    #2. Calc Time
    $iDay = $iDt;
    $iTime += ( 0.0005 / 86400.0 );
    if ($iTime >= 1.0)
    {
        $iTime -= int($iTime);
        $iwDay = ($iwDay == 6) ? 0 : $iwDay + 1;
        if ($iDay == $iMD)
        {
            if ($iMon == 12)
            {
                $iMon = 1;
                $iYear++;
            }
            else
            {
                $iMon++;
            }
            $iDay = 1;
        }
        else
        {
            $iDay++;
        }
    }

    # Localtime month is 0 based.
    $iMon  -= 1;
    $iTime *= 24.0;
    $iHour = int($iTime);
    $iTime -= $iHour;
    $iTime *= 60.0;
    $iMin = int($iTime);
    $iTime -= $iMin;
    $iTime *= 60.0;
    $iSec = int($iTime);
    $iTime -= $iSec;
    $iTime *= 1000.0;
    $iMSec = int($iTime);

    return ( $iSec, $iMin, $iHour, $iDay, $iMon, $iYear, $iwDay, $iMSec );
}

# -----------------------------------------------------------------------------
# col2int (for Spreadsheet::ParseExcel::Utility)
#------------------------------------------------------------------------------
# converts a excel row letter into an int for use in an array
sub col2int {
    my $result = 0;
    my $str    = shift;
    my $incr   = 0;

    for ( my $i = length($str) ; $i > 0 ; $i-- ) {
        my $char = substr( $str, $i - 1 );
        my $curr += ord( lc($char) ) - ord('a') + 1;
        $curr *= $incr if ($incr);
        $result += $curr;
        $incr   += 26;
    }

    # this is one out as we range 0..x-1 not 1..x
    $result--;

    return $result;
}

# -----------------------------------------------------------------------------
# int2col (for Spreadsheet::ParseExcel::Utility)
#------------------------------------------------------------------------------
### int2col
# convert a column number into column letters
# @note this is quite a brute force coarse method
#   does not manage values over 701 (ZZ)
# @arg number, to convert
# @returns string, column name
#
sub int2col {
    my $out = "";
    my $val = shift;

    do {
        $out .= chr( ( $val % 26 ) + ord('A') );
        $val = int( $val / 26 ) - 1;
    } while ( $val >= 0 );

    return scalar reverse $out;
}

# -----------------------------------------------------------------------------
# sheetRef (for Spreadsheet::ParseExcel::Utility)
#------------------------------------------------------------------------------
# -----------------------------------------------------------------------------
### sheetRef
# convert an excel letter-number address into a useful array address
# @note that also Excel uses X-Y notation, we normally use Y-X in arrays
# @args $str, excel coord eg. A2
# @returns an array - 2 elements - column, row, or undefined
#
sub sheetRef {
    my $str = shift;
    my @ret;

    $str =~ m/^(\D+)(\d+)$/;

    if ( $1 && $2 ) {
        push( @ret, $2 - 1, col2int($1) );
    }
    if ( $ret[0] < 0 ) {
        undef @ret;
    }

    return @ret;
}

# -----------------------------------------------------------------------------
# xls2csv (for Spreadsheet::ParseExcel::Utility)
#------------------------------------------------------------------------------
### xls2csv
# convert a chunk of an excel file into csv text chunk
# @args $param, sheet-colrow:colrow (1-A1:B2 or A1:B2 for sheet 1
# @args $rotate, 0 or 1 decides if output should be rotated or not
# @returns string containing a chunk of csv
#
sub xls2csv {
    my ( $filename, $regions, $rotate ) = @_;
    my $sheet = 0;

    # We need Text::CSV_XS for proper CSV handling.
    require Text::CSV_XS;

    # extract any sheet number from the region string
    $regions =~ m/^(\d+)-(.*)/;

    if ($2) {
        $sheet   = $1 - 1;
        $regions = $2;
    }

    # now extract the start and end regions
    $regions =~ m/(.*):(.*)/;

    if ( !$1 || !$2 ) {
        print STDERR "Bad Params";
        return "";
    }

    my @start = sheetRef($1);
    my @end   = sheetRef($2);
    if ( !@start ) {
        print STDERR "Bad coorinates - $1";
        return "";
    }
    if ( !@end ) {
        print STDERR "Bad coorinates - $2";
        return "";
    }

    if ( $start[1] > $end[1] ) {
        print STDERR "Bad COLUMN ordering\n";
        print STDERR "Start column " . int2col( $start[1] );
        print STDERR " after end column " . int2col( $end[1] ) . "\n";
        return "";
    }
    if ( $start[0] > $end[0] ) {
        print STDERR "Bad ROW ordering\n";
        print STDERR "Start row " . ( $start[0] + 1 );
        print STDERR " after end row " . ( $end[0] + 1 ) . "\n";
        exit;
    }

    # start the excel object now
    my $oExcel = new Spreadsheet::ParseExcel;
    my $oBook  = $oExcel->Parse($filename);

    # open the sheet
    my $oWkS = $oBook->{Worksheet}[$sheet];

    # now check that the region exists in the file
    # if not truncate to the possible region
    # output a warning msg
    if ( $start[1] < $oWkS->{MinCol} ) {
        print STDERR int2col( $start[1] )
          . " < min col "
          . int2col( $oWkS->{MinCol} )
          . " Resetting\n";
        $start[1] = $oWkS->{MinCol};
    }
    if ( $end[1] > $oWkS->{MaxCol} ) {
        print STDERR int2col( $end[1] )
          . " > max col "
          . int2col( $oWkS->{MaxCol} )
          . " Resetting\n";
        $end[1] = $oWkS->{MaxCol};
    }
    if ( $start[0] < $oWkS->{MinRow} ) {
        print STDERR ""
          . ( $start[0] + 1 )
          . " < min row "
          . ( $oWkS->{MinRow} + 1 )
          . " Resetting\n";
        $start[0] = $oWkS->{MinCol};
    }
    if ( $end[0] > $oWkS->{MaxRow} ) {
        print STDERR ""
          . ( $end[0] + 1 )
          . " > max row "
          . ( $oWkS->{MaxRow} + 1 )
          . " Resetting\n";
        $end[0] = $oWkS->{MaxRow};

    }

    my $x1 = $start[1];
    my $y1 = $start[0];
    my $x2 = $end[1];
    my $y2 = $end[0];

    my @cell_data;
    my $row = 0;

    if ( !$rotate ) {
        for ( my $y = $y1 ; $y <= $y2 ; $y++ ) {
            for ( my $x = $x1 ; $x <= $x2 ; $x++ ) {
                my $cell = $oWkS->{Cells}[$y][$x];

                my $value;
                if ( defined $cell ) {
                    $value .= $cell->value();
                }
                else {
                    $value = '';
                }

                push @{ $cell_data[$row] }, $value;
            }
            $row++;
        }
    }
    else {
        for ( my $x = $x1 ; $x <= $x2 ; $x++ ) {
            for ( my $y = $y1 ; $y <= $y2 ; $y++ ) {
                my $cell = $oWkS->{Cells}[$y][$x];

                my $value;
                if ( defined $cell ) {
                    $value .= $cell->value();
                }
                else {
                    $value = '';
                }

                push @{ $cell_data[$row] }, $value;
            }
            $row++;
        }
    }

    # Create the CSV output string.
    my $csv = Text::CSV_XS->new( { binary => 1, eol => $/ } );
    my $output = "";

    for my $row (@cell_data) {
        $csv->combine(@$row);
        $output .= $csv->string();
    }

    return $output;
}

1;

__END__

=pod

=head1 NAME

Spreadsheet::ParseExcel::Utility - Utility functions for Spreadsheet::ParseExcel.

=head1 SYNOPSIS

    use Spreadsheet::ParseExcel::Utility qw(ExcelFmt ExcelLocaltime LocaltimeExcel);

    # Convert localtime to Excel time
    my $datetime = LocaltimeExcel(11, 10, 12, 23, 2, 64); # 1964-3-23 12:10:11

    print $datetime, "\n"; # 23459.5070717593 (Excel date/time format)

    # Convert Excel Time to localtime
    my @time = ExcelLocaltime($datetime);
    print join(":", @time), "\n";   # 11:10:12:23:2:64:1:0

    # Formatting
    print ExcelFmt('yyyy-mm-dd', $datetime), "\n"; # 1964-3-23
    print ExcelFmt('m-d-yy',     $datetime), "\n"; # 3-23-64
    print ExcelFmt('#,##0',      $datetime), "\n"; # 23,460
    print ExcelFmt('#,##0.00',   $datetime), "\n"; # 23,459.51

=head1 DESCRIPTION

The C<Spreadsheet::ParseExcel::Utility> module provides utility functions for working with ParseExcel and Excel data.

=head1 Functions

C<Spreadsheet::ParseExcel::Utility> can export the following functions:

    ExcelFmt
    ExcelLocaltime
    LocaltimeExcel
    col2int
    int2col
    sheetRef
    xls2csv

These functions must be imported implicitly:

    # Just one function.
    use Spreadsheet::ParseExcel::Utility 'col2int';

    # More than one.
    use Spreadsheet::ParseExcel::Utility qw(ExcelFmt ExcelLocaltime LocaltimeExcel);


=head2 ExcelFmt($format_string, $number, $is_1904)

Excel stores data such as dates and currency values as numbers. The way these numbers are displayed is controlled by the number format string for the cell. For example a cell with a number format of C<'$#,##0.00'> for currency and a value of 1234.567 would be displayed as follows:

    '$#,##0.00' + 1234.567 = '$1,234.57'.

The C<ExcelFmt()> function tries to emulate this formatting so that the user can convert raw numbers returned by C<Spreadsheet::ParseExel> to a desired format. For example:

    print ExcelFmt('$#,##0.00', 1234.567); # $1,234.57.

The syntax of the function is:

    my $text = ExcelFmt($format_string, $number, $is_1904);

Where C<$format_string> is an Excel number format string, C<$number> is a real or integer number and C<is_1904> is an optional flag to indicate that dates should use Excel's 1904 epoch instead of the default 1900 epoch.

C<ExcelFmt()> is also used internally to convert numbers returned by the C<Cell::unformatted()> method to the formatted value returned by the C<Cell::value()> method:


    my $cell = $worksheet->get_cell( 0, 0 );

    print $cell->unformatted(), "\n"; # 1234.567
    print $cell->value(),       "\n"; # $1,234.57

The most common usage for C<ExcelFmt> is to convert numbers to dates. Dates and times in Excel are represented by real numbers, for example "1 Jan 2001 12:30 PM" is represented by the number 36892.521. The integer part of the number stores the number of days since the epoch and the fractional part stores the percentage of the day. By applying an Excel number format the number is converted to the desired string representation:

    print ExcelFmt('d mmm yyyy h:mm AM/PM', 36892.521);  # 1 Jan 2001 12:30 PM

C<$is_1904> is an optional flag to indicate that dates should use Excel's 1904 epoch instead of the default 1900 epoch. Excel for Windows generally uses 1900 and Excel for Mac OS uses 1904. The C<$is1904> flag isn't required very often by a casual user and can usually be ignored.


=head2 ExcelLocaltime($excel_datetime, $is_1904)

The C<ExcelLocaltime()> function converts from an Excel date/time number to a C<localtime()>-like array of values:

        my @time = ExcelLocaltime($excel_datetime);

        #    0     1     2      3     4       5      6      7
        my ( $sec, $min, $hour, $day, $month, $year, $wday, $msec ) = @time;

The array elements from C<(0 .. 6)> are the same as Perl's C<localtime()>. The last element C<$msec> is milliseconds. In particular it should be noted that, in common with C<localtime()>, the month is zero indexed and the year is the number of years since 1900. This means that you will usually need to do the following:

        $month++;
        $year += 1900;

See also Perl's documentation for L<localtime()|perlfunc>:

The C<$is_1904> flag is an optional. It is used to indicate that dates should use Excel's 1904 epoch instead of the default 1900 epoch.

=head2 LocaltimeExcel($sec, $min, $hour, $day, $month, $year, $wday, $msec, $is_1904)

The C<LocaltimeExcel()> function converts from a C<localtime()>-like array of values to an Excel date/time number:

    $excel_datetime = LocaltimeExcel($sec, $min, $hour, $day, $month, $year, $wday, $msec);

The array elements from C<(0 .. 6)> are the same as Perl's C<localtime()>. The last element C<$msec> is milliseconds. In particular it should be noted that, in common with C<localtime()>, the month is zero indexed and the year is the number of years since 1900. See also Perl's documentation for L<localtime()|perlfunc>:

The C<$wday> and C<$msec> elements are usually optional. This time elements can also be zeroed if they aren't of interest:

                                    # sec, min, hour, day, month, year
    $excel_datetime = LocaltimeExcel( 0,   0,   0,    1,   0,     101 );

    print ExcelFmt('d mmm yyyy', $excel_datetime);  # 1 Jan 2001

The C<$is_1904> flag is also optional. It is used to indicate that dates should use Excel's 1904 epoch instead of the default 1900 epoch.


=head2 col2int($column)

The C<col2int()> function converts an Excel column letter to an zero-indexed column number:

    print col2int('A');  # 0
    print col2int('AA'); # 26

This function was contributed by Kevin Mulholland.


=head2 int2col($column_number)

The C<int2col()> function converts an zero-indexed Excel column number to a column letter:

    print int2col(0);  # 'A'
    print int2col(26); # 'AA'

This function was contributed by Kevin Mulholland.


=head2 sheetRef($cell_string)

The C<sheetRef()> function converts an Excel cell reference in 'A1' notation to a zero-indexed C<(row, col)> pair.

    my ($row, $col) = sheetRef('A1'); # ( 0, 0 )
    my ($row, $col) = sheetRef('C2'); # ( 1, 2 )

This function was contributed by Kevin Mulholland.


=head2 xls2csv($filename, $region, $rotate)

The C<xls2csv()> function converts a section of an Excel file into a CSV text string.

    $csv_text = xls2csv($filename, $region, $rotate);

Where:

    $region = "sheet-colrow:colrow"
    For example '1-A1:B2' means 'A1:B2' for sheet 1.

    and

    $rotate  = 0 or 1 (output is rotated/transposed or not)

This function requires C<Text::CSV_XS> to be installed. It was contributed by Kevin Mulholland along with the C<xls2csv> script in the C<sample> directory of the distro.

See also the following xls2csv utilities: Ken Prows' C<xls2csv>: http://search.cpan.org/~ken/xls2csv/script/xls2csv and H.Merijn Brand's C<xls2csv> (which is part of Spreadsheet::Read): http://search.cpan.org/~hmbrand/Spreadsheet-Read/


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
