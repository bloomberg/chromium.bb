#####################################################################
#
# The Perl::Tidy::Logger class writes the .LOG and .ERR files
#
#####################################################################

package Perl::Tidy::Logger;
use strict;
use warnings;
our $VERSION = '20181120';

sub new {

    my ( $class, $rOpts, $log_file, $warning_file, $fh_stderr, $saw_extrude ) =
      @_;

    my $fh_warnings = $rOpts->{'standard-error-output'} ? $fh_stderr : undef;

    # remove any old error output file if we might write a new one
    unless ( $fh_warnings || ref($warning_file) ) {
        if ( -e $warning_file ) {
            unlink($warning_file)
              or Perl::Tidy::Die(
                "couldn't unlink warning file $warning_file: $!\n");
        }
    }

    my $logfile_gap =
      defined( $rOpts->{'logfile-gap'} )
      ? $rOpts->{'logfile-gap'}
      : 50;
    if ( $logfile_gap == 0 ) { $logfile_gap = 1 }

    return bless {
        _log_file                      => $log_file,
        _logfile_gap                   => $logfile_gap,
        _rOpts                         => $rOpts,
        _fh_warnings                   => $fh_warnings,
        _last_input_line_written       => 0,
        _at_end_of_file                => 0,
        _use_prefix                    => 1,
        _block_log_output              => 0,
        _line_of_tokens                => undef,
        _output_line_number            => undef,
        _wrote_line_information_string => 0,
        _wrote_column_headings         => 0,
        _warning_file                  => $warning_file,
        _warning_count                 => 0,
        _complaint_count               => 0,
        _saw_code_bug    => -1,             # -1=no 0=maybe 1=for sure
        _saw_brace_error => 0,
        _saw_extrude     => $saw_extrude,
        _output_array    => [],
    }, $class;
}

sub get_warning_count {
    my $self = shift;
    return $self->{_warning_count};
}

sub get_use_prefix {
    my $self = shift;
    return $self->{_use_prefix};
}

sub block_log_output {
    my $self = shift;
    $self->{_block_log_output} = 1;
    return;
}

sub unblock_log_output {
    my $self = shift;
    $self->{_block_log_output} = 0;
    return;
}

sub interrupt_logfile {
    my $self = shift;
    $self->{_use_prefix} = 0;
    $self->warning("\n");
    $self->write_logfile_entry( '#' x 24 . "  WARNING  " . '#' x 25 . "\n" );
    return;
}

sub resume_logfile {
    my $self = shift;
    $self->write_logfile_entry( '#' x 60 . "\n" );
    $self->{_use_prefix} = 1;
    return;
}

sub we_are_at_the_last_line {
    my $self = shift;
    unless ( $self->{_wrote_line_information_string} ) {
        $self->write_logfile_entry("Last line\n\n");
    }
    $self->{_at_end_of_file} = 1;
    return;
}

# record some stuff in case we go down in flames
sub black_box {
    my ( $self, $line_of_tokens, $output_line_number ) = @_;
    my $input_line        = $line_of_tokens->{_line_text};
    my $input_line_number = $line_of_tokens->{_line_number};

    # save line information in case we have to write a logfile message
    $self->{_line_of_tokens}                = $line_of_tokens;
    $self->{_output_line_number}            = $output_line_number;
    $self->{_wrote_line_information_string} = 0;

    my $last_input_line_written = $self->{_last_input_line_written};
    my $rOpts                   = $self->{_rOpts};
    if (
        (
            ( $input_line_number - $last_input_line_written ) >=
            $self->{_logfile_gap}
        )
        || ( $input_line =~ /^\s*(sub|package)\s+(\w+)/ )
      )
    {
        my $structural_indentation_level = $line_of_tokens->{_level_0};
        $structural_indentation_level = 0
          if ( $structural_indentation_level < 0 );
        $self->{_last_input_line_written} = $input_line_number;
        ( my $out_str = $input_line ) =~ s/^\s*//;
        chomp $out_str;

        $out_str = ( '.' x $structural_indentation_level ) . $out_str;

        if ( length($out_str) > 35 ) {
            $out_str = substr( $out_str, 0, 35 ) . " ....";
        }
        $self->logfile_output( "", "$out_str\n" );
    }
    return;
}

sub write_logfile_entry {

    my ( $self, @msg ) = @_;

    # add leading >>> to avoid confusing error messages and code
    $self->logfile_output( ">>>", "@msg" );
    return;
}

sub write_column_headings {
    my $self = shift;

    $self->{_wrote_column_headings} = 1;
    my $routput_array = $self->{_output_array};
    push @{$routput_array}, <<EOM;
The nesting depths in the table below are at the start of the lines.
The indicated output line numbers are not always exact.
ci = levels of continuation indentation; bk = 1 if in BLOCK, 0 if not.

in:out indent c b  nesting   code + messages; (messages begin with >>>)
lines  levels i k            (code begins with one '.' per indent level)
------  ----- - - --------   -------------------------------------------
EOM
    return;
}

