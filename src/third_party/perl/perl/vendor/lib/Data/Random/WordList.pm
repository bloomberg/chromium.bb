################################################################################
# Data::Random
#
# A module used to generate random data.
#
# Author: Adekunle Olonoh
#   Date: October 2000
################################################################################

package Data::Random::WordList;

################################################################################
# - Modules and Libraries
################################################################################
use strict;
use warnings;

use FileHandle;
use File::Basename qw(dirname);

################################################################################
# - Global Constants and Variables
################################################################################
our $VERSION = '0.13';

################################################################################
# - Subroutines
################################################################################

################################################################################
# new()
################################################################################
sub new {
    my $proto   = shift;
    my %options = @_;

    # Check if what was passed in was a prototype reference or a class name
    my $class = ref($proto) || $proto;

    $options{'wordlist'} ||= dirname($INC{'Data/Random.pm'}).'/Random/dict';

    # Create a new filehandle object
    my $fh = new FileHandle $options{'wordlist'}
      or die "could not open $options{'wordlist'} : $!";

    # Calculate the number of lines in the file
    my $size = 0;
    while (<$fh>) {
        $size++;
    }

    # Create the object
    my $self = bless {
        'fh'   => $fh,
        'size' => $size,
    }, $class;

    return $self;
}

################################################################################
# close()
################################################################################
sub close {
    my $self = shift;

    # Close the filehandle
    $self->{'fh'}->close;
}

################################################################################
# get_words()
################################################################################
sub get_words {
    my $self = shift;
    my $num  = shift || 1;

    my $fh = $self->{'fh'};

    # Perform some error checking
    die 'the size value must be a positive integer'
      if $num < 0 || $num != int($num);
    die
"$num words were requested but only $self->{'size'} words exist in the wordlist"
      if $num > $self->{'size'};

    # Pick which lines we want
    my %rand_lines = ();
    for ( my $i = 0 ; $i < $num ; $i++ ) {
        my $rand_line;

        do {
            $rand_line = int( rand( $self->{'size'} ) );
        } while ( exists( $rand_lines{$rand_line} ) );

        $rand_lines{$rand_line} = 1;
    }

    my $line       = 0;
    my @rand_words = ();

    # Seek to the beginning of the filehandle
    $fh->seek( 0, 0 ) or die "could not seek to position 0 in wordlist: $!";

    # Now get the lines
    while (<$fh>) {
        chomp;
        push ( @rand_words, $_ ) if $rand_lines{$line};

        $line++;
    }

# Return an array or an array reference, depending on the context in which the sub was called
    if ( wantarray() ) {
        return @rand_words;
    }
    else {
        return \@rand_words;
    }
}

1;



=head1 NAME

Data::Random::WordList - Perl module to get random words from a word list


=head1 SYNOPSIS

  use Data::Random::WordList;

  my $wl = new Data::Random::WordList( wordlist => '/usr/dict/words' );

  my @rand_words = $wl->get_words(10);

  $wl->close();


=head1 DESCRIPTION

Data::Random::WordList is a module that manages a file containing a list of words.

The module expects each line of the word list file to contain only one word.  It could thus be easily used to select random lines from a file, but for coherency's sake, I'll keep referring to each line as a word.

The module uses a persistent filehandle so that there isn't a lot of overhead every time you want to fetch a list of random words.  However, it's much more efficient to grab multiple words at a time than it is to fetch one word at a time multiple times.

The module also refrains from reading the whole file into memory, so it can be safer to use with larger files.


=head1 METHODS

=head2 new()

Returns a reference to a new Data::Random::WordList object.  Use the "wordlist" param to initialize the object:

=over 4

=item *

wordlist - the path to the wordlist file.  If a path isn't supplied, the wordlist distributed with this module is used.

=back

=head2 get_words([NUM])

NUM contains the number of words you want from the wordlist.  NUM defaults to 1 if it's not specified.  get_words() dies if NUM is greater than the number of words in the wordlist.  This function returns an array or an array reference depending on the context in which it's called.

=head2 close()

Closes the filehandle associated with the word list.  It's good practice to do this every time you're done with the word list.


=head1 VERSION

0.12


=head1 AUTHOR

Originally written by: Adekunle Olonoh

Currently maintained by: Buddy Burden (barefoot@cpan.org), starting with version 0.06


=head1 COPYRIGHT

Copyright (c) 2000-2011 Adekunle Olonoh.
Copyright (c) 2011-2015 Buddy Burden.
All rights reserved.  This program is free software; you
can redistribute it and/or modify it under the same terms as Perl itself.


=head1 SEE ALSO

L<Data::Random>

=cut
