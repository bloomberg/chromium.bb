#####################################################################
#
# the Perl::Tidy::VerticalAligner::Line class supplies an object to
# contain a single output line
#
#####################################################################

package Perl::Tidy::VerticalAligner::Line;
use strict;
use warnings;
our $VERSION = '20181120';

{

    ##use Carp;

    my %default_data = (
        jmax                      => undef,
        jmax_original_line        => undef,
        rtokens                   => undef,
        rfields                   => undef,
        rpatterns                 => undef,
        indentation               => undef,
        leading_space_count       => undef,
        outdent_long_lines        => undef,
        list_type                 => undef,
        is_hanging_side_comment   => undef,
        ralignments               => [],
        maximum_line_length       => undef,
        rvertical_tightness_flags => undef
    );
    {

        # methods to count object population
        my $_count = 0;
        sub get_count        { return $_count; }
        sub _increment_count { return ++$_count }
        sub _decrement_count { return --$_count }
    }

    # Constructor may be called as a class method
    sub new {
        my ( $caller, %arg ) = @_;
        my $caller_is_obj = ref($caller);
        my $class         = $caller_is_obj || $caller;
        ##no strict "refs";
        my $self = bless {}, $class;

        $self->{_ralignments} = [];

        foreach my $key ( keys %default_data ) {
            my $_key = '_' . $key;

            # Caller keys do not have an underscore
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

    sub get_jmax { my $self = shift; return $self->{_jmax} }

    sub get_jmax_original_line {
        my $self = shift;
        return $self->{_jmax_original_line};
    }
    sub get_rtokens     { my $self = shift; return $self->{_rtokens} }
    sub get_rfields     { my $self = shift; return $self->{_rfields} }
    sub get_rpatterns   { my $self = shift; return $self->{_rpatterns} }
    sub get_indentation { my $self = shift; return $self->{_indentation} }

    sub get_leading_space_count {
        my $self = shift;
        return $self->{_leading_space_count};
    }

    sub get_outdent_long_lines {
        my $self = shift;
        return $self->{_outdent_long_lines};
    }
    sub get_list_type { my $self = shift; return $self->{_list_type} }

    sub get_is_hanging_side_comment {
        my $self = shift;
        return $self->{_is_hanging_side_comment};
    }

    sub get_rvertical_tightness_flags {
        my $self = shift;
        return $self->{_rvertical_tightness_flags};
    }

    sub set_column {
        ## FIXME: does caller ever supply $val??
        my ( $self, $j, $val ) = @_;
        return $self->{_ralignments}->[$j]->set_column($val);
    }

    sub get_alignment {
        my ( $self, $j ) = @_;
        return $self->{_ralignments}->[$j];
    }
    sub get_alignments { my $self = shift; return @{ $self->{_ralignments} } }

    sub get_column {
        my ( $self, $j ) = @_;
        return $self->{_ralignments}->[$j]->get_column();
    }

    sub get_starting_column {
        my ( $self, $j ) = @_;
        return $self->{_ralignments}->[$j]->get_starting_column();
    }

    sub increment_column {
        my ( $self, $k, $pad ) = @_;
        $self->{_ralignments}->[$k]->increment_column($pad);
        return;
    }

    sub set_alignments {
        my ( $self, @args ) = @_;
        @{ $self->{_ralignments} } = @args;
        return;
    }

    sub current_field_width {
        my ( $self, $j ) = @_;
        if ( $j == 0 ) {
            return $self->get_column($j);
        }
        else {
            return $self->get_column($j) - $self->get_column( $j - 1 );
        }
    }

    sub field_width_growth {
        my ( $self, $j ) = @_;
        return $self->get_column($j) - $self->get_starting_column($j);
    }

    sub starting_field_width {
        my ( $self, $j ) = @_;
        if ( $j == 0 ) {
            return $self->get_starting_column($j);
        }
        else {
            return $self->get_starting_column($j) -
              $self->get_starting_column( $j - 1 );
        }
    }

    sub increase_field_width {

        my ( $self, $j, $pad ) = @_;
        my $jmax = $self->get_jmax();
        for my $k ( $j .. $jmax ) {
            $self->increment_column( $k, $pad );
        }
        return;
    }

    sub get_available_space_on_right {
        my $self = shift;
        my $jmax = $self->get_jmax();
        return $self->{_maximum_line_length} - $self->get_column($jmax);
    }

    sub set_jmax { my ( $self, $val ) = @_; $self->{_jmax} = $val; return }

    sub set_jmax_original_line {
        my ( $self, $val ) = @_;
        $self->{_jmax_original_line} = $val;
        return;
    }

    sub set_rtokens {
        my ( $self, $val ) = @_;
        $self->{_rtokens} = $val;
        return;
    }

    sub set_rfields {
        my ( $self, $val ) = @_;
        $self->{_rfields} = $val;
        return;
    }

    sub set_rpatterns {
        my ( $self, $val ) = @_;
        $self->{_rpatterns} = $val;
        return;
    }

    sub set_indentation {
        my ( $self, $val ) = @_;
        $self->{_indentation} = $val;
        return;
    }

    sub set_leading_space_count {
        my ( $self, $val ) = @_;
        $self->{_leading_space_count} = $val;
        return;
    }

    sub set_outdent_long_lines {
        my ( $self, $val ) = @_;
        $self->{_outdent_long_lines} = $val;
        return;
    }

    sub set_list_type {
        my ( $self, $val ) = @_;
        $self->{_list_type} = $val;
        return;
    }

    sub set_is_hanging_side_comment {
        my ( $self, $val ) = @_;
        $self->{_is_hanging_side_comment} = $val;
        return;
    }

    sub set_alignment {
        my ( $self, $j, $val ) = @_;
        $self->{_ralignments}->[$j] = $val;
        return;
    }

}

1;

