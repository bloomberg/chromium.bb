################################################################################
# Data::Random
#
# A module used to generate random data.
################################################################################

package Data::Random;

################################################################################
# - Modules and Libraries
################################################################################
use strict;
use warnings;
use 5.005_62;

use Carp qw(cluck);
use Time::Piece;
#use Data::Random::WordList;

require Exporter;

################################################################################
# - Global Constants and Variables
################################################################################
use vars qw(
  @ISA
  %EXPORT_TAGS
  @EXPORT_OK
  @EXPORT
);

@ISA = qw(Exporter);

%EXPORT_TAGS = (
    'all' => [
        qw(
        rand_words
        rand_chars
        rand_set
        rand_enum
        rand_date
        rand_time
        rand_datetime
        rand_image
        )
    ]
);

@EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
@EXPORT    = qw();

our $VERSION = '0.13';

################################################################################
# - Subroutines
################################################################################

################################################################################
# rand_words()
################################################################################
sub rand_words {

    # Get the options hash
    my %options = @_;

    # Initialize max and min vars
    $options{'min'} ||= 1;
    $options{'max'} ||= 1;

    # Initialize the wordlist param
    $options{'wordlist'} ||= '';

    # Make sure the max and min vars are OK
    cluck('min value cannot be larger than max value') && return
      if $options{'min'} > $options{'max'};
    cluck('min value must be a positive integer') && return
      if $options{'min'} < 0 || $options{'min'} != int( $options{'min'} );
    cluck('max value must be a positive integer') && return
      if $options{'max'} < 0 || $options{'max'} != int( $options{'max'} );

    # Initialize the size var
    $options{'size'} ||=
      int( rand( $options{'max'} - $options{'min'} + 1 ) ) + $options{'min'};

    # Make sure the size var is OK
    cluck('size value must be a positive integer') && return
      if $options{'size'} < 0 || $options{'size'} != int( $options{'size'} );

    # Initialize the shuffle flag
    $options{'shuffle'} =
      exists( $options{'shuffle'} ) ? $options{'shuffle'} : 1;

    my $wl;
    my $close_wl = 1;

    # Check for a pre-existing wordlist object
    if ( ref( $options{'wordlist'} ) ) {
        $wl       = $options{'wordlist'};
        $close_wl = 0;
    }
    else {
        require Data::Random::WordList;

        # Create a new wordlist object
        $wl = Data::Random::WordList->new( wordlist => $options{'wordlist'} );
    }

    # Get the random words
    my $rand_words = $wl->get_words( $options{'size'} );

    # Close the word list
    $wl->close() if $close_wl;

    # Shuffle the words around
    _shuffle($rand_words) if $options{'shuffle'};

# Return an array or an array reference, depending on the context in which the sub was called
    if ( wantarray() ) {
        return @$rand_words;
    }
    else {
        return $rand_words;
    }
}

