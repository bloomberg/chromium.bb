#####################################################################
#
# the Perl::Tidy::VerticalAligner::Alignment class holds information
# on a single column being aligned
#
#####################################################################
package Perl::Tidy::VerticalAligner::Alignment;
use strict;
use warnings;
our $VERSION = '20181120';

{

    #use Carp;

    #    _column          # the current column number
    #    _starting_column # column number when created
    #    _matching_token  # what token we are matching
    #    _starting_line   # the line index of creation
    #    _ending_line
    # the most recent line to use it
    #    _saved_column
    #    _serial_number   # unique number for this alignment

    my %default_data = (
        column          => undef,
        starting_column => undef,
        matching_token  => undef,
        starting_line   => undef,
        ending_line     => undef,
        saved_column    => undef,
        serial_number   => undef,
    );

    # class population count
    {
        my $_count = 0;
        sub get_count        { return $_count }
        sub _increment_count { return ++$_count }
        sub _decrement_count { return --$_count }
    }

    # constructor
    sub new {
        my ( $caller, %arg ) = @_;
        my $caller_is_obj = ref($caller);
        my $class         = $caller_is_obj || $caller;
        ##no strict "refs";
        my $self = bless {}, $class;

        foreach my $key ( keys %default_data ) {
            my $_key = '_' . $key;
            if    ( exists $arg{$key} ) { $self->{$_key} = $arg{$key} }
            elsif ($caller_is_obj)      { $self->{$_key} = $caller->{$_key} }
            else { $self->{$_key} = $default_data{$_key} }
        }
        $self->_increment_count();
        return $self;
    }

    sub DESTROY {
        my $self = shift;
        $self->_decrement_count();
        return;
    }

    sub get_column { my $self = shift; return $self->{_column} }

    sub get_starting_column {
        my $self = shift;
        return $self->{_starting_column};
    }
    sub get_matching_token { my $self = shift; return $self->{_matching_token} }
    sub get_starting_line  { my $self = shift; return $self->{_starting_line} }
    sub get_ending_line    { my $self = shift; return $self->{_ending_line} }
    sub get_serial_number  { my $self = shift; return $self->{_serial_number} }

    sub set_column { my ( $self, $val ) = @_; $self->{_column} = $val; return }

    sub set_starting_column {
        my ( $self, $val ) = @_;
        $self->{_starting_column} = $val;
        return;
    }

    sub set_matching_token {
        my ( $self, $val ) = @_;
        $self->{_matching_token} = $val;
        return;
    }

    sub set_starting_line {
        my ( $self, $val ) = @_;
        $self->{_starting_line} = $val;
        return;
    }

    sub set_ending_line {
        my ( $self, $val ) = @_;
        $self->{_ending_line} = $val;
        return;
    }

    sub increment_column {
        my ( $self, $val ) = @_;
        $self->{_column} += $val;
        return;
    }

    sub save_column {
        my $self = shift;
        $self->{_saved_column} = $self->{_column};
        return;
    }

    sub restore_column {
        my $self = shift;
        $self->{_column} = $self->{_saved_column};
        return;
    }
}

1;