sub make_line_information_string {

    # make columns of information when a logfile message needs to go out
    my $self                    = shift;
    my $line_of_tokens          = $self->{_line_of_tokens};
    my $input_line_number       = $line_of_tokens->{_line_number};
    my $line_information_string = "";
    if ($input_line_number) {

        my $output_line_number   = $self->{_output_line_number};
        my $brace_depth          = $line_of_tokens->{_curly_brace_depth};
        my $paren_depth          = $line_of_tokens->{_paren_depth};
        my $square_bracket_depth = $line_of_tokens->{_square_bracket_depth};
        my $guessed_indentation_level =
          $line_of_tokens->{_guessed_indentation_level};
        ##my $rtoken_array = $line_of_tokens->{_rtoken_array};

        my $structural_indentation_level = $line_of_tokens->{_level_0};

        $self->write_column_headings() unless $self->{_wrote_column_headings};

        # keep logfile columns aligned for scripts up to 999 lines;
        # for longer scripts it doesn't really matter
        my $extra_space = "";
        $extra_space .=
            ( $input_line_number < 10 )  ? "  "
          : ( $input_line_number < 100 ) ? " "
          :                                "";
        $extra_space .=
            ( $output_line_number < 10 )  ? "  "
          : ( $output_line_number < 100 ) ? " "
          :                                 "";

        # there are 2 possible nesting strings:
        # the original which looks like this:  (0 [1 {2
        # the new one, which looks like this:  {{[
        # the new one is easier to read, and shows the order, but
        # could be arbitrarily long, so we use it unless it is too long
        my $nesting_string =
          "($paren_depth [$square_bracket_depth {$brace_depth";
        my $nesting_string_new = $line_of_tokens->{_nesting_tokens_0};
        my $ci_level           = $line_of_tokens->{_ci_level_0};
        if ( $ci_level > 9 ) { $ci_level = '*' }
        my $bk = ( $line_of_tokens->{_nesting_blocks_0} =~ /1$/ ) ? '1' : '0';

        if ( length($nesting_string_new) <= 8 ) {
            $nesting_string =
              $nesting_string_new . " " x ( 8 - length($nesting_string_new) );
        }
        $line_information_string =
"L$input_line_number:$output_line_number$extra_space i$guessed_indentation_level:$structural_indentation_level $ci_level $bk $nesting_string";
    }
    return $line_information_string;
}

sub logfile_output {
    my ( $self, $prompt, $msg ) = @_;
    return if ( $self->{_block_log_output} );

    my $routput_array = $self->{_output_array};
    if ( $self->{_at_end_of_file} || !$self->{_use_prefix} ) {
        push @{$routput_array}, "$msg";
    }
    else {
        my $line_information_string = $self->make_line_information_string();
        $self->{_wrote_line_information_string} = 1;

        if ($line_information_string) {
            push @{$routput_array}, "$line_information_string   $prompt$msg";
        }
        else {
            push @{$routput_array}, "$msg";
        }
    }
    return;
}

sub get_saw_brace_error {
    my $self = shift;
    return $self->{_saw_brace_error};
}

sub increment_brace_error {
    my $self = shift;
    $self->{_saw_brace_error}++;
    return;
}

sub brace_warning {
    my ( $self, $msg ) = @_;

    #use constant BRACE_WARNING_LIMIT => 10;
    my $BRACE_WARNING_LIMIT = 10;
    my $saw_brace_error     = $self->{_saw_brace_error};

    if ( $saw_brace_error < $BRACE_WARNING_LIMIT ) {
        $self->warning($msg);
    }
    $saw_brace_error++;
    $self->{_saw_brace_error} = $saw_brace_error;

    if ( $saw_brace_error == $BRACE_WARNING_LIMIT ) {
        $self->warning("No further warnings of this type will be given\n");
    }
    return;
}

sub complain {

    # handle non-critical warning messages based on input flag
    my ( $self, $msg ) = @_;
    my $rOpts = $self->{_rOpts};

    # these appear in .ERR output only if -w flag is used
    if ( $rOpts->{'warning-output'} ) {
        $self->warning($msg);
    }

    # otherwise, they go to the .LOG file
    else {
        $self->{_complaint_count}++;
        $self->write_logfile_entry($msg);
    }
    return;
}

