package SQL::Statement::Functions;

######################################################################
#
# This module is copyright (c), 2001,2005 by Jeff Zucker.
# This module is copyright (c), 2011,2012 by Brendan Byrd.
# This module is copyright (c), 2009-2017 by Jens Rehsack.
# All rights reserved.
#
# It may be freely distributed under the same terms as Perl itself.
# See below for help and copyright information (search for SYNOPSIS).
#
######################################################################

use strict;
use warnings FATAL => "all";
# no warnings 'uninitialized';  # please don't bother me with these useless warnings...

use Carp qw(croak);
use Params::Util qw(_ARRAY0 _HASH0 _INSTANCE);
use Scalar::Util qw(looks_like_number);
use List::Util qw(max);      # core module since Perl 5.8.0
use Time::HiRes qw(time);    # core module since Perl 5.7.2
use Encode;                  # core module since Perl 5.7.1
use Math::Trig;              # core module since Perl 5.004
use Module::Runtime qw(require_module use_module);

=pod

=head1 NAME

SQL::Statement::Functions - built-in & user-defined SQL functions

=head1 SYNOPSIS

 SELECT Func(args);
 SELECT * FROM Func(args);
 SELECT * FROM x WHERE Funcs(args);
 SELECT * FROM x WHERE y < Funcs(args);

=head1 DESCRIPTION

This module contains the built-in functions for L<SQL::Parser> and L<SQL::Statement>.  All of the functions are also available in any DBDs that subclass those modules (e.g. DBD::CSV, DBD::DBM, DBD::File, DBD::AnyData, DBD::Excel, etc.).

This documentation covers built-in functions and also explains how to create your own functions to supplement the built-in ones.  It's easy.  If you create one that is generally useful, see below for how to submit it to become a built-in function.

=head1 Function syntax

When using L<SQL::Statement>/L<SQL::Parser> directly to parse SQL, functions (either built-in or user-defined) may occur anywhere in a SQL statement that values, column names, table names, or predicates may occur.  When using the modules through a DBD or in any other context in which the SQL is both parsed and executed, functions can occur in the same places except that they can not occur in the column selection clause of a SELECT statement that contains a FROM clause.

 # valid for both parsing and executing

     SELECT MyFunc(args);
     SELECT * FROM MyFunc(args);
     SELECT * FROM x WHERE MyFuncs(args);
     SELECT * FROM x WHERE y < MyFuncs(args);

 # valid only for parsing (won't work from a DBD)

     SELECT MyFunc(args) FROM x WHERE y;

=head1 User-Defined Functions

=head2 Loading User-Defined Functions

In addition to the built-in functions, you can create any number of your own user-defined functions (UDFs).  In order to use a UDF in a script, you first have to create a perl subroutine (see below), then you need to make the function available to your database handle with the CREATE FUNCTION or LOAD commands:

 # load a single function "foo" from a subroutine
 # named "foo" in the current package

      $dbh->do(" CREATE FUNCTION foo EXTERNAL ");

 # load a single function "foo" from a subroutine
 # named "bar" in the current package

      $dbh->do(" CREATE FUNCTION foo EXTERNAL NAME bar");


 # load a single function "foo" from a subroutine named "foo"
 # in another package

      $dbh->do(' CREATE FUNCTION foo EXTERNAL NAME "Bar::Baz::foo" ');

 # load all the functions in another package

      $dbh->do(' LOAD "Bar::Baz" ');

Functions themselves should follow SQL identifier naming rules.  Subroutines loaded with CREATE FUNCTION can have any valid perl subroutine name.  Subroutines loaded with LOAD must start with SQL_FUNCTION_ and then the actual function name.  For example:

 package Qux::Quimble;
 sub SQL_FUNCTION_FOO { ... }
 sub SQL_FUNCTION_BAR { ... }
 sub some_other_perl_subroutine_not_a_function { ... }
 1;

 # in another package
 $dbh->do("LOAD Qux::Quimble");

 # This loads FOO and BAR as SQL functions.

=head2 Creating User-Defined Functions

User-defined functions (UDFs) are perl subroutines that return values appropriate to the context of the function in a SQL statement.  For example the built-in CURRENT_TIME returns a string value and therefore may be used anywhere in a SQL statement that a string value can.  Here' the entire perl code for the function:

 # CURRENT_TIME
 #
 # arguments : none
 # returns   : string containing current time as hh::mm::ss
 #
 sub SQL_FUNCTION_CURRENT_TIME {
     sprintf "%02s::%02s::%02s",(localtime)[2,1,0]
 }

More complex functions can make use of a number of arguments always passed to functions automatically.  Functions always receive these values in @_:

 sub FOO {
     my($self,$sth,@params);
 }

The first argument, $self, is whatever class the function is defined in, not generally useful unless you have an entire module to support the function.

The second argument, $sth is the active statement handle of the current statement.  Like all active statement handles it contains the current database handle in the {Database} attribute so you can have access to the database handle in any function:

 sub FOO {
     my($self,$sth,@params);
     my $dbh = $sth->{Database};
     # $dbh->do( ...), etc.
 }

In actual practice you probably want to use $sth->{Database} directly rather than making a local copy, so $sth->{Database}->do(...).

