#####################################################################
#
# the Perl::Tidy::FileWriter class writes the output file
#
#####################################################################

package Perl::Tidy::FileWriter;
use strict;
use warnings;
our $VERSION = '20181120';

# Maximum number of little messages; probably need not be changed.
my $MAX_NAG_MESSAGES = 6;

sub write_logfile_entry {
    my ( $self, $msg ) = @_;
    my $logger_object = $self->{_logger_object};
    if ($logger_object) {
        $logger_object->write_logfile_entry($msg);
    }
    return;
}

sub new {
    my ( $class, $line_sink_object, $rOpts, $logger_object ) = @_;

    return bless {
        _line_sink_object           => $line_sink_object,
        _logger_object              => $logger_object,
        _rOpts                      => $rOpts,
        _output_line_number         => 1,
        _consecutive_blank_lines    => 0,
        _consecutive_nonblank_lines => 0,
        _first_line_length_error    => 0,
        _max_line_length_error      => 0,
        _last_line_length_error     => 0,
        _first_line_length_error_at => 0,
        _max_line_length_error_at   => 0,
        _last_line_length_error_at  => 0,
        _line_length_error_count    => 0,
        _max_output_line_length     => 0,
        _max_output_line_length_at  => 0,
    }, $class;
}

sub tee_on {
    my $self = shift;
    $self->{_line_sink_object}->tee_on();
    return;
}

sub tee_off {
    my $self = shift;
    $self->{_line_sink_object}->tee_off();
    return;
}

sub get_output_line_number {
    my $self = shift;
    return $self->{_output_line_number};
}

sub decrement_output_line_number {
    my $self = shift;
    $self->{_output_line_number}--;
    return;
}

sub get_consecutive_nonblank_lines {
    my $self = shift;
    return $self->{_consecutive_nonblank_lines};
}

sub reset_consecutive_blank_lines {
    my $self = shift;
    $self->{_consecutive_blank_lines} = 0;
    return;
}

sub want_blank_line {
    my $self = shift;
    unless ( $self->{_consecutive_blank_lines} ) {
        $self->write_blank_code_line();
    }
    return;
}

sub require_blank_code_lines {

    # write out the requested number of blanks regardless of the value of -mbl
    # unless -mbl=0.  This allows extra blank lines to be written for subs and
    # packages even with the default -mbl=1
    my ( $self, $count ) = @_;
    my $need   = $count - $self->{_consecutive_blank_lines};
    my $rOpts  = $self->{_rOpts};
    my $forced = $rOpts->{'maximum-consecutive-blank-lines'} > 0;
    foreach my $i ( 0 .. $need - 1 ) {
        $self->write_blank_code_line($forced);
    }
    return;
}

sub write_blank_code_line {
    my $self   = shift;
    my $forced = shift;
    my $rOpts  = $self->{_rOpts};
    return
      if (!$forced
        && $self->{_consecutive_blank_lines} >=
        $rOpts->{'maximum-consecutive-blank-lines'} );
    $self->{_consecutive_blank_lines}++;
    $self->{_consecutive_nonblank_lines} = 0;
    $self->write_line("\n");
    return;
}

sub write_code_line {
    my $self = shift;
    my $a    = shift;

    if ( $a =~ /^\s*$/ ) {
        my $rOpts = $self->{_rOpts};
        return
          if ( $self->{_consecutive_blank_lines} >=
            $rOpts->{'maximum-consecutive-blank-lines'} );
        $self->{_consecutive_blank_lines}++;
        $self->{_consecutive_nonblank_lines} = 0;
    }
    else {
        $self->{_consecutive_blank_lines} = 0;
        $self->{_consecutive_nonblank_lines}++;
    }
    $self->write_line($a);
    return;
}

sub write_line {
    my ( $self, $a ) = @_;

    # TODO: go through and see if the test is necessary here
    if ( $a =~ /\n$/ ) { $self->{_output_line_number}++; }

    $self->{_line_sink_object}->write_line($a);

    # This calculation of excess line length ignores any internal tabs
    my $rOpts  = $self->{_rOpts};
    my $exceed = length($a) - $rOpts->{'maximum-line-length'} - 1;
    if ( $a =~ /^\t+/g ) {
        $exceed += pos($a) * ( $rOpts->{'indent-columns'} - 1 );
    }

    # Note that we just incremented output line number to future value
    # so we must subtract 1 for current line number
    if ( length($a) > 1 + $self->{_max_output_line_length} ) {
        $self->{_max_output_line_length}    = length($a) - 1;
        $self->{_max_output_line_length_at} = $self->{_output_line_number} - 1;
    }

    if ( $exceed > 0 ) {
        my $output_line_number = $self->{_output_line_number};
        $self->{_last_line_length_error}    = $exceed;
        $self->{_last_line_length_error_at} = $output_line_number - 1;
        if ( $self->{_line_length_error_count} == 0 ) {
            $self->{_first_line_length_error}    = $exceed;
            $self->{_first_line_length_error_at} = $output_line_number - 1;
        }

        if (
            $self->{_last_line_length_error} > $self->{_max_line_length_error} )
        {
            $self->{_max_line_length_error}    = $exceed;
            $self->{_max_line_length_error_at} = $output_line_number - 1;
        }

        if ( $self->{_line_length_error_count} < $MAX_NAG_MESSAGES ) {
            $self->write_logfile_entry(
                "Line length exceeded by $exceed characters\n");
        }
        $self->{_line_length_error_count}++;
    }
    return;
}

sub report_line_length_errors {
    my $self                    = shift;
    my $rOpts                   = $self->{_rOpts};
    my $line_length_error_count = $self->{_line_length_error_count};
    if ( $line_length_error_count == 0 ) {
        $self->write_logfile_entry(
            "No lines exceeded $rOpts->{'maximum-line-length'} characters\n");
        my $max_output_line_length    = $self->{_max_output_line_length};
        my $max_output_line_length_at = $self->{_max_output_line_length_at};
        $self->write_logfile_entry(
"  Maximum output line length was $max_output_line_length at line $max_output_line_length_at\n"
        );

    }
    else {

        my $word = ( $line_length_error_count > 1 ) ? "s" : "";
        $self->write_logfile_entry(
"$line_length_error_count output line$word exceeded $rOpts->{'maximum-line-length'} characters:\n"
        );

        $word = ( $line_length_error_count > 1 ) ? "First" : "";
        my $first_line_length_error    = $self->{_first_line_length_error};
        my $first_line_length_error_at = $self->{_first_line_length_error_at};
        $self->write_logfile_entry(
" $word at line $first_line_length_error_at by $first_line_length_error characters\n"
        );

        if ( $line_length_error_count > 1 ) {
            my $max_line_length_error     = $self->{_max_line_length_error};
            my $max_line_length_error_at  = $self->{_max_line_length_error_at};
            my $last_line_length_error    = $self->{_last_line_length_error};
            my $last_line_length_error_at = $self->{_last_line_length_error_at};
            $self->write_logfile_entry(
" Maximum at line $max_line_length_error_at by $max_line_length_error characters\n"
            );
            $self->write_logfile_entry(
" Last at line $last_line_length_error_at by $last_line_length_error characters\n"
            );
        }
    }
    return;
}
1;