sub warning {

    # report errors to .ERR file (or stdout)
    my ( $self, $msg ) = @_;

    #use constant WARNING_LIMIT => 50;
    my $WARNING_LIMIT = 50;

    my $rOpts = $self->{_rOpts};
    unless ( $rOpts->{'quiet'} ) {

        my $warning_count = $self->{_warning_count};
        my $fh_warnings   = $self->{_fh_warnings};
        if ( !$fh_warnings ) {
            my $warning_file = $self->{_warning_file};
            ( $fh_warnings, my $filename ) =
              Perl::Tidy::streamhandle( $warning_file, 'w' );
            $fh_warnings or Perl::Tidy::Die("couldn't open $filename $!\n");
            Perl::Tidy::Warn("## Please see file $filename\n")
              unless ref($warning_file);
            $self->{_fh_warnings} = $fh_warnings;
            $fh_warnings->print("Perltidy version is $Perl::Tidy::VERSION\n");
        }

        if ( $warning_count < $WARNING_LIMIT ) {
            if ( $self->get_use_prefix() > 0 ) {
                my $input_line_number =
                  Perl::Tidy::Tokenizer::get_input_line_number();
                if ( !defined($input_line_number) ) { $input_line_number = -1 }
                $fh_warnings->print("$input_line_number:\t$msg");
                $self->write_logfile_entry("WARNING: $msg");
            }
            else {
                $fh_warnings->print($msg);
                $self->write_logfile_entry($msg);
            }
        }
        $warning_count++;
        $self->{_warning_count} = $warning_count;

        if ( $warning_count == $WARNING_LIMIT ) {
            $fh_warnings->print("No further warnings will be given\n");
        }
    }
    return;
}

# programming bug codes:
#   -1 = no bug
#    0 = maybe, not sure.
#    1 = definitely
sub report_possible_bug {
    my $self         = shift;
    my $saw_code_bug = $self->{_saw_code_bug};
    $self->{_saw_code_bug} = ( $saw_code_bug < 0 ) ? 0 : $saw_code_bug;
    return;
}

sub report_definite_bug {
    my $self = shift;
    $self->{_saw_code_bug} = 1;
    return;
}

sub ask_user_for_bug_report {

    my ( $self, $infile_syntax_ok, $formatter ) = @_;
    my $saw_code_bug = $self->{_saw_code_bug};
    if ( ( $saw_code_bug == 0 ) && ( $infile_syntax_ok == 1 ) ) {
        $self->warning(<<EOM);

You may have encountered a code bug in perltidy.  If you think so, and
the problem is not listed in the BUGS file at
http://perltidy.sourceforge.net, please report it so that it can be
corrected.  Include the smallest possible script which has the problem,
along with the .LOG file. See the manual pages for contact information.
Thank you!
EOM

    }
    elsif ( $saw_code_bug == 1 ) {
        if ( $self->{_saw_extrude} ) {
            $self->warning(<<EOM);

You may have encountered a bug in perltidy.  However, since you are using the
-extrude option, the problem may be with perl or one of its modules, which have
occasional problems with this type of file.  If you believe that the
problem is with perltidy, and the problem is not listed in the BUGS file at
http://perltidy.sourceforge.net, please report it so that it can be corrected.
Include the smallest possible script which has the problem, along with the .LOG
file. See the manual pages for contact information.
Thank you!
EOM
        }
        else {
            $self->warning(<<EOM);

Oops, you seem to have encountered a bug in perltidy.  Please check the
BUGS file at http://perltidy.sourceforge.net.  If the problem is not
listed there, please report it so that it can be corrected.  Include the
smallest possible script which produces this message, along with the
.LOG file if appropriate.  See the manual pages for contact information.
Your efforts are appreciated.  
Thank you!
EOM
            my $added_semicolon_count = 0;
            eval {
                $added_semicolon_count =
                  $formatter->get_added_semicolon_count();
            };
            if ( $added_semicolon_count > 0 ) {
                $self->warning(<<EOM);

The log file shows that perltidy added $added_semicolon_count semicolons.
Please rerun with -nasc to see if that is the cause of the syntax error.  Even
if that is the problem, please report it so that it can be fixed.
EOM

            }
        }
    }
    return;
}

sub finish {

    # called after all formatting to summarize errors
    my ( $self, $infile_syntax_ok, $formatter ) = @_;

    my $rOpts         = $self->{_rOpts};
    my $warning_count = $self->{_warning_count};
    my $saw_code_bug  = $self->{_saw_code_bug};

    my $save_logfile =
         ( $saw_code_bug == 0 && $infile_syntax_ok == 1 )
      || $saw_code_bug == 1
      || $rOpts->{'logfile'};
    my $log_file = $self->{_log_file};
    if ($warning_count) {
        if ($save_logfile) {
            $self->block_log_output();    # avoid echoing this to the logfile
            $self->warning(
                "The logfile $log_file may contain useful information\n");
            $self->unblock_log_output();
        }

        if ( $self->{_complaint_count} > 0 ) {
            $self->warning(
"To see $self->{_complaint_count} non-critical warnings rerun with -w\n"
            );
        }

        if ( $self->{_saw_brace_error}
            && ( $self->{_logfile_gap} > 1 || !$save_logfile ) )
        {
            $self->warning("To save a full .LOG file rerun with -g\n");
        }
    }
    $self->ask_user_for_bug_report( $infile_syntax_ok, $formatter );

    if ($save_logfile) {
        my $log_file = $self->{_log_file};
        my ( $fh, $filename ) = Perl::Tidy::streamhandle( $log_file, 'w' );
        if ($fh) {
            my $routput_array = $self->{_output_array};
            foreach ( @{$routput_array} ) { $fh->print($_) }
            if ( $log_file ne '-' && !ref $log_file ) {
                eval { $fh->close() };
            }
        }
    }
    return;
}
1;

