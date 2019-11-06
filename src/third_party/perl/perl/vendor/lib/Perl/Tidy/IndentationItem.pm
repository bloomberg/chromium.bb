#####################################################################
#
# the Perl::Tidy::IndentationItem class supplies items which contain
# how much whitespace should be used at the start of a line
#
#####################################################################

package Perl::Tidy::IndentationItem;
use strict;
use warnings;
our $VERSION = '20181120';

sub new {

    # Create an 'indentation_item' which describes one level of leading
    # whitespace when the '-lp' indentation is used.
    my (
        $class,               $spaces,           $level,
        $ci_level,            $available_spaces, $index,
        $gnu_sequence_number, $align_paren,      $stack_depth,
        $starting_index,
    ) = @_;

    my $closed            = -1;
    my $arrow_count       = 0;
    my $comma_count       = 0;
    my $have_child        = 0;
    my $want_right_spaces = 0;
    my $marked            = 0;

    # DEFINITIONS:
    # spaces             =>  # total leading white spaces
    # level              =>  # the indentation 'level'
    # ci_level           =>  # the 'continuation level'
    # available_spaces   =>  # how many left spaces available
    #                        # for this level
    # closed             =>  # index where we saw closing '}'
    # comma_count        =>  # how many commas at this level?
    # sequence_number    =>  # output batch number
    # index              =>  # index in output batch list
    # have_child         =>  # any dependents?
    # recoverable_spaces =>  # how many spaces to the right
    #                        # we would like to move to get
    #                        # alignment (negative if left)
    # align_paren        =>  # do we want to try to align
    #                        # with an opening structure?
    # marked             =>  # if visited by corrector logic
    # stack_depth        =>  # indentation nesting depth
    # starting_index     =>  # first token index of this level
    # arrow_count        =>  # how many =>'s

    return bless {
        _spaces             => $spaces,
        _level              => $level,
        _ci_level           => $ci_level,
        _available_spaces   => $available_spaces,
        _closed             => $closed,
        _comma_count        => $comma_count,
        _sequence_number    => $gnu_sequence_number,
        _index              => $index,
        _have_child         => $have_child,
        _recoverable_spaces => $want_right_spaces,
        _align_paren        => $align_paren,
        _marked             => $marked,
        _stack_depth        => $stack_depth,
        _starting_index     => $starting_index,
        _arrow_count        => $arrow_count,
    }, $class;
}

sub permanently_decrease_available_spaces {

    # make a permanent reduction in the available indentation spaces
    # at one indentation item.  NOTE: if there are child nodes, their
    # total SPACES must be reduced by the caller.

    my ( $item, $spaces_needed ) = @_;
    my $available_spaces = $item->get_available_spaces();
    my $deleted_spaces =
      ( $available_spaces > $spaces_needed )
      ? $spaces_needed
      : $available_spaces;
    $item->decrease_available_spaces($deleted_spaces);
    $item->decrease_SPACES($deleted_spaces);
    $item->set_recoverable_spaces(0);

    return $deleted_spaces;
}

sub tentatively_decrease_available_spaces {

    # We are asked to tentatively delete $spaces_needed of indentation
    # for a indentation item.  We may want to undo this later.  NOTE: if
    # there are child nodes, their total SPACES must be reduced by the
    # caller.
    my ( $item, $spaces_needed ) = @_;
    my $available_spaces = $item->get_available_spaces();
    my $deleted_spaces =
      ( $available_spaces > $spaces_needed )
      ? $spaces_needed
      : $available_spaces;
    $item->decrease_available_spaces($deleted_spaces);
    $item->decrease_SPACES($deleted_spaces);
    $item->increase_recoverable_spaces($deleted_spaces);
    return $deleted_spaces;
}

sub get_stack_depth {
    my $self = shift;
    return $self->{_stack_depth};
}

sub get_spaces {
    my $self = shift;
    return $self->{_spaces};
}

sub get_marked {
    my $self = shift;
    return $self->{_marked};
}

sub set_marked {
    my ( $self, $value ) = @_;
    if ( defined($value) ) {
        $self->{_marked} = $value;
    }
    return $self->{_marked};
}

sub get_available_spaces {
    my $self = shift;
    return $self->{_available_spaces};
}

sub decrease_SPACES {
    my ( $self, $value ) = @_;
    if ( defined($value) ) {
        $self->{_spaces} -= $value;
    }
    return $self->{_spaces};
}

sub decrease_available_spaces {
    my ( $self, $value ) = @_;
    if ( defined($value) ) {
        $self->{_available_spaces} -= $value;
    }
    return $self->{_available_spaces};
}

sub get_align_paren {
    my $self = shift;
    return $self->{_align_paren};
}

sub get_recoverable_spaces {
    my $self = shift;
    return $self->{_recoverable_spaces};
}

sub set_recoverable_spaces {
    my ( $self, $value ) = @_;
    if ( defined($value) ) {
        $self->{_recoverable_spaces} = $value;
    }
    return $self->{_recoverable_spaces};
}

sub increase_recoverable_spaces {
    my ( $self, $value ) = @_;
    if ( defined($value) ) {
        $self->{_recoverable_spaces} += $value;
    }
    return $self->{_recoverable_spaces};
}

sub get_ci_level {
    my $self = shift;
    return $self->{_ci_level};
}

sub get_level {
    my $self = shift;
    return $self->{_level};
}

sub get_sequence_number {
    my $self = shift;
    return $self->{_sequence_number};
}

sub get_index {
    my $self = shift;
    return $self->{_index};
}

sub get_starting_index {
    my $self = shift;
    return $self->{_starting_index};
}

sub set_have_child {
    my ( $self, $value ) = @_;
    if ( defined($value) ) {
        $self->{_have_child} = $value;
    }
    return $self->{_have_child};
}

sub get_have_child {
    my $self = shift;
    return $self->{_have_child};
}

sub set_arrow_count {
    my ( $self, $value ) = @_;
    if ( defined($value) ) {
        $self->{_arrow_count} = $value;
    }
    return $self->{_arrow_count};
}

sub get_arrow_count {
    my $self = shift;
    return $self->{_arrow_count};
}

sub set_comma_count {
    my ( $self, $value ) = @_;
    if ( defined($value) ) {
        $self->{_comma_count} = $value;
    }
    return $self->{_comma_count};
}

sub get_comma_count {
    my $self = shift;
    return $self->{_comma_count};
}

sub set_closed {
    my ( $self, $value ) = @_;
    if ( defined($value) ) {
        $self->{_closed} = $value;
    }
    return $self->{_closed};
}

sub get_closed {
    my $self = shift;
    return $self->{_closed};
}
1;