################################################################################
# rand_chars()
################################################################################
sub rand_chars {

    # Get the options hash
    my %options = @_;
    my @chars;

    # Build named character sets if one wasn't supplied
    if ( ref( $options{'set'} ) ne 'ARRAY' ) {
        my @charset = ();

        if ( $options{'set'} eq 'all' ) {
            @charset =
              ( 0 .. 9, 'a' .. 'z', 'A' .. 'Z', '#', ',',
                qw(~ ! @ $ % ^ & * ( ) _ + = - { } | : " < > ? / . ' ; ] [ \ `)
            );
        }
        elsif ( $options{'set'} eq 'alpha' ) {
            @charset = ( 'a' .. 'z', 'A' .. 'Z' );
        }
        elsif ( $options{'set'} eq 'upperalpha' ) {
            @charset = ( 'A' .. 'Z' );
        }
        elsif ( $options{'set'} eq 'loweralpha' ) {
            @charset = ( 'a' .. 'z' );
        }
        elsif ( $options{'set'} eq 'numeric' ) {
            @charset = ( 0 .. 9 );
        }
        elsif ( $options{'set'} eq 'alphanumeric' ) {
            @charset = ( 0 .. 9, 'a' .. 'z', 'A' .. 'Z' );
        }
        elsif ( $options{'set'} =~ /^(misc|char)$/ ) {
            @charset =
              ( '#', ',',
                qw(~ ! @ $ % ^ & * ( ) _ + = - { } | : " < > ? / . ' ; ] [ \ `)
            );
        }

        $options{'set'} = \@charset;
    }

    @chars = rand_set(%options);
    return wantarray ? @chars : join('', @chars);
}

################################################################################
# rand_set()
################################################################################
sub rand_set {

    # Get the options hash
    my %options = @_;

    # Make sure the set array was defined
    cluck('set array is not defined') && return if !$options{'set'};

    $options{'size'} = 1
      unless exists( $options{'min'} ) || exists( $options{'max'} )
      || exists( $options{'size'} );

    # Initialize max and min vars
    $options{'min'} ||= 0;
    $options{'max'} ||= @{ $options{'set'} };

    # Make sure the max and min vars are OK
    cluck('min value cannot be larger than max value') && return
      if $options{'min'} > $options{'max'};
    cluck('min value must be a positive integer') && return
      if $options{'min'} < 0 || $options{'min'} != int( $options{'min'} );
    cluck('max value must be a positive integer') && return
      if $options{'max'} < 0 || $options{'max'} != int( $options{'max'} );

    # Initialize the size var
    $options{'size'} ||=
      int( rand( $options{'max'} - $options{'min'} + 1 ) ) + $options{'min'};

    # Make sure the size var is OK
    cluck('size value must be a positive integer') && return
      if $options{'size'} < 0 || $options{'size'} != int( $options{'size'} );
    cluck('size value exceeds set size') && return
      if $options{'size'} > @{ $options{'set'} };

    # Initialize the shuffle flag
    $options{'shuffle'} =
      exists( $options{'shuffle'} ) ? $options{'shuffle'} : 1;

    # Get the random items
    my %results = ();
    for ( my $i = 0 ; $i < $options{'size'} ; $i++ ) {
        my $result;

        do {
            $result = int( rand( @{ $options{'set'} } ) );
        } while ( exists( $results{$result} ) );

        $results{$result} = 1;
    }

    my @results = sort { $a <=> $b } keys %results;

    # Shuffle the items
    _shuffle( \@results ) if $options{'shuffle'};

# Return an array or an array reference, depending on the context in which the sub was called
    if ( wantarray() ) {
        return @{ $options{'set'} }[@results];
    }
    else {
        return \@{ $options{'set'} }[@results];
    }
}

################################################################################
# rand_enum()
################################################################################
sub rand_enum {

    # Get the options hash
    my %options = @_ == 1 && ref $_[0] eq 'ARRAY' ? ( set => @_ ) : @_;

    # Make sure the set array was defined
    cluck('set array is not defined') && return if !$options{'set'};

    return $options{'set'}->[ int( rand( @{ $options{'set'} } ) ) ];
}

################################################################################
# rand_date()
################################################################################
sub rand_date {

    # Get the options hash
    my %options = @_;

    my $min;
    my $max;
    # Get today's date
    my $t = localtime;
    my ( $year, $month, $day ) = split('-', $t->ymd);
    my $today = Time::Piece->strptime($t->ymd, "%Y-%m-%d");

    if ( $options{'min'} ) {
        if ( $options{'min'} eq 'now' ) {
            $min = $today;
        }
        else {
            $min = Time::Piece->strptime($options{'min'}, '%Y-%m-%d');
        }
    }
    else {
        $min = $today;
    }
    if ( $options{'max'} ) {
        if ( $options{'max'} eq 'now' ) {
            $max = $today;
        }
        else {
            $max = Time::Piece->strptime($options{max}, "%Y-%m-%d");
        }
    }
    else {
          $max = $min->add_years(1);
    }

    my $delta_days = int($max->julian_day) - int($min->julian_day);
    cluck('max date is later than min date') && return if $delta_days < 0;

    my $result = $min + ( 3600 * 24  * int( rand($delta_days + 1) ) );
    return $result->ymd;
}

################################################################################
# rand_time()
################################################################################
sub rand_time {

    # Get the options hash
    my %options = @_;

    my ( $min_hour, $min_min, $min_sec, $max_hour, $max_min, $max_sec );

    if ( $options{'min'} ) {
        if ( $options{'min'} eq 'now' ) {

            # Get the current time
            my ( $hour, $min, $sec ) = ( localtime() )[ 2, 1, 0 ];

            ( $min_hour, $min_min, $min_sec ) = ( $hour, $min, $sec );
        }
        else {
            eval {
                my $min = Time::Piece->strptime( $options{min}, '%T' );
                ( $min_hour, $min_min, $min_sec )
                    = ( $min->hour, $min->min, $min->sec );
            };
            if ($@) {
                cluck 'minimum time is not in valid time format HH:MM:SS';
                return;
            }
        }
    }
    else {
        ( $min_hour, $min_min, $min_sec ) = ( 0, 0, 0 );
    }

    if ( $options{'max'} ) {
        if ( $options{'max'} eq 'now' ) {

            # Get the current time
            my ( $hour, $min, $sec ) = ( localtime() )[ 2, 1, 0 ];

            ( $max_hour, $max_min, $max_sec ) = ( $hour, $min, $sec );
        }
        else {
            eval {
                my $max = Time::Piece->strptime( $options{max}, '%T' );
                ( $max_hour, $max_min, $max_sec )
                    = ( $max->hour, $max->min, $max->sec );
            };
            if ($@) {
                cluck 'maximum time is not in valid time format HH:MM:SS';
                return;
            }
        }
    }
    else {
        ( $max_hour, $max_min, $max_sec ) = ( 23, 59, 59 );
    }

    my $min_secs = $min_hour * 3600 + $min_min * 60 + $min_sec;
    my $max_secs = ( $max_hour * 3600 ) + ( $max_min * 60 ) + $max_sec;

    my $delta_secs = $max_secs - $min_secs;

    cluck('min time is later than max time') && return if $delta_secs < 0;

    $delta_secs = int( rand( $delta_secs + 1 ) );

    my $result_secs = $min_secs + $delta_secs;

    my $hour = int( $result_secs / 3600 );
    my $min  = int( ( $result_secs - ( $hour * 3600 ) ) / 60 );
    my $sec  = $result_secs % 60;

    return sprintf( "%02u:%02u:%02u", $hour, $min, $sec );
}

################################################################################
# rand_datetime()
################################################################################
sub rand_datetime {

    # Get the options hash
    my %options = @_;

    # Get today's date
    my $now = localtime;
    my $minimum;
    my $maximum;

    if ( $options{min} ) {
        if ( $options{min} eq 'now' ) {
            $minimum = Time::Piece->strptime(
              $now->strftime('%Y-%m-%d %H:%M:%S'),
              '%Y-%m-%d %H:%M:%S'
            );
        }
        else {
            $minimum = Time::Piece->strptime(
              $options{min},
              '%Y-%m-%d %H:%M:%S'
            );
        }
    }
    else {
        $minimum = $now;
    }

    if ( $options{max} ) {
        if ( $options{max} eq 'now' ) {
            $maximum = Time::Piece->strptime(
              $now->strftime('%Y-%m-%d %H:%M:%S'),
              '%Y-%m-%d %H:%M:%S'
            );
        }
        else {
            $maximum = Time::Piece->strptime(
              $options{max},
              '%Y-%m-%d %H:%M:%S'
            );
        }
    }
    else {
        $maximum = $minimum->add_years(1);
    }

    my $delta_secs = $maximum - $minimum;
    cluck('max_date is later than min date') && return if $delta_secs < 0;
    $delta_secs = int( rand( $delta_secs + 1 ) );

    my $result = $minimum + $delta_secs;

    return $result->strftime('%Y-%m-%d %H:%M:%S');
}

################################################################################
# rand_image()
################################################################################
sub rand_image {

    # Get the options hash
    my %options = @_;

    eval q{ require GD; };
    cluck($@) && return if $@;

    $options{'minwidth'} ||= 1;
    $options{'maxwidth'} ||= 100;
    $options{'width'} ||=
      int( rand( $options{'maxwidth'} - $options{'minwidth'} + 1 ) ) +
      $options{'minwidth'};

    $options{'minheight'} ||= 1;
    $options{'maxheight'} ||= 100;
    $options{'height'} ||=
      int( rand( $options{'maxheight'} - $options{'minheight'} + 1 ) ) +
      $options{'minheight'};

    $options{'minpixels'} ||= 0;
    $options{'maxpixels'} ||= $options{'width'} * $options{'height'};
    $options{'pixels'} ||=
      int( rand( $options{'maxpixels'} - $options{'minpixels'} + 1 ) ) +
      $options{'minpixels'};

    $options{'bgcolor'} ||= _color();
    $options{'fgcolor'} ||= _color();

    my $image = GD::Image->new( $options{'width'}, $options{'height'} );

    my $bgcolor = $image->colorAllocate( @{ $options{'bgcolor'} } );
    my $fgcolor = $image->colorAllocate( @{ $options{'fgcolor'} } );

    $image->rectangle( 0, 0, $options{'width'}, $options{'height'}, $bgcolor );

    for ( my $i = 0 ; $i < $options{'pixels'} ; $i++ ) {
        my $x = int( rand( $options{'width'} + 1 ) );
        my $y = int( rand( $options{'height'} + 1 ) );

        $image->setPixel( $x, $y, $fgcolor );
    }

    return $image->png();

    sub _color {
        return [ int( rand(256) ), int( rand(256) ), int( rand(256) ) ];
    }
}

################################################################################
# _shuffle()
################################################################################
sub _shuffle {
    my $array = shift;

    for ( my $i = @$array - 1 ; $i >= 0 ; $i-- ) {
        my $j = int( rand( $i + 1 ) );

        @$array[ $i, $j ] = @$array[ $j, $i ] if $i != $j;
    }
}

1;



=head1 NAME

Data::Random - Perl module to generate random data


=head1 SYNOPSIS

  use Data::Random qw(:all);

  my @random_words = rand_words( size => 10 );

  my @random_chars = rand_chars( set => 'all', min => 5, max => 8 );
  my $string       = rand_chars( set => 'all', min => 5, max => 8 );

  my @random_set = rand_set( set => \@set, size => 5 );

  my $random_enum = rand_enum( set => \@set );
  my $random_enum = rand_enum( \@set ); # shortcut

  my $random_date = rand_date();

  my $random_time = rand_time();

  my $random_datetime = rand_datetime();

  open(my $file, ">", "rand_image.png") or die $!;
  binmode($file);
  print $file rand_image( bgcolor => [0, 0, 0] );
  close($file);


=head1 DESCRIPTION

A module used to generate random data.  Useful mostly for test programs.


=head1 METHODS

=head2 rand_words()

This returns a list of random words given a wordlist.  See below for possible parameters.

=over 4

=item *

wordlist - the path to the wordlist file.  A lot of systems have one at /usr/dict/words.  You can also optionally supply a Data::Random::WordList object to keep a persistent wordlist.  The default is the wordlist distributed with this module.

=item *

min - the minimum number of words to return.  The default is 1.

=item *

max - the maximum number of words to return.  The default is 1.

=item *

size - the number of words to return.  The default is 1.  If you supply a value for 'size', then 'min' and 'max' aren't paid attention to.

=item *

shuffle - whether or not the words should be randomly shuffled.  Set this to 0 if you don't want the words shuffled.  The default is 1.  Random::Data::WordList returns words in the order that they're viewed in the word list file, so shuffling will make sure that the results are a little more random.

=back


=head2 rand_chars()

When called in a list context this returns
a list of random characters given a set of characters.
In a scalar context it returns a string of random characters.
See below for possible parameters.

=over 4

=item *

set - the set of characters to be used.  This value can be either a reference to an array of strings, or one of the following:

    alpha        - alphabetic characters: a-z, A-Z
    upperalpha   - upper case alphabetic characters: A-Z
    loweralpha   - lower case alphabetic characters: a-z
    numeric      - numeric characters: 0-9
    alphanumeric - alphanumeric characters: a-z, A-Z, 0-9
    char         - non-alphanumeric characters: # ~ ! @ $ % ^ & * ( ) _ + = - { } | : " < > ? / . ' ; ] [ \ `
    misc         - same as 'char'
    all          - all of the above

=item *

min - the minimum number of characters to return.  The default is 0.

=item *

max - the maximum number of characters to return.  The default is the size of the set.

=item *

size - the number of characters to return.  The default is 1.  If you supply a value for 'size', then 'min' and 'max' aren't paid attention to.

=item *

shuffle - whether or not the characters should be randomly shuffled.  Set this to 0 if you want the characters to stay in the order received.  The default is 1.

=back


=head2 rand_set()

This returns a random set of elements given an initial set.  See below for possible parameters.

=over 4

=item *

set - the set of strings to be used.  This should be a reference to an array of strings.

=item *

min - the minimum number of strings to return.  The default is 0.

=item *

max - the maximum number of strings to return.  The default is the size of the set.

=item *

size - the number of strings to return.  The default is 1.  If you supply a value for 'size', then 'min' and 'max' aren't paid attention to.

=item *

shuffle - whether or not the strings should be randomly shuffled.  Set this to 0 if you want the strings to stay in the order received.  The default is 1.

=back


=head2 rand_enum()

This returns a random element given an initial set.  See below for possible parameters.

=over 4

=item *

set - the set of strings to be used.  This should be a reference to an array of strings. The C<set> key will be assumed if the array reference is passed as the only argument.

=back


=head2 rand_date()

This returns a random date in the form "YYYY-MM-DD".  2-digit years are not currently supported.  Efforts are made to make sure you're returned a truly valid date--ie, you'll never be returned the date February 31st.  See the options below to find out how to control the date range.  Here are a few examples:

    # returns a date somewhere in between the current date, and one year from the current date
    $date = rand_date();

    # returns a date somewhere in between September 21, 1978 and September 21, 1979
    $date = rand_date( min => '1978-9-21' );

    # returns a date somewhere in between September 21, 1978 and the current date
    $date = rand_date( min => '1978-9-21', max => 'now' );

    # returns a date somewhere in between the current date and September 21, 2008
    $date = rand_date( min => 'now', max => '2008-9-21' );

See below for possible parameters.

=over 4

=item *

min - the minimum date to be returned. It should be in the form "YYYY-MM-DD" or you can alternatively use the string "now" to represent the current date.  The default is the current date;

=item *

max - the maximum date to be returned. It should be in the form "YYYY-MM-DD" or you can alternatively use the string "now" to represent the current date.  The default is one year from the minimum date;

=back


=head2 rand_time()

This returns a random time in the form "HH:MM:SS".  24 hour times are supported.  See the options below to find out how to control the time range.  Here are a few examples:

    # returns a random 24-hr time (between 00:00:00 and 23:59:59)
    $time = rand_time();

    # returns a time somewhere in between 04:00:00 and the end of the day
    $time = rand_time( min => '4:0:0' );

    # returns a time somewhere in between 8:00:00 and the current time (if it's after 8:00)
    $time = rand_time( min => '12:00:00', max => 'now' );

    # returns a date somewhere in between the current time and the end of the day
    $time = rand_time( min => 'now' );

See below for possible parameters.

=over 4

=item *

min - the minimum time to be returned. It should be in the form "HH:MM:SS" or you can alternatively use the string "now" to represent the current time.  The default is 00:00:00;

=item *

max - the maximum time to be returned. It should be in the form "HH:MM:SS" or you can alternatively use the string "now" to represent the current time.  The default is 23:59:59;

=back


=head2 rand_datetime()

This returns a random date and time in the form "YYYY-MM-DD HH:MM:SS".  See the options below to find out how to control the date/time range.  Here are a few examples:

    # returns a date somewhere in between the current date/time, and one year from the current date/time
    $datetime = rand_datetime();

    # returns a date somewhere in between 4:00 September 21, 1978 and 4:00 September 21, 1979
    $datetime = rand_datetime( min => '1978-9-21 4:0:0' );

    # returns a date somewhere in between 4:00 September 21, 1978 and the current date
    $datetime = rand_datetime( min => '1978-9-21 4:0:0', max => 'now' );

    # returns a date somewhere in between the current date/time and the end of the day September 21, 2008
    $datetime = rand_datetime( min => 'now', max => '2008-9-21 23:59:59' );

See below for possible parameters.

=over 4

=item *

min - the minimum date/time to be returned. It should be in the form "YYYY-MM-DD HH:MM:SS" or you can alternatively use the string "now" to represent the current date/time.  The default is the current date/time;

=item *

max - the maximum date/time to be returned. It should be in the form "YYYY-MM-DD HH:MM:SS" or you can alternatively use the string "now" to represent the current date/time.  The default is one year from the minimum date/time;

=back


=head2 rand_image()

This returns a random image.  Currently only PNG images are supported.  See below for possible parameters.

=over 4

=item *

minwidth - the minimum width of the image.  The default is 1.

=item *

maxwidth - the maximum width of the image.  The default is 100.

=item *

width - the width of the image.  If you supply a value for 'width', then 'minwidth' and 'maxwidth' aren't paid attention to.

=item *

minheight - the minimum height of the image.  The default is 1.

=item *

maxheight - the maximum height of the image.  The default is 100.

=item *

height - the height of the image.  If you supply a value for 'width', then 'minwidth' and 'maxwidth' aren't paid attention to.

=item *

minpixels - the minimum number of random pixels to display on the image.  The default is 0.

=item *

maxpixels - the maximum number of random pixels to display on the image.  The default is width * height.

=item *

pixels - the number of random pixels to display on the image.  If you supply a value for 'pixels', then 'minpixels' and 'maxpixels' aren't paid attention to.

=item *

bgcolor - the background color of the image.  The value must be a reference to an RGB array where each element is an integer between 0 and 255 (eg. [ 55, 120, 255 ]).

=item *

fgcolor - the foreground color of the image.  The value must be a reference to an RGB array where each element is an integer between 0 and 255 (eg. [ 55, 120, 255 ]).

=back


=head1 VERSION

0.12


=head1 AUTHOR

Originally written by: Adekunle Olonoh

Currently maintained by: Buddy Burden (barefoot@cpan.org), starting with version 0.06


=head1 CREDITS

    Hiroki Chalfant
    David Sarno
	Michiel Beijen


=head1 COPYRIGHT

Copyright (c) 2000-2011 Adekunle Olonoh.
Copyright (c) 2011-2015 Buddy Burden.
All rights reserved.  This program is free software; you
can redistribute it and/or modify it under the same terms as Perl itself.


=head1 SEE ALSO

L<Data::Random::WordList>

=cut