The remaining arguments, @params, are arguments passed by users to the function, either directly or with placeholders; another silly example which just returns the results of multiplying the arguments passed to it:

 sub MULTIPLY {
     my($self,$sth,@params);
     return $params[0] * $params[1];
 }

 # first make the function available
 #
 $dbh->do("CREATE FUNCTION MULTIPLY");

 # then multiply col3 in each row times seven
 #
 my $sth=$dbh->prepare("SELECT col1 FROM tbl1 WHERE col2 = MULTIPLY(col3,7)");
 $sth->execute;
 #
 # or
 #
 my $sth=$dbh->prepare("SELECT col1 FROM tbl1 WHERE col2 = MULTIPLY(col3,?)");
 $sth->execute(7);

=head2 Creating In-Memory Tables with functions

A function can return almost anything, as long is it is an appropriate return for the context the function will be used in.  In the special case of table-returning functions, the function should return a reference to an array of array references with the first row being the column names and the remaining rows the data.  For example:

B<1. create a function that returns an AoA>,

  sub Japh {[
      [qw( id word   )],
      [qw( 1 Hacker  )],
      [qw( 2 Perl    )],
      [qw( 3 Another )],
      [qw( 4 Just    )],
  ]}

B<2. make your database handle aware of the function>

  $dbh->do("CREATE FUNCTION 'Japh');

B<3. Access the data in the AoA from SQL>

  $sth = $dbh->prepare("SELECT word FROM Japh ORDER BY id DESC");

Or here's an example that does a join on two in-memory tables:

  sub Prof  {[ [qw(pid pname)],[qw(1 Sue )],[qw(2 Bob)],[qw(3 Tom )] ]}
  sub Class {[ [qw(pid cname)],[qw(1 Chem)],[qw(2 Bio)],[qw(2 Math)] ]}
  $dbh->do("CREATE FUNCTION $_) for qw(Prof Class);
  $sth = $dbh->prepare("SELECT * FROM Prof NATURAL JOIN Class");

The "Prof" and "Class" functions return tables which can be used like any SQL table.

More complex functions might do something like scrape an RSS feed, or search a file system and put the results in AoA.  For example, to search a directory with SQL:

 sub Dir {
     my($self,$sth,$dir)=@_;
     opendir D, $dir or die "'$dir':$!";
     my @files = readdir D;
     my $data = [[qw(fileName fileExt)]];
     for (@files) {
         my($fn,$ext) = /^(.*)(\.[^\.]+)$/;
         push @$data, [$fn,$ext];
     }
     return $data;
 }
 $dbh->do("CREATE FUNCTION Dir");
 printf "%s\n", join'   ',@{ $dbh->selectcol_arrayref("
     SELECT fileName FROM Dir('./') WHERE fileExt = '.pl'
 ")};

Obviously, that function could be expanded with File::Find and/or stat to provide more information and it could be made to accept a list of directories rather than a single directory.

Table-Returning functions are a way to turn *anything* that can be modeled as an AoA into a DBI data source.

=head1 Built-in Functions

=head2 SQL-92/ODBC Compatibility

All ODBC 3.0 functions are available except for the following: 

 ### SQL-92 / ODBC Functions
 
 # CONVERT / CAST - Complex to implement, but a draft is in the works.
 # DIFFERENCE     - Function is not clearly defined in spec and has very limited applications
 # EXTRACT        - Contains a FROM keyword and requires rather freeform datetime/interval expression
 
 ### ODBC 3.0 Time/Date Functions only
 
 # DAYOFMONTH, DAYOFWEEK, DAYOFYEAR, HOUR, MINUTE, MONTH, MONTHNAME, QUARTER, SECOND, TIMESTAMPDIFF, 
 #    WEEK, YEAR - Requires freeform datetime/interval expressions.  In a later release, these could
 #                    be implemented with the help of Date::Parse.

ODBC 3.0 functions that are implemented with differences include:

 # SOUNDEX  - Returns true/false, instead of a SOUNDEX code
 # RAND     - Seed value is a second parameter with a new first parameter for max limit
 # LOG      - Returns base X (or 10) log of number, not natural log.  LN is used for natural log, and
 #               LOG10 is still available for standards compatibility.
 # POSITION - Does not use 'IN' keyword; cannot be fixed as previous versions of SQL::Statement defined
 #               the function as such.
 # REPLACE / SUBSTITUTE - Uses a regular expression string for the second parameter, replacing the last two
 #                           parameters of the typical ODBC function

=cut

use vars qw($VERSION);
$VERSION = '1.412';

=pod

=head2 Aggregate Functions

=head3 MIN, MAX, AVG, SUM, COUNT

Aggregate functions are handled elsewhere, see L<SQL::Parser> for documentation.

=pod

=head2 Date and Time Functions

These functions can be used without parentheses.

=head3 CURRENT_DATE aka CURDATE

 # purpose   : find current date
 # arguments : none
 # returns   : string containing current date as yyyy-mm-dd

=cut

sub SQL_FUNCTION_CURRENT_DATE
{
    my ( $sec, $min, $hour, $day, $mon, $year ) = localtime;
    return sprintf( '%4s-%02s-%02s', $year + 1900, $mon + 1, $day );
}
no warnings 'once';
*SQL_FUNCTION_CURDATE = \&SQL_FUNCTION_CURRENT_DATE;
use warnings 'all';

=pod

=head3 CURRENT_TIME aka CURTIME

 # purpose   : find current time
 # arguments : optional seconds precision
 # returns   : string containing current time as hh:mm:ss (or ss.sss...)

=cut

sub SQL_FUNCTION_CURRENT_TIME
{
    return substr( SQL_FUNCTION_CURRENT_TIMESTAMP( @_[ 0 .. 2 ] ), 11 );
}
no warnings 'once';
*SQL_FUNCTION_CURTIME = \&SQL_FUNCTION_CURRENT_TIME;
use warnings 'all';

=pod

=head3 CURRENT_TIMESTAMP aka NOW

 # purpose   : find current date and time
 # arguments : optional seconds precision
 # returns   : string containing current timestamp as yyyy-mm-dd hh:mm:ss (or ss.sss...)

=cut

sub SQL_FUNCTION_CURRENT_TIMESTAMP
{
    my $prec;

    my $curtime = time;
    my ( $sec, $min, $hour, $day, $mon, $year ) = localtime($curtime);

    my $sec_frac;
    if ( $_[2] )
    {
        $prec     = int( $_[2] );
        $sec_frac = sprintf( '%.*f', $prec, $curtime - int($curtime) );
        $sec_frac = substr( $sec_frac, 2 );                               # truncate 0. from decimal
    }

    return sprintf(
        '%4s-%02s-%02s %02s:%02s:%02s' . ( $prec ? '.%s' : '' ),
        $year + 1900,
        $mon + 1, $day, $hour, $min, $sec, ( $prec ? $sec_frac : ())
    );
}
no warnings 'once';
*SQL_FUNCTION_NOW = \&SQL_FUNCTION_CURRENT_TIMESTAMP;
use warnings 'all';

=pod

=head3 UNIX_TIMESTAMP

 # purpose   : find the current time in UNIX epoch format
 # arguments : optional seconds precision (unlike the MySQL version)
 # returns   : a (64-bit) number, possibly with decimals

=cut

sub SQL_FUNCTION_UNIX_TIMESTAMP { return sprintf( "%.*f", $_[2] ? int( $_[2] ) : 0, time ); }

=pod

=head2 String Functions

=head3 ASCII & CHAR

 # purpose   : same as ord and chr, respectively (NULL for any NULL args)
 # arguments : string or character (or number for CHAR); CHAR can have any amount of numbers for a string

=cut

sub SQL_FUNCTION_ASCII { return defined $_[2] ? ord( $_[2] ) : undef; }

sub SQL_FUNCTION_CHAR
{
    my ( $self, $owner, @params ) = @_;
    ( defined || return undef ) for (@params);
    return join '', map { chr } @params;
}

=pod

=head3 BIT_LENGTH

 # purpose   : length of the string in bits
 # arguments : string

=cut

sub SQL_FUNCTION_BIT_LENGTH
{
    my @v   = @_[ 0 .. 1 ];
    my $str = $_[2];
    # Number of bits on first character = INT(LOG2(ord($str)) + 1) + rest of string = OCTET_LENGTH(substr($str, 1)) * 8
    return int( SQL_FUNCTION_LOG( @v, 2, ord($str) ) + 1 ) + SQL_FUNCTION_OCTET_LENGTH( @v, substr( $str, 1 ) ) * 8;
}

=pod

=head3 CHARACTER_LENGTH aka CHAR_LENGTH

 # purpose   : find length in characters of a string
 # arguments : a string
 # returns   : a number - the length of the string in characters

=cut

sub SQL_FUNCTION_CHAR_LENGTH
{
    my ( $self, $owner, $str ) = @_;
    return length($str);
}
no warnings 'once';
*SQL_FUNCTION_CHARACTER_LENGTH = \&SQL_FUNCTION_CHAR_LENGTH;
use warnings 'all';

=pod
 
=head3 COALESCE aka NVL aka IFNULL
 
 # purpose   : return the first non-NULL value from a list
 # arguments : 1 or more expressions
 # returns   : the first expression (reading left to right)
 #             which is not NULL; returns NULL if all are NULL
 #
 
=cut

sub SQL_FUNCTION_COALESCE
{
    my ( $self, $owner, @params ) = @_;

    #
    #	eval each expr in list until a non-null
    #	is encountered, then return it
    #
    foreach (@params)
    {
        return $_
          if defined($_);
    }
    return undef;
}
no warnings 'once';
*SQL_FUNCTION_NVL    = \&SQL_FUNCTION_COALESCE;
*SQL_FUNCTION_IFNULL = \&SQL_FUNCTION_COALESCE;
use warnings 'all';

=pod

=head3 CONCAT

 # purpose   : concatenate 1 or more strings into a single string;
 #			an alternative to the '||' operator
 # arguments : 1 or more strings
 # returns   : the concatenated string
 #
 # example   : SELECT CONCAT(first_string, 'this string', ' that string')
 #              returns "<value-of-first-string>this string that string"
 # note      : if any argument evaluates to NULL, the returned value is NULL

=cut

sub SQL_FUNCTION_CONCAT
{
    my ( $self, $owner, @params ) = @_;
    ( defined || return undef ) for (@params);
    return join '', @params;
}

=pod

=head3 CONV

 # purpose   : convert a number X from base Y to base Z (from base 2 to 64)
 # arguments : X (can by a number or string depending on the base), Y, Z (Z defaults to 10)
               Valid bases for Y and Z are: 2, 8, 10, 16 and 64
 # returns   : either a string or number, in base Z
 # notes     : Behavioral table
 #
 #      base | valuation
 #     ------+-----------
 #         2 | binary, base 2 - (0,1)
 #         8 | octal, base 8 - (0..7)
 #        10 | decimal, base 10 - (0..9)
 #        16 | hexadecimal, base 16 - (0..9,a..f)
 #        64 | 0-63 from MIME::Base64
 #

=cut

sub SQL_FUNCTION_CONV
{
    my ( $self, $owner, $num, $sbase, $ebase ) = @_;

    $ebase ||= 10;
    $_ and $_ != 2 and $_ != 8 and $_ != 10 and $_ != 16 and $_ != 64 and croak("Invalid base: $_") for ($sbase, $ebase);

    scalar use_module("Math::Base::Convert")->new($sbase, $ebase)->cnv($num);
}

=pod

=head3 DECODE

 # purpose   : compare the first argument against
 #             succeeding arguments at position 1 + 2N
 #             (N = 0 to (# of arguments - 2)/2), and if equal,
 #				return the value of the argument at 1 + 2N + 1; if no
 #             arguments are equal, the last argument value is returned
 # arguments : 4 or more expressions, must be even # of arguments
 # returns   : the value of the argument at 1 + 2N + 1 if argument 1 + 2N
 #             is equal to argument1; else the last argument value
 #
 # example   : SELECT DECODE(some_column,
 #                    'first value', 'first value matched'
 #                    '2nd value', '2nd value matched'
 #                    'no value matched'
 #                    )

=cut

#
#	emulate Oracle DECODE; behaves same as
#	CASE expr WHEN <expr2> THEN expr3
#	WHEN expr4 THEN expr5
#	...
#	ELSE exprN END
#
sub SQL_FUNCTION_DECODE
{
    my ( $self, $owner, @params ) = @_;

    #
    #	check param list size, must be at least 4,
    #	and even in length
    #
    no warnings 'precedence';
    die 'Invalid DECODE argument list!' unless ( ( scalar @params > 3 ) && ( $#params & 1 == 1 ) );

    #
    #	eval first argument, and last argument,
    #	then eval and compare each succeeding pair of args
    #	be careful about NULLs!
    #
    my $lhs     = shift @params;
    my $default = pop @params;
    return $default unless defined($lhs);
    my $lhs_isnum = looks_like_number($lhs);

    while (@params)
    {
        my $rhs = shift @params;
        shift @params, next
          unless defined($rhs);
        return shift @params
          if ( ( looks_like_number($rhs) && $lhs_isnum && ( $lhs == $rhs ) )
            || ( $lhs eq $rhs ) );
        shift @params;
    }
    return $default;
}

=pod

=head3 INSERT

 # purpose   : string where L characters have been deleted from STR1, beginning at S,
 #             and where STR2 has been inserted into STR1, beginning at S.  NULL for any NULL args.
 # arguments : STR1, S, L, STR2

=cut

sub SQL_FUNCTION_INSERT
{    # just like a 4-parameter substr in Perl
    ( defined || return undef ) for ( @_[ 2 .. 5 ] );
    my $str = $_[2];
    no warnings 'void';
    substr( $str, $_[3] - 1, $_[4], $_[5] );
    return $str;
}

=pod

=head3 HEX & OCT & BIN

 # purpose   : convert number X from decimal to hex/octal/binary; equiv. to CONV(X, 10, 16/8/2)
 # arguments : X

=cut

sub SQL_FUNCTION_HEX { return shift->SQL_FUNCTION_CONV( @_[ 0 .. 1 ], 10, 16 ); }
sub SQL_FUNCTION_OCT { return shift->SQL_FUNCTION_CONV( @_[ 0 .. 1 ], 10, 8 ); }
sub SQL_FUNCTION_BIN { return shift->SQL_FUNCTION_CONV( @_[ 0 .. 1 ], 10, 2 ); }

=pod

=head3 LEFT & RIGHT

 # purpose   : leftmost or rightmost L characters in STR, or NULL for any NULL args
 # arguments : STR1, L

=cut

sub SQL_FUNCTION_LEFT
{
    ( defined || return undef ) for ( @_[ 2 .. 3 ] );
    return substr( $_[2], 0, $_[3] );
}

sub SQL_FUNCTION_RIGHT
{
    ( defined || return undef ) for ( @_[ 2 .. 3 ] );
    return substr( $_[2], -$_[3] );
}

=pod

=head3 LOCATE aka POSITION

 # purpose   : starting position (one-based) of the first occurrence of STR1
               within STR2; 0 if it doesn't occur and NULL for any NULL args
 # arguments : STR1, STR2, and an optional S (starting position to search)

=cut

sub SQL_FUNCTION_LOCATE
{
    ( defined || return undef ) for ( @_[ 2 .. 3 ] );
    my ( $self, $owner, $substr, $str, $s ) = @_;
    $s = int( $s || 0 );
    my $pos = index( substr( $str, $s ), $substr ) + 1;
    return $pos && $pos + $s;
}
no warnings 'once';
*SQL_FUNCTION_POSITION = \&SQL_FUNCTION_LOCATE;
use warnings 'all';

=pod

=head3 LOWER & UPPER aka LCASE & UCASE

 # purpose   : lower-case or upper-case a string
 # arguments : a string
 # returns   : the sting lower or upper cased

=cut

sub SQL_FUNCTION_LOWER
{
    my ( $self, $owner, $str ) = @_;
    return lc($str);
}

sub SQL_FUNCTION_UPPER
{
    my ( $self, $owner, $str ) = @_;
    return uc($str);
}

no warnings 'once';
*SQL_FUNCTION_UCASE = \&SQL_FUNCTION_UPPER;
*SQL_FUNCTION_LCASE = \&SQL_FUNCTION_LOWER;
use warnings 'all';

=pod

=head3 LTRIM & RTRIM

 # purpose   : left/right counterparts for TRIM
 # arguments : string

=cut

sub SQL_FUNCTION_LTRIM
{
    my $str = $_[2];
    $str =~ s/^\s+//;
    return $str;
}

sub SQL_FUNCTION_RTRIM
{
    my $str = $_[2];
    $str =~ s/\s+$//;
    return $str;
}

=pod

=head3 OCTET_LENGTH

 # purpose   : length of the string in bytes (not characters)
 # arguments : string

=cut

sub SQL_FUNCTION_OCTET_LENGTH { return length( Encode::encode_utf8( $_[2] ) ); }    # per Perldoc

=pod

=head3 REGEX

 # purpose   : test if a string matches a perl regular expression
 # arguments : a string and a regex to match the string against
 # returns   : boolean value of the regex match
 #
 # example   : ... WHERE REGEX(col3,'/^fun/i') ... matches rows
 #             in which col3 starts with "fun", ignoring case

=cut

sub SQL_FUNCTION_REGEX
{
    my ( $self, $owner, @params ) = @_;
    ( defined || return 0 ) for ( @params[ 0 .. 1 ] );
    my ( $pattern, $modifier ) = $params[1] =~ m~^/(.+)/([a-z]*)$~;
    $pattern = "(?$modifier:$pattern)" if ($modifier);
    return ( $params[0] =~ qr($pattern) ) ? 1 : 0;
}

=pod

=head3 REPEAT

 # purpose   : string composed of STR1 repeated C times, or NULL for any NULL args
 # arguments : STR1, C

=cut

sub SQL_FUNCTION_REPEAT
{
    ( defined || return undef ) for ( @_[ 2 .. 3 ] );
    return $_[2] x int( $_[3] );
}

=pod

=head3 REPLACE aka SUBSTITUTE

 # purpose   : perform perl subsitution on input string
 # arguments : a string and a substitute pattern string
 # returns   : the result of the substitute operation
 #
 # example   : ... WHERE REPLACE(col3,'s/fun(\w+)nier/$1/ig') ... replaces
 #			all instances of /fun(\w+)nier/ in col3 with the string
 #			between 'fun' and 'nier'

=cut

sub SQL_FUNCTION_REPLACE
{
    my ( $self, $owner, @params ) = @_;
    return undef unless defined $params[0] and defined $params[1];

    eval "\$params[0]=~$params[1]";
    return $@ ? undef : $params[0];
}
no warnings 'once';
*SQL_FUNCTION_SUBSTITUTE = \&SQL_FUNCTION_REPLACE;
use warnings 'all';

=pod

=head3 SOUNDEX

 # purpose   : test if two strings have matching soundex codes
 # arguments : two strings
 # returns   : true if the strings share the same soundex code
 #
 # example   : ... WHERE SOUNDEX(col3,'fun') ... matches rows
 #             in which col3 is a soundex match for "fun"

=cut

sub SQL_FUNCTION_SOUNDEX
{
    my ( $self, $owner, @params ) = @_;
    require_module("Text::Soundex");
    my $s1 = Text::Soundex::soundex( $params[0] ) or return 0;
    my $s2 = Text::Soundex::soundex( $params[1] ) or return 0;
    return ( $s1 eq $s2 ) ? 1 : 0;
}

=pod

=head3 SPACE

 # purpose   : a string of spaces
 # arguments : number of spaces

=cut

sub SQL_FUNCTION_SPACE { return ' ' x int( $_[2] ); }

=pod

=head3 SUBSTRING

  SUBSTRING( string FROM start_pos [FOR length] )

Returns the substring starting at start_pos and extending for
"length" character or until the end of the string, if no
"length" is supplied.  Examples:

  SUBSTRING( 'foobar' FROM 4 )       # returns "bar"

  SUBSTRING( 'foobar' FROM 4 FOR 2)  # returns "ba"

Note: The SUBSTRING function is implemented in L<SQL::Parser> and L<SQL::Statement> and, at the current time, can not be over-ridden.

=head3 SUBSTR

 # purpose   : same as SUBSTRING, except with comma-delimited params, instead of
               words (NULL for any NULL args)
 # arguments : string, start_pos, [length]

=cut

sub SQL_FUNCTION_SUBSTR
{
    my ( $self, $owner, @params ) = @_;
    ( defined || return undef ) for ( @params[ 0 .. 2 ] );
    my $string = $params[0] || '';
    my $start  = $params[1] || 0;
    my $offset = $params[2] || length $string;
    my $value  = '';
    $value = substr( $string, $start - 1, $offset )
      if length $string >= $start - 2 + $offset;
    return $value;
}

=pod

=head3 TRANSLATE

 # purpose   : transliteration; replace a set of characters in a string with another
               set of characters (a la tr///), or NULL for any NULL args
 # arguments : string, string to replace, replacement string

=cut

sub SQL_FUNCTION_TRANSLATE
{
    my ( $self, $owner, $str, $oldlist, $newlist ) = @_;
    $oldlist =~ s{(/\-)}{\\$1}g;
    $newlist =~ s{(/\-)}{\\$1}g;
    eval "\$str =~ tr/$oldlist/$newlist/";
    return $str;
}

=pod

=head3 TRIM

  TRIM ( [ [LEADING|TRAILING|BOTH] ['trim_char'] FROM ] string )

Removes all occurrences of <trim_char> from the front, back, or
both sides of a string.

 BOTH is the default if neither LEADING nor TRAILING is specified.

 Space is the default if no trim_char is specified.

 Examples:

 TRIM( string )
   trims leading and trailing spaces from string

 TRIM( LEADING FROM str )
   trims leading spaces from string

 TRIM( 'x' FROM str )
   trims leading and trailing x's from string

Note: The TRIM function is implemented in L<SQL::Parser> and L<SQL::Statement> and, at the current time, can not be over-ridden.

=pod

=head3 UNHEX

 # purpose   : convert each pair of hexadecimal digits to a byte (or a Unicode character)
 # arguments : string of hex digits, with an optional encoding name of the data string

=cut

sub SQL_FUNCTION_UNHEX
{
    my ( $self, $owner, $hex, $encoding ) = @_;
    return undef unless ( defined $hex );

    $hex =~ s/\s+//g;
    $hex =~ s/[^0-9a-fA-F]+//g;

    my $str = '';
    foreach my $i ( 0 .. int( ( length($hex) - 1 ) / 2 ) )
    {
        $str .= pack( 'C', SQL_FUNCTION_CONV( $self, $owner, substr( $hex, $i * 2, 2 ), 16, 10 ) );
    }
    return $encoding ? Encode::decode( $encoding, $str, Encode::FB_WARN ) : $str;
}

=head2 Numeric Functions

=head3 ABS

 # purpose   : find the absolute value of a given numeric expression
 # arguments : numeric expression

=cut

sub SQL_FUNCTION_ABS { return abs( $_[2] ); }

=pod

=head3 CEILING (aka CEIL) & FLOOR

 # purpose   : rounds up/down to the nearest integer
 # arguments : numeric expression

=cut

sub SQL_FUNCTION_CEILING
{
    my $i = int( $_[2] );
    return $i == $_[2] ? $i : SQL_FUNCTION_ROUND( @_[ 0 .. 1 ], $_[2] + 0.5, 0 );
}

sub SQL_FUNCTION_FLOOR
{
    my $i = int( $_[2] );
    return $i == $_[2] ? $i : SQL_FUNCTION_ROUND( @_[ 0 .. 1 ], $_[2] - 0.5, 0 );
}
no warnings 'once';
*SQL_FUNCTION_CEIL = \&SQL_FUNCTION_CEILING;
use warnings 'all';

=pod

=head3 EXP

 # purpose   : raise e to the power of a number
 # arguments : numeric expression

=cut

sub SQL_FUNCTION_EXP { return ( sinh(1) + cosh(1) )**$_[2]; }    # e = sinh(X)+cosh(X)

=pod

=head3 LOG

 # purpose   : base B logarithm of X
 # arguments : B, X or just one argument of X for base 10

=cut

sub SQL_FUNCTION_LOG { return $_[3] ? log( $_[3] ) / log( $_[2] ) : log( $_[2] ) / log(10); }

=pod

=head3 LN & LOG10

 # purpose   : natural logarithm (base e) or base 10 of X
 # arguments : numeric expression

=cut

sub SQL_FUNCTION_LN    { return log( $_[2] ); }
sub SQL_FUNCTION_LOG10 { return SQL_FUNCTION_LOG( @_[ 0 .. 2 ] ); }

=pod

=head3 MOD

 # purpose   : modulus, or remainder, left over from dividing X / Y
 # arguments : X, Y

=cut

sub SQL_FUNCTION_MOD { return $_[2] % $_[3]; }

=pod

=head3 POWER aka POW

 # purpose   : X to the power of Y
 # arguments : X, Y

=cut

sub SQL_FUNCTION_POWER { return $_[2]**$_[3]; }
no warnings 'once';
*SQL_FUNCTION_POW = \&SQL_FUNCTION_POWER;
use warnings 'all';

=pod

=head3 RAND

 # purpose   : random fractional number greater than or equal to 0 and less than the value of X
 # arguments : X (with optional seed value of Y)

=cut

sub SQL_FUNCTION_RAND { $_[3] && srand( $_[3] ); return rand( $_[2] ); }

=pod

=head3 ROUND

 # purpose   : round X with Y number of decimal digits (precision)
 # arguments : X, optional Y defaults to 0

=cut

sub SQL_FUNCTION_ROUND { return sprintf( "%.*f", $_[3] ? int( $_[3] ) : 0, $_[2] ); }

=pod

=head3 SIGN

 # purpose   : returns -1, 0, 1, NULL for negative, 0, positive, NULL values, respectively
 # arguments : numeric expression

=cut

sub SQL_FUNCTION_SIGN { return defined( $_[2] ) ? ( $_[2] <=> 0 ) : undef; }

=pod

=head3 SQRT

 # purpose   : square root of X
 # arguments : X

=cut

sub SQL_FUNCTION_SQRT { return sqrt( $_[2] ); }

=pod

=head3 TRUNCATE aka TRUNC

 # purpose   : similar to ROUND, but removes the decimal
 # arguments : X, optional Y defaults to 0

=cut

sub SQL_FUNCTION_TRUNCATE
{
    my $c = int( $_[3] || 0 );
    my $d = 10**$c;
    return sprintf( "%.*f", $c, int( $_[2] * $d ) / $d );
}
no warnings 'once';
*SQL_FUNCTION_TRUNC = \&SQL_FUNCTION_TRUNCATE;
use warnings 'all';

=pod

=head2 Trigonometric Functions

All of these functions work exactly like their counterparts in L<Math::Trig>; go there for documentation.

=cut

=over

=item ACOS

=item ACOSEC

=item ACOSECH

=item ACOSH

=item ACOT

=item ACOTAN

=item ACOTANH

=item ACOTH

=item ACSC

=item ACSCH

=item ASEC

=item ASECH

=item ASIN

=item ASINH

=item ATAN

=item ATANH

=item COS

=item COSEC

=item COSECH

=item COSH

=item COT

=item COTAN

=item COTANH

=item COTH

=item CSC

=item CSCH

=item SEC

=item SECH

=item SIN

=item SINH

=item TAN

=item TANH

Takes a single parameter.  All of L<Math::Trig>'s aliases are included.

=item ATAN2

The y,x version of arc tangent.

=item DEG2DEG

=item DEG2GRAD

=item DEG2RAD

Converts out-of-bounds values into its correct range.

=item GRAD2DEG

=item GRAD2GRAD

=item GRAD2RAD

=item RAD2DEG

=item RAD2GRAD

=item RAD2RAD

Like their L<Math::Trig>'s counterparts, accepts an optional 2nd boolean parameter (like B<TRUE>) to keep prevent range wrapping.

=item DEGREES

=item RADIANS

B<DEGREES> and B<RADIANS> are included for SQL-92 compatibility, and map to B<RAD2DEG> and B<DEG2RAD>, respectively.

=item PI

B<PI> can be used without parentheses. 

=back

=cut

sub SQL_FUNCTION_ACOS    { return acos( $_[2]    || 0 ); }
sub SQL_FUNCTION_ACOSEC  { return acosec( $_[2]  || 0 ); }
sub SQL_FUNCTION_ACOSECH { return acosech( $_[2] || 0 ); }
sub SQL_FUNCTION_ACOSH   { return acosh( $_[2]   || 0 ); }
sub SQL_FUNCTION_ACOT    { return acot( $_[2]    || 0 ); }
sub SQL_FUNCTION_ACOTAN  { return acotan( $_[2]  || 0 ); }
sub SQL_FUNCTION_ACOTANH { return acotanh( $_[2] || 0 ); }
sub SQL_FUNCTION_ACOTH   { return acoth( $_[2]   || 0 ); }
sub SQL_FUNCTION_ACSC    { return acsc( $_[2]    || 0 ); }
sub SQL_FUNCTION_ACSCH   { return acsch( $_[2]   || 0 ); }
sub SQL_FUNCTION_ASEC    { return asec( $_[2]    || 0 ); }
sub SQL_FUNCTION_ASECH   { return asech( $_[2]   || 0 ); }
sub SQL_FUNCTION_ASIN    { return asin( $_[2]    || 0 ); }
sub SQL_FUNCTION_ASINH   { return asinh( $_[2]   || 0 ); }
sub SQL_FUNCTION_ATAN    { return atan( $_[2]    || 0 ); }
sub SQL_FUNCTION_ATAN2 { return atan2( $_[2] || 0, $_[3] || 0 ); }
sub SQL_FUNCTION_ATANH { return atanh( $_[2] || 0 ); }
sub SQL_FUNCTION_COS   { return cos( $_[2]   || 0 ); }
sub SQL_FUNCTION_COSEC { return cosec( $_[2] || 0 ); }
sub SQL_FUNCTION_COSECH    { return cosech( $_[2]    || 0 ); }
sub SQL_FUNCTION_COSH      { return cosh( $_[2]      || 0 ); }
sub SQL_FUNCTION_COT       { return cot( $_[2]       || 0 ); }
sub SQL_FUNCTION_COTAN     { return cotan( $_[2]     || 0 ); }
sub SQL_FUNCTION_COTANH    { return cotanh( $_[2]    || 0 ); }
sub SQL_FUNCTION_COTH      { return coth( $_[2]      || 0 ); }
sub SQL_FUNCTION_CSC       { return csc( $_[2]       || 0 ); }
sub SQL_FUNCTION_CSCH      { return csch( $_[2]      || 0 ); }
sub SQL_FUNCTION_DEG2DEG   { return deg2deg( $_[2]   || 0 ); }
sub SQL_FUNCTION_RAD2RAD   { return rad2rad( $_[2]   || 0 ); }
sub SQL_FUNCTION_GRAD2GRAD { return grad2grad( $_[2] || 0 ); }
sub SQL_FUNCTION_DEG2GRAD  { return deg2grad( $_[2]  || 0, $_[3] || 0 ); }
sub SQL_FUNCTION_DEG2RAD   { return deg2rad( $_[2]   || 0, $_[3] || 0 ); }
sub SQL_FUNCTION_DEGREES   { return rad2deg( $_[2]   || 0, $_[3] || 0 ); }
sub SQL_FUNCTION_GRAD2DEG  { return grad2deg( $_[2]  || 0, $_[3] || 0 ); }
sub SQL_FUNCTION_GRAD2RAD  { return grad2rad( $_[2]  || 0, $_[3] || 0 ); }
sub SQL_FUNCTION_PI       { return pi; }
sub SQL_FUNCTION_RAD2DEG  { return rad2deg( $_[2] || 0, $_[3] || 0 ); }
sub SQL_FUNCTION_RAD2GRAD { return rad2grad( $_[2] || 0, $_[3] || 0 ); }
sub SQL_FUNCTION_RADIANS  { return deg2rad( $_[2] || 0, $_[3] || 0 ); }
sub SQL_FUNCTION_SEC  { return sec( $_[2]  || 0 ); }
sub SQL_FUNCTION_SECH { return sech( $_[2] || 0 ); }
sub SQL_FUNCTION_SIN  { return sin( $_[2]  || 0 ); }
sub SQL_FUNCTION_SINH { return sinh( $_[2] || 0 ); }
sub SQL_FUNCTION_TAN  { return tan( $_[2]  || 0 ); }
sub SQL_FUNCTION_TANH { return tanh( $_[2] || 0 ); }

=head2 System Functions

=head3 DBNAME & USERNAME (aka USER)

 # purpose   : name of the database / username
 # arguments : none

=cut

sub SQL_FUNCTION_DBNAME   { return $_[1]->{Database}{Name}; }
sub SQL_FUNCTION_USERNAME { return $_[1]->{Database}{CURRENT_USER}; }
no warnings 'once';
*SQL_FUNCTION_USER = \&SQL_FUNCTION_USERNAME;
use warnings 'all';

=head2 Special Utility Functions

=head3 IMPORT

 CREATE TABLE foo AS IMPORT(?)    ,{},$external_executed_sth
 CREATE TABLE foo AS IMPORT(?)    ,{},$AoA

=cut

sub SQL_FUNCTION_IMPORT
{
    my ( $self, $owner, @params ) = @_;
    if ( _ARRAY0( $params[0] ) )
    {
        return $params[0] unless ( _HASH0( $params[0]->[0] ) );
        my @tbl = ();
        for my $row ( @{ $params[0] } )
        {
            my @cols = sort keys %{$row};
            push @tbl, \@cols unless @tbl;
            push @tbl, [ @$row{@cols} ];
        }
        return \@tbl;
    }
    elsif ( _INSTANCE( $params[0], 'DBI::st' ) )
    {
        my @cols;
        @cols = @{ $params[0]->{NAME} } unless @cols;

        #    push @{$sth->{org_names}},$_ for @cols;
        my $tbl = [ \@cols ];
        while ( my @row = $params[0]->fetchrow_array() )
        {
            push @$tbl, \@row;
        }

        return $tbl;
    }
}

=head3 RUN

Takes the name of a file containing SQL statements and runs the statements; see
L<SQL::Parser> for documentation.

=cut

sub SQL_FUNCTION_RUN
{
    my ( $self, $owner, $file ) = @_;
    my @params = $owner->{sql_stmt}->params();
    @params = () unless @params;
    local *IN;
    open( IN, '<', $file ) or die "Couldn't open SQL File '$file': $!\n";
    my @stmts = split /;\s*\n+/, join '', <IN>;
    $stmts[-1] =~ s/;\s*$//;
    close IN;
    my @results = ();

    for my $sql (@stmts)
    {
        my $tmp_sth = $owner->{Database}->prepare($sql);
        $tmp_sth->execute(@params);
        next unless $tmp_sth->{NUM_OF_FIELDS};
        push @results, $tmp_sth->{NAME} unless @results;
        while ( my @r = $tmp_sth->fetchrow_array() ) { push @results, \@r }
    }

    #use Data::Dumper; print Dumper \@results and exit if @results;
    return \@results;
}

=pod

=head1 Submitting built-in functions

If you make a generally useful UDF, why not submit it to me and have it (and your name) included with the built-in functions?  Please follow the format shown in the module including a description of the arguments and return values for the function as well as an example.  Send them to the dbi-dev@perl.org mailing list (see L<http://dbi.perl.org>).

Thanks in advance :-).

=head1 ACKNOWLEDGEMENTS

Dean Arnold supplied DECODE, COALESCE, REPLACE, many thanks!
Brendan Byrd added in the Numeric/Trig/System functions and filled in the SQL92/ODBC gaps for the date/string functions.

=head1 AUTHOR & COPYRIGHT

Copyright (c) 2005 by Jeff Zucker: jzuckerATcpan.org
Copyright (c) 2009-2017 by Jens Rehsack: rehsackATcpan.org

All rights reserved.

The module may be freely distributed under the same terms as
Perl itself using either the "GPL License" or the "Artistic
License" as specified in the Perl README file.

=cut

1;
