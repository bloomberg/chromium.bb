########################################################################
#
# the Perl::Tidy::Tokenizer package is essentially a filter which
# reads lines of perl source code from a source object and provides
# corresponding tokenized lines through its get_line() method.  Lines
# flow from the source_object to the caller like this:
#
# source_object --> LineBuffer_object --> Tokenizer -->  calling routine
#   get_line()         get_line()           get_line()     line_of_tokens
#
# The source object can be any object with a get_line() method which
# supplies one line (a character string) perl call.
# The LineBuffer object is created by the Tokenizer.
# The Tokenizer returns a reference to a data structure 'line_of_tokens'
# containing one tokenized line for each call to its get_line() method.
#
# WARNING: This is not a real class yet.  Only one tokenizer my be used.
#
########################################################################

package Perl::Tidy::Tokenizer;
use strict;
use warnings;
our $VERSION = '20181120';

use Perl::Tidy::LineBuffer;

BEGIN {

    # Caution: these debug flags produce a lot of output
    # They should all be 0 except when debugging small scripts

    use constant TOKENIZER_DEBUG_FLAG_EXPECT   => 0;
    use constant TOKENIZER_DEBUG_FLAG_NSCAN    => 0;
    use constant TOKENIZER_DEBUG_FLAG_QUOTE    => 0;
    use constant TOKENIZER_DEBUG_FLAG_SCAN_ID  => 0;
    use constant TOKENIZER_DEBUG_FLAG_TOKENIZE => 0;

    my $debug_warning = sub {
        print STDOUT "TOKENIZER_DEBUGGING with key $_[0]\n";
    };

    TOKENIZER_DEBUG_FLAG_EXPECT   && $debug_warning->('EXPECT');
    TOKENIZER_DEBUG_FLAG_NSCAN    && $debug_warning->('NSCAN');
    TOKENIZER_DEBUG_FLAG_QUOTE    && $debug_warning->('QUOTE');
    TOKENIZER_DEBUG_FLAG_SCAN_ID  && $debug_warning->('SCAN_ID');
    TOKENIZER_DEBUG_FLAG_TOKENIZE && $debug_warning->('TOKENIZE');

}

use Carp;

# PACKAGE VARIABLES for processing an entire FILE.
use vars qw{
  $tokenizer_self

  $last_nonblank_token
  $last_nonblank_type
  $last_nonblank_block_type
  $statement_type
  $in_attribute_list
  $current_package
  $context

  %is_constant
  %is_user_function
  %user_function_prototype
  %is_block_function
  %is_block_list_function
  %saw_function_definition

  $brace_depth
  $paren_depth
  $square_bracket_depth

  @current_depth
  @total_depth
  $total_depth
  @nesting_sequence_number
  @current_sequence_number
  @paren_type
  @paren_semicolon_count
  @paren_structural_type
  @brace_type
  @brace_structural_type
  @brace_context
  @brace_package
  @square_bracket_type
  @square_bracket_structural_type
  @depth_array
  @nested_ternary_flag
  @nested_statement_type
  @starting_line_of_current_depth
};

# GLOBAL CONSTANTS for routines in this package
use vars qw{
  %is_indirect_object_taker
  %is_block_operator
  %expecting_operator_token
  %expecting_operator_types
  %expecting_term_types
  %expecting_term_token
  %is_digraph
  %is_file_test_operator
  %is_trigraph
  %is_tetragraph
  %is_valid_token_type
  %is_keyword
  %is_code_block_token
  %really_want_term
  @opening_brace_names
  @closing_brace_names
  %is_keyword_taking_list
  %is_keyword_taking_optional_args
  %is_q_qq_qw_qx_qr_s_y_tr_m
};

# possible values of operator_expected()
use constant TERM     => -1;
use constant UNKNOWN  => 0;
use constant OPERATOR => 1;

# possible values of context
use constant SCALAR_CONTEXT  => -1;
use constant UNKNOWN_CONTEXT => 0;
use constant LIST_CONTEXT    => 1;

# Maximum number of little messages; probably need not be changed.
use constant MAX_NAG_MESSAGES => 6;

{

    # methods to count instances
    my $_count = 0;
    sub get_count        { return $_count; }
    sub _increment_count { return ++$_count }
    sub _decrement_count { return --$_count }
}

sub DESTROY {
    my $self = shift;
    $self->_decrement_count();
    return;
}

sub new {

    my ( $class, @args ) = @_;

    # Note: 'tabs' and 'indent_columns' are temporary and should be
    # removed asap
    my %defaults = (
        source_object        => undef,
        debugger_object      => undef,
        diagnostics_object   => undef,
        logger_object        => undef,
        starting_level       => undef,
        indent_columns       => 4,
        tabsize              => 8,
        look_for_hash_bang   => 0,
        trim_qw              => 1,
        look_for_autoloader  => 1,
        look_for_selfloader  => 1,
        starting_line_number => 1,
        extended_syntax      => 0,
    );
    my %args = ( %defaults, @args );

    # we are given an object with a get_line() method to supply source lines
    my $source_object = $args{source_object};

    # we create another object with a get_line() and peek_ahead() method
    my $line_buffer_object = Perl::Tidy::LineBuffer->new($source_object);

    # Tokenizer state data is as follows:
    # _rhere_target_list    reference to list of here-doc targets
    # _here_doc_target      the target string for a here document
    # _here_quote_character the type of here-doc quoting (" ' ` or none)
    #                       to determine if interpolation is done
    # _quote_target         character we seek if chasing a quote
    # _line_start_quote     line where we started looking for a long quote
    # _in_here_doc          flag indicating if we are in a here-doc
    # _in_pod               flag set if we are in pod documentation
    # _in_error             flag set if we saw severe error (binary in script)
    # _in_data              flag set if we are in __DATA__ section
    # _in_end               flag set if we are in __END__ section
    # _in_format            flag set if we are in a format description
    # _in_attribute_list    flag telling if we are looking for attributes
    # _in_quote             flag telling if we are chasing a quote
    # _starting_level       indentation level of first line
    # _line_buffer_object   object with get_line() method to supply source code
    # _diagnostics_object   place to write debugging information
    # _unexpected_error_count  error count used to limit output
    # _lower_case_labels_at  line numbers where lower case labels seen
    # _hit_bug		     program bug detected
    $tokenizer_self = {
        _rhere_target_list                  => [],
        _in_here_doc                        => 0,
        _here_doc_target                    => "",
        _here_quote_character               => "",
        _in_data                            => 0,
        _in_end                             => 0,
        _in_format                          => 0,
        _in_error                           => 0,
        _in_pod                             => 0,
        _in_attribute_list                  => 0,
        _in_quote                           => 0,
        _quote_target                       => "",
        _line_start_quote                   => -1,
        _starting_level                     => $args{starting_level},
        _know_starting_level                => defined( $args{starting_level} ),
        _tabsize                            => $args{tabsize},
        _indent_columns                     => $args{indent_columns},
        _look_for_hash_bang                 => $args{look_for_hash_bang},
        _trim_qw                            => $args{trim_qw},
        _continuation_indentation           => $args{continuation_indentation},
        _outdent_labels                     => $args{outdent_labels},
        _last_line_number                   => $args{starting_line_number} - 1,
        _saw_perl_dash_P                    => 0,
        _saw_perl_dash_w                    => 0,
        _saw_use_strict                     => 0,
        _saw_v_string                       => 0,
        _hit_bug                            => 0,
        _look_for_autoloader                => $args{look_for_autoloader},
        _look_for_selfloader                => $args{look_for_selfloader},
        _saw_autoloader                     => 0,
        _saw_selfloader                     => 0,
        _saw_hash_bang                      => 0,
        _saw_end                            => 0,
        _saw_data                           => 0,
        _saw_negative_indentation           => 0,
        _started_tokenizing                 => 0,
        _line_buffer_object                 => $line_buffer_object,
        _debugger_object                    => $args{debugger_object},
        _diagnostics_object                 => $args{diagnostics_object},
        _logger_object                      => $args{logger_object},
        _unexpected_error_count             => 0,
        _started_looking_for_here_target_at => 0,
        _nearly_matched_here_target_at      => undef,
        _line_text                          => "",
        _rlower_case_labels_at              => undef,
        _extended_syntax                    => $args{extended_syntax},
    };

    prepare_for_a_new_file();
    find_starting_indentation_level();

    bless $tokenizer_self, $class;

    # This is not a full class yet, so die if an attempt is made to
    # create more than one object.

    if ( _increment_count() > 1 ) {
        confess
"Attempt to create more than 1 object in $class, which is not a true class yet\n";
    }

    return $tokenizer_self;

}

# interface to Perl::Tidy::Logger routines
sub warning {
    my $msg           = shift;
    my $logger_object = $tokenizer_self->{_logger_object};
    if ($logger_object) {
        $logger_object->warning($msg);
    }
    return;
}

sub complain {
    my $msg           = shift;
    my $logger_object = $tokenizer_self->{_logger_object};
    if ($logger_object) {
        $logger_object->complain($msg);
    }
    return;
}

sub write_logfile_entry {
    my $msg           = shift;
    my $logger_object = $tokenizer_self->{_logger_object};
    if ($logger_object) {
        $logger_object->write_logfile_entry($msg);
    }
    return;
}

sub interrupt_logfile {
    my $logger_object = $tokenizer_self->{_logger_object};
    if ($logger_object) {
        $logger_object->interrupt_logfile();
    }
    return;
}

sub resume_logfile {
    my $logger_object = $tokenizer_self->{_logger_object};
    if ($logger_object) {
        $logger_object->resume_logfile();
    }
    return;
}

sub increment_brace_error {
    my $logger_object = $tokenizer_self->{_logger_object};
    if ($logger_object) {
        $logger_object->increment_brace_error();
    }
    return;
}

sub report_definite_bug {
    $tokenizer_self->{_hit_bug} = 1;
    my $logger_object = $tokenizer_self->{_logger_object};
    if ($logger_object) {
        $logger_object->report_definite_bug();
    }
    return;
}

sub brace_warning {
    my $msg           = shift;
    my $logger_object = $tokenizer_self->{_logger_object};
    if ($logger_object) {
        $logger_object->brace_warning($msg);
    }
    return;
}

sub get_saw_brace_error {
    my $logger_object = $tokenizer_self->{_logger_object};
    if ($logger_object) {
        return $logger_object->get_saw_brace_error();
    }
    else {
        return 0;
    }
}

# interface to Perl::Tidy::Diagnostics routines
sub write_diagnostics {
    my $msg = shift;
    if ( $tokenizer_self->{_diagnostics_object} ) {
        $tokenizer_self->{_diagnostics_object}->write_diagnostics($msg);
    }
    return;
}

sub report_tokenization_errors {

    my $self         = shift;
    my $severe_error = $self->{_in_error};

    my $level = get_indentation_level();
    if ( $level != $tokenizer_self->{_starting_level} ) {
        warning("final indentation level: $level\n");
    }

    check_final_nesting_depths();

    if ( $tokenizer_self->{_look_for_hash_bang}
        && !$tokenizer_self->{_saw_hash_bang} )
    {
        warning(
            "hit EOF without seeing hash-bang line; maybe don't need -x?\n");
    }

    if ( $tokenizer_self->{_in_format} ) {
        warning("hit EOF while in format description\n");
    }

    if ( $tokenizer_self->{_in_pod} ) {

        # Just write log entry if this is after __END__ or __DATA__
        # because this happens to often, and it is not likely to be
        # a parsing error.
        if ( $tokenizer_self->{_saw_data} || $tokenizer_self->{_saw_end} ) {
            write_logfile_entry(
"hit eof while in pod documentation (no =cut seen)\n\tthis can cause trouble with some pod utilities\n"
            );
        }

        else {
            complain(
"hit eof while in pod documentation (no =cut seen)\n\tthis can cause trouble with some pod utilities\n"
            );
        }

    }

    if ( $tokenizer_self->{_in_here_doc} ) {
        $severe_error = 1;
        my $here_doc_target = $tokenizer_self->{_here_doc_target};
        my $started_looking_for_here_target_at =
          $tokenizer_self->{_started_looking_for_here_target_at};
        if ($here_doc_target) {
            warning(
"hit EOF in here document starting at line $started_looking_for_here_target_at with target: $here_doc_target\n"
            );
        }
        else {
            warning(
"hit EOF in here document starting at line $started_looking_for_here_target_at with empty target string\n"
            );
        }
        my $nearly_matched_here_target_at =
          $tokenizer_self->{_nearly_matched_here_target_at};
        if ($nearly_matched_here_target_at) {
            warning(
"NOTE: almost matched at input line $nearly_matched_here_target_at except for whitespace\n"
            );
        }
    }

    if ( $tokenizer_self->{_in_quote} ) {
        $severe_error = 1;
        my $line_start_quote = $tokenizer_self->{_line_start_quote};
        my $quote_target     = $tokenizer_self->{_quote_target};
        my $what =
          ( $tokenizer_self->{_in_attribute_list} )
          ? "attribute list"
          : "quote/pattern";
        warning(
"hit EOF seeking end of $what starting at line $line_start_quote ending in $quote_target\n"
        );
    }

    if ( $tokenizer_self->{_hit_bug} ) {
        $severe_error = 1;
    }

    my $logger_object = $tokenizer_self->{_logger_object};

# TODO: eventually may want to activate this to cause file to be output verbatim
    if (0) {

        # Set the severe error for a fairly high warning count because
        # some of the warnings do not harm formatting, such as duplicate
        # sub names.
        my $warning_count = $logger_object->{_warning_count};
        if ( $warning_count > 50 ) {
            $severe_error = 1;
        }

        # Brace errors are significant, so set the severe error flag at
        # a low number.
        my $saw_brace_error = $logger_object->{_saw_brace_error};
        if ( $saw_brace_error > 2 ) {
            $severe_error = 1;
        }
    }

    unless ( $tokenizer_self->{_saw_perl_dash_w} ) {
        if ( $] < 5.006 ) {
            write_logfile_entry("Suggest including '-w parameter'\n");
        }
        else {
            write_logfile_entry("Suggest including 'use warnings;'\n");
        }
    }

    if ( $tokenizer_self->{_saw_perl_dash_P} ) {
        write_logfile_entry("Use of -P parameter for defines is discouraged\n");
    }

    unless ( $tokenizer_self->{_saw_use_strict} ) {
        write_logfile_entry("Suggest including 'use strict;'\n");
    }

    # it is suggested that labels have at least one upper case character
    # for legibility and to avoid code breakage as new keywords are introduced
    if ( $tokenizer_self->{_rlower_case_labels_at} ) {
        my @lower_case_labels_at =
          @{ $tokenizer_self->{_rlower_case_labels_at} };
        write_logfile_entry(
            "Suggest using upper case characters in label(s)\n");
        local $" = ')(';
        write_logfile_entry("  defined at line(s): (@lower_case_labels_at)\n");
    }
    return $severe_error;
}

sub report_v_string {

    # warn if this version can't handle v-strings
    my $tok = shift;
    unless ( $tokenizer_self->{_saw_v_string} ) {
        $tokenizer_self->{_saw_v_string} = $tokenizer_self->{_last_line_number};
    }
    if ( $] < 5.006 ) {
        warning(
"Found v-string '$tok' but v-strings are not implemented in your version of perl; see Camel 3 book ch 2\n"
        );
    }
    return;
}

sub get_input_line_number {
    return $tokenizer_self->{_last_line_number};
}

# returns the next tokenized line
sub get_line {

    my $self = shift;

    # USES GLOBAL VARIABLES: $tokenizer_self, $brace_depth,
    # $square_bracket_depth, $paren_depth

    my $input_line = $tokenizer_self->{_line_buffer_object}->get_line();
    $tokenizer_self->{_line_text} = $input_line;

    return unless ($input_line);

    my $input_line_number = ++$tokenizer_self->{_last_line_number};

    # Find and remove what characters terminate this line, including any
    # control r
    my $input_line_separator = "";
    if ( chomp($input_line) ) { $input_line_separator = $/ }

    # TODO: what other characters should be included here?
    if ( $input_line =~ s/((\r|\035|\032)+)$// ) {
        $input_line_separator = $2 . $input_line_separator;
    }

    # for backwards compatibility we keep the line text terminated with
    # a newline character
    $input_line .= "\n";
    $tokenizer_self->{_line_text} = $input_line;    # update

    # create a data structure describing this line which will be
    # returned to the caller.

    # _line_type codes are:
    #   SYSTEM         - system-specific code before hash-bang line
    #   CODE           - line of perl code (including comments)
    #   POD_START      - line starting pod, such as '=head'
    #   POD            - pod documentation text
    #   POD_END        - last line of pod section, '=cut'
    #   HERE           - text of here-document
    #   HERE_END       - last line of here-doc (target word)
    #   FORMAT         - format section
    #   FORMAT_END     - last line of format section, '.'
    #   DATA_START     - __DATA__ line
    #   DATA           - unidentified text following __DATA__
    #   END_START      - __END__ line
    #   END            - unidentified text following __END__
    #   ERROR          - we are in big trouble, probably not a perl script

    # Other variables:
    #   _curly_brace_depth     - depth of curly braces at start of line
    #   _square_bracket_depth  - depth of square brackets at start of line
    #   _paren_depth           - depth of parens at start of line
    #   _starting_in_quote     - this line continues a multi-line quote
    #                            (so don't trim leading blanks!)
    #   _ending_in_quote       - this line ends in a multi-line quote
    #                            (so don't trim trailing blanks!)
    my $line_of_tokens = {
        _line_type                 => 'EOF',
        _line_text                 => $input_line,
        _line_number               => $input_line_number,
        _rtoken_type               => undef,
        _rtokens                   => undef,
        _rlevels                   => undef,
        _rslevels                  => undef,
        _rblock_type               => undef,
        _rcontainer_type           => undef,
        _rcontainer_environment    => undef,
        _rtype_sequence            => undef,
        _rnesting_tokens           => undef,
        _rci_levels                => undef,
        _rnesting_blocks           => undef,
        _guessed_indentation_level => 0,
        _starting_in_quote    => 0,                    # to be set by subroutine
        _ending_in_quote      => 0,
        _curly_brace_depth    => $brace_depth,
        _square_bracket_depth => $square_bracket_depth,
        _paren_depth          => $paren_depth,
        _quote_character      => '',
    };

    # must print line unchanged if we are in a here document
    if ( $tokenizer_self->{_in_here_doc} ) {

        $line_of_tokens->{_line_type} = 'HERE';
        my $here_doc_target      = $tokenizer_self->{_here_doc_target};
        my $here_quote_character = $tokenizer_self->{_here_quote_character};
        my $candidate_target     = $input_line;
        chomp $candidate_target;

        # Handle <<~ targets, which are indicated here by a leading space on
        # the here quote character
        if ( $here_quote_character =~ /^\s/ ) {
            $candidate_target =~ s/^\s*//;
        }
        if ( $candidate_target eq $here_doc_target ) {
            $tokenizer_self->{_nearly_matched_here_target_at} = undef;
            $line_of_tokens->{_line_type}                     = 'HERE_END';
            write_logfile_entry("Exiting HERE document $here_doc_target\n");

            my $rhere_target_list = $tokenizer_self->{_rhere_target_list};
            if ( @{$rhere_target_list} ) {  # there can be multiple here targets
                ( $here_doc_target, $here_quote_character ) =
                  @{ shift @{$rhere_target_list} };
                $tokenizer_self->{_here_doc_target} = $here_doc_target;
                $tokenizer_self->{_here_quote_character} =
                  $here_quote_character;
                write_logfile_entry(
                    "Entering HERE document $here_doc_target\n");
                $tokenizer_self->{_nearly_matched_here_target_at} = undef;
                $tokenizer_self->{_started_looking_for_here_target_at} =
                  $input_line_number;
            }
            else {
                $tokenizer_self->{_in_here_doc}          = 0;
                $tokenizer_self->{_here_doc_target}      = "";
                $tokenizer_self->{_here_quote_character} = "";
            }
        }

        # check for error of extra whitespace
        # note for PERL6: leading whitespace is allowed
        else {
            $candidate_target =~ s/\s*$//;
            $candidate_target =~ s/^\s*//;
            if ( $candidate_target eq $here_doc_target ) {
                $tokenizer_self->{_nearly_matched_here_target_at} =
                  $input_line_number;
            }
        }
        return $line_of_tokens;
    }

    # must print line unchanged if we are in a format section
    elsif ( $tokenizer_self->{_in_format} ) {

        if ( $input_line =~ /^\.[\s#]*$/ ) {
            write_logfile_entry("Exiting format section\n");
            $tokenizer_self->{_in_format} = 0;
            $line_of_tokens->{_line_type} = 'FORMAT_END';
        }
        else {
            $line_of_tokens->{_line_type} = 'FORMAT';
        }
        return $line_of_tokens;
    }

    # must print line unchanged if we are in pod documentation
    elsif ( $tokenizer_self->{_in_pod} ) {

        $line_of_tokens->{_line_type} = 'POD';
        if ( $input_line =~ /^=cut/ ) {
            $line_of_tokens->{_line_type} = 'POD_END';
            write_logfile_entry("Exiting POD section\n");
            $tokenizer_self->{_in_pod} = 0;
        }
        if ( $input_line =~ /^\#\!.*perl\b/ ) {
            warning(
                "Hash-bang in pod can cause older versions of perl to fail! \n"
            );
        }

        return $line_of_tokens;
    }

    # must print line unchanged if we have seen a severe error (i.e., we
    # are seeing illegal tokens and cannot continue.  Syntax errors do
    # not pass this route).  Calling routine can decide what to do, but
    # the default can be to just pass all lines as if they were after __END__
    elsif ( $tokenizer_self->{_in_error} ) {
        $line_of_tokens->{_line_type} = 'ERROR';
        return $line_of_tokens;
    }

    # print line unchanged if we are __DATA__ section
    elsif ( $tokenizer_self->{_in_data} ) {

        # ...but look for POD
        # Note that the _in_data and _in_end flags remain set
        # so that we return to that state after seeing the
        # end of a pod section
        if ( $input_line =~ /^=(?!cut)/ ) {
            $line_of_tokens->{_line_type} = 'POD_START';
            write_logfile_entry("Entering POD section\n");
            $tokenizer_self->{_in_pod} = 1;
            return $line_of_tokens;
        }
        else {
            $line_of_tokens->{_line_type} = 'DATA';
            return $line_of_tokens;
        }
    }

    # print line unchanged if we are in __END__ section
    elsif ( $tokenizer_self->{_in_end} ) {

        # ...but look for POD
        # Note that the _in_data and _in_end flags remain set
        # so that we return to that state after seeing the
        # end of a pod section
        if ( $input_line =~ /^=(?!cut)/ ) {
            $line_of_tokens->{_line_type} = 'POD_START';
            write_logfile_entry("Entering POD section\n");
            $tokenizer_self->{_in_pod} = 1;
            return $line_of_tokens;
        }
        else {
            $line_of_tokens->{_line_type} = 'END';
            return $line_of_tokens;
        }
    }

    # check for a hash-bang line if we haven't seen one
    if ( !$tokenizer_self->{_saw_hash_bang} ) {
        if ( $input_line =~ /^\#\!.*perl\b/ ) {
            $tokenizer_self->{_saw_hash_bang} = $input_line_number;

            # check for -w and -P flags
            if ( $input_line =~ /^\#\!.*perl\s.*-.*P/ ) {
                $tokenizer_self->{_saw_perl_dash_P} = 1;
            }

            if ( $input_line =~ /^\#\!.*perl\s.*-.*w/ ) {
                $tokenizer_self->{_saw_perl_dash_w} = 1;
            }

            if (
                ( $input_line_number > 1 )

                # leave any hash bang in a BEGIN block alone
                # i.e. see 'debugger-duck_type.t'
                && !(
                       $last_nonblank_block_type
                    && $last_nonblank_block_type eq 'BEGIN'
                )
                && ( !$tokenizer_self->{_look_for_hash_bang} )
              )
            {

                # this is helpful for VMS systems; we may have accidentally
                # tokenized some DCL commands
                if ( $tokenizer_self->{_started_tokenizing} ) {
                    warning(
"There seems to be a hash-bang after line 1; do you need to run with -x ?\n"
                    );
                }
                else {
                    complain("Useless hash-bang after line 1\n");
                }
            }

            # Report the leading hash-bang as a system line
            # This will prevent -dac from deleting it
            else {
                $line_of_tokens->{_line_type} = 'SYSTEM';
                return $line_of_tokens;
            }
        }
    }

    # wait for a hash-bang before parsing if the user invoked us with -x
    if ( $tokenizer_self->{_look_for_hash_bang}
        && !$tokenizer_self->{_saw_hash_bang} )
    {
        $line_of_tokens->{_line_type} = 'SYSTEM';
        return $line_of_tokens;
    }

    # a first line of the form ': #' will be marked as SYSTEM
    # since lines of this form may be used by tcsh
    if ( $input_line_number == 1 && $input_line =~ /^\s*\:\s*\#/ ) {
        $line_of_tokens->{_line_type} = 'SYSTEM';
        return $line_of_tokens;
    }

    # now we know that it is ok to tokenize the line...
    # the line tokenizer will modify any of these private variables:
    #        _rhere_target_list
    #        _in_data
    #        _in_end
    #        _in_format
    #        _in_error
    #        _in_pod
    #        _in_quote
    my $ending_in_quote_last = $tokenizer_self->{_in_quote};
    tokenize_this_line($line_of_tokens);

    # Now finish defining the return structure and return it
    $line_of_tokens->{_ending_in_quote} = $tokenizer_self->{_in_quote};

    # handle severe error (binary data in script)
    if ( $tokenizer_self->{_in_error} ) {
        $tokenizer_self->{_in_quote} = 0;    # to avoid any more messages
        warning("Giving up after error\n");
        $line_of_tokens->{_line_type} = 'ERROR';
        reset_indentation_level(0);          # avoid error messages
        return $line_of_tokens;
    }

    # handle start of pod documentation
    if ( $tokenizer_self->{_in_pod} ) {

        # This gets tricky..above a __DATA__ or __END__ section, perl
        # accepts '=cut' as the start of pod section. But afterwards,
        # only pod utilities see it and they may ignore an =cut without
        # leading =head.  In any case, this isn't good.
        if ( $input_line =~ /^=cut\b/ ) {
            if ( $tokenizer_self->{_saw_data} || $tokenizer_self->{_saw_end} ) {
                complain("=cut while not in pod ignored\n");
                $tokenizer_self->{_in_pod}    = 0;
                $line_of_tokens->{_line_type} = 'POD_END';
            }
            else {
                $line_of_tokens->{_line_type} = 'POD_START';
                complain(
"=cut starts a pod section .. this can fool pod utilities.\n"
                );
                write_logfile_entry("Entering POD section\n");
            }
        }

        else {
            $line_of_tokens->{_line_type} = 'POD_START';
            write_logfile_entry("Entering POD section\n");
        }

        return $line_of_tokens;
    }

    # update indentation levels for log messages
    if ( $input_line !~ /^\s*$/ ) {
        my $rlevels = $line_of_tokens->{_rlevels};
        $line_of_tokens->{_guessed_indentation_level} =
          guess_old_indentation_level($input_line);
    }

    # see if this line contains here doc targets
    my $rhere_target_list = $tokenizer_self->{_rhere_target_list};
    if ( @{$rhere_target_list} ) {

        my ( $here_doc_target, $here_quote_character ) =
          @{ shift @{$rhere_target_list} };
        $tokenizer_self->{_in_here_doc}          = 1;
        $tokenizer_self->{_here_doc_target}      = $here_doc_target;
        $tokenizer_self->{_here_quote_character} = $here_quote_character;
        write_logfile_entry("Entering HERE document $here_doc_target\n");
        $tokenizer_self->{_started_looking_for_here_target_at} =
          $input_line_number;
    }

    # NOTE: __END__ and __DATA__ statements are written unformatted
    # because they can theoretically contain additional characters
    # which are not tokenized (and cannot be read with <DATA> either!).
    if ( $tokenizer_self->{_in_data} ) {
        $line_of_tokens->{_line_type} = 'DATA_START';
        write_logfile_entry("Starting __DATA__ section\n");
        $tokenizer_self->{_saw_data} = 1;

        # keep parsing after __DATA__ if use SelfLoader was seen
        if ( $tokenizer_self->{_saw_selfloader} ) {
            $tokenizer_self->{_in_data} = 0;
            write_logfile_entry(
                "SelfLoader seen, continuing; -nlsl deactivates\n");
        }

        return $line_of_tokens;
    }

    elsif ( $tokenizer_self->{_in_end} ) {
        $line_of_tokens->{_line_type} = 'END_START';
        write_logfile_entry("Starting __END__ section\n");
        $tokenizer_self->{_saw_end} = 1;

        # keep parsing after __END__ if use AutoLoader was seen
        if ( $tokenizer_self->{_saw_autoloader} ) {
            $tokenizer_self->{_in_end} = 0;
            write_logfile_entry(
                "AutoLoader seen, continuing; -nlal deactivates\n");
        }
        return $line_of_tokens;
    }

    # now, finally, we know that this line is type 'CODE'
    $line_of_tokens->{_line_type} = 'CODE';

    # remember if we have seen any real code
    if (  !$tokenizer_self->{_started_tokenizing}
        && $input_line !~ /^\s*$/
        && $input_line !~ /^\s*#/ )
    {
        $tokenizer_self->{_started_tokenizing} = 1;
    }

    if ( $tokenizer_self->{_debugger_object} ) {
        $tokenizer_self->{_debugger_object}->write_debug_entry($line_of_tokens);
    }

    # Note: if keyword 'format' occurs in this line code, it is still CODE
    # (keyword 'format' need not start a line)
    if ( $tokenizer_self->{_in_format} ) {
        write_logfile_entry("Entering format section\n");
    }

    if ( $tokenizer_self->{_in_quote}
        and ( $tokenizer_self->{_line_start_quote} < 0 ) )
    {

        #if ( ( my $quote_target = get_quote_target() ) !~ /^\s*$/ ) {
        if (
            ( my $quote_target = $tokenizer_self->{_quote_target} ) !~ /^\s*$/ )
        {
            $tokenizer_self->{_line_start_quote} = $input_line_number;
            write_logfile_entry(
                "Start multi-line quote or pattern ending in $quote_target\n");
        }
    }
    elsif ( ( $tokenizer_self->{_line_start_quote} >= 0 )
        && !$tokenizer_self->{_in_quote} )
    {
        $tokenizer_self->{_line_start_quote} = -1;
        write_logfile_entry("End of multi-line quote or pattern\n");
    }

    # we are returning a line of CODE
    return $line_of_tokens;
}

sub find_starting_indentation_level {

    # We need to find the indentation level of the first line of the
    # script being formatted.  Often it will be zero for an entire file,
    # but if we are formatting a local block of code (within an editor for
    # example) it may not be zero.  The user may specify this with the
    # -sil=n parameter but normally doesn't so we have to guess.
    #
    # USES GLOBAL VARIABLES: $tokenizer_self
    my $starting_level = 0;

    # use value if given as parameter
    if ( $tokenizer_self->{_know_starting_level} ) {
        $starting_level = $tokenizer_self->{_starting_level};
    }

    # if we know there is a hash_bang line, the level must be zero
    elsif ( $tokenizer_self->{_look_for_hash_bang} ) {
        $tokenizer_self->{_know_starting_level} = 1;
    }

    # otherwise figure it out from the input file
    else {
        my $line;
        my $i = 0;

        # keep looking at lines until we find a hash bang or piece of code
        my $msg = "";
        while ( $line =
            $tokenizer_self->{_line_buffer_object}->peek_ahead( $i++ ) )
        {

            # if first line is #! then assume starting level is zero
            if ( $i == 1 && $line =~ /^\#\!/ ) {
                $starting_level = 0;
                last;
            }
            next if ( $line =~ /^\s*#/ );    # skip past comments
            next if ( $line =~ /^\s*$/ );    # skip past blank lines
            $starting_level = guess_old_indentation_level($line);
            last;
        }
        $msg = "Line $i implies starting-indentation-level = $starting_level\n";
        write_logfile_entry("$msg");
    }
    $tokenizer_self->{_starting_level} = $starting_level;
    reset_indentation_level($starting_level);
    return;
}

sub guess_old_indentation_level {
    my ($line) = @_;

    # Guess the indentation level of an input line.
    #
    # For the first line of code this result will define the starting
    # indentation level.  It will mainly be non-zero when perltidy is applied
    # within an editor to a local block of code.
    #
    # This is an impossible task in general because we can't know what tabs
    # meant for the old script and how many spaces were used for one
    # indentation level in the given input script.  For example it may have
    # been previously formatted with -i=7 -et=3.  But we can at least try to
    # make sure that perltidy guesses correctly if it is applied repeatedly to
    # a block of code within an editor, so that the block stays at the same
    # level when perltidy is applied repeatedly.
    #
    # USES GLOBAL VARIABLES: $tokenizer_self
    my $level = 0;

    # find leading tabs, spaces, and any statement label
    my $spaces = 0;
    if ( $line =~ /^(\t+)?(\s+)?(\w+:[^:])?/ ) {

        # If there are leading tabs, we use the tab scheme for this run, if
        # any, so that the code will remain stable when editing.
        if ($1) { $spaces += length($1) * $tokenizer_self->{_tabsize} }

        if ($2) { $spaces += length($2) }

        # correct for outdented labels
        if ( $3 && $tokenizer_self->{'_outdent_labels'} ) {
            $spaces += $tokenizer_self->{_continuation_indentation};
        }
    }

    # compute indentation using the value of -i for this run.
    # If -i=0 is used for this run (which is possible) it doesn't matter
    # what we do here but we'll guess that the old run used 4 spaces per level.
    my $indent_columns = $tokenizer_self->{_indent_columns};
    $indent_columns = 4 if ( !$indent_columns );
    $level          = int( $spaces / $indent_columns );
    return ($level);
}

# This is a currently unused debug routine
sub dump_functions {

    my $fh = *STDOUT;
    foreach my $pkg ( keys %is_user_function ) {
        print $fh "\nnon-constant subs in package $pkg\n";

        foreach my $sub ( keys %{ $is_user_function{$pkg} } ) {
            my $msg = "";
            if ( $is_block_list_function{$pkg}{$sub} ) {
                $msg = 'block_list';
            }

            if ( $is_block_function{$pkg}{$sub} ) {
                $msg = 'block';
            }
            print $fh "$sub $msg\n";
        }
    }

    foreach my $pkg ( keys %is_constant ) {
        print $fh "\nconstants and constant subs in package $pkg\n";

        foreach my $sub ( keys %{ $is_constant{$pkg} } ) {
            print $fh "$sub\n";
        }
    }
    return;
}

sub ones_count {

    # count number of 1's in a string of 1's and 0's
    # example: ones_count("010101010101") gives 6
    my $str = shift;
    return $str =~ tr/1/0/;
}

sub prepare_for_a_new_file {

    # previous tokens needed to determine what to expect next
    $last_nonblank_token      = ';';    # the only possible starting state which
    $last_nonblank_type       = ';';    # will make a leading brace a code block
    $last_nonblank_block_type = '';

    # scalars for remembering statement types across multiple lines
    $statement_type    = '';            # '' or 'use' or 'sub..' or 'case..'
    $in_attribute_list = 0;

    # scalars for remembering where we are in the file
    $current_package = "main";
    $context         = UNKNOWN_CONTEXT;

    # hashes used to remember function information
    %is_constant             = ();      # user-defined constants
    %is_user_function        = ();      # user-defined functions
    %user_function_prototype = ();      # their prototypes
    %is_block_function       = ();
    %is_block_list_function  = ();
    %saw_function_definition = ();

    # variables used to track depths of various containers
    # and report nesting errors
    $paren_depth          = 0;
    $brace_depth          = 0;
    $square_bracket_depth = 0;
    @current_depth[ 0 .. $#closing_brace_names ] =
      (0) x scalar @closing_brace_names;
    $total_depth = 0;
    @total_depth = ();
    @nesting_sequence_number[ 0 .. $#closing_brace_names ] =
      ( 0 .. $#closing_brace_names );
    @current_sequence_number             = ();
    $paren_type[$paren_depth]            = '';
    $paren_semicolon_count[$paren_depth] = 0;
    $paren_structural_type[$brace_depth] = '';
    $brace_type[$brace_depth] = ';';    # identify opening brace as code block
    $brace_structural_type[$brace_depth]                   = '';
    $brace_context[$brace_depth]                           = UNKNOWN_CONTEXT;
    $brace_package[$paren_depth]                           = $current_package;
    $square_bracket_type[$square_bracket_depth]            = '';
    $square_bracket_structural_type[$square_bracket_depth] = '';

    initialize_tokenizer_state();
    return;
}

{                                       # begin tokenize_this_line

    use constant BRACE          => 0;
    use constant SQUARE_BRACKET => 1;
    use constant PAREN          => 2;
    use constant QUESTION_COLON => 3;

    # TV1: scalars for processing one LINE.
    # Re-initialized on each entry to sub tokenize_this_line.
    my (
        $block_type,        $container_type,    $expecting,
        $i,                 $i_tok,             $input_line,
        $input_line_number, $last_nonblank_i,   $max_token_index,
        $next_tok,          $next_type,         $peeked_ahead,
        $prototype,         $rhere_target_list, $rtoken_map,
        $rtoken_type,       $rtokens,           $tok,
        $type,              $type_sequence,     $indent_flag,
    );

    # TV2: refs to ARRAYS for processing one LINE
    # Re-initialized on each call.
    my $routput_token_list     = [];    # stack of output token indexes
    my $routput_token_type     = [];    # token types
    my $routput_block_type     = [];    # types of code block
    my $routput_container_type = [];    # paren types, such as if, elsif, ..
    my $routput_type_sequence  = [];    # nesting sequential number
    my $routput_indent_flag    = [];    #

    # TV3: SCALARS for quote variables.  These are initialized with a
    # subroutine call and continually updated as lines are processed.
    my ( $in_quote, $quote_type, $quote_character, $quote_pos, $quote_depth,
        $quoted_string_1, $quoted_string_2, $allowed_quote_modifiers, );

    # TV4: SCALARS for multi-line identifiers and
    # statements. These are initialized with a subroutine call
    # and continually updated as lines are processed.
    my ( $id_scan_state, $identifier, $want_paren, $indented_if_level );

    # TV5: SCALARS for tracking indentation level.
    # Initialized once and continually updated as lines are
    # processed.
    my (
        $nesting_token_string,      $nesting_type_string,
        $nesting_block_string,      $nesting_block_flag,
        $nesting_list_string,       $nesting_list_flag,
        $ci_string_in_tokenizer,    $continuation_string_in_tokenizer,
        $in_statement_continuation, $level_in_tokenizer,
        $slevel_in_tokenizer,       $rslevel_stack,
    );

    # TV6: SCALARS for remembering several previous
    # tokens. Initialized once and continually updated as
    # lines are processed.
    my (
        $last_nonblank_container_type,     $last_nonblank_type_sequence,
        $last_last_nonblank_token,         $last_last_nonblank_type,
        $last_last_nonblank_block_type,    $last_last_nonblank_container_type,
        $last_last_nonblank_type_sequence, $last_nonblank_prototype,
    );

    # ----------------------------------------------------------------
    # beginning of tokenizer variable access and manipulation routines
    # ----------------------------------------------------------------

    sub initialize_tokenizer_state {

        # TV1: initialized on each call
        # TV2: initialized on each call
        # TV3:
        $in_quote                = 0;
        $quote_type              = 'Q';
        $quote_character         = "";
        $quote_pos               = 0;
        $quote_depth             = 0;
        $quoted_string_1         = "";
        $quoted_string_2         = "";
        $allowed_quote_modifiers = "";

        # TV4:
        $id_scan_state     = '';
        $identifier        = '';
        $want_paren        = "";
        $indented_if_level = 0;

        # TV5:
        $nesting_token_string             = "";
        $nesting_type_string              = "";
        $nesting_block_string             = '1';    # initially in a block
        $nesting_block_flag               = 1;
        $nesting_list_string              = '0';    # initially not in a list
        $nesting_list_flag                = 0;      # initially not in a list
        $ci_string_in_tokenizer           = "";
        $continuation_string_in_tokenizer = "0";
        $in_statement_continuation        = 0;
        $level_in_tokenizer               = 0;
        $slevel_in_tokenizer              = 0;
        $rslevel_stack                    = [];

        # TV6:
        $last_nonblank_container_type      = '';
        $last_nonblank_type_sequence       = '';
        $last_last_nonblank_token          = ';';
        $last_last_nonblank_type           = ';';
        $last_last_nonblank_block_type     = '';
        $last_last_nonblank_container_type = '';
        $last_last_nonblank_type_sequence  = '';
        $last_nonblank_prototype           = "";
        return;
    }

    sub save_tokenizer_state {

        my $rTV1 = [
            $block_type,        $container_type,    $expecting,
            $i,                 $i_tok,             $input_line,
            $input_line_number, $last_nonblank_i,   $max_token_index,
            $next_tok,          $next_type,         $peeked_ahead,
            $prototype,         $rhere_target_list, $rtoken_map,
            $rtoken_type,       $rtokens,           $tok,
            $type,              $type_sequence,     $indent_flag,
        ];

        my $rTV2 = [
            $routput_token_list,    $routput_token_type,
            $routput_block_type,    $routput_container_type,
            $routput_type_sequence, $routput_indent_flag,
        ];

        my $rTV3 = [
            $in_quote,        $quote_type,
            $quote_character, $quote_pos,
            $quote_depth,     $quoted_string_1,
            $quoted_string_2, $allowed_quote_modifiers,
        ];

        my $rTV4 =
          [ $id_scan_state, $identifier, $want_paren, $indented_if_level ];

        my $rTV5 = [
            $nesting_token_string,      $nesting_type_string,
            $nesting_block_string,      $nesting_block_flag,
            $nesting_list_string,       $nesting_list_flag,
            $ci_string_in_tokenizer,    $continuation_string_in_tokenizer,
            $in_statement_continuation, $level_in_tokenizer,
            $slevel_in_tokenizer,       $rslevel_stack,
        ];

        my $rTV6 = [
            $last_nonblank_container_type,
            $last_nonblank_type_sequence,
            $last_last_nonblank_token,
            $last_last_nonblank_type,
            $last_last_nonblank_block_type,
            $last_last_nonblank_container_type,
            $last_last_nonblank_type_sequence,
            $last_nonblank_prototype,
        ];
        return [ $rTV1, $rTV2, $rTV3, $rTV4, $rTV5, $rTV6 ];
    }

    sub restore_tokenizer_state {
        my ($rstate) = @_;
        my ( $rTV1, $rTV2, $rTV3, $rTV4, $rTV5, $rTV6 ) = @{$rstate};
        (
            $block_type,        $container_type,    $expecting,
            $i,                 $i_tok,             $input_line,
            $input_line_number, $last_nonblank_i,   $max_token_index,
            $next_tok,          $next_type,         $peeked_ahead,
            $prototype,         $rhere_target_list, $rtoken_map,
            $rtoken_type,       $rtokens,           $tok,
            $type,              $type_sequence,     $indent_flag,
        ) = @{$rTV1};

        (
            $routput_token_list,    $routput_token_type,
            $routput_block_type,    $routput_container_type,
            $routput_type_sequence, $routput_type_sequence,
        ) = @{$rTV2};

        (
            $in_quote, $quote_type, $quote_character, $quote_pos, $quote_depth,
            $quoted_string_1, $quoted_string_2, $allowed_quote_modifiers,
        ) = @{$rTV3};

        ( $id_scan_state, $identifier, $want_paren, $indented_if_level ) =
          @{$rTV4};

        (
            $nesting_token_string,      $nesting_type_string,
            $nesting_block_string,      $nesting_block_flag,
            $nesting_list_string,       $nesting_list_flag,
            $ci_string_in_tokenizer,    $continuation_string_in_tokenizer,
            $in_statement_continuation, $level_in_tokenizer,
            $slevel_in_tokenizer,       $rslevel_stack,
        ) = @{$rTV5};

        (
            $last_nonblank_container_type,
            $last_nonblank_type_sequence,
            $last_last_nonblank_token,
            $last_last_nonblank_type,
            $last_last_nonblank_block_type,
            $last_last_nonblank_container_type,
            $last_last_nonblank_type_sequence,
            $last_nonblank_prototype,
        ) = @{$rTV6};
        return;
    }

    sub get_indentation_level {

        # patch to avoid reporting error if indented if is not terminated
        if ($indented_if_level) { return $level_in_tokenizer - 1 }
        return $level_in_tokenizer;
    }

    sub reset_indentation_level {
        $level_in_tokenizer = $slevel_in_tokenizer = shift;
        push @{$rslevel_stack}, $slevel_in_tokenizer;
        return;
    }

    sub peeked_ahead {
        my $flag = shift;
        $peeked_ahead = defined($flag) ? $flag : $peeked_ahead;
        return $peeked_ahead;
    }

    # ------------------------------------------------------------
    # end of tokenizer variable access and manipulation routines
    # ------------------------------------------------------------

    # ------------------------------------------------------------
    # beginning of various scanner interface routines
    # ------------------------------------------------------------
    sub scan_replacement_text {

        # check for here-docs in replacement text invoked by
        # a substitution operator with executable modifier 'e'.
        #
        # given:
        #  $replacement_text
        # return:
        #  $rht = reference to any here-doc targets
        my ($replacement_text) = @_;

        # quick check
        return unless ( $replacement_text =~ /<</ );

        write_logfile_entry("scanning replacement text for here-doc targets\n");

        # save the logger object for error messages
        my $logger_object = $tokenizer_self->{_logger_object};

        # localize all package variables
        local (
            $tokenizer_self,                 $last_nonblank_token,
            $last_nonblank_type,             $last_nonblank_block_type,
            $statement_type,                 $in_attribute_list,
            $current_package,                $context,
            %is_constant,                    %is_user_function,
            %user_function_prototype,        %is_block_function,
            %is_block_list_function,         %saw_function_definition,
            $brace_depth,                    $paren_depth,
            $square_bracket_depth,           @current_depth,
            @total_depth,                    $total_depth,
            @nesting_sequence_number,        @current_sequence_number,
            @paren_type,                     @paren_semicolon_count,
            @paren_structural_type,          @brace_type,
            @brace_structural_type,          @brace_context,
            @brace_package,                  @square_bracket_type,
            @square_bracket_structural_type, @depth_array,
            @starting_line_of_current_depth, @nested_ternary_flag,
            @nested_statement_type,
        );

        # save all lexical variables
        my $rstate = save_tokenizer_state();
        _decrement_count();    # avoid error check for multiple tokenizers

        # make a new tokenizer
        my $rOpts = {};
        my $rpending_logfile_message;
        my $source_object =
          Perl::Tidy::LineSource->new( \$replacement_text, $rOpts,
            $rpending_logfile_message );
        my $tokenizer = Perl::Tidy::Tokenizer->new(
            source_object        => $source_object,
            logger_object        => $logger_object,
            starting_line_number => $input_line_number,
        );

        # scan the replacement text
        1 while ( $tokenizer->get_line() );

        # remove any here doc targets
        my $rht = undef;
        if ( $tokenizer_self->{_in_here_doc} ) {
            $rht = [];
            push @{$rht},
              [
                $tokenizer_self->{_here_doc_target},
                $tokenizer_self->{_here_quote_character}
              ];
            if ( $tokenizer_self->{_rhere_target_list} ) {
                push @{$rht}, @{ $tokenizer_self->{_rhere_target_list} };
                $tokenizer_self->{_rhere_target_list} = undef;
            }
            $tokenizer_self->{_in_here_doc} = undef;
        }

        # now its safe to report errors
        my $severe_error = $tokenizer->report_tokenization_errors();

        # TODO: Could propagate a severe error up

        # restore all tokenizer lexical variables
        restore_tokenizer_state($rstate);

        # return the here doc targets
        return $rht;
    }

    sub scan_bare_identifier {
        ( $i, $tok, $type, $prototype ) =
          scan_bare_identifier_do( $input_line, $i, $tok, $type, $prototype,
            $rtoken_map, $max_token_index );
        return;
    }

    sub scan_identifier {
        ( $i, $tok, $type, $id_scan_state, $identifier ) =
          scan_identifier_do( $i, $id_scan_state, $identifier, $rtokens,
            $max_token_index, $expecting, $paren_type[$paren_depth] );
        return;
    }

    sub scan_id {
        ( $i, $tok, $type, $id_scan_state ) =
          scan_id_do( $input_line, $i, $tok, $rtokens, $rtoken_map,
            $id_scan_state, $max_token_index );
        return;
    }

    sub scan_number {
        my $number;
        ( $i, $type, $number ) =
          scan_number_do( $input_line, $i, $rtoken_map, $type,
            $max_token_index );
        return $number;
    }

    # a sub to warn if token found where term expected
    sub error_if_expecting_TERM {
        if ( $expecting == TERM ) {
            if ( $really_want_term{$last_nonblank_type} ) {
                report_unexpected( $tok, "term", $i_tok, $last_nonblank_i,
                    $rtoken_map, $rtoken_type, $input_line );
                return 1;
            }
        }
        return;
    }

    # a sub to warn if token found where operator expected
    sub error_if_expecting_OPERATOR {
        my $thing = shift;
        if ( $expecting == OPERATOR ) {
            if ( !defined($thing) ) { $thing = $tok }
            report_unexpected( $thing, "operator", $i_tok, $last_nonblank_i,
                $rtoken_map, $rtoken_type, $input_line );
            if ( $i_tok == 0 ) {
                interrupt_logfile();
                warning("Missing ';' above?\n");
                resume_logfile();
            }
            return 1;
        }
        return;
    }

    # ------------------------------------------------------------
    # end scanner interfaces
    # ------------------------------------------------------------

    my %is_for_foreach;
    @_ = qw(for foreach);
    @is_for_foreach{@_} = (1) x scalar(@_);

    my %is_my_our;
    @_ = qw(my our);
    @is_my_our{@_} = (1) x scalar(@_);

    # These keywords may introduce blocks after parenthesized expressions,
    # in the form:
    # keyword ( .... ) { BLOCK }
    # patch for SWITCH/CASE: added 'switch' 'case' 'given' 'when'
    my %is_blocktype_with_paren;
    @_ =
      qw(if elsif unless while until for foreach switch case given when catch);
    @is_blocktype_with_paren{@_} = (1) x scalar(@_);

    # ------------------------------------------------------------
    # begin hash of code for handling most token types
    # ------------------------------------------------------------
    my $tokenization_code = {

        # no special code for these types yet, but syntax checks
        # could be added

##      '!'   => undef,
##      '!='  => undef,
##      '!~'  => undef,
##      '%='  => undef,
##      '&&=' => undef,
##      '&='  => undef,
##      '+='  => undef,
##      '-='  => undef,
##      '..'  => undef,
##      '..'  => undef,
##      '...' => undef,
##      '.='  => undef,
##      '<<=' => undef,
##      '<='  => undef,
##      '<=>' => undef,
##      '<>'  => undef,
##      '='   => undef,
##      '=='  => undef,
##      '=~'  => undef,
##      '>='  => undef,
##      '>>'  => undef,
##      '>>=' => undef,
##      '\\'  => undef,
##      '^='  => undef,
##      '|='  => undef,
##      '||=' => undef,
##      '//=' => undef,
##      '~'   => undef,
##      '~~'  => undef,
##      '!~~'  => undef,

        '>' => sub {
            error_if_expecting_TERM()
              if ( $expecting == TERM );
        },
        '|' => sub {
            error_if_expecting_TERM()
              if ( $expecting == TERM );
        },
        '$' => sub {

            # start looking for a scalar
            error_if_expecting_OPERATOR("Scalar")
              if ( $expecting == OPERATOR );
            scan_identifier();

            if ( $identifier eq '$^W' ) {
                $tokenizer_self->{_saw_perl_dash_w} = 1;
            }

            # Check for identifier in indirect object slot
            # (vorboard.pl, sort.t).  Something like:
            #   /^(print|printf|sort|exec|system)$/
            if (
                $is_indirect_object_taker{$last_nonblank_token}

                || ( ( $last_nonblank_token eq '(' )
                    && $is_indirect_object_taker{ $paren_type[$paren_depth] } )
                || ( $last_nonblank_type =~ /^[Uw]$/ )    # possible object
              )
            {
                $type = 'Z';
            }
        },
        '(' => sub {

            ++$paren_depth;
            $paren_semicolon_count[$paren_depth] = 0;
            if ($want_paren) {
                $container_type = $want_paren;
                $want_paren     = "";
            }
            elsif ( $statement_type =~ /^sub\b/ ) {
                $container_type = $statement_type;
            }
            else {
                $container_type = $last_nonblank_token;

                # We can check for a syntax error here of unexpected '(',
                # but this is going to get messy...
                if (
                    $expecting == OPERATOR

                    # be sure this is not a method call of the form
                    # &method(...), $method->(..), &{method}(...),
                    # $ref[2](list) is ok & short for $ref[2]->(list)
                    # NOTE: at present, braces in something like &{ xxx }
                    # are not marked as a block, we might have a method call
                    && $last_nonblank_token !~ /^([\]\}\&]|\-\>)/

                  )
                {

                    # ref: camel 3 p 703.
                    if ( $last_last_nonblank_token eq 'do' ) {
                        complain(
"do SUBROUTINE is deprecated; consider & or -> notation\n"
                        );
                    }
                    else {

                        # if this is an empty list, (), then it is not an
                        # error; for example, we might have a constant pi and
                        # invoke it with pi() or just pi;
                        my ( $next_nonblank_token, $i_next ) =
                          find_next_nonblank_token( $i, $rtokens,
                            $max_token_index );
                        if ( $next_nonblank_token ne ')' ) {
                            my $hint;
                            error_if_expecting_OPERATOR('(');

                            if ( $last_nonblank_type eq 'C' ) {
                                $hint =
                                  "$last_nonblank_token has a void prototype\n";
                            }
                            elsif ( $last_nonblank_type eq 'i' ) {
                                if (   $i_tok > 0
                                    && $last_nonblank_token =~ /^\$/ )
                                {
                                    $hint =
"Do you mean '$last_nonblank_token->(' ?\n";
                                }
                            }
                            if ($hint) {
                                interrupt_logfile();
                                warning($hint);
                                resume_logfile();
                            }
                        } ## end if ( $next_nonblank_token...
                    } ## end else [ if ( $last_last_nonblank_token...
                } ## end if ( $expecting == OPERATOR...
            }
            $paren_type[$paren_depth] = $container_type;
            ( $type_sequence, $indent_flag ) =
              increase_nesting_depth( PAREN, $rtoken_map->[$i_tok] );

            # propagate types down through nested parens
            # for example: the second paren in 'if ((' would be structural
            # since the first is.

            if ( $last_nonblank_token eq '(' ) {
                $type = $last_nonblank_type;
            }

            #     We exclude parens as structural after a ',' because it
            #     causes subtle problems with continuation indentation for
            #     something like this, where the first 'or' will not get
            #     indented.
            #
            #         assert(
            #             __LINE__,
            #             ( not defined $check )
            #               or ref $check
            #               or $check eq "new"
            #               or $check eq "old",
            #         );
            #
            #     Likewise, we exclude parens where a statement can start
            #     because of problems with continuation indentation, like
            #     these:
            #
            #         ($firstline =~ /^#\!.*perl/)
            #         and (print $File::Find::name, "\n")
            #           and (return 1);
            #
            #         (ref($usage_fref) =~ /CODE/)
            #         ? &$usage_fref
            #           : (&blast_usage, &blast_params, &blast_general_params);

            else {
                $type = '{';
            }

            if ( $last_nonblank_type eq ')' ) {
                warning(
                    "Syntax error? found token '$last_nonblank_type' then '('\n"
                );
            }
            $paren_structural_type[$paren_depth] = $type;

        },
        ')' => sub {
            ( $type_sequence, $indent_flag ) =
              decrease_nesting_depth( PAREN, $rtoken_map->[$i_tok] );

            if ( $paren_structural_type[$paren_depth] eq '{' ) {
                $type = '}';
            }

            $container_type = $paren_type[$paren_depth];

            # restore statement type as 'sub' at closing paren of a signature
            # so that a subsequent ':' is identified as an attribute
            if ( $container_type =~ /^sub\b/ ) {
                $statement_type = $container_type;
            }

            #    /^(for|foreach)$/
            if ( $is_for_foreach{ $paren_type[$paren_depth] } ) {
                my $num_sc = $paren_semicolon_count[$paren_depth];
                if ( $num_sc > 0 && $num_sc != 2 ) {
                    warning("Expected 2 ';' in 'for(;;)' but saw $num_sc\n");
                }
            }

            if ( $paren_depth > 0 ) { $paren_depth-- }
        },
        ',' => sub {
            if ( $last_nonblank_type eq ',' ) {
                complain("Repeated ','s \n");
            }

            # patch for operator_expected: note if we are in the list (use.t)
            if ( $statement_type eq 'use' ) { $statement_type = '_use' }
##                FIXME: need to move this elsewhere, perhaps check after a '('
##                elsif ($last_nonblank_token eq '(') {
##                    warning("Leading ','s illegal in some versions of perl\n");
##                }
        },
        ';' => sub {
            $context        = UNKNOWN_CONTEXT;
            $statement_type = '';
            $want_paren     = "";

            #    /^(for|foreach)$/
            if ( $is_for_foreach{ $paren_type[$paren_depth] } )
            {    # mark ; in for loop

                # Be careful: we do not want a semicolon such as the
                # following to be included:
                #
                #    for (sort {strcoll($a,$b);} keys %investments) {

                if (   $brace_depth == $depth_array[PAREN][BRACE][$paren_depth]
                    && $square_bracket_depth ==
                    $depth_array[PAREN][SQUARE_BRACKET][$paren_depth] )
                {

                    $type = 'f';
                    $paren_semicolon_count[$paren_depth]++;
                }
            }

        },
        '"' => sub {
            error_if_expecting_OPERATOR("String")
              if ( $expecting == OPERATOR );
            $in_quote                = 1;
            $type                    = 'Q';
            $allowed_quote_modifiers = "";
        },
        "'" => sub {
            error_if_expecting_OPERATOR("String")
              if ( $expecting == OPERATOR );
            $in_quote                = 1;
            $type                    = 'Q';
            $allowed_quote_modifiers = "";
        },
        '`' => sub {
            error_if_expecting_OPERATOR("String")
              if ( $expecting == OPERATOR );
            $in_quote                = 1;
            $type                    = 'Q';
            $allowed_quote_modifiers = "";
        },
        '/' => sub {
            my $is_pattern;

            # a pattern cannot follow certain keywords which take optional
            # arguments, like 'shift' and 'pop'. See also '?'.
            if (   $last_nonblank_type eq 'k'
                && $is_keyword_taking_optional_args{$last_nonblank_token} )
            {
                $is_pattern = 0;
            }
            elsif ( $expecting == UNKNOWN ) {    # indeterminate, must guess..
                my $msg;
                ( $is_pattern, $msg ) =
                  guess_if_pattern_or_division( $i, $rtokens, $rtoken_map,
                    $max_token_index );

                if ($msg) {
                    write_diagnostics("DIVIDE:$msg\n");
                    write_logfile_entry($msg);
                }
            }
            else { $is_pattern = ( $expecting == TERM ) }

            if ($is_pattern) {
                $in_quote                = 1;
                $type                    = 'Q';
                $allowed_quote_modifiers = '[msixpodualngc]';
            }
            else {    # not a pattern; check for a /= token

                if ( $rtokens->[ $i + 1 ] eq '=' ) {    # form token /=
                    $i++;
                    $tok  = '/=';
                    $type = $tok;
                }

              #DEBUG - collecting info on what tokens follow a divide
              # for development of guessing algorithm
              #if ( numerator_expected( $i, $rtokens, $max_token_index ) < 0 ) {
              #    #write_diagnostics( "DIVIDE? $input_line\n" );
              #}
            }
        },
        '{' => sub {

            # if we just saw a ')', we will label this block with
            # its type.  We need to do this to allow sub
            # code_block_type to determine if this brace starts a
            # code block or anonymous hash.  (The type of a paren
            # pair is the preceding token, such as 'if', 'else',
            # etc).
            $container_type = "";

            # ATTRS: for a '{' following an attribute list, reset
            # things to look like we just saw the sub name
            if ( $statement_type =~ /^sub/ ) {
                $last_nonblank_token = $statement_type;
                $last_nonblank_type  = 'i';
                $statement_type      = "";
            }

            # patch for SWITCH/CASE: hide these keywords from an immediately
            # following opening brace
            elsif ( ( $statement_type eq 'case' || $statement_type eq 'when' )
                && $statement_type eq $last_nonblank_token )
            {
                $last_nonblank_token = ";";
            }

            elsif ( $last_nonblank_token eq ')' ) {
                $last_nonblank_token = $paren_type[ $paren_depth + 1 ];

                # defensive move in case of a nesting error (pbug.t)
                # in which this ')' had no previous '('
                # this nesting error will have been caught
                if ( !defined($last_nonblank_token) ) {
                    $last_nonblank_token = 'if';
                }

                # check for syntax error here;
                unless ( $is_blocktype_with_paren{$last_nonblank_token} ) {
                    if ( $tokenizer_self->{'_extended_syntax'} ) {

                        # we append a trailing () to mark this as an unknown
                        # block type.  This allows perltidy to format some
                        # common extensions of perl syntax.
                        # This is used by sub code_block_type
                        $last_nonblank_token .= '()';
                    }
                    else {
                        my $list =
                          join( ' ', sort keys %is_blocktype_with_paren );
                        warning(
"syntax error at ') {', didn't see one of: <<$list>>; If this code is okay try using the -xs flag\n"
                        );
                    }
                }
            }

            # patch for paren-less for/foreach glitch, part 2.
            # see note below under 'qw'
            elsif ($last_nonblank_token eq 'qw'
                && $is_for_foreach{$want_paren} )
            {
                $last_nonblank_token = $want_paren;
                if ( $last_last_nonblank_token eq $want_paren ) {
                    warning(
"syntax error at '$want_paren .. {' -- missing \$ loop variable\n"
                    );

                }
                $want_paren = "";
            }

            # now identify which of the three possible types of
            # curly braces we have: hash index container, anonymous
            # hash reference, or code block.

            # non-structural (hash index) curly brace pair
            # get marked 'L' and 'R'
            if ( is_non_structural_brace() ) {
                $type = 'L';

                # patch for SWITCH/CASE:
                # allow paren-less identifier after 'when'
                # if the brace is preceded by a space
                if (   $statement_type eq 'when'
                    && $last_nonblank_type eq 'i'
                    && $last_last_nonblank_type eq 'k'
                    && ( $i_tok == 0 || $rtoken_type->[ $i_tok - 1 ] eq 'b' ) )
                {
                    $type       = '{';
                    $block_type = $statement_type;
                }
            }

            # code and anonymous hash have the same type, '{', but are
            # distinguished by 'block_type',
            # which will be blank for an anonymous hash
            else {

                $block_type = code_block_type( $i_tok, $rtokens, $rtoken_type,
                    $max_token_index );

                # patch to promote bareword type to function taking block
                if (   $block_type
                    && $last_nonblank_type eq 'w'
                    && $last_nonblank_i >= 0 )
                {
                    if ( $routput_token_type->[$last_nonblank_i] eq 'w' ) {
                        $routput_token_type->[$last_nonblank_i] = 'G';
                    }
                }

                # patch for SWITCH/CASE: if we find a stray opening block brace
                # where we might accept a 'case' or 'when' block, then take it
                if (   $statement_type eq 'case'
                    || $statement_type eq 'when' )
                {
                    if ( !$block_type || $block_type eq '}' ) {
                        $block_type = $statement_type;
                    }
                }
            }

            $brace_type[ ++$brace_depth ]        = $block_type;
            $brace_package[$brace_depth]         = $current_package;
            $brace_structural_type[$brace_depth] = $type;
            $brace_context[$brace_depth]         = $context;
            ( $type_sequence, $indent_flag ) =
              increase_nesting_depth( BRACE, $rtoken_map->[$i_tok] );
        },
        '}' => sub {
            $block_type = $brace_type[$brace_depth];
            if ($block_type) { $statement_type = '' }
            if ( defined( $brace_package[$brace_depth] ) ) {
                $current_package = $brace_package[$brace_depth];
            }

            # can happen on brace error (caught elsewhere)
            else {
            }
            ( $type_sequence, $indent_flag ) =
              decrease_nesting_depth( BRACE, $rtoken_map->[$i_tok] );

            if ( $brace_structural_type[$brace_depth] eq 'L' ) {
                $type = 'R';
            }

            # propagate type information for 'do' and 'eval' blocks, and also
            # for smartmatch operator.  This is necessary to enable us to know
            # if an operator or term is expected next.
            if ( $is_block_operator{$block_type} ) {
                $tok = $block_type;
            }

            $context = $brace_context[$brace_depth];
            if ( $brace_depth > 0 ) { $brace_depth--; }
        },
        '&' => sub {    # maybe sub call? start looking

            # We have to check for sub call unless we are sure we
            # are expecting an operator.  This example from s2p
            # got mistaken as a q operator in an early version:
            #   print BODY &q(<<'EOT');
            if ( $expecting != OPERATOR ) {

                # But only look for a sub call if we are expecting a term or
                # if there is no existing space after the &.
                # For example we probably don't want & as sub call here:
                #    Fcntl::S_IRUSR & $mode;
                if ( $expecting == TERM || $next_type ne 'b' ) {
                    scan_identifier();
                }
            }
            else {
            }
        },
        '<' => sub {    # angle operator or less than?

            if ( $expecting != OPERATOR ) {
                ( $i, $type ) =
                  find_angle_operator_termination( $input_line, $i, $rtoken_map,
                    $expecting, $max_token_index );

                if ( $type eq '<' && $expecting == TERM ) {
                    error_if_expecting_TERM();
                    interrupt_logfile();
                    warning("Unterminated <> operator?\n");
                    resume_logfile();
                }
            }
            else {
            }
        },
        '?' => sub {    # ?: conditional or starting pattern?

            my $is_pattern;

            # Patch for rt #126965
            # a pattern cannot follow certain keywords which take optional
            # arguments, like 'shift' and 'pop'. See also '/'.
            if (   $last_nonblank_type eq 'k'
                && $is_keyword_taking_optional_args{$last_nonblank_token} )
            {
                $is_pattern = 0;
            }
            elsif ( $expecting == UNKNOWN ) {

                my $msg;
                ( $is_pattern, $msg ) =
                  guess_if_pattern_or_conditional( $i, $rtokens, $rtoken_map,
                    $max_token_index );

                if ($msg) { write_logfile_entry($msg) }
            }
            else { $is_pattern = ( $expecting == TERM ) }

            if ($is_pattern) {
                $in_quote                = 1;
                $type                    = 'Q';
                $allowed_quote_modifiers = '[msixpodualngc]';
            }
            else {
                ( $type_sequence, $indent_flag ) =
                  increase_nesting_depth( QUESTION_COLON,
                    $rtoken_map->[$i_tok] );
            }
        },
        '*' => sub {    # typeglob, or multiply?

            if ( $expecting == TERM ) {
                scan_identifier();
            }
            else {

                if ( $rtokens->[ $i + 1 ] eq '=' ) {
                    $tok  = '*=';
                    $type = $tok;
                    $i++;
                }
                elsif ( $rtokens->[ $i + 1 ] eq '*' ) {
                    $tok  = '**';
                    $type = $tok;
                    $i++;
                    if ( $rtokens->[ $i + 1 ] eq '=' ) {
                        $tok  = '**=';
                        $type = $tok;
                        $i++;
                    }
                }
            }
        },
        '.' => sub {    # what kind of . ?

            if ( $expecting != OPERATOR ) {
                scan_number();
                if ( $type eq '.' ) {
                    error_if_expecting_TERM()
                      if ( $expecting == TERM );
                }
            }
            else {
            }
        },
        ':' => sub {

            # if this is the first nonblank character, call it a label
            # since perl seems to just swallow it
            if ( $input_line_number == 1 && $last_nonblank_i == -1 ) {
                $type = 'J';
            }

            # ATTRS: check for a ':' which introduces an attribute list
            # (this might eventually get its own token type)
            elsif ( $statement_type =~ /^sub\b/ ) {
                $type              = 'A';
                $in_attribute_list = 1;
            }

            # check for scalar attribute, such as
            # my $foo : shared = 1;
            elsif ($is_my_our{$statement_type}
                && $current_depth[QUESTION_COLON] == 0 )
            {
                $type              = 'A';
                $in_attribute_list = 1;
            }

            # otherwise, it should be part of a ?/: operator
            else {
                ( $type_sequence, $indent_flag ) =
                  decrease_nesting_depth( QUESTION_COLON,
                    $rtoken_map->[$i_tok] );
                if ( $last_nonblank_token eq '?' ) {
                    warning("Syntax error near ? :\n");
                }
            }
        },
        '+' => sub {    # what kind of plus?

            if ( $expecting == TERM ) {
                my $number = scan_number();

                # unary plus is safest assumption if not a number
                if ( !defined($number) ) { $type = 'p'; }
            }
            elsif ( $expecting == OPERATOR ) {
            }
            else {
                if ( $next_type eq 'w' ) { $type = 'p' }
            }
        },
        '@' => sub {

            error_if_expecting_OPERATOR("Array")
              if ( $expecting == OPERATOR );
            scan_identifier();
        },
        '%' => sub {    # hash or modulo?

            # first guess is hash if no following blank
            if ( $expecting == UNKNOWN ) {
                if ( $next_type ne 'b' ) { $expecting = TERM }
            }
            if ( $expecting == TERM ) {
                scan_identifier();
            }
        },
        '[' => sub {
            $square_bracket_type[ ++$square_bracket_depth ] =
              $last_nonblank_token;
            ( $type_sequence, $indent_flag ) =
              increase_nesting_depth( SQUARE_BRACKET, $rtoken_map->[$i_tok] );

            # It may seem odd, but structural square brackets have
            # type '{' and '}'.  This simplifies the indentation logic.
            if ( !is_non_structural_brace() ) {
                $type = '{';
            }
            $square_bracket_structural_type[$square_bracket_depth] = $type;
        },
        ']' => sub {
            ( $type_sequence, $indent_flag ) =
              decrease_nesting_depth( SQUARE_BRACKET, $rtoken_map->[$i_tok] );

            if ( $square_bracket_structural_type[$square_bracket_depth] eq '{' )
            {
                $type = '}';
            }

            # propagate type information for smartmatch operator.  This is
            # necessary to enable us to know if an operator or term is expected
            # next.
            if ( $square_bracket_type[$square_bracket_depth] eq '~~' ) {
                $tok = $square_bracket_type[$square_bracket_depth];
            }

            if ( $square_bracket_depth > 0 ) { $square_bracket_depth--; }
        },
        '-' => sub {    # what kind of minus?

            if ( ( $expecting != OPERATOR )
                && $is_file_test_operator{$next_tok} )
            {
                my ( $next_nonblank_token, $i_next ) =
                  find_next_nonblank_token( $i + 1, $rtokens,
                    $max_token_index );

                # check for a quoted word like "-w=>xx";
                # it is sufficient to just check for a following '='
                if ( $next_nonblank_token eq '=' ) {
                    $type = 'm';
                }
                else {
                    $i++;
                    $tok .= $next_tok;
                    $type = 'F';
                }
            }
            elsif ( $expecting == TERM ) {
                my $number = scan_number();

                # maybe part of bareword token? unary is safest
                if ( !defined($number) ) { $type = 'm'; }

            }
            elsif ( $expecting == OPERATOR ) {
            }
            else {

                if ( $next_type eq 'w' ) {
                    $type = 'm';
                }
            }
        },

        '^' => sub {

            # check for special variables like ${^WARNING_BITS}
            if ( $expecting == TERM ) {

                # FIXME: this should work but will not catch errors
                # because we also have to be sure that previous token is
                # a type character ($,@,%).
                if ( $last_nonblank_token eq '{'
                    && ( $next_tok =~ /^[A-Za-z_]/ ) )
                {

                    if ( $next_tok eq 'W' ) {
                        $tokenizer_self->{_saw_perl_dash_w} = 1;
                    }
                    $tok  = $tok . $next_tok;
                    $i    = $i + 1;
                    $type = 'w';
                }

                else {
                    unless ( error_if_expecting_TERM() ) {

                        # Something like this is valid but strange:
                        # undef ^I;
                        complain("The '^' seems unusual here\n");
                    }
                }
            }
        },

        '::' => sub {    # probably a sub call
            scan_bare_identifier();
        },
        '<<' => sub {    # maybe a here-doc?
            return
              unless ( $i < $max_token_index )
              ;          # here-doc not possible if end of line

            if ( $expecting != OPERATOR ) {
                my ( $found_target, $here_doc_target, $here_quote_character,
                    $saw_error );
                (
                    $found_target, $here_doc_target, $here_quote_character, $i,
                    $saw_error
                  )
                  = find_here_doc( $expecting, $i, $rtokens, $rtoken_map,
                    $max_token_index );

                if ($found_target) {
                    push @{$rhere_target_list},
                      [ $here_doc_target, $here_quote_character ];
                    $type = 'h';
                    if ( length($here_doc_target) > 80 ) {
                        my $truncated = substr( $here_doc_target, 0, 80 );
                        complain("Long here-target: '$truncated' ...\n");
                    }
                    elsif ( $here_doc_target !~ /^[A-Z_]\w+$/ ) {
                        complain(
                            "Unconventional here-target: '$here_doc_target'\n");
                    }
                }
                elsif ( $expecting == TERM ) {
                    unless ($saw_error) {

                        # shouldn't happen..
                        warning("Program bug; didn't find here doc target\n");
                        report_definite_bug();
                    }
                }
            }
            else {
            }
        },
        '<<~' => sub {    # a here-doc, new type added in v26
            return
              unless ( $i < $max_token_index )
              ;           # here-doc not possible if end of line
            if ( $expecting != OPERATOR ) {
                my ( $found_target, $here_doc_target, $here_quote_character,
                    $saw_error );
                (
                    $found_target, $here_doc_target, $here_quote_character, $i,
                    $saw_error
                  )
                  = find_here_doc( $expecting, $i, $rtokens, $rtoken_map,
                    $max_token_index );

                if ($found_target) {

                    if ( length($here_doc_target) > 80 ) {
                        my $truncated = substr( $here_doc_target, 0, 80 );
                        complain("Long here-target: '$truncated' ...\n");
                    }
                    elsif ( $here_doc_target !~ /^[A-Z_]\w+$/ ) {
                        complain(
                            "Unconventional here-target: '$here_doc_target'\n");
                    }

                    # Note that we put a leading space on the here quote
                    # character indicate that it may be preceded by spaces
                    $here_quote_character = " " . $here_quote_character;
                    push @{$rhere_target_list},
                      [ $here_doc_target, $here_quote_character ];
                    $type = 'h';
                }
                elsif ( $expecting == TERM ) {
                    unless ($saw_error) {

                        # shouldn't happen..
                        warning("Program bug; didn't find here doc target\n");
                        report_definite_bug();
                    }
                }
            }
            else {
            }
        },
        '->' => sub {

            # if -> points to a bare word, we must scan for an identifier,
            # otherwise something like ->y would look like the y operator
            scan_identifier();
        },

        # type = 'pp' for pre-increment, '++' for post-increment
        '++' => sub {
            if ( $expecting == TERM ) { $type = 'pp' }
            elsif ( $expecting == UNKNOWN ) {
                my ( $next_nonblank_token, $i_next ) =
                  find_next_nonblank_token( $i, $rtokens, $max_token_index );
                if ( $next_nonblank_token eq '$' ) { $type = 'pp' }
            }
        },

        '=>' => sub {
            if ( $last_nonblank_type eq $tok ) {
                complain("Repeated '=>'s \n");
            }

            # patch for operator_expected: note if we are in the list (use.t)
            # TODO: make version numbers a new token type
            if ( $statement_type eq 'use' ) { $statement_type = '_use' }
        },

        # type = 'mm' for pre-decrement, '--' for post-decrement
        '--' => sub {

            if ( $expecting == TERM ) { $type = 'mm' }
            elsif ( $expecting == UNKNOWN ) {
                my ( $next_nonblank_token, $i_next ) =
                  find_next_nonblank_token( $i, $rtokens, $max_token_index );
                if ( $next_nonblank_token eq '$' ) { $type = 'mm' }
            }
        },

        '&&' => sub {
            error_if_expecting_TERM()
              if ( $expecting == TERM );
        },

        '||' => sub {
            error_if_expecting_TERM()
              if ( $expecting == TERM );
        },

        '//' => sub {
            error_if_expecting_TERM()
              if ( $expecting == TERM );
        },
    };

    # ------------------------------------------------------------
    # end hash of code for handling individual token types
    # ------------------------------------------------------------

    my %matching_start_token = ( '}' => '{', ']' => '[', ')' => '(' );

    # These block types terminate statements and do not need a trailing
    # semicolon
    # patched for SWITCH/CASE/
    my %is_zero_continuation_block_type;
    @_ = qw( } { BEGIN END CHECK INIT AUTOLOAD DESTROY UNITCHECK continue ;
      if elsif else unless while until for foreach switch case given when);
    @is_zero_continuation_block_type{@_} = (1) x scalar(@_);

    my %is_not_zero_continuation_block_type;
    @_ = qw(sort grep map do eval);
    @is_not_zero_continuation_block_type{@_} = (1) x scalar(@_);

    my %is_logical_container;
    @_ = qw(if elsif unless while and or err not && !  || for foreach);
    @is_logical_container{@_} = (1) x scalar(@_);

    my %is_binary_type;
    @_ = qw(|| &&);
    @is_binary_type{@_} = (1) x scalar(@_);

    my %is_binary_keyword;
    @_ = qw(and or err eq ne cmp);
    @is_binary_keyword{@_} = (1) x scalar(@_);

    # 'L' is token for opening { at hash key
    my %is_opening_type;
    @_ = qw< L { ( [ >;
    @is_opening_type{@_} = (1) x scalar(@_);

    # 'R' is token for closing } at hash key
    my %is_closing_type;
    @_ = qw< R } ) ] >;
    @is_closing_type{@_} = (1) x scalar(@_);

    my %is_redo_last_next_goto;
    @_ = qw(redo last next goto);
    @is_redo_last_next_goto{@_} = (1) x scalar(@_);

    my %is_use_require;
    @_ = qw(use require);
    @is_use_require{@_} = (1) x scalar(@_);

    my %is_sub_package;
    @_ = qw(sub package);
    @is_sub_package{@_} = (1) x scalar(@_);

    # This hash holds the hash key in $tokenizer_self for these keywords:
    my %is_format_END_DATA = (
        'format'   => '_in_format',
        '__END__'  => '_in_end',
        '__DATA__' => '_in_data',
    );

    # original ref: camel 3 p 147,
    # but perl may accept undocumented flags
    # perl 5.10 adds 'p' (preserve)
    # Perl version 5.22 added 'n'
    # From http://perldoc.perl.org/perlop.html we have
    # /PATTERN/msixpodualngc or m?PATTERN?msixpodualngc
    # s/PATTERN/REPLACEMENT/msixpodualngcer
    # y/SEARCHLIST/REPLACEMENTLIST/cdsr
    # tr/SEARCHLIST/REPLACEMENTLIST/cdsr
    # qr/STRING/msixpodualn
    my %quote_modifiers = (
        's'  => '[msixpodualngcer]',
        'y'  => '[cdsr]',
        'tr' => '[cdsr]',
        'm'  => '[msixpodualngc]',
        'qr' => '[msixpodualn]',
        'q'  => "",
        'qq' => "",
        'qw' => "",
        'qx' => "",
    );

    # table showing how many quoted things to look for after quote operator..
    # s, y, tr have 2 (pattern and replacement)
    # others have 1 (pattern only)
    my %quote_items = (
        's'  => 2,
        'y'  => 2,
        'tr' => 2,
        'm'  => 1,
        'qr' => 1,
        'q'  => 1,
        'qq' => 1,
        'qw' => 1,
        'qx' => 1,
    );

    sub tokenize_this_line {

  # This routine breaks a line of perl code into tokens which are of use in
  # indentation and reformatting.  One of my goals has been to define tokens
  # such that a newline may be inserted between any pair of tokens without
  # changing or invalidating the program. This version comes close to this,
  # although there are necessarily a few exceptions which must be caught by
  # the formatter.  Many of these involve the treatment of bare words.
  #
  # The tokens and their types are returned in arrays.  See previous
  # routine for their names.
  #
  # See also the array "valid_token_types" in the BEGIN section for an
  # up-to-date list.
  #
  # To simplify things, token types are either a single character, or they
  # are identical to the tokens themselves.
  #
  # As a debugging aid, the -D flag creates a file containing a side-by-side
  # comparison of the input string and its tokenization for each line of a file.
  # This is an invaluable debugging aid.
  #
  # In addition to tokens, and some associated quantities, the tokenizer
  # also returns flags indication any special line types.  These include
  # quotes, here_docs, formats.
  #
  # -----------------------------------------------------------------------
  #
  # How to add NEW_TOKENS:
  #
  # New token types will undoubtedly be needed in the future both to keep up
  # with changes in perl and to help adapt the tokenizer to other applications.
  #
  # Here are some notes on the minimal steps.  I wrote these notes while
  # adding the 'v' token type for v-strings, which are things like version
  # numbers 5.6.0, and ip addresses, and will use that as an example.  ( You
  # can use your editor to search for the string "NEW_TOKENS" to find the
  # appropriate sections to change):
  #
  # *. Try to talk somebody else into doing it!  If not, ..
  #
  # *. Make a backup of your current version in case things don't work out!
  #
  # *. Think of a new, unused character for the token type, and add to
  # the array @valid_token_types in the BEGIN section of this package.
  # For example, I used 'v' for v-strings.
  #
  # *. Implement coding to recognize the $type of the token in this routine.
  # This is the hardest part, and is best done by imitating or modifying
  # some of the existing coding.  For example, to recognize v-strings, I
  # patched 'sub scan_bare_identifier' to recognize v-strings beginning with
  # 'v' and 'sub scan_number' to recognize v-strings without the leading 'v'.
  #
  # *. Update sub operator_expected.  This update is critically important but
  # the coding is trivial.  Look at the comments in that routine for help.
  # For v-strings, which should behave like numbers, I just added 'v' to the
  # regex used to handle numbers and strings (types 'n' and 'Q').
  #
  # *. Implement a 'bond strength' rule in sub set_bond_strengths in
  # Perl::Tidy::Formatter for breaking lines around this token type.  You can
  # skip this step and take the default at first, then adjust later to get
  # desired results.  For adding type 'v', I looked at sub bond_strength and
  # saw that number type 'n' was using default strengths, so I didn't do
  # anything.  I may tune it up someday if I don't like the way line
  # breaks with v-strings look.
  #
  # *. Implement a 'whitespace' rule in sub set_whitespace_flags in
  # Perl::Tidy::Formatter.  For adding type 'v', I looked at this routine
  # and saw that type 'n' used spaces on both sides, so I just added 'v'
  # to the array @spaces_both_sides.
  #
  # *. Update HtmlWriter package so that users can colorize the token as
  # desired.  This is quite easy; see comments identified by 'NEW_TOKENS' in
  # that package.  For v-strings, I initially chose to use a default color
  # equal to the default for numbers, but it might be nice to change that
  # eventually.
  #
  # *. Update comments in Perl::Tidy::Tokenizer::dump_token_types.
  #
  # *. Run lots and lots of debug tests.  Start with special files designed
  # to test the new token type.  Run with the -D flag to create a .DEBUG
  # file which shows the tokenization.  When these work ok, test as many old
  # scripts as possible.  Start with all of the '.t' files in the 'test'
  # directory of the distribution file.  Compare .tdy output with previous
  # version and updated version to see the differences.  Then include as
  # many more files as possible. My own technique has been to collect a huge
  # number of perl scripts (thousands!) into one directory and run perltidy
  # *, then run diff between the output of the previous version and the
  # current version.
  #
  # *. For another example, search for the smartmatch operator '~~'
  # with your editor to see where updates were made for it.
  #
  # -----------------------------------------------------------------------

        my $line_of_tokens = shift;
        my ($untrimmed_input_line) = $line_of_tokens->{_line_text};

        # patch while coding change is underway
        # make callers private data to allow access
        # $tokenizer_self = $caller_tokenizer_self;

        # extract line number for use in error messages
        $input_line_number = $line_of_tokens->{_line_number};

        # reinitialize for multi-line quote
        $line_of_tokens->{_starting_in_quote} = $in_quote && $quote_type eq 'Q';

        # check for pod documentation
        if ( ( $untrimmed_input_line =~ /^=[A-Za-z_]/ ) ) {

            # must not be in multi-line quote
            # and must not be in an equation
            if ( !$in_quote && ( operator_expected( 'b', '=', 'b' ) == TERM ) )
            {
                $tokenizer_self->{_in_pod} = 1;
                return;
            }
        }

        $input_line = $untrimmed_input_line;

        chomp $input_line;

        # trim start of this line unless we are continuing a quoted line
        # do not trim end because we might end in a quote (test: deken4.pl)
        # Perl::Tidy::Formatter will delete needless trailing blanks
        unless ( $in_quote && ( $quote_type eq 'Q' ) ) {
            $input_line =~ s/^\s*//;    # trim left end
        }

        # Set a flag to indicate if we might be at an __END__ or __DATA__ line
        # This will be used below to avoid quoting a bare word followed by
        # a fat comma.
        my $is_END_or_DATA = $input_line =~ /^\s*__(END|DATA)__\s*$/;

        # update the copy of the line for use in error messages
        # This must be exactly what we give the pre_tokenizer
        $tokenizer_self->{_line_text} = $input_line;

        # re-initialize for the main loop
        $routput_token_list     = [];    # stack of output token indexes
        $routput_token_type     = [];    # token types
        $routput_block_type     = [];    # types of code block
        $routput_container_type = [];    # paren types, such as if, elsif, ..
        $routput_type_sequence  = [];    # nesting sequential number

        $rhere_target_list = [];

        $tok             = $last_nonblank_token;
        $type            = $last_nonblank_type;
        $prototype       = $last_nonblank_prototype;
        $last_nonblank_i = -1;
        $block_type      = $last_nonblank_block_type;
        $container_type  = $last_nonblank_container_type;
        $type_sequence   = $last_nonblank_type_sequence;
        $indent_flag     = 0;
        $peeked_ahead    = 0;

        # tokenization is done in two stages..
        # stage 1 is a very simple pre-tokenization
        my $max_tokens_wanted = 0; # this signals pre_tokenize to get all tokens

        # a little optimization for a full-line comment
        if ( !$in_quote && ( $input_line =~ /^#/ ) ) {
            $max_tokens_wanted = 1    # no use tokenizing a comment
        }

        # start by breaking the line into pre-tokens
        ( $rtokens, $rtoken_map, $rtoken_type ) =
          pre_tokenize( $input_line, $max_tokens_wanted );

        $max_token_index = scalar( @{$rtokens} ) - 1;
        push( @{$rtokens}, ' ', ' ', ' ' );  # extra whitespace simplifies logic
        push( @{$rtoken_map},  0,   0,   0 );     # shouldn't be referenced
        push( @{$rtoken_type}, 'b', 'b', 'b' );

        # initialize for main loop
        foreach my $ii ( 0 .. $max_token_index + 3 ) {
            $routput_token_type->[$ii]     = "";
            $routput_block_type->[$ii]     = "";
            $routput_container_type->[$ii] = "";
            $routput_type_sequence->[$ii]  = "";
            $routput_indent_flag->[$ii]    = 0;
        }
        $i     = -1;
        $i_tok = -1;

        # ------------------------------------------------------------
        # begin main tokenization loop
        # ------------------------------------------------------------

        # we are looking at each pre-token of one line and combining them
        # into tokens
        while ( ++$i <= $max_token_index ) {

            if ($in_quote) {    # continue looking for end of a quote
                $type = $quote_type;

                unless ( @{$routput_token_list} )
                {               # initialize if continuation line
                    push( @{$routput_token_list}, $i );
                    $routput_token_type->[$i] = $type;

                }
                $tok = $quote_character unless ( $quote_character =~ /^\s*$/ );

                # scan for the end of the quote or pattern
                (
                    $i, $in_quote, $quote_character, $quote_pos, $quote_depth,
                    $quoted_string_1, $quoted_string_2
                  )
                  = do_quote(
                    $i,               $in_quote,    $quote_character,
                    $quote_pos,       $quote_depth, $quoted_string_1,
                    $quoted_string_2, $rtokens,     $rtoken_map,
                    $max_token_index
                  );

                # all done if we didn't find it
                last if ($in_quote);

                # save pattern and replacement text for rescanning
                my $qs1 = $quoted_string_1;
                my $qs2 = $quoted_string_2;

                # re-initialize for next search
                $quote_character = '';
                $quote_pos       = 0;
                $quote_type      = 'Q';
                $quoted_string_1 = "";
                $quoted_string_2 = "";
                last if ( ++$i > $max_token_index );

                # look for any modifiers
                if ($allowed_quote_modifiers) {

                    # check for exact quote modifiers
                    if ( $rtokens->[$i] =~ /^[A-Za-z_]/ ) {
                        my $str = $rtokens->[$i];
                        my $saw_modifier_e;
                        while ( $str =~ /\G$allowed_quote_modifiers/gc ) {
                            my $pos  = pos($str);
                            my $char = substr( $str, $pos - 1, 1 );
                            $saw_modifier_e ||= ( $char eq 'e' );
                        }

                        # For an 'e' quote modifier we must scan the replacement
                        # text for here-doc targets.
                        if ($saw_modifier_e) {

                            my $rht = scan_replacement_text($qs1);

                            # Change type from 'Q' to 'h' for quotes with
                            # here-doc targets so that the formatter (see sub
                            # print_line_of_tokens) will not make any line
                            # breaks after this point.
                            if ($rht) {
                                push @{$rhere_target_list}, @{$rht};
                                $type = 'h';
                                if ( $i_tok < 0 ) {
                                    my $ilast = $routput_token_list->[-1];
                                    $routput_token_type->[$ilast] = $type;
                                }
                            }
                        }

                        if ( defined( pos($str) ) ) {

                            # matched
                            if ( pos($str) == length($str) ) {
                                last if ( ++$i > $max_token_index );
                            }

                            # Looks like a joined quote modifier
                            # and keyword, maybe something like
                            # s/xxx/yyy/gefor @k=...
                            # Example is "galgen.pl".  Would have to split
                            # the word and insert a new token in the
                            # pre-token list.  This is so rare that I haven't
                            # done it.  Will just issue a warning citation.

                            # This error might also be triggered if my quote
                            # modifier characters are incomplete
                            else {
                                warning(<<EOM);

Partial match to quote modifier $allowed_quote_modifiers at word: '$str'
Please put a space between quote modifiers and trailing keywords.
EOM

                         # print "token $rtokens->[$i]\n";
                         # my $num = length($str) - pos($str);
                         # $rtokens->[$i]=substr($rtokens->[$i],pos($str),$num);
                         # print "continuing with new token $rtokens->[$i]\n";

                                # skipping past this token does least damage
                                last if ( ++$i > $max_token_index );
                            }
                        }
                        else {

                            # example file: rokicki4.pl
                            # This error might also be triggered if my quote
                            # modifier characters are incomplete
                            write_logfile_entry(
"Note: found word $str at quote modifier location\n"
                            );
                        }
                    }

                    # re-initialize
                    $allowed_quote_modifiers = "";
                }
            }

            unless ( $tok =~ /^\s*$/ || $tok eq 'CORE::' ) {

                # try to catch some common errors
                if ( ( $type eq 'n' ) && ( $tok ne '0' ) ) {

                    if ( $last_nonblank_token eq 'eq' ) {
                        complain("Should 'eq' be '==' here ?\n");
                    }
                    elsif ( $last_nonblank_token eq 'ne' ) {
                        complain("Should 'ne' be '!=' here ?\n");
                    }
                }

                $last_last_nonblank_token      = $last_nonblank_token;
                $last_last_nonblank_type       = $last_nonblank_type;
                $last_last_nonblank_block_type = $last_nonblank_block_type;
                $last_last_nonblank_container_type =
                  $last_nonblank_container_type;
                $last_last_nonblank_type_sequence =
                  $last_nonblank_type_sequence;
                $last_nonblank_token          = $tok;
                $last_nonblank_type           = $type;
                $last_nonblank_prototype      = $prototype;
                $last_nonblank_block_type     = $block_type;
                $last_nonblank_container_type = $container_type;
                $last_nonblank_type_sequence  = $type_sequence;
                $last_nonblank_i              = $i_tok;
            }

            # store previous token type
            if ( $i_tok >= 0 ) {
                $routput_token_type->[$i_tok]     = $type;
                $routput_block_type->[$i_tok]     = $block_type;
                $routput_container_type->[$i_tok] = $container_type;
                $routput_type_sequence->[$i_tok]  = $type_sequence;
                $routput_indent_flag->[$i_tok]    = $indent_flag;
            }
            my $pre_tok  = $rtokens->[$i];        # get the next pre-token
            my $pre_type = $rtoken_type->[$i];    # and type
            $tok  = $pre_tok;
            $type = $pre_type;                    # to be modified as necessary
            $block_type = "";    # blank for all tokens except code block braces
            $container_type = "";    # blank for all tokens except some parens
            $type_sequence  = "";    # blank for all tokens except ?/:
            $indent_flag    = 0;
            $prototype = "";    # blank for all tokens except user defined subs
            $i_tok     = $i;

            # this pre-token will start an output token
            push( @{$routput_token_list}, $i_tok );

            # continue gathering identifier if necessary
            # but do not start on blanks and comments
            if ( $id_scan_state && $pre_type !~ /[b#]/ ) {

                if ( $id_scan_state =~ /^(sub|package)/ ) {
                    scan_id();
                }
                else {
                    scan_identifier();
                }

                last if ($id_scan_state);
                next if ( ( $i > 0 ) || $type );

                # didn't find any token; start over
                $type = $pre_type;
                $tok  = $pre_tok;
            }

            # handle whitespace tokens..
            next if ( $type eq 'b' );
            my $prev_tok  = $i > 0 ? $rtokens->[ $i - 1 ]     : ' ';
            my $prev_type = $i > 0 ? $rtoken_type->[ $i - 1 ] : 'b';

            # Build larger tokens where possible, since we are not in a quote.
            #
            # First try to assemble digraphs.  The following tokens are
            # excluded and handled specially:
            # '/=' is excluded because the / might start a pattern.
            # 'x=' is excluded since it might be $x=, with $ on previous line
            # '**' and *= might be typeglobs of punctuation variables
            # I have allowed tokens starting with <, such as <=,
            # because I don't think these could be valid angle operators.
            # test file: storrs4.pl
            my $test_tok   = $tok . $rtokens->[ $i + 1 ];
            my $combine_ok = $is_digraph{$test_tok};

            # check for special cases which cannot be combined
            if ($combine_ok) {

                # '//' must be defined_or operator if an operator is expected.
                # TODO: Code for other ambiguous digraphs (/=, x=, **, *=)
                # could be migrated here for clarity

              # Patch for RT#102371, misparsing a // in the following snippet:
              #     state $b //= ccc();
              # The solution is to always accept the digraph (or trigraph) after
              # token type 'Z' (possible file handle).  The reason is that
              # sub operator_expected gives TERM expected here, which is
              # wrong in this case.
                if ( $test_tok eq '//' && $last_nonblank_type ne 'Z' ) {
                    my $next_type = $rtokens->[ $i + 1 ];
                    my $expecting =
                      operator_expected( $prev_type, $tok, $next_type );

                    # Patched for RT#101547, was 'unless ($expecting==OPERATOR)'
                    $combine_ok = 0 if ( $expecting == TERM );
                }

                # Patch for RT #114359: Missparsing of "print $x ** 0.5;
                # Accept the digraphs '**' only after type 'Z'
                # Otherwise postpone the decision.
                if ( $test_tok eq '**' ) {
                    if ( $last_nonblank_type ne 'Z' ) { $combine_ok = 0 }
                }
            }

            if (
                $combine_ok

                && ( $test_tok ne '/=' )    # might be pattern
                && ( $test_tok ne 'x=' )    # might be $x
                && ( $test_tok ne '*=' )    # typeglob?

                # Moved above as part of fix for
                # RT #114359: Missparsing of "print $x ** 0.5;
                # && ( $test_tok ne '**' )    # typeglob?
              )
            {
                $tok = $test_tok;
                $i++;

                # Now try to assemble trigraphs.  Note that all possible
                # perl trigraphs can be constructed by appending a character
                # to a digraph.
                $test_tok = $tok . $rtokens->[ $i + 1 ];

                if ( $is_trigraph{$test_tok} ) {
                    $tok = $test_tok;
                    $i++;
                }

                # The only current tetragraph is the double diamond operator
                # and its first three characters are not a trigraph, so
                # we do can do a special test for it
                elsif ( $test_tok eq '<<>' ) {
                    $test_tok .= $rtokens->[ $i + 2 ];
                    if ( $is_tetragraph{$test_tok} ) {
                        $tok = $test_tok;
                        $i += 2;
                    }
                }
            }

            $type      = $tok;
            $next_tok  = $rtokens->[ $i + 1 ];
            $next_type = $rtoken_type->[ $i + 1 ];

            TOKENIZER_DEBUG_FLAG_TOKENIZE && do {
                local $" = ')(';
                my @debug_list = (
                    $last_nonblank_token,      $tok,
                    $next_tok,                 $brace_depth,
                    $brace_type[$brace_depth], $paren_depth,
                    $paren_type[$paren_depth]
                );
                print STDOUT "TOKENIZE:(@debug_list)\n";
            };

            # turn off attribute list on first non-blank, non-bareword
            if ( $pre_type ne 'w' ) { $in_attribute_list = 0 }

            ###############################################################
            # We have the next token, $tok.
            # Now we have to examine this token and decide what it is
            # and define its $type
            #
            # section 1: bare words
            ###############################################################

            if ( $pre_type eq 'w' ) {
                $expecting = operator_expected( $prev_type, $tok, $next_type );
                my ( $next_nonblank_token, $i_next ) =
                  find_next_nonblank_token( $i, $rtokens, $max_token_index );

                # ATTRS: handle sub and variable attributes
                if ($in_attribute_list) {

                    # treat bare word followed by open paren like qw(
                    if ( $next_nonblank_token eq '(' ) {
                        $in_quote                = $quote_items{'q'};
                        $allowed_quote_modifiers = $quote_modifiers{'q'};
                        $type                    = 'q';
                        $quote_type              = 'q';
                        next;
                    }

                    # handle bareword not followed by open paren
                    else {
                        $type = 'w';
                        next;
                    }
                }

                # quote a word followed by => operator
                # unless the word __END__ or __DATA__ and the only word on
                # the line.
                if ( !$is_END_or_DATA && $next_nonblank_token eq '=' ) {

                    if ( $rtokens->[ $i_next + 1 ] eq '>' ) {
                        if ( $is_constant{$current_package}{$tok} ) {
                            $type = 'C';
                        }
                        elsif ( $is_user_function{$current_package}{$tok} ) {
                            $type = 'U';
                            $prototype =
                              $user_function_prototype{$current_package}{$tok};
                        }
                        elsif ( $tok =~ /^v\d+$/ ) {
                            $type = 'v';
                            report_v_string($tok);
                        }
                        else { $type = 'w' }

                        next;
                    }
                }

     # quote a bare word within braces..like xxx->{s}; note that we
     # must be sure this is not a structural brace, to avoid
     # mistaking {s} in the following for a quoted bare word:
     #     for(@[){s}bla}BLA}
     # Also treat q in something like var{-q} as a bare word, not qoute operator
                if (
                    $next_nonblank_token eq '}'
                    && (
                        $last_nonblank_type eq 'L'
                        || (   $last_nonblank_type eq 'm'
                            && $last_last_nonblank_type eq 'L' )
                    )
                  )
                {
                    $type = 'w';
                    next;
                }

                # a bare word immediately followed by :: is not a keyword;
                # use $tok_kw when testing for keywords to avoid a mistake
                my $tok_kw = $tok;
                if (   $rtokens->[ $i + 1 ] eq ':'
                    && $rtokens->[ $i + 2 ] eq ':' )
                {
                    $tok_kw .= '::';
                }

                # handle operator x (now we know it isn't $x=)
                if ( ( $tok =~ /^x\d*$/ ) && ( $expecting == OPERATOR ) ) {
                    if ( $tok eq 'x' ) {

                        if ( $rtokens->[ $i + 1 ] eq '=' ) {    # x=
                            $tok  = 'x=';
                            $type = $tok;
                            $i++;
                        }
                        else {
                            $type = 'x';
                        }
                    }

                    # FIXME: Patch: mark something like x4 as an integer for now
                    # It gets fixed downstream.  This is easier than
                    # splitting the pretoken.
                    else {
                        $type = 'n';
                    }
                }
                elsif ( $tok_kw eq 'CORE::' ) {
                    $type = $tok = $tok_kw;
                    $i += 2;
                }
                elsif ( ( $tok eq 'strict' )
                    and ( $last_nonblank_token eq 'use' ) )
                {
                    $tokenizer_self->{_saw_use_strict} = 1;
                    scan_bare_identifier();
                }

                elsif ( ( $tok eq 'warnings' )
                    and ( $last_nonblank_token eq 'use' ) )
                {
                    $tokenizer_self->{_saw_perl_dash_w} = 1;

                    # scan as identifier, so that we pick up something like:
                    # use warnings::register
                    scan_bare_identifier();
                }

                elsif (
                       $tok eq 'AutoLoader'
                    && $tokenizer_self->{_look_for_autoloader}
                    && (
                        $last_nonblank_token eq 'use'

                        # these regexes are from AutoSplit.pm, which we want
                        # to mimic
                        || $input_line =~ /^\s*(use|require)\s+AutoLoader\b/
                        || $input_line =~ /\bISA\s*=.*\bAutoLoader\b/
                    )
                  )
                {
                    write_logfile_entry("AutoLoader seen, -nlal deactivates\n");
                    $tokenizer_self->{_saw_autoloader}      = 1;
                    $tokenizer_self->{_look_for_autoloader} = 0;
                    scan_bare_identifier();
                }

                elsif (
                       $tok eq 'SelfLoader'
                    && $tokenizer_self->{_look_for_selfloader}
                    && (   $last_nonblank_token eq 'use'
                        || $input_line =~ /^\s*(use|require)\s+SelfLoader\b/
                        || $input_line =~ /\bISA\s*=.*\bSelfLoader\b/ )
                  )
                {
                    write_logfile_entry("SelfLoader seen, -nlsl deactivates\n");
                    $tokenizer_self->{_saw_selfloader}      = 1;
                    $tokenizer_self->{_look_for_selfloader} = 0;
                    scan_bare_identifier();
                }

                elsif ( ( $tok eq 'constant' )
                    and ( $last_nonblank_token eq 'use' ) )
                {
                    scan_bare_identifier();
                    my ( $next_nonblank_token, $i_next ) =
                      find_next_nonblank_token( $i, $rtokens,
                        $max_token_index );

                    if ($next_nonblank_token) {

                        if ( $is_keyword{$next_nonblank_token} ) {

                            # Assume qw is used as a quote and okay, as in:
                            #  use constant qw{ DEBUG 0 };
                            # Not worth trying to parse for just a warning

                            # NOTE: This warning is deactivated because recent
                            # versions of perl do not complain here, but
                            # the coding is retained for reference.
                            if ( 0 && $next_nonblank_token ne 'qw' ) {
                                warning(
"Attempting to define constant '$next_nonblank_token' which is a perl keyword\n"
                                );
                            }
                        }

                        # FIXME: could check for error in which next token is
                        # not a word (number, punctuation, ..)
                        else {
                            $is_constant{$current_package}{$next_nonblank_token}
                              = 1;
                        }
                    }
                }

                # various quote operators
                elsif ( $is_q_qq_qw_qx_qr_s_y_tr_m{$tok} ) {
##NICOL PATCH
                    if ( $expecting == OPERATOR ) {

                        # Be careful not to call an error for a qw quote
                        # where a parenthesized list is allowed.  For example,
                        # it could also be a for/foreach construct such as
                        #
                        #    foreach my $key qw\Uno Due Tres Quadro\ {
                        #        print "Set $key\n";
                        #    }
                        #

                        # Or it could be a function call.
                        # NOTE: Braces in something like &{ xxx } are not
                        # marked as a block, we might have a method call.
                        # &method(...), $method->(..), &{method}(...),
                        # $ref[2](list) is ok & short for $ref[2]->(list)
                        #
                        # See notes in 'sub code_block_type' and
                        # 'sub is_non_structural_brace'

                        unless (
                            $tok eq 'qw'
                            && (   $last_nonblank_token =~ /^([\]\}\&]|\-\>)/
                                || $is_for_foreach{$want_paren} )
                          )
                        {
                            error_if_expecting_OPERATOR();
                        }
                    }
                    $in_quote                = $quote_items{$tok};
                    $allowed_quote_modifiers = $quote_modifiers{$tok};

                   # All quote types are 'Q' except possibly qw quotes.
                   # qw quotes are special in that they may generally be trimmed
                   # of leading and trailing whitespace.  So they are given a
                   # separate type, 'q', unless requested otherwise.
                    $type =
                      ( $tok eq 'qw' && $tokenizer_self->{_trim_qw} )
                      ? 'q'
                      : 'Q';
                    $quote_type = $type;
                }

                # check for a statement label
                elsif (
                       ( $next_nonblank_token eq ':' )
                    && ( $rtokens->[ $i_next + 1 ] ne ':' )
                    && ( $i_next <= $max_token_index )      # colon on same line
                    && label_ok()
                  )
                {
                    if ( $tok !~ /[A-Z]/ ) {
                        push @{ $tokenizer_self->{_rlower_case_labels_at} },
                          $input_line_number;
                    }
                    $type = 'J';
                    $tok .= ':';
                    $i = $i_next;
                    next;
                }

                #      'sub' || 'package'
                elsif ( $is_sub_package{$tok_kw} ) {
                    error_if_expecting_OPERATOR()
                      if ( $expecting == OPERATOR );
                    scan_id();
                }

                # Note on token types for format, __DATA__, __END__:
                # It simplifies things to give these type ';', so that when we
                # start rescanning we will be expecting a token of type TERM.
                # We will switch to type 'k' before outputting the tokens.
                elsif ( $is_format_END_DATA{$tok_kw} ) {
                    $type = ';';    # make tokenizer look for TERM next
                    $tokenizer_self->{ $is_format_END_DATA{$tok_kw} } = 1;
                    last;
                }

                elsif ( $is_keyword{$tok_kw} ) {
                    $type = 'k';

                    # Since for and foreach may not be followed immediately
                    # by an opening paren, we have to remember which keyword
                    # is associated with the next '('
                    if ( $is_for_foreach{$tok} ) {
                        if ( new_statement_ok() ) {
                            $want_paren = $tok;
                        }
                    }

                    # recognize 'use' statements, which are special
                    elsif ( $is_use_require{$tok} ) {
                        $statement_type = $tok;
                        error_if_expecting_OPERATOR()
                          if ( $expecting == OPERATOR );
                    }

                    # remember my and our to check for trailing ": shared"
                    elsif ( $is_my_our{$tok} ) {
                        $statement_type = $tok;
                    }

                    # Check for misplaced 'elsif' and 'else', but allow isolated
                    # else or elsif blocks to be formatted.  This is indicated
                    # by a last noblank token of ';'
                    elsif ( $tok eq 'elsif' ) {
                        if (   $last_nonblank_token ne ';'
                            && $last_nonblank_block_type !~
                            /^(if|elsif|unless)$/ )
                        {
                            warning(
"expecting '$tok' to follow one of 'if|elsif|unless'\n"
                            );
                        }
                    }
                    elsif ( $tok eq 'else' ) {

                        # patched for SWITCH/CASE
                        if (
                               $last_nonblank_token ne ';'
                            && $last_nonblank_block_type !~
                            /^(if|elsif|unless|case|when)$/

                            # patch to avoid an unwanted error message for
                            # the case of a parenless 'case' (RT 105484):
                            # switch ( 1 ) { case x { 2 } else { } }
                            && $statement_type !~
                            /^(if|elsif|unless|case|when)$/
                          )
                        {
                            warning(
"expecting '$tok' to follow one of 'if|elsif|unless|case|when'\n"
                            );
                        }
                    }
                    elsif ( $tok eq 'continue' ) {
                        if (   $last_nonblank_token ne ';'
                            && $last_nonblank_block_type !~
                            /(^(\{|\}|;|while|until|for|foreach)|:$)/ )
                        {

                            # note: ';' '{' and '}' in list above
                            # because continues can follow bare blocks;
                            # ':' is labeled block
                            #
                            ############################################
                            # NOTE: This check has been deactivated because
                            # continue has an alternative usage for given/when
                            # blocks in perl 5.10
                            ## warning("'$tok' should follow a block\n");
                            ############################################
                        }
                    }

                    # patch for SWITCH/CASE if 'case' and 'when are
                    # treated as keywords.
                    elsif ( $tok eq 'when' || $tok eq 'case' ) {
                        $statement_type = $tok;    # next '{' is block
                    }

                    #
                    # indent trailing if/unless/while/until
                    # outdenting will be handled by later indentation loop
## DEACTIVATED: unfortunately this can cause some unwanted indentation like:
##$opt_o = 1
##  if !(
##             $opt_b
##          || $opt_c
##          || $opt_d
##          || $opt_f
##          || $opt_i
##          || $opt_l
##          || $opt_o
##          || $opt_x
##  );
##                    if (   $tok =~ /^(if|unless|while|until)$/
##                        && $next_nonblank_token ne '(' )
##                    {
##                        $indent_flag = 1;
##                    }
                }

                # check for inline label following
                #         /^(redo|last|next|goto)$/
                elsif (( $last_nonblank_type eq 'k' )
                    && ( $is_redo_last_next_goto{$last_nonblank_token} ) )
                {
                    $type = 'j';
                    next;
                }

                # something else --
                else {

                    scan_bare_identifier();
                    if ( $type eq 'w' ) {

                        if ( $expecting == OPERATOR ) {

                            # don't complain about possible indirect object
                            # notation.
                            # For example:
                            #   package main;
                            #   sub new($) { ... }
                            #   $b = new A::;  # calls A::new
                            #   $c = new A;    # same thing but suspicious
                            # This will call A::new but we have a 'new' in
                            # main:: which looks like a constant.
                            #
                            if ( $last_nonblank_type eq 'C' ) {
                                if ( $tok !~ /::$/ ) {
                                    complain(<<EOM);
Expecting operator after '$last_nonblank_token' but found bare word '$tok'
       Maybe indirectet object notation?
EOM
                                }
                            }
                            else {
                                error_if_expecting_OPERATOR("bareword");
                            }
                        }

                        # mark bare words immediately followed by a paren as
                        # functions
                        $next_tok = $rtokens->[ $i + 1 ];
                        if ( $next_tok eq '(' ) {
                            $type = 'U';
                        }

                        # underscore after file test operator is file handle
                        if ( $tok eq '_' && $last_nonblank_type eq 'F' ) {
                            $type = 'Z';
                        }

                        # patch for SWITCH/CASE if 'case' and 'when are
                        # not treated as keywords:
                        if (
                            (
                                   $tok eq 'case'
                                && $brace_type[$brace_depth] eq 'switch'
                            )
                            || (   $tok eq 'when'
                                && $brace_type[$brace_depth] eq 'given' )
                          )
                        {
                            $statement_type = $tok;    # next '{' is block
                            $type = 'k';    # for keyword syntax coloring
                        }

                        # patch for SWITCH/CASE if switch and given not keywords
                        # Switch is not a perl 5 keyword, but we will gamble
                        # and mark switch followed by paren as a keyword.  This
                        # is only necessary to get html syntax coloring nice,
                        # and does not commit this as being a switch/case.
                        if ( $next_nonblank_token eq '('
                            && ( $tok eq 'switch' || $tok eq 'given' ) )
                        {
                            $type = 'k';    # for keyword syntax coloring
                        }
                    }
                }
            }

            ###############################################################
            # section 2: strings of digits
            ###############################################################
            elsif ( $pre_type eq 'd' ) {
                $expecting = operator_expected( $prev_type, $tok, $next_type );
                error_if_expecting_OPERATOR("Number")
                  if ( $expecting == OPERATOR );
                my $number = scan_number();
                if ( !defined($number) ) {

                    # shouldn't happen - we should always get a number
                    warning("non-number beginning with digit--program bug\n");
                    report_definite_bug();
                }
            }

            ###############################################################
            # section 3: all other tokens
            ###############################################################

            else {
                last if ( $tok eq '#' );
                my $code = $tokenization_code->{$tok};
                if ($code) {
                    $expecting =
                      operator_expected( $prev_type, $tok, $next_type );
                    $code->();
                    redo if $in_quote;
                }
            }
        }

        # -----------------------------
        # end of main tokenization loop
        # -----------------------------

        if ( $i_tok >= 0 ) {
            $routput_token_type->[$i_tok]     = $type;
            $routput_block_type->[$i_tok]     = $block_type;
            $routput_container_type->[$i_tok] = $container_type;
            $routput_type_sequence->[$i_tok]  = $type_sequence;
            $routput_indent_flag->[$i_tok]    = $indent_flag;
        }

        unless ( ( $type eq 'b' ) || ( $type eq '#' ) ) {
            $last_last_nonblank_token          = $last_nonblank_token;
            $last_last_nonblank_type           = $last_nonblank_type;
            $last_last_nonblank_block_type     = $last_nonblank_block_type;
            $last_last_nonblank_container_type = $last_nonblank_container_type;
            $last_last_nonblank_type_sequence  = $last_nonblank_type_sequence;
            $last_nonblank_token               = $tok;
            $last_nonblank_type                = $type;
            $last_nonblank_block_type          = $block_type;
            $last_nonblank_container_type      = $container_type;
            $last_nonblank_type_sequence       = $type_sequence;
            $last_nonblank_prototype           = $prototype;
        }

        # reset indentation level if necessary at a sub or package
        # in an attempt to recover from a nesting error
        if ( $level_in_tokenizer < 0 ) {
            if ( $input_line =~ /^\s*(sub|package)\s+(\w+)/ ) {
                reset_indentation_level(0);
                brace_warning("resetting level to 0 at $1 $2\n");
            }
        }

        # all done tokenizing this line ...
        # now prepare the final list of tokens and types

        my @token_type     = ();   # stack of output token types
        my @block_type     = ();   # stack of output code block types
        my @container_type = ();   # stack of output code container types
        my @type_sequence  = ();   # stack of output type sequence numbers
        my @tokens         = ();   # output tokens
        my @levels         = ();   # structural brace levels of output tokens
        my @slevels        = ();   # secondary nesting levels of output tokens
        my @nesting_tokens = ();   # string of tokens leading to this depth
        my @nesting_types  = ();   # string of token types leading to this depth
        my @nesting_blocks = ();   # string of block types leading to this depth
        my @nesting_lists  = ();   # string of list types leading to this depth
        my @ci_string = ();  # string needed to compute continuation indentation
        my @container_environment = ();    # BLOCK or LIST
        my $container_environment = '';
        my $im                    = -1;    # previous $i value
        my $num;
        my $ci_string_sum = ones_count($ci_string_in_tokenizer);

# Computing Token Indentation
#
#     The final section of the tokenizer forms tokens and also computes
#     parameters needed to find indentation.  It is much easier to do it
#     in the tokenizer than elsewhere.  Here is a brief description of how
#     indentation is computed.  Perl::Tidy computes indentation as the sum
#     of 2 terms:
#
#     (1) structural indentation, such as if/else/elsif blocks
#     (2) continuation indentation, such as long parameter call lists.
#
#     These are occasionally called primary and secondary indentation.
#
#     Structural indentation is introduced by tokens of type '{', although
#     the actual tokens might be '{', '(', or '['.  Structural indentation
#     is of two types: BLOCK and non-BLOCK.  Default structural indentation
#     is 4 characters if the standard indentation scheme is used.
#
#     Continuation indentation is introduced whenever a line at BLOCK level
#     is broken before its termination.  Default continuation indentation
#     is 2 characters in the standard indentation scheme.
#
#     Both types of indentation may be nested arbitrarily deep and
#     interlaced.  The distinction between the two is somewhat arbitrary.
#
#     For each token, we will define two variables which would apply if
#     the current statement were broken just before that token, so that
#     that token started a new line:
#
#     $level = the structural indentation level,
#     $ci_level = the continuation indentation level
#
#     The total indentation will be $level * (4 spaces) + $ci_level * (2 spaces),
#     assuming defaults.  However, in some special cases it is customary
#     to modify $ci_level from this strict value.
#
#     The total structural indentation is easy to compute by adding and
#     subtracting 1 from a saved value as types '{' and '}' are seen.  The
#     running value of this variable is $level_in_tokenizer.
#
#     The total continuation is much more difficult to compute, and requires
#     several variables.  These variables are:
#
#     $ci_string_in_tokenizer = a string of 1's and 0's indicating, for
#       each indentation level, if there are intervening open secondary
#       structures just prior to that level.
#     $continuation_string_in_tokenizer = a string of 1's and 0's indicating
#       if the last token at that level is "continued", meaning that it
#       is not the first token of an expression.
#     $nesting_block_string = a string of 1's and 0's indicating, for each
#       indentation level, if the level is of type BLOCK or not.
#     $nesting_block_flag = the most recent 1 or 0 of $nesting_block_string
#     $nesting_list_string = a string of 1's and 0's indicating, for each
#       indentation level, if it is appropriate for list formatting.
#       If so, continuation indentation is used to indent long list items.
#     $nesting_list_flag = the most recent 1 or 0 of $nesting_list_string
#     @{$rslevel_stack} = a stack of total nesting depths at each
#       structural indentation level, where "total nesting depth" means
#       the nesting depth that would occur if every nesting token -- '{', '[',
#       and '(' -- , regardless of context, is used to compute a nesting
#       depth.

        #my $nesting_block_flag = ($nesting_block_string =~ /1$/);
        #my $nesting_list_flag = ($nesting_list_string =~ /1$/);

        my ( $ci_string_i, $level_i, $nesting_block_string_i,
            $nesting_list_string_i, $nesting_token_string_i,
            $nesting_type_string_i, );

        foreach my $i ( @{$routput_token_list} )
        {    # scan the list of pre-tokens indexes

            # self-checking for valid token types
            my $type                    = $routput_token_type->[$i];
            my $forced_indentation_flag = $routput_indent_flag->[$i];

            # See if we should undo the $forced_indentation_flag.
            # Forced indentation after 'if', 'unless', 'while' and 'until'
            # expressions without trailing parens is optional and doesn't
            # always look good.  It is usually okay for a trailing logical
            # expression, but if the expression is a function call, code block,
            # or some kind of list it puts in an unwanted extra indentation
            # level which is hard to remove.
            #
            # Example where extra indentation looks ok:
            # return 1
            #   if $det_a < 0 and $det_b > 0
            #       or $det_a > 0 and $det_b < 0;
            #
            # Example where extra indentation is not needed because
            # the eval brace also provides indentation:
            # print "not " if defined eval {
            #     reduce { die if $b > 2; $a + $b } 0, 1, 2, 3, 4;
            # };
            #
            # The following rule works fairly well:
            #   Undo the flag if the end of this line, or start of the next
            #   line, is an opening container token or a comma.
            # This almost always works, but if not after another pass it will
            # be stable.
            if ( $forced_indentation_flag && $type eq 'k' ) {
                my $ixlast  = -1;
                my $ilast   = $routput_token_list->[$ixlast];
                my $toklast = $routput_token_type->[$ilast];
                if ( $toklast eq '#' ) {
                    $ixlast--;
                    $ilast   = $routput_token_list->[$ixlast];
                    $toklast = $routput_token_type->[$ilast];
                }
                if ( $toklast eq 'b' ) {
                    $ixlast--;
                    $ilast   = $routput_token_list->[$ixlast];
                    $toklast = $routput_token_type->[$ilast];
                }
                if ( $toklast =~ /^[\{,]$/ ) {
                    $forced_indentation_flag = 0;
                }
                else {
                    ( $toklast, my $i_next ) =
                      find_next_nonblank_token( $max_token_index, $rtokens,
                        $max_token_index );
                    if ( $toklast =~ /^[\{,]$/ ) {
                        $forced_indentation_flag = 0;
                    }
                }
            }

            # if we are already in an indented if, see if we should outdent
            if ($indented_if_level) {

                # don't try to nest trailing if's - shouldn't happen
                if ( $type eq 'k' ) {
                    $forced_indentation_flag = 0;
                }

                # check for the normal case - outdenting at next ';'
                elsif ( $type eq ';' ) {
                    if ( $level_in_tokenizer == $indented_if_level ) {
                        $forced_indentation_flag = -1;
                        $indented_if_level       = 0;
                    }
                }

                # handle case of missing semicolon
                elsif ( $type eq '}' ) {
                    if ( $level_in_tokenizer == $indented_if_level ) {
                        $indented_if_level = 0;

                        # TBD: This could be a subroutine call
                        $level_in_tokenizer--;
                        if ( @{$rslevel_stack} > 1 ) {
                            pop( @{$rslevel_stack} );
                        }
                        if ( length($nesting_block_string) > 1 )
                        {    # true for valid script
                            chop $nesting_block_string;
                            chop $nesting_list_string;
                        }

                    }
                }
            }

            my $tok = $rtokens->[$i];  # the token, but ONLY if same as pretoken
            $level_i = $level_in_tokenizer;

            # This can happen by running perltidy on non-scripts
            # although it could also be bug introduced by programming change.
            # Perl silently accepts a 032 (^Z) and takes it as the end
            if ( !$is_valid_token_type{$type} ) {
                my $val = ord($type);
                warning(
                    "unexpected character decimal $val ($type) in script\n");
                $tokenizer_self->{_in_error} = 1;
            }

            # ----------------------------------------------------------------
            # TOKEN TYPE PATCHES
            #  output __END__, __DATA__, and format as type 'k' instead of ';'
            # to make html colors correct, etc.
            my $fix_type = $type;
            if ( $type eq ';' && $tok =~ /\w/ ) { $fix_type = 'k' }

            # output anonymous 'sub' as keyword
            if ( $type eq 't' && $tok eq 'sub' ) { $fix_type = 'k' }

            # -----------------------------------------------------------------

            $nesting_token_string_i = $nesting_token_string;
            $nesting_type_string_i  = $nesting_type_string;
            $nesting_block_string_i = $nesting_block_string;
            $nesting_list_string_i  = $nesting_list_string;

            # set primary indentation levels based on structural braces
            # Note: these are set so that the leading braces have a HIGHER
            # level than their CONTENTS, which is convenient for indentation
            # Also, define continuation indentation for each token.
            if ( $type eq '{' || $type eq 'L' || $forced_indentation_flag > 0 )
            {

                # use environment before updating
                $container_environment =
                    $nesting_block_flag ? 'BLOCK'
                  : $nesting_list_flag  ? 'LIST'
                  :                       "";

                # if the difference between total nesting levels is not 1,
                # there are intervening non-structural nesting types between
                # this '{' and the previous unclosed '{'
                my $intervening_secondary_structure = 0;
                if ( @{$rslevel_stack} ) {
                    $intervening_secondary_structure =
                      $slevel_in_tokenizer - $rslevel_stack->[-1];
                }

     # Continuation Indentation
     #
     # Having tried setting continuation indentation both in the formatter and
     # in the tokenizer, I can say that setting it in the tokenizer is much,
     # much easier.  The formatter already has too much to do, and can't
     # make decisions on line breaks without knowing what 'ci' will be at
     # arbitrary locations.
     #
     # But a problem with setting the continuation indentation (ci) here
     # in the tokenizer is that we do not know where line breaks will actually
     # be.  As a result, we don't know if we should propagate continuation
     # indentation to higher levels of structure.
     #
     # For nesting of only structural indentation, we never need to do this.
     # For example, in a long if statement, like this
     #
     #   if ( !$output_block_type[$i]
     #     && ($in_statement_continuation) )
     #   {           <--outdented
     #       do_something();
     #   }
     #
     # the second line has ci but we do normally give the lines within the BLOCK
     # any ci.  This would be true if we had blocks nested arbitrarily deeply.
     #
     # But consider something like this, where we have created a break after
     # an opening paren on line 1, and the paren is not (currently) a
     # structural indentation token:
     #
     # my $file = $menubar->Menubutton(
     #   qw/-text File -underline 0 -menuitems/ => [
     #       [
     #           Cascade    => '~View',
     #           -menuitems => [
     #           ...
     #
     # The second line has ci, so it would seem reasonable to propagate it
     # down, giving the third line 1 ci + 1 indentation.  This suggests the
     # following rule, which is currently used to propagating ci down: if there
     # are any non-structural opening parens (or brackets, or braces), before
     # an opening structural brace, then ci is propagated down, and otherwise
     # not.  The variable $intervening_secondary_structure contains this
     # information for the current token, and the string
     # "$ci_string_in_tokenizer" is a stack of previous values of this
     # variable.

                # save the current states
                push( @{$rslevel_stack}, 1 + $slevel_in_tokenizer );
                $level_in_tokenizer++;

                if ($forced_indentation_flag) {

                    # break BEFORE '?' when there is forced indentation
                    if ( $type eq '?' ) { $level_i = $level_in_tokenizer; }
                    if ( $type eq 'k' ) {
                        $indented_if_level = $level_in_tokenizer;
                    }

                    # do not change container environment here if we are not
                    # at a real list. Adding this check prevents "blinkers"
                    # often near 'unless" clauses, such as in the following
                    # code:
##          next
##            unless -e (
##                    $archive =
##                      File::Spec->catdir( $_, "auto", $root, "$sub$lib_ext" )
##            );

                    $nesting_block_string .= "$nesting_block_flag";
                }
                else {

                    if ( $routput_block_type->[$i] ) {
                        $nesting_block_flag = 1;
                        $nesting_block_string .= '1';
                    }
                    else {
                        $nesting_block_flag = 0;
                        $nesting_block_string .= '0';
                    }
                }

                # we will use continuation indentation within containers
                # which are not blocks and not logical expressions
                my $bit = 0;
                if ( !$routput_block_type->[$i] ) {

                    # propagate flag down at nested open parens
                    if ( $routput_container_type->[$i] eq '(' ) {
                        $bit = 1 if $nesting_list_flag;
                    }

                  # use list continuation if not a logical grouping
                  # /^(if|elsif|unless|while|and|or|not|&&|!|\|\||for|foreach)$/
                    else {
                        $bit = 1
                          unless
                          $is_logical_container{ $routput_container_type->[$i]
                          };
                    }
                }
                $nesting_list_string .= $bit;
                $nesting_list_flag = $bit;

                $ci_string_in_tokenizer .=
                  ( $intervening_secondary_structure != 0 ) ? '1' : '0';
                $ci_string_sum = ones_count($ci_string_in_tokenizer);
                $continuation_string_in_tokenizer .=
                  ( $in_statement_continuation > 0 ) ? '1' : '0';

   #  Sometimes we want to give an opening brace continuation indentation,
   #  and sometimes not.  For code blocks, we don't do it, so that the leading
   #  '{' gets outdented, like this:
   #
   #   if ( !$output_block_type[$i]
   #     && ($in_statement_continuation) )
   #   {           <--outdented
   #
   #  For other types, we will give them continuation indentation.  For example,
   #  here is how a list looks with the opening paren indented:
   #
   #     @LoL =
   #       ( [ "fred", "barney" ], [ "george", "jane", "elroy" ],
   #         [ "homer", "marge", "bart" ], );
   #
   #  This looks best when 'ci' is one-half of the indentation  (i.e., 2 and 4)

                my $total_ci = $ci_string_sum;
                if (
                    !$routput_block_type->[$i]    # patch: skip for BLOCK
                    && ($in_statement_continuation)
                    && !( $forced_indentation_flag && $type eq ':' )
                  )
                {
                    $total_ci += $in_statement_continuation
                      unless ( $ci_string_in_tokenizer =~ /1$/ );
                }

                $ci_string_i               = $total_ci;
                $in_statement_continuation = 0;
            }

            elsif ($type eq '}'
                || $type eq 'R'
                || $forced_indentation_flag < 0 )
            {

                # only a nesting error in the script would prevent popping here
                if ( @{$rslevel_stack} > 1 ) { pop( @{$rslevel_stack} ); }

                $level_i = --$level_in_tokenizer;

                # restore previous level values
                if ( length($nesting_block_string) > 1 )
                {    # true for valid script
                    chop $nesting_block_string;
                    $nesting_block_flag = ( $nesting_block_string =~ /1$/ );
                    chop $nesting_list_string;
                    $nesting_list_flag = ( $nesting_list_string =~ /1$/ );

                    chop $ci_string_in_tokenizer;
                    $ci_string_sum = ones_count($ci_string_in_tokenizer);

                    $in_statement_continuation =
                      chop $continuation_string_in_tokenizer;

                    # zero continuation flag at terminal BLOCK '}' which
                    # ends a statement.
                    if ( $routput_block_type->[$i] ) {

                        # ...These include non-anonymous subs
                        # note: could be sub ::abc { or sub 'abc
                        if ( $routput_block_type->[$i] =~ m/^sub\s*/gc ) {

                         # note: older versions of perl require the /gc modifier
                         # here or else the \G does not work.
                            if ( $routput_block_type->[$i] =~ /\G('|::|\w)/gc )
                            {
                                $in_statement_continuation = 0;
                            }
                        }

# ...and include all block types except user subs with
# block prototypes and these: (sort|grep|map|do|eval)
# /^(\}|\{|BEGIN|END|CHECK|INIT|AUTOLOAD|DESTROY|UNITCHECK|continue|;|if|elsif|else|unless|while|until|for|foreach)$/
                        elsif (
                            $is_zero_continuation_block_type{
                                $routput_block_type->[$i]
                            } )
                        {
                            $in_statement_continuation = 0;
                        }

                        # ..but these are not terminal types:
                        #     /^(sort|grep|map|do|eval)$/ )
                        elsif (
                            $is_not_zero_continuation_block_type{
                                $routput_block_type->[$i]
                            } )
                        {
                        }

                        # ..and a block introduced by a label
                        # /^\w+\s*:$/gc ) {
                        elsif ( $routput_block_type->[$i] =~ /:$/ ) {
                            $in_statement_continuation = 0;
                        }

                        # user function with block prototype
                        else {
                            $in_statement_continuation = 0;
                        }
                    }

                    # If we are in a list, then
                    # we must set continuation indentation at the closing
                    # paren of something like this (paren after $check):
                    #     assert(
                    #         __LINE__,
                    #         ( not defined $check )
                    #           or ref $check
                    #           or $check eq "new"
                    #           or $check eq "old",
                    #     );
                    elsif ( $tok eq ')' ) {
                        $in_statement_continuation = 1
                          if $routput_container_type->[$i] =~ /^[;,\{\}]$/;
                    }

                    elsif ( $tok eq ';' ) { $in_statement_continuation = 0 }
                }

                # use environment after updating
                $container_environment =
                    $nesting_block_flag ? 'BLOCK'
                  : $nesting_list_flag  ? 'LIST'
                  :                       "";
                $ci_string_i = $ci_string_sum + $in_statement_continuation;
                $nesting_block_string_i = $nesting_block_string;
                $nesting_list_string_i  = $nesting_list_string;
            }

            # not a structural indentation type..
            else {

                $container_environment =
                    $nesting_block_flag ? 'BLOCK'
                  : $nesting_list_flag  ? 'LIST'
                  :                       "";

                # zero the continuation indentation at certain tokens so
                # that they will be at the same level as its container.  For
                # commas, this simplifies the -lp indentation logic, which
                # counts commas.  For ?: it makes them stand out.
                if ($nesting_list_flag) {
                    if ( $type =~ /^[,\?\:]$/ ) {
                        $in_statement_continuation = 0;
                    }
                }

                # be sure binary operators get continuation indentation
                if (
                    $container_environment
                    && (   $type eq 'k' && $is_binary_keyword{$tok}
                        || $is_binary_type{$type} )
                  )
                {
                    $in_statement_continuation = 1;
                }

                # continuation indentation is sum of any open ci from previous
                # levels plus the current level
                $ci_string_i = $ci_string_sum + $in_statement_continuation;

                # update continuation flag ...
                # if this isn't a blank or comment..
                if ( $type ne 'b' && $type ne '#' ) {

                    # and we are in a BLOCK
                    if ($nesting_block_flag) {

                        # the next token after a ';' and label starts a new stmt
                        if ( $type eq ';' || $type eq 'J' ) {
                            $in_statement_continuation = 0;
                        }

                        # otherwise, we are continuing the current statement
                        else {
                            $in_statement_continuation = 1;
                        }
                    }

                    # if we are not in a BLOCK..
                    else {

                        # do not use continuation indentation if not list
                        # environment (could be within if/elsif clause)
                        if ( !$nesting_list_flag ) {
                            $in_statement_continuation = 0;
                        }

                        # otherwise, the token after a ',' starts a new term

                        # Patch FOR RT#99961; no continuation after a ';'
                        # This is needed because perltidy currently marks
                        # a block preceded by a type character like % or @
                        # as a non block, to simplify formatting. But these
                        # are actually blocks and can have semicolons.
                        # See code_block_type() and is_non_structural_brace().
                        elsif ( $type eq ',' || $type eq ';' ) {
                            $in_statement_continuation = 0;
                        }

                        # otherwise, we are continuing the current term
                        else {
                            $in_statement_continuation = 1;
                        }
                    }
                }
            }

            if ( $level_in_tokenizer < 0 ) {
                unless ( $tokenizer_self->{_saw_negative_indentation} ) {
                    $tokenizer_self->{_saw_negative_indentation} = 1;
                    warning("Starting negative indentation\n");
                }
            }

            # set secondary nesting levels based on all containment token types
            # Note: these are set so that the nesting depth is the depth
            # of the PREVIOUS TOKEN, which is convenient for setting
            # the strength of token bonds
            my $slevel_i = $slevel_in_tokenizer;

            #    /^[L\{\(\[]$/
            if ( $is_opening_type{$type} ) {
                $slevel_in_tokenizer++;
                $nesting_token_string .= $tok;
                $nesting_type_string  .= $type;
            }

            #       /^[R\}\)\]]$/
            elsif ( $is_closing_type{$type} ) {
                $slevel_in_tokenizer--;
                my $char = chop $nesting_token_string;

                if ( $char ne $matching_start_token{$tok} ) {
                    $nesting_token_string .= $char . $tok;
                    $nesting_type_string  .= $type;
                }
                else {
                    chop $nesting_type_string;
                }
            }

            push( @block_type,            $routput_block_type->[$i] );
            push( @ci_string,             $ci_string_i );
            push( @container_environment, $container_environment );
            push( @container_type,        $routput_container_type->[$i] );
            push( @levels,                $level_i );
            push( @nesting_tokens,        $nesting_token_string_i );
            push( @nesting_types,         $nesting_type_string_i );
            push( @slevels,               $slevel_i );
            push( @token_type,            $fix_type );
            push( @type_sequence,         $routput_type_sequence->[$i] );
            push( @nesting_blocks,        $nesting_block_string );
            push( @nesting_lists,         $nesting_list_string );

            # now form the previous token
            if ( $im >= 0 ) {
                $num =
                  $rtoken_map->[$i] - $rtoken_map->[$im];  # how many characters

                if ( $num > 0 ) {
                    push( @tokens,
                        substr( $input_line, $rtoken_map->[$im], $num ) );
                }
            }
            $im = $i;
        }

        $num = length($input_line) - $rtoken_map->[$im];   # make the last token
        if ( $num > 0 ) {
            push( @tokens, substr( $input_line, $rtoken_map->[$im], $num ) );
        }

        $tokenizer_self->{_in_attribute_list} = $in_attribute_list;
        $tokenizer_self->{_in_quote}          = $in_quote;
        $tokenizer_self->{_quote_target} =
          $in_quote ? matching_end_token($quote_character) : "";
        $tokenizer_self->{_rhere_target_list} = $rhere_target_list;

        $line_of_tokens->{_rtoken_type}            = \@token_type;
        $line_of_tokens->{_rtokens}                = \@tokens;
        $line_of_tokens->{_rblock_type}            = \@block_type;
        $line_of_tokens->{_rcontainer_type}        = \@container_type;
        $line_of_tokens->{_rcontainer_environment} = \@container_environment;
        $line_of_tokens->{_rtype_sequence}         = \@type_sequence;
        $line_of_tokens->{_rlevels}                = \@levels;
        $line_of_tokens->{_rslevels}               = \@slevels;
        $line_of_tokens->{_rnesting_tokens}        = \@nesting_tokens;
        $line_of_tokens->{_rci_levels}             = \@ci_string;
        $line_of_tokens->{_rnesting_blocks}        = \@nesting_blocks;

        return;
    }
}    # end tokenize_this_line

#########i#############################################################
# Tokenizer routines which assist in identifying token types
#######################################################################

sub operator_expected {

    # Many perl symbols have two or more meanings.  For example, '<<'
    # can be a shift operator or a here-doc operator.  The
    # interpretation of these symbols depends on the current state of
    # the tokenizer, which may either be expecting a term or an
    # operator.  For this example, a << would be a shift if an operator
    # is expected, and a here-doc if a term is expected.  This routine
    # is called to make this decision for any current token.  It returns
    # one of three possible values:
    #
    #     OPERATOR - operator expected (or at least, not a term)
    #     UNKNOWN  - can't tell
    #     TERM     - a term is expected (or at least, not an operator)
    #
    # The decision is based on what has been seen so far.  This
    # information is stored in the "$last_nonblank_type" and
    # "$last_nonblank_token" variables.  For example, if the
    # $last_nonblank_type is '=~', then we are expecting a TERM, whereas
    # if $last_nonblank_type is 'n' (numeric), we are expecting an
    # OPERATOR.
    #
    # If a UNKNOWN is returned, the calling routine must guess. A major
    # goal of this tokenizer is to minimize the possibility of returning
    # UNKNOWN, because a wrong guess can spoil the formatting of a
    # script.
    #
    # adding NEW_TOKENS: it is critically important that this routine be
    # updated to allow it to determine if an operator or term is to be
    # expected after the new token.  Doing this simply involves adding
    # the new token character to one of the regexes in this routine or
    # to one of the hash lists
    # that it uses, which are initialized in the BEGIN section.
    # USES GLOBAL VARIABLES: $last_nonblank_type, $last_nonblank_token,
    # $statement_type

    my ( $prev_type, $tok, $next_type ) = @_;

    my $op_expected = UNKNOWN;

##print "tok=$tok last type=$last_nonblank_type last tok=$last_nonblank_token\n";

# Note: function prototype is available for token type 'U' for future
# program development.  It contains the leading and trailing parens,
# and no blanks.  It might be used to eliminate token type 'C', for
# example (prototype = '()'). Thus:
# if ($last_nonblank_type eq 'U') {
#     print "previous token=$last_nonblank_token  type=$last_nonblank_type prototype=$last_nonblank_prototype\n";
# }

    # A possible filehandle (or object) requires some care...
    if ( $last_nonblank_type eq 'Z' ) {

        # angle.t
        if ( $last_nonblank_token =~ /^[A-Za-z_]/ ) {
            $op_expected = UNKNOWN;
        }

        # For possible file handle like "$a", Perl uses weird parsing rules.
        # For example:
        # print $a/2,"/hi";   - division
        # print $a / 2,"/hi"; - division
        # print $a/ 2,"/hi";  - division
        # print $a /2,"/hi";  - pattern (and error)!
        elsif ( ( $prev_type eq 'b' ) && ( $next_type ne 'b' ) ) {
            $op_expected = TERM;
        }

        # Note when an operation is being done where a
        # filehandle might be expected, since a change in whitespace
        # could change the interpretation of the statement.
        else {
            if ( $tok =~ /^([x\/\+\-\*\%\&\.\?\<]|\>\>)$/ ) {
                complain("operator in print statement not recommended\n");
                $op_expected = OPERATOR;
            }
        }
    }

    # Check for smartmatch operator before preceding brace or square bracket.
    # For example, at the ? after the ] in the following expressions we are
    # expecting an operator:
    #
    # qr/3/ ~~ ['1234'] ? 1 : 0;
    # map { $_ ~~ [ '0', '1' ] ? 'x' : 'o' } @a;
    elsif ( $last_nonblank_type eq '}' && $last_nonblank_token eq '~~' ) {
        $op_expected = OPERATOR;
    }

    # handle something after 'do' and 'eval'
    elsif ( $is_block_operator{$last_nonblank_token} ) {

        # something like $a = eval "expression";
        #                          ^
        if ( $last_nonblank_type eq 'k' ) {
            $op_expected = TERM;    # expression or list mode following keyword
        }

        # something like $a = do { BLOCK } / 2;
        # or this ? after a smartmatch anonynmous hash or array reference:
        #   qr/3/ ~~ ['1234'] ? 1 : 0;
        #                                  ^
        else {
            $op_expected = OPERATOR;    # block mode following }
        }
    }

    # handle bare word..
    elsif ( $last_nonblank_type eq 'w' ) {

        # unfortunately, we can't tell what type of token to expect next
        # after most bare words
        $op_expected = UNKNOWN;
    }

    # operator, but not term possible after these types
    # Note: moved ')' from type to token because parens in list context
    # get marked as '{' '}' now.  This is a minor glitch in the following:
    #    my %opts = (ref $_[0] eq 'HASH') ? %{shift()} : ();
    #
    elsif (( $last_nonblank_type =~ /^[\]RnviQh]$/ )
        || ( $last_nonblank_token =~ /^(\)|\$|\-\>)/ ) )
    {
        $op_expected = OPERATOR;

        # in a 'use' statement, numbers and v-strings are not true
        # numbers, so to avoid incorrect error messages, we will
        # mark them as unknown for now (use.t)
        # TODO: it would be much nicer to create a new token V for VERSION
        # number in a use statement.  Then this could be a check on type V
        # and related patches which change $statement_type for '=>'
        # and ',' could be removed.  Further, it would clean things up to
        # scan the 'use' statement with a separate subroutine.
        if (   ( $statement_type eq 'use' )
            && ( $last_nonblank_type =~ /^[nv]$/ ) )
        {
            $op_expected = UNKNOWN;
        }

        # expecting VERSION or {} after package NAMESPACE
        elsif ($statement_type =~ /^package\b/
            && $last_nonblank_token =~ /^package\b/ )
        {
            $op_expected = TERM;
        }
    }

    # no operator after many keywords, such as "die", "warn", etc
    elsif ( $expecting_term_token{$last_nonblank_token} ) {

        # // may follow perl functions which may be unary operators
        # see test file dor.t (defined or);
        if (   $tok eq '/'
            && $next_type eq '/'
            && $last_nonblank_type eq 'k'
            && $is_keyword_taking_optional_args{$last_nonblank_token} )
        {
            $op_expected = OPERATOR;
        }
        else {
            $op_expected = TERM;
        }
    }

    # no operator after things like + - **  (i.e., other operators)
    elsif ( $expecting_term_types{$last_nonblank_type} ) {
        $op_expected = TERM;
    }

    # a few operators, like "time", have an empty prototype () and so
    # take no parameters but produce a value to operate on
    elsif ( $expecting_operator_token{$last_nonblank_token} ) {
        $op_expected = OPERATOR;
    }

    # post-increment and decrement produce values to be operated on
    elsif ( $expecting_operator_types{$last_nonblank_type} ) {
        $op_expected = OPERATOR;
    }

    # no value to operate on after sub block
    elsif ( $last_nonblank_token =~ /^sub\s/ ) { $op_expected = TERM; }

    # a right brace here indicates the end of a simple block.
    # all non-structural right braces have type 'R'
    # all braces associated with block operator keywords have been given those
    # keywords as "last_nonblank_token" and caught above.
    # (This statement is order dependent, and must come after checking
    # $last_nonblank_token).
    elsif ( $last_nonblank_type eq '}' ) {

        # patch for dor.t (defined or).
        if (   $tok eq '/'
            && $next_type eq '/'
            && $last_nonblank_token eq ']' )
        {
            $op_expected = OPERATOR;
        }

        # Patch for RT #116344: misparse a ternary operator after an anonymous
        # hash, like this:
        #   return ref {} ? 1 : 0;
        # The right brace should really be marked type 'R' in this case, and
        # it is safest to return an UNKNOWN here. Expecting a TERM will
        # cause the '?' to always be interpreted as a pattern delimiter
        # rather than introducing a ternary operator.
        elsif ( $tok eq '?' ) {
            $op_expected = UNKNOWN;
        }
        else {
            $op_expected = TERM;
        }
    }

    # something else..what did I forget?
    else {

        # collecting diagnostics on unknown operator types..see what was missed
        $op_expected = UNKNOWN;
        write_diagnostics(
"OP: unknown after type=$last_nonblank_type  token=$last_nonblank_token\n"
        );
    }

    TOKENIZER_DEBUG_FLAG_EXPECT && do {
        print STDOUT
"EXPECT: returns $op_expected for last type $last_nonblank_type token $last_nonblank_token\n";
    };
    return $op_expected;
}

sub new_statement_ok {

    # return true if the current token can start a new statement
    # USES GLOBAL VARIABLES: $last_nonblank_type

    return label_ok()    # a label would be ok here

      || $last_nonblank_type eq 'J';    # or we follow a label

}

sub label_ok {

    # Decide if a bare word followed by a colon here is a label
    # USES GLOBAL VARIABLES: $last_nonblank_token, $last_nonblank_type,
    # $brace_depth, @brace_type

    # if it follows an opening or closing code block curly brace..
    if ( ( $last_nonblank_token eq '{' || $last_nonblank_token eq '}' )
        && $last_nonblank_type eq $last_nonblank_token )
    {

        # it is a label if and only if the curly encloses a code block
        return $brace_type[$brace_depth];
    }

    # otherwise, it is a label if and only if it follows a ';' (real or fake)
    # or another label
    else {
        return ( $last_nonblank_type eq ';' || $last_nonblank_type eq 'J' );
    }
}

sub code_block_type {

    # Decide if this is a block of code, and its type.
    # Must be called only when $type = $token = '{'
    # The problem is to distinguish between the start of a block of code
    # and the start of an anonymous hash reference
    # Returns "" if not code block, otherwise returns 'last_nonblank_token'
    # to indicate the type of code block.  (For example, 'last_nonblank_token'
    # might be 'if' for an if block, 'else' for an else block, etc).
    # USES GLOBAL VARIABLES: $last_nonblank_token, $last_nonblank_type,
    # $last_nonblank_block_type, $brace_depth, @brace_type

    # handle case of multiple '{'s

# print "BLOCK_TYPE EXAMINING: type=$last_nonblank_type tok=$last_nonblank_token\n";

    my ( $i, $rtokens, $rtoken_type, $max_token_index ) = @_;
    if (   $last_nonblank_token eq '{'
        && $last_nonblank_type eq $last_nonblank_token )
    {

        # opening brace where a statement may appear is probably
        # a code block but might be and anonymous hash reference
        if ( $brace_type[$brace_depth] ) {
            return decide_if_code_block( $i, $rtokens, $rtoken_type,
                $max_token_index );
        }

        # cannot start a code block within an anonymous hash
        else {
            return "";
        }
    }

    elsif ( $last_nonblank_token eq ';' ) {

        # an opening brace where a statement may appear is probably
        # a code block but might be and anonymous hash reference
        return decide_if_code_block( $i, $rtokens, $rtoken_type,
            $max_token_index );
    }

    # handle case of '}{'
    elsif ($last_nonblank_token eq '}'
        && $last_nonblank_type eq $last_nonblank_token )
    {

        # a } { situation ...
        # could be hash reference after code block..(blktype1.t)
        if ($last_nonblank_block_type) {
            return decide_if_code_block( $i, $rtokens, $rtoken_type,
                $max_token_index );
        }

        # must be a block if it follows a closing hash reference
        else {
            return $last_nonblank_token;
        }
    }

    ################################################################
    # NOTE: braces after type characters start code blocks, but for
    # simplicity these are not identified as such.  See also
    # sub is_non_structural_brace.
    ################################################################

##    elsif ( $last_nonblank_type eq 't' ) {
##       return $last_nonblank_token;
##    }

    # brace after label:
    elsif ( $last_nonblank_type eq 'J' ) {
        return $last_nonblank_token;
    }

# otherwise, look at previous token.  This must be a code block if
# it follows any of these:
# /^(BEGIN|END|CHECK|INIT|AUTOLOAD|DESTROY|UNITCHECK|continue|if|elsif|else|unless|do|while|until|eval|for|foreach|map|grep|sort)$/
    elsif ( $is_code_block_token{$last_nonblank_token} ) {

        # Bug Patch: Note that the opening brace after the 'if' in the following
        # snippet is an anonymous hash ref and not a code block!
        #   print 'hi' if { x => 1, }->{x};
        # We can identify this situation because the last nonblank type
        # will be a keyword (instead of a closing peren)
        if (   $last_nonblank_token =~ /^(if|unless)$/
            && $last_nonblank_type eq 'k' )
        {
            return "";
        }
        else {
            return $last_nonblank_token;
        }
    }

    # or a sub or package BLOCK
    elsif ( ( $last_nonblank_type eq 'i' || $last_nonblank_type eq 't' )
        && $last_nonblank_token =~ /^(sub|package)\b/ )
    {
        return $last_nonblank_token;
    }

    elsif ( $statement_type =~ /^(sub|package)\b/ ) {
        return $statement_type;
    }

    # user-defined subs with block parameters (like grep/map/eval)
    elsif ( $last_nonblank_type eq 'G' ) {
        return $last_nonblank_token;
    }

    # check bareword
    elsif ( $last_nonblank_type eq 'w' ) {
        return decide_if_code_block( $i, $rtokens, $rtoken_type,
            $max_token_index );
    }

    # Patch for bug # RT #94338 reported by Daniel Trizen
    # for-loop in a parenthesized block-map triggering an error message:
    #    map( { foreach my $item ( '0', '1' ) { print $item} } qw(a b c) );
    # Check for a code block within a parenthesized function call
    elsif ( $last_nonblank_token eq '(' ) {
        my $paren_type = $paren_type[$paren_depth];
        if ( $paren_type && $paren_type =~ /^(map|grep|sort)$/ ) {

            # We will mark this as a code block but use type 't' instead
            # of the name of the contining function.  This will allow for
            # correct parsing but will usually produce better formatting.
            # Braces with block type 't' are not broken open automatically
            # in the formatter as are other code block types, and this usually
            # works best.
            return 't';    # (Not $paren_type)
        }
        else {
            return "";
        }
    }

    # handle unknown syntax ') {'
    # we previously appended a '()' to mark this case
    elsif ( $last_nonblank_token =~ /\(\)$/ ) {
        return $last_nonblank_token;
    }

    # anything else must be anonymous hash reference
    else {
        return "";
    }
}

sub decide_if_code_block {

    # USES GLOBAL VARIABLES: $last_nonblank_token
    my ( $i, $rtokens, $rtoken_type, $max_token_index ) = @_;

    my ( $next_nonblank_token, $i_next ) =
      find_next_nonblank_token( $i, $rtokens, $max_token_index );

    # we are at a '{' where a statement may appear.
    # We must decide if this brace starts an anonymous hash or a code
    # block.
    # return "" if anonymous hash, and $last_nonblank_token otherwise

    # initialize to be code BLOCK
    my $code_block_type = $last_nonblank_token;

    # Check for the common case of an empty anonymous hash reference:
    # Maybe something like sub { { } }
    if ( $next_nonblank_token eq '}' ) {
        $code_block_type = "";
    }

    else {

        # To guess if this '{' is an anonymous hash reference, look ahead
        # and test as follows:
        #
        # it is a hash reference if next come:
        #   - a string or digit followed by a comma or =>
        #   - bareword followed by =>
        # otherwise it is a code block
        #
        # Examples of anonymous hash ref:
        # {'aa',};
        # {1,2}
        #
        # Examples of code blocks:
        # {1; print "hello\n", 1;}
        # {$a,1};

        # We are only going to look ahead one more (nonblank/comment) line.
        # Strange formatting could cause a bad guess, but that's unlikely.
        my @pre_types;
        my @pre_tokens;

        # Ignore the rest of this line if it is a side comment
        if ( $next_nonblank_token ne '#' ) {
            @pre_types  = @{$rtoken_type}[ $i + 1 .. $max_token_index ];
            @pre_tokens = @{$rtokens}[ $i + 1 .. $max_token_index ];
        }
        my ( $rpre_tokens, $rpre_types ) =
          peek_ahead_for_n_nonblank_pre_tokens(20);    # 20 is arbitrary but
                                                       # generous, and prevents
                                                       # wasting lots of
                                                       # time in mangled files
        if ( defined($rpre_types) && @{$rpre_types} ) {
            push @pre_types,  @{$rpre_types};
            push @pre_tokens, @{$rpre_tokens};
        }

        # put a sentinel token to simplify stopping the search
        push @pre_types, '}';
        push @pre_types, '}';

        my $jbeg = 0;
        $jbeg = 1 if $pre_types[0] eq 'b';

        # first look for one of these
        #  - bareword
        #  - bareword with leading -
        #  - digit
        #  - quoted string
        my $j = $jbeg;
        if ( $pre_types[$j] =~ /^[\'\"]/ ) {

            # find the closing quote; don't worry about escapes
            my $quote_mark = $pre_types[$j];
            foreach my $k ( $j + 1 .. $#pre_types - 1 ) {
                if ( $pre_types[$k] eq $quote_mark ) {
                    $j = $k + 1;
                    my $next = $pre_types[$j];
                    last;
                }
            }
        }
        elsif ( $pre_types[$j] eq 'd' ) {
            $j++;
        }
        elsif ( $pre_types[$j] eq 'w' ) {
            $j++;
        }
        elsif ( $pre_types[$j] eq '-' && $pre_types[ ++$j ] eq 'w' ) {
            $j++;
        }
        if ( $j > $jbeg ) {

            $j++ if $pre_types[$j] eq 'b';

            # Patched for RT #95708
            if (

                # it is a comma which is not a pattern delimeter except for qw
                (
                       $pre_types[$j] eq ','
                    && $pre_tokens[$jbeg] !~ /^(s|m|y|tr|qr|q|qq|qx)$/
                )

                # or a =>
                || ( $pre_types[$j] eq '=' && $pre_types[ ++$j ] eq '>' )
              )
            {
                $code_block_type = "";
            }
        }
    }

    return $code_block_type;
}

sub report_unexpected {

    # report unexpected token type and show where it is
    # USES GLOBAL VARIABLES: $tokenizer_self
    my ( $found, $expecting, $i_tok, $last_nonblank_i, $rpretoken_map,
        $rpretoken_type, $input_line )
      = @_;

    if ( ++$tokenizer_self->{_unexpected_error_count} <= MAX_NAG_MESSAGES ) {
        my $msg = "found $found where $expecting expected";
        my $pos = $rpretoken_map->[$i_tok];
        interrupt_logfile();
        my $input_line_number = $tokenizer_self->{_last_line_number};
        my ( $offset, $numbered_line, $underline ) =
          make_numbered_line( $input_line_number, $input_line, $pos );
        $underline = write_on_underline( $underline, $pos - $offset, '^' );

        my $trailer = "";
        if ( ( $i_tok > 0 ) && ( $last_nonblank_i >= 0 ) ) {
            my $pos_prev = $rpretoken_map->[$last_nonblank_i];
            my $num;
            if ( $rpretoken_type->[ $i_tok - 1 ] eq 'b' ) {
                $num = $rpretoken_map->[ $i_tok - 1 ] - $pos_prev;
            }
            else {
                $num = $pos - $pos_prev;
            }
            if ( $num > 40 ) { $num = 40; $pos_prev = $pos - 40; }

            $underline =
              write_on_underline( $underline, $pos_prev - $offset, '-' x $num );
            $trailer = " (previous token underlined)";
        }
        warning( $numbered_line . "\n" );
        warning( $underline . "\n" );
        warning( $msg . $trailer . "\n" );
        resume_logfile();
    }
    return;
}

sub is_non_structural_brace {

    # Decide if a brace or bracket is structural or non-structural
    # by looking at the previous token and type
    # USES GLOBAL VARIABLES: $last_nonblank_type, $last_nonblank_token

    # EXPERIMENTAL: Mark slices as structural; idea was to improve formatting.
    # Tentatively deactivated because it caused the wrong operator expectation
    # for this code:
    #      $user = @vars[1] / 100;
    # Must update sub operator_expected before re-implementing.
    # if ( $last_nonblank_type eq 'i' && $last_nonblank_token =~ /^@/ ) {
    #    return 0;
    # }

    ################################################################
    # NOTE: braces after type characters start code blocks, but for
    # simplicity these are not identified as such.  See also
    # sub code_block_type
    ################################################################

    ##if ($last_nonblank_type eq 't') {return 0}

    # otherwise, it is non-structural if it is decorated
    # by type information.
    # For example, the '{' here is non-structural:   ${xxx}
    return (
        $last_nonblank_token =~ /^([\$\@\*\&\%\)]|->|::)/

          # or if we follow a hash or array closing curly brace or bracket
          # For example, the second '{' in this is non-structural: $a{'x'}{'y'}
          # because the first '}' would have been given type 'R'
          || $last_nonblank_type =~ /^([R\]])$/
    );
}

#########i#############################################################
# Tokenizer routines for tracking container nesting depths
#######################################################################

# The following routines keep track of nesting depths of the nesting
# types, ( [ { and ?.  This is necessary for determining the indentation
# level, and also for debugging programs.  Not only do they keep track of
# nesting depths of the individual brace types, but they check that each
# of the other brace types is balanced within matching pairs.  For
# example, if the program sees this sequence:
#
#         {  ( ( ) }
#
# then it can determine that there is an extra left paren somewhere
# between the { and the }.  And so on with every other possible
# combination of outer and inner brace types.  For another
# example:
#
#         ( [ ..... ]  ] )
#
# which has an extra ] within the parens.
#
# The brace types have indexes 0 .. 3 which are indexes into
# the matrices.
#
# The pair ? : are treated as just another nesting type, with ? acting
# as the opening brace and : acting as the closing brace.
#
# The matrix
#
#         $depth_array[$a][$b][ $current_depth[$a] ] = $current_depth[$b];
#
# saves the nesting depth of brace type $b (where $b is either of the other
# nesting types) when brace type $a enters a new depth.  When this depth
# decreases, a check is made that the current depth of brace types $b is
# unchanged, or otherwise there must have been an error.  This can
# be very useful for localizing errors, particularly when perl runs to
# the end of a large file (such as this one) and announces that there
# is a problem somewhere.
#
# A numerical sequence number is maintained for every nesting type,
# so that each matching pair can be uniquely identified in a simple
# way.

sub increase_nesting_depth {
    my ( $aa, $pos ) = @_;

    # USES GLOBAL VARIABLES: $tokenizer_self, @current_depth,
    # @current_sequence_number, @depth_array, @starting_line_of_current_depth,
    # $statement_type
    $current_depth[$aa]++;
    $total_depth++;
    $total_depth[$aa][ $current_depth[$aa] ] = $total_depth;
    my $input_line_number = $tokenizer_self->{_last_line_number};
    my $input_line        = $tokenizer_self->{_line_text};

    # Sequence numbers increment by number of items.  This keeps
    # a unique set of numbers but still allows the relative location
    # of any type to be determined.
    $nesting_sequence_number[$aa] += scalar(@closing_brace_names);
    my $seqno = $nesting_sequence_number[$aa];
    $current_sequence_number[$aa][ $current_depth[$aa] ] = $seqno;

    $starting_line_of_current_depth[$aa][ $current_depth[$aa] ] =
      [ $input_line_number, $input_line, $pos ];

    for my $bb ( 0 .. $#closing_brace_names ) {
        next if ( $bb == $aa );
        $depth_array[$aa][$bb][ $current_depth[$aa] ] = $current_depth[$bb];
    }

    # set a flag for indenting a nested ternary statement
    my $indent = 0;
    if ( $aa == QUESTION_COLON ) {
        $nested_ternary_flag[ $current_depth[$aa] ] = 0;
        if ( $current_depth[$aa] > 1 ) {
            if ( $nested_ternary_flag[ $current_depth[$aa] - 1 ] == 0 ) {
                my $pdepth = $total_depth[$aa][ $current_depth[$aa] - 1 ];
                if ( $pdepth == $total_depth - 1 ) {
                    $indent = 1;
                    $nested_ternary_flag[ $current_depth[$aa] - 1 ] = -1;
                }
            }
        }
    }
    $nested_statement_type[$aa][ $current_depth[$aa] ] = $statement_type;
    $statement_type = "";
    return ( $seqno, $indent );
}

sub decrease_nesting_depth {

    my ( $aa, $pos ) = @_;

    # USES GLOBAL VARIABLES: $tokenizer_self, @current_depth,
    # @current_sequence_number, @depth_array, @starting_line_of_current_depth
    # $statement_type
    my $seqno             = 0;
    my $input_line_number = $tokenizer_self->{_last_line_number};
    my $input_line        = $tokenizer_self->{_line_text};

    my $outdent = 0;
    $total_depth--;
    if ( $current_depth[$aa] > 0 ) {

        # set a flag for un-indenting after seeing a nested ternary statement
        $seqno = $current_sequence_number[$aa][ $current_depth[$aa] ];
        if ( $aa == QUESTION_COLON ) {
            $outdent = $nested_ternary_flag[ $current_depth[$aa] ];
        }
        $statement_type = $nested_statement_type[$aa][ $current_depth[$aa] ];

        # check that any brace types $bb contained within are balanced
        for my $bb ( 0 .. $#closing_brace_names ) {
            next if ( $bb == $aa );

            unless ( $depth_array[$aa][$bb][ $current_depth[$aa] ] ==
                $current_depth[$bb] )
            {
                my $diff =
                  $current_depth[$bb] -
                  $depth_array[$aa][$bb][ $current_depth[$aa] ];

                # don't whine too many times
                my $saw_brace_error = get_saw_brace_error();
                if (
                    $saw_brace_error <= MAX_NAG_MESSAGES

                    # if too many closing types have occurred, we probably
                    # already caught this error
                    && ( ( $diff > 0 ) || ( $saw_brace_error <= 0 ) )
                  )
                {
                    interrupt_logfile();
                    my $rsl =
                      $starting_line_of_current_depth[$aa]
                      [ $current_depth[$aa] ];
                    my $sl  = $rsl->[0];
                    my $rel = [ $input_line_number, $input_line, $pos ];
                    my $el  = $rel->[0];
                    my ($ess);

                    if ( $diff == 1 || $diff == -1 ) {
                        $ess = '';
                    }
                    else {
                        $ess = 's';
                    }
                    my $bname =
                      ( $diff > 0 )
                      ? $opening_brace_names[$bb]
                      : $closing_brace_names[$bb];
                    write_error_indicator_pair( @{$rsl}, '^' );
                    my $msg = <<"EOM";
Found $diff extra $bname$ess between $opening_brace_names[$aa] on line $sl and $closing_brace_names[$aa] on line $el
EOM

                    if ( $diff > 0 ) {
                        my $rml =
                          $starting_line_of_current_depth[$bb]
                          [ $current_depth[$bb] ];
                        my $ml = $rml->[0];
                        $msg .=
"    The most recent un-matched $bname is on line $ml\n";
                        write_error_indicator_pair( @{$rml}, '^' );
                    }
                    write_error_indicator_pair( @{$rel}, '^' );
                    warning($msg);
                    resume_logfile();
                }
                increment_brace_error();
            }
        }
        $current_depth[$aa]--;
    }
    else {

        my $saw_brace_error = get_saw_brace_error();
        if ( $saw_brace_error <= MAX_NAG_MESSAGES ) {
            my $msg = <<"EOM";
There is no previous $opening_brace_names[$aa] to match a $closing_brace_names[$aa] on line $input_line_number
EOM
            indicate_error( $msg, $input_line_number, $input_line, $pos, '^' );
        }
        increment_brace_error();
    }
    return ( $seqno, $outdent );
}

sub check_final_nesting_depths {

    # USES GLOBAL VARIABLES: @current_depth, @starting_line_of_current_depth

    for my $aa ( 0 .. $#closing_brace_names ) {

        if ( $current_depth[$aa] ) {
            my $rsl =
              $starting_line_of_current_depth[$aa][ $current_depth[$aa] ];
            my $sl  = $rsl->[0];
            my $msg = <<"EOM";
Final nesting depth of $opening_brace_names[$aa]s is $current_depth[$aa]
The most recent un-matched $opening_brace_names[$aa] is on line $sl
EOM
            indicate_error( $msg, @{$rsl}, '^' );
            increment_brace_error();
        }
    }
    return;
}

#########i#############################################################
# Tokenizer routines for looking ahead in input stream
#######################################################################

sub peek_ahead_for_n_nonblank_pre_tokens {

    # returns next n pretokens if they exist
    # returns undef's if hits eof without seeing any pretokens
    # USES GLOBAL VARIABLES: $tokenizer_self
    my $max_pretokens = shift;
    my $line;
    my $i = 0;
    my ( $rpre_tokens, $rmap, $rpre_types );

    while ( $line = $tokenizer_self->{_line_buffer_object}->peek_ahead( $i++ ) )
    {
        $line =~ s/^\s*//;    # trim leading blanks
        next if ( length($line) <= 0 );    # skip blank
        next if ( $line =~ /^#/ );         # skip comment
        ( $rpre_tokens, $rmap, $rpre_types ) =
          pre_tokenize( $line, $max_pretokens );
        last;
    }
    return ( $rpre_tokens, $rpre_types );
}

# look ahead for next non-blank, non-comment line of code
sub peek_ahead_for_nonblank_token {

    # USES GLOBAL VARIABLES: $tokenizer_self
    my ( $rtokens, $max_token_index ) = @_;
    my $line;
    my $i = 0;

    while ( $line = $tokenizer_self->{_line_buffer_object}->peek_ahead( $i++ ) )
    {
        $line =~ s/^\s*//;    # trim leading blanks
        next if ( length($line) <= 0 );    # skip blank
        next if ( $line =~ /^#/ );         # skip comment
        my ( $rtok, $rmap, $rtype ) =
          pre_tokenize( $line, 2 );        # only need 2 pre-tokens
        my $j = $max_token_index + 1;

        foreach my $tok ( @{$rtok} ) {
            last if ( $tok =~ "\n" );
            $rtokens->[ ++$j ] = $tok;
        }
        last;
    }
    return $rtokens;
}

#########i#############################################################
# Tokenizer guessing routines for ambiguous situations
#######################################################################

sub guess_if_pattern_or_conditional {

    # this routine is called when we have encountered a ? following an
    # unknown bareword, and we must decide if it starts a pattern or not
    # input parameters:
    #   $i - token index of the ? starting possible pattern
    # output parameters:
    #   $is_pattern = 0 if probably not pattern,  =1 if probably a pattern
    #   msg = a warning or diagnostic message
    # USES GLOBAL VARIABLES: $last_nonblank_token

    # FIXME: this needs to be rewritten

    my ( $i, $rtokens, $rtoken_map, $max_token_index ) = @_;
    my $is_pattern = 0;
    my $msg        = "guessing that ? after $last_nonblank_token starts a ";

    if ( $i >= $max_token_index ) {
        $msg .= "conditional (no end to pattern found on the line)\n";
    }
    else {
        my $ibeg = $i;
        $i = $ibeg + 1;
        my $next_token = $rtokens->[$i];    # first token after ?

        # look for a possible ending ? on this line..
        my $in_quote        = 1;
        my $quote_depth     = 0;
        my $quote_character = '';
        my $quote_pos       = 0;
        my $quoted_string;
        (
            $i, $in_quote, $quote_character, $quote_pos, $quote_depth,
            $quoted_string
          )
          = follow_quoted_string( $ibeg, $in_quote, $rtokens, $quote_character,
            $quote_pos, $quote_depth, $max_token_index );

        if ($in_quote) {

            # we didn't find an ending ? on this line,
            # so we bias towards conditional
            $is_pattern = 0;
            $msg .= "conditional (no ending ? on this line)\n";

            # we found an ending ?, so we bias towards a pattern
        }
        else {

            # Watch out for an ending ? in quotes, like this
            #    my $case_flag = File::Spec->case_tolerant ? '(?i)' : '';
            my $s_quote = 0;
            my $d_quote = 0;
            my $colons  = 0;
            foreach my $ii ( $ibeg + 1 .. $i - 1 ) {
                my $tok = $rtokens->[$ii];
                if ( $tok eq ":" ) { $colons++ }
                if ( $tok eq "'" ) { $s_quote++ }
                if ( $tok eq '"' ) { $d_quote++ }
            }
            if ( $s_quote % 2 || $d_quote % 2 || $colons ) {
                $is_pattern = 0;
                $msg .= "found ending ? but unbalanced quote chars\n";
            }
            elsif ( pattern_expected( $i, $rtokens, $max_token_index ) >= 0 ) {
                $is_pattern = 1;
                $msg .= "pattern (found ending ? and pattern expected)\n";
            }
            else {
                $msg .= "pattern (uncertain, but found ending ?)\n";
            }
        }
    }
    return ( $is_pattern, $msg );
}

sub guess_if_pattern_or_division {

    # this routine is called when we have encountered a / following an
    # unknown bareword, and we must decide if it starts a pattern or is a
    # division
    # input parameters:
    #   $i - token index of the / starting possible pattern
    # output parameters:
    #   $is_pattern = 0 if probably division,  =1 if probably a pattern
    #   msg = a warning or diagnostic message
    # USES GLOBAL VARIABLES: $last_nonblank_token
    my ( $i, $rtokens, $rtoken_map, $max_token_index ) = @_;
    my $is_pattern = 0;
    my $msg        = "guessing that / after $last_nonblank_token starts a ";

    if ( $i >= $max_token_index ) {
        $msg .= "division (no end to pattern found on the line)\n";
    }
    else {
        my $ibeg = $i;
        my $divide_expected =
          numerator_expected( $i, $rtokens, $max_token_index );
        $i = $ibeg + 1;
        my $next_token = $rtokens->[$i];    # first token after slash

        # look for a possible ending / on this line..
        my $in_quote        = 1;
        my $quote_depth     = 0;
        my $quote_character = '';
        my $quote_pos       = 0;
        my $quoted_string;
        (
            $i, $in_quote, $quote_character, $quote_pos, $quote_depth,
            $quoted_string
          )
          = follow_quoted_string( $ibeg, $in_quote, $rtokens, $quote_character,
            $quote_pos, $quote_depth, $max_token_index );

        if ($in_quote) {

            # we didn't find an ending / on this line,
            # so we bias towards division
            if ( $divide_expected >= 0 ) {
                $is_pattern = 0;
                $msg .= "division (no ending / on this line)\n";
            }
            else {
                $msg        = "multi-line pattern (division not possible)\n";
                $is_pattern = 1;
            }

        }

        # we found an ending /, so we bias towards a pattern
        else {

            if ( pattern_expected( $i, $rtokens, $max_token_index ) >= 0 ) {

                if ( $divide_expected >= 0 ) {

                    if ( $i - $ibeg > 60 ) {
                        $msg .= "division (matching / too distant)\n";
                        $is_pattern = 0;
                    }
                    else {
                        $msg .= "pattern (but division possible too)\n";
                        $is_pattern = 1;
                    }
                }
                else {
                    $is_pattern = 1;
                    $msg .= "pattern (division not possible)\n";
                }
            }
            else {

                if ( $divide_expected >= 0 ) {
                    $is_pattern = 0;
                    $msg .= "division (pattern not possible)\n";
                }
                else {
                    $is_pattern = 1;
                    $msg .=
                      "pattern (uncertain, but division would not work here)\n";
                }
            }
        }
    }
    return ( $is_pattern, $msg );
}

# try to resolve here-doc vs. shift by looking ahead for
# non-code or the end token (currently only looks for end token)
# returns 1 if it is probably a here doc, 0 if not
sub guess_if_here_doc {

    # This is how many lines we will search for a target as part of the
    # guessing strategy.  It is a constant because there is probably
    # little reason to change it.
    # USES GLOBAL VARIABLES: $tokenizer_self, $current_package
    # %is_constant,
    my $HERE_DOC_WINDOW = 40;

    my $next_token        = shift;
    my $here_doc_expected = 0;
    my $line;
    my $k   = 0;
    my $msg = "checking <<";

    while ( $line = $tokenizer_self->{_line_buffer_object}->peek_ahead( $k++ ) )
    {
        chomp $line;

        if ( $line =~ /^$next_token$/ ) {
            $msg .= " -- found target $next_token ahead $k lines\n";
            $here_doc_expected = 1;    # got it
            last;
        }
        last if ( $k >= $HERE_DOC_WINDOW );
    }

    unless ($here_doc_expected) {

        if ( !defined($line) ) {
            $here_doc_expected = -1;    # hit eof without seeing target
            $msg .= " -- must be shift; target $next_token not in file\n";

        }
        else {                          # still unsure..taking a wild guess

            if ( !$is_constant{$current_package}{$next_token} ) {
                $here_doc_expected = 1;
                $msg .=
                  " -- guessing it's a here-doc ($next_token not a constant)\n";
            }
            else {
                $msg .=
                  " -- guessing it's a shift ($next_token is a constant)\n";
            }
        }
    }
    write_logfile_entry($msg);
    return $here_doc_expected;
}

#########i#############################################################
# Tokenizer Routines for scanning identifiers and related items
#######################################################################

sub scan_bare_identifier_do {

    # this routine is called to scan a token starting with an alphanumeric
    # variable or package separator, :: or '.
    # USES GLOBAL VARIABLES: $current_package, $last_nonblank_token,
    # $last_nonblank_type,@paren_type, $paren_depth

    my ( $input_line, $i, $tok, $type, $prototype, $rtoken_map,
        $max_token_index )
      = @_;
    my $i_begin = $i;
    my $package = undef;

    my $i_beg = $i;

    # we have to back up one pretoken at a :: since each : is one pretoken
    if ( $tok eq '::' ) { $i_beg-- }
    if ( $tok eq '->' ) { $i_beg-- }
    my $pos_beg = $rtoken_map->[$i_beg];
    pos($input_line) = $pos_beg;

    #  Examples:
    #   A::B::C
    #   A::
    #   ::A
    #   A'B
    if ( $input_line =~ m/\G\s*((?:\w*(?:'|::)))*(?:(?:->)?(\w+))?/gc ) {

        my $pos  = pos($input_line);
        my $numc = $pos - $pos_beg;
        $tok = substr( $input_line, $pos_beg, $numc );

        # type 'w' includes anything without leading type info
        # ($,%,@,*) including something like abc::def::ghi
        $type = 'w';

        my $sub_name = "";
        if ( defined($2) ) { $sub_name = $2; }
        if ( defined($1) ) {
            $package = $1;

            # patch: don't allow isolated package name which just ends
            # in the old style package separator (single quote).  Example:
            #   use CGI':all';
            if ( !($sub_name) && substr( $package, -1, 1 ) eq '\'' ) {
                $pos--;
            }

            $package =~ s/\'/::/g;
            if ( $package =~ /^\:/ ) { $package = 'main' . $package }
            $package =~ s/::$//;
        }
        else {
            $package = $current_package;

            if ( $is_keyword{$tok} ) {
                $type = 'k';
            }
        }

        # if it is a bareword..
        if ( $type eq 'w' ) {

            # check for v-string with leading 'v' type character
            # (This seems to have precedence over filehandle, type 'Y')
            if ( $tok =~ /^v\d[_\d]*$/ ) {

                # we only have the first part - something like 'v101' -
                # look for more
                if ( $input_line =~ m/\G(\.\d[_\d]*)+/gc ) {
                    $pos  = pos($input_line);
                    $numc = $pos - $pos_beg;
                    $tok  = substr( $input_line, $pos_beg, $numc );
                }
                $type = 'v';

                # warn if this version can't handle v-strings
                report_v_string($tok);
            }

            elsif ( $is_constant{$package}{$sub_name} ) {
                $type = 'C';
            }

            # bareword after sort has implied empty prototype; for example:
            # @sorted = sort numerically ( 53, 29, 11, 32, 7 );
            # This has priority over whatever the user has specified.
            elsif ($last_nonblank_token eq 'sort'
                && $last_nonblank_type eq 'k' )
            {
                $type = 'Z';
            }

            # Note: strangely, perl does not seem to really let you create
            # functions which act like eval and do, in the sense that eval
            # and do may have operators following the final }, but any operators
            # that you create with prototype (&) apparently do not allow
            # trailing operators, only terms.  This seems strange.
            # If this ever changes, here is the update
            # to make perltidy behave accordingly:

            # elsif ( $is_block_function{$package}{$tok} ) {
            #    $tok='eval'; # patch to do braces like eval  - doesn't work
            #    $type = 'k';
            #}
            # FIXME: This could become a separate type to allow for different
            # future behavior:
            elsif ( $is_block_function{$package}{$sub_name} ) {
                $type = 'G';
            }

            elsif ( $is_block_list_function{$package}{$sub_name} ) {
                $type = 'G';
            }
            elsif ( $is_user_function{$package}{$sub_name} ) {
                $type      = 'U';
                $prototype = $user_function_prototype{$package}{$sub_name};
            }

            # check for indirect object
            elsif (

                # added 2001-03-27: must not be followed immediately by '('
                # see fhandle.t
                ( $input_line !~ m/\G\(/gc )

                # and
                && (

                    # preceded by keyword like 'print', 'printf' and friends
                    $is_indirect_object_taker{$last_nonblank_token}

                    # or preceded by something like 'print(' or 'printf('
                    || (
                        ( $last_nonblank_token eq '(' )
                        && $is_indirect_object_taker{ $paren_type[$paren_depth]
                        }

                    )
                )
              )
            {

                # may not be indirect object unless followed by a space
                if ( $input_line =~ m/\G\s+/gc ) {
                    $type = 'Y';

                    # Abandon Hope ...
                    # Perl's indirect object notation is a very bad
                    # thing and can cause subtle bugs, especially for
                    # beginning programmers.  And I haven't even been
                    # able to figure out a sane warning scheme which
                    # doesn't get in the way of good scripts.

                    # Complain if a filehandle has any lower case
                    # letters.  This is suggested good practice.
                    # Use 'sub_name' because something like
                    # main::MYHANDLE is ok for filehandle
                    if ( $sub_name =~ /[a-z]/ ) {

                        # could be bug caused by older perltidy if
                        # followed by '('
                        if ( $input_line =~ m/\G\s*\(/gc ) {
                            complain(
"Caution: unknown word '$tok' in indirect object slot\n"
                            );
                        }
                    }
                }

                # bareword not followed by a space -- may not be filehandle
                # (may be function call defined in a 'use' statement)
                else {
                    $type = 'Z';
                }
            }
        }

        # Now we must convert back from character position
        # to pre_token index.
        # I don't think an error flag can occur here ..but who knows
        my $error;
        ( $i, $error ) =
          inverse_pretoken_map( $i, $pos, $rtoken_map, $max_token_index );
        if ($error) {
            warning("scan_bare_identifier: Possibly invalid tokenization\n");
        }
    }

    # no match but line not blank - could be syntax error
    # perl will take '::' alone without complaint
    else {
        $type = 'w';

        # change this warning to log message if it becomes annoying
        warning("didn't find identifier after leading ::\n");
    }
    return ( $i, $tok, $type, $prototype );
}

sub scan_id_do {

# This is the new scanner and will eventually replace scan_identifier.
# Only type 'sub' and 'package' are implemented.
# Token types $ * % @ & -> are not yet implemented.
#
# Scan identifier following a type token.
# The type of call depends on $id_scan_state: $id_scan_state = ''
# for starting call, in which case $tok must be the token defining
# the type.
#
# If the type token is the last nonblank token on the line, a value
# of $id_scan_state = $tok is returned, indicating that further
# calls must be made to get the identifier.  If the type token is
# not the last nonblank token on the line, the identifier is
# scanned and handled and a value of '' is returned.
# USES GLOBAL VARIABLES: $current_package, $last_nonblank_token, $in_attribute_list,
# $statement_type, $tokenizer_self

    my ( $input_line, $i, $tok, $rtokens, $rtoken_map, $id_scan_state,
        $max_token_index )
      = @_;
    my $type = '';
    my ( $i_beg, $pos_beg );

    #print "NSCAN:entering i=$i, tok=$tok, type=$type, state=$id_scan_state\n";
    #my ($a,$b,$c) = caller;
    #print "NSCAN: scan_id called with tok=$tok $a $b $c\n";

    # on re-entry, start scanning at first token on the line
    if ($id_scan_state) {
        $i_beg = $i;
        $type  = '';
    }

    # on initial entry, start scanning just after type token
    else {
        $i_beg         = $i + 1;
        $id_scan_state = $tok;
        $type          = 't';
    }

    # find $i_beg = index of next nonblank token,
    # and handle empty lines
    my $blank_line          = 0;
    my $next_nonblank_token = $rtokens->[$i_beg];
    if ( $i_beg > $max_token_index ) {
        $blank_line = 1;
    }
    else {

        # only a '#' immediately after a '$' is not a comment
        if ( $next_nonblank_token eq '#' ) {
            unless ( $tok eq '$' ) {
                $blank_line = 1;
            }
        }

        if ( $next_nonblank_token =~ /^\s/ ) {
            ( $next_nonblank_token, $i_beg ) =
              find_next_nonblank_token_on_this_line( $i_beg, $rtokens,
                $max_token_index );
            if ( $next_nonblank_token =~ /(^#|^\s*$)/ ) {
                $blank_line = 1;
            }
        }
    }

    # handle non-blank line; identifier, if any, must follow
    unless ($blank_line) {

        if ( $id_scan_state eq 'sub' ) {
            ( $i, $tok, $type, $id_scan_state ) = do_scan_sub(
                $input_line, $i,             $i_beg,
                $tok,        $type,          $rtokens,
                $rtoken_map, $id_scan_state, $max_token_index
            );
        }

        elsif ( $id_scan_state eq 'package' ) {
            ( $i, $tok, $type ) =
              do_scan_package( $input_line, $i, $i_beg, $tok, $type, $rtokens,
                $rtoken_map, $max_token_index );
            $id_scan_state = '';
        }

        else {
            warning("invalid token in scan_id: $tok\n");
            $id_scan_state = '';
        }
    }

    if ( $id_scan_state && ( !defined($type) || !$type ) ) {

        # shouldn't happen:
        warning(
"Program bug in scan_id: undefined type but scan_state=$id_scan_state\n"
        );
        report_definite_bug();
    }

    TOKENIZER_DEBUG_FLAG_NSCAN && do {
        print STDOUT
          "NSCAN: returns i=$i, tok=$tok, type=$type, state=$id_scan_state\n";
    };
    return ( $i, $tok, $type, $id_scan_state );
}

sub check_prototype {
    my ( $proto, $package, $subname ) = @_;
    return unless ( defined($package) && defined($subname) );
    if ( defined($proto) ) {
        $proto =~ s/^\s*\(\s*//;
        $proto =~ s/\s*\)$//;
        if ($proto) {
            $is_user_function{$package}{$subname}        = 1;
            $user_function_prototype{$package}{$subname} = "($proto)";

            # prototypes containing '&' must be treated specially..
            if ( $proto =~ /\&/ ) {

                # right curly braces of prototypes ending in
                # '&' may be followed by an operator
                if ( $proto =~ /\&$/ ) {
                    $is_block_function{$package}{$subname} = 1;
                }

                # right curly braces of prototypes NOT ending in
                # '&' may NOT be followed by an operator
                elsif ( $proto !~ /\&$/ ) {
                    $is_block_list_function{$package}{$subname} = 1;
                }
            }
        }
        else {
            $is_constant{$package}{$subname} = 1;
        }
    }
    else {
        $is_user_function{$package}{$subname} = 1;
    }
    return;
}

sub do_scan_package {

    # do_scan_package parses a package name
    # it is called with $i_beg equal to the index of the first nonblank
    # token following a 'package' token.
    # USES GLOBAL VARIABLES: $current_package,

    # package NAMESPACE
    # package NAMESPACE VERSION
    # package NAMESPACE BLOCK
    # package NAMESPACE VERSION BLOCK
    #
    # If VERSION is provided, package sets the $VERSION variable in the given
    # namespace to a version object with the VERSION provided. VERSION must be
    # a "strict" style version number as defined by the version module: a
    # positive decimal number (integer or decimal-fraction) without
    # exponentiation or else a dotted-decimal v-string with a leading 'v'
    # character and at least three components.
    # reference http://perldoc.perl.org/functions/package.html

    my ( $input_line, $i, $i_beg, $tok, $type, $rtokens, $rtoken_map,
        $max_token_index )
      = @_;
    my $package = undef;
    my $pos_beg = $rtoken_map->[$i_beg];
    pos($input_line) = $pos_beg;

    # handle non-blank line; package name, if any, must follow
    if ( $input_line =~ m/\G\s*((?:\w*(?:'|::))*\w+)/gc ) {
        $package = $1;
        $package = ( defined($1) && $1 ) ? $1 : 'main';
        $package =~ s/\'/::/g;
        if ( $package =~ /^\:/ ) { $package = 'main' . $package }
        $package =~ s/::$//;
        my $pos  = pos($input_line);
        my $numc = $pos - $pos_beg;
        $tok  = 'package ' . substr( $input_line, $pos_beg, $numc );
        $type = 'i';

        # Now we must convert back from character position
        # to pre_token index.
        # I don't think an error flag can occur here ..but ?
        my $error;
        ( $i, $error ) =
          inverse_pretoken_map( $i, $pos, $rtoken_map, $max_token_index );
        if ($error) { warning("Possibly invalid package\n") }
        $current_package = $package;

        # we should now have package NAMESPACE
        # now expecting VERSION, BLOCK, or ; to follow ...
        # package NAMESPACE VERSION
        # package NAMESPACE BLOCK
        # package NAMESPACE VERSION BLOCK
        my ( $next_nonblank_token, $i_next ) =
          find_next_nonblank_token( $i, $rtokens, $max_token_index );

        # check that something recognizable follows, but do not parse.
        # A VERSION number will be parsed later as a number or v-string in the
        # normal way.  What is important is to set the statement type if
        # everything looks okay so that the operator_expected() routine
        # knows that the number is in a package statement.
        # Examples of valid primitive tokens that might follow are:
        #  1235  . ; { } v3  v
        if ( $next_nonblank_token =~ /^([v\.\d;\{\}])|v\d|\d+$/ ) {
            $statement_type = $tok;
        }
        else {
            warning(
                "Unexpected '$next_nonblank_token' after package name '$tok'\n"
            );
        }
    }

    # no match but line not blank --
    # could be a label with name package, like package:  , for example.
    else {
        $type = 'k';
    }

    return ( $i, $tok, $type );
}

sub scan_identifier_do {

    # This routine assembles tokens into identifiers.  It maintains a
    # scan state, id_scan_state.  It updates id_scan_state based upon
    # current id_scan_state and token, and returns an updated
    # id_scan_state and the next index after the identifier.
    # USES GLOBAL VARIABLES: $context, $last_nonblank_token,
    # $last_nonblank_type

    my ( $i, $id_scan_state, $identifier, $rtokens, $max_token_index,
        $expecting, $container_type )
      = @_;
    my $i_begin   = $i;
    my $type      = '';
    my $tok_begin = $rtokens->[$i_begin];
    if ( $tok_begin eq ':' ) { $tok_begin = '::' }
    my $id_scan_state_begin = $id_scan_state;
    my $identifier_begin    = $identifier;
    my $tok                 = $tok_begin;
    my $message             = "";

    my $in_prototype_or_signature = $container_type =~ /^sub/;

    # these flags will be used to help figure out the type:
    my $saw_alpha = ( $tok =~ /^[A-Za-z_]/ );
    my $saw_type;

    # allow old package separator (') except in 'use' statement
    my $allow_tick = ( $last_nonblank_token ne 'use' );

    # get started by defining a type and a state if necessary
    unless ($id_scan_state) {
        $context = UNKNOWN_CONTEXT;

        # fixup for digraph
        if ( $tok eq '>' ) {
            $tok       = '->';
            $tok_begin = $tok;
        }
        $identifier = $tok;

        if ( $tok eq '$' || $tok eq '*' ) {
            $id_scan_state = '$';
            $context       = SCALAR_CONTEXT;
        }
        elsif ( $tok eq '%' || $tok eq '@' ) {
            $id_scan_state = '$';
            $context       = LIST_CONTEXT;
        }
        elsif ( $tok eq '&' ) {
            $id_scan_state = '&';
        }
        elsif ( $tok eq 'sub' or $tok eq 'package' ) {
            $saw_alpha     = 0;     # 'sub' is considered type info here
            $id_scan_state = '$';
            $identifier .= ' ';     # need a space to separate sub from sub name
        }
        elsif ( $tok eq '::' ) {
            $id_scan_state = 'A';
        }
        elsif ( $tok =~ /^[A-Za-z_]/ ) {
            $id_scan_state = ':';
        }
        elsif ( $tok eq '->' ) {
            $id_scan_state = '$';
        }
        else {

            # shouldn't happen
            my ( $a, $b, $c ) = caller;
            warning("Program Bug: scan_identifier given bad token = $tok \n");
            warning("   called from sub $a  line: $c\n");
            report_definite_bug();
        }
        $saw_type = !$saw_alpha;
    }
    else {
        $i--;
        $saw_type = ( $tok =~ /([\$\%\@\*\&])/ );
    }

    # now loop to gather the identifier
    my $i_save = $i;

    while ( $i < $max_token_index ) {
        $i_save = $i unless ( $tok =~ /^\s*$/ );
        $tok    = $rtokens->[ ++$i ];

        if ( ( $tok eq ':' ) && ( $rtokens->[ $i + 1 ] eq ':' ) ) {
            $tok = '::';
            $i++;
        }

        if ( $id_scan_state eq '$' ) {    # starting variable name

            if ( $tok eq '$' ) {

                $identifier .= $tok;

                # we've got a punctuation variable if end of line (punct.t)
                if ( $i == $max_token_index ) {
                    $type          = 'i';
                    $id_scan_state = '';
                    last;
                }
            }

            # POSTDEFREF ->@ ->% ->& ->*
            elsif ( ( $tok =~ /^[\@\%\&\*]$/ ) && $identifier =~ /\-\>$/ ) {
                $identifier .= $tok;
            }
            elsif ( $tok =~ /^[A-Za-z_]/ ) {    # alphanumeric ..
                $saw_alpha     = 1;
                $id_scan_state = ':';           # now need ::
                $identifier .= $tok;
            }
            elsif ( $tok eq "'" && $allow_tick ) {    # alphanumeric ..
                $saw_alpha     = 1;
                $id_scan_state = ':';                 # now need ::
                $identifier .= $tok;

                # Perl will accept leading digits in identifiers,
                # although they may not always produce useful results.
                # Something like $main::0 is ok.  But this also works:
                #
                #  sub howdy::123::bubba{ print "bubba $54321!\n" }
                #  howdy::123::bubba();
                #
            }
            elsif ( $tok =~ /^[0-9]/ ) {    # numeric
                $saw_alpha     = 1;
                $id_scan_state = ':';       # now need ::
                $identifier .= $tok;
            }
            elsif ( $tok eq '::' ) {
                $id_scan_state = 'A';
                $identifier .= $tok;
            }

            # $# and POSTDEFREF ->$#
            elsif ( ( $tok eq '#' ) && ( $identifier =~ /\$$/ ) ) {    # $#array
                $identifier .= $tok;    # keep same state, a $ could follow
            }
            elsif ( $tok eq '{' ) {

                # check for something like ${#} or ${©}
                if (
                    (
                           $identifier eq '$'
                        || $identifier eq '@'
                        || $identifier eq '$#'
                    )
                    && $i + 2 <= $max_token_index
                    && $rtokens->[ $i + 2 ] eq '}'
                    && $rtokens->[ $i + 1 ] !~ /[\s\w]/
                  )
                {
                    my $next2 = $rtokens->[ $i + 2 ];
                    my $next1 = $rtokens->[ $i + 1 ];
                    $identifier .= $tok . $next1 . $next2;
                    $i += 2;
                    $id_scan_state = '';
                    last;
                }

                # skip something like ${xxx} or ->{
                $id_scan_state = '';

                # if this is the first token of a line, any tokens for this
                # identifier have already been accumulated
                if ( $identifier eq '$' || $i == 0 ) { $identifier = ''; }
                $i = $i_save;
                last;
            }

            # space ok after leading $ % * & @
            elsif ( $tok =~ /^\s*$/ ) {

                if ( $identifier =~ /^[\$\%\*\&\@]/ ) {

                    if ( length($identifier) > 1 ) {
                        $id_scan_state = '';
                        $i             = $i_save;
                        $type          = 'i';    # probably punctuation variable
                        last;
                    }
                    else {

                        # spaces after $'s are common, and space after @
                        # is harmless, so only complain about space
                        # after other type characters. Space after $ and
                        # @ will be removed in formatting.  Report space
                        # after % and * because they might indicate a
                        # parsing error.  In other words '% ' might be a
                        # modulo operator.  Delete this warning if it
                        # gets annoying.
                        if ( $identifier !~ /^[\@\$]$/ ) {
                            $message =
                              "Space in identifier, following $identifier\n";
                        }
                    }
                }

                # else:
                # space after '->' is ok
            }
            elsif ( $tok eq '^' ) {

                # check for some special variables like $^W
                if ( $identifier =~ /^[\$\*\@\%]$/ ) {
                    $identifier .= $tok;
                    $id_scan_state = 'A';

                    # Perl accepts '$^]' or '@^]', but
                    # there must not be a space before the ']'.
                    my $next1 = $rtokens->[ $i + 1 ];
                    if ( $next1 eq ']' ) {
                        $i++;
                        $identifier .= $next1;
                        $id_scan_state = "";
                        last;
                    }
                }
                else {
                    $id_scan_state = '';
                }
            }
            else {    # something else

                if ( $in_prototype_or_signature && $tok =~ /^[\),=]/ ) {
                    $id_scan_state = '';
                    $i             = $i_save;
                    $type          = 'i';       # probably punctuation variable
                    last;
                }

                # check for various punctuation variables
                if ( $identifier =~ /^[\$\*\@\%]$/ ) {
                    $identifier .= $tok;
                }

                # POSTDEFREF: Postfix reference ->$* ->%*  ->@* ->** ->&* ->$#*
                elsif ( $tok eq '*' && $identifier =~ /([\@\%\$\*\&]|\$\#)$/ ) {
                    $identifier .= $tok;
                }

                elsif ( $identifier eq '$#' ) {

                    if ( $tok eq '{' ) { $type = 'i'; $i = $i_save }

                    # perl seems to allow just these: $#: $#- $#+
                    elsif ( $tok =~ /^[\:\-\+]$/ ) {
                        $type = 'i';
                        $identifier .= $tok;
                    }
                    else {
                        $i = $i_save;
                        write_logfile_entry( 'Use of $# is deprecated' . "\n" );
                    }
                }
                elsif ( $identifier eq '$$' ) {

                    # perl does not allow references to punctuation
                    # variables without braces.  For example, this
                    # won't work:
                    #  $:=\4;
                    #  $a = $$:;
                    # You would have to use
                    #  $a = ${$:};

                    $i = $i_save;
                    if   ( $tok eq '{' ) { $type = 't' }
                    else                 { $type = 'i' }
                }
                elsif ( $identifier eq '->' ) {
                    $i = $i_save;
                }
                else {
                    $i = $i_save;
                    if ( length($identifier) == 1 ) { $identifier = ''; }
                }
                $id_scan_state = '';
                last;
            }
        }
        elsif ( $id_scan_state eq '&' ) {    # starting sub call?

            if ( $tok =~ /^[\$A-Za-z_]/ ) {    # alphanumeric ..
                $id_scan_state = ':';          # now need ::
                $saw_alpha     = 1;
                $identifier .= $tok;
            }
            elsif ( $tok eq "'" && $allow_tick ) {    # alphanumeric ..
                $id_scan_state = ':';                 # now need ::
                $saw_alpha     = 1;
                $identifier .= $tok;
            }
            elsif ( $tok =~ /^[0-9]/ ) {    # numeric..see comments above
                $id_scan_state = ':';       # now need ::
                $saw_alpha     = 1;
                $identifier .= $tok;
            }
            elsif ( $tok =~ /^\s*$/ ) {     # allow space
            }
            elsif ( $tok eq '::' ) {        # leading ::
                $id_scan_state = 'A';       # accept alpha next
                $identifier .= $tok;
            }
            elsif ( $tok eq '{' ) {
                if ( $identifier eq '&' || $i == 0 ) { $identifier = ''; }
                $i             = $i_save;
                $id_scan_state = '';
                last;
            }
            else {

                # punctuation variable?
                # testfile: cunningham4.pl
                #
                # We have to be careful here.  If we are in an unknown state,
                # we will reject the punctuation variable.  In the following
                # example the '&' is a binary operator but we are in an unknown
                # state because there is no sigil on 'Prima', so we don't
                # know what it is.  But it is a bad guess that
                # '&~' is a function variable.
                # $self->{text}->{colorMap}->[
                #   Prima::PodView::COLOR_CODE_FOREGROUND
                #   & ~tb::COLOR_INDEX ] =
                #   $sec->{ColorCode}
                if ( $identifier eq '&' && $expecting ) {
                    $identifier .= $tok;
                }
                else {
                    $identifier = '';
                    $i          = $i_save;
                    $type       = '&';
                }
                $id_scan_state = '';
                last;
            }
        }
        elsif ( $id_scan_state eq 'A' ) {    # looking for alpha (after ::)

            if ( $tok =~ /^[A-Za-z_]/ ) {    # found it
                $identifier .= $tok;
                $id_scan_state = ':';        # now need ::
                $saw_alpha     = 1;
            }
            elsif ( $tok eq "'" && $allow_tick ) {
                $identifier .= $tok;
                $id_scan_state = ':';        # now need ::
                $saw_alpha     = 1;
            }
            elsif ( $tok =~ /^[0-9]/ ) {     # numeric..see comments above
                $identifier .= $tok;
                $id_scan_state = ':';        # now need ::
                $saw_alpha     = 1;
            }
            elsif ( ( $identifier =~ /^sub / ) && ( $tok =~ /^\s*$/ ) ) {
                $id_scan_state = '(';
                $identifier .= $tok;
            }
            elsif ( ( $identifier =~ /^sub / ) && ( $tok eq '(' ) ) {
                $id_scan_state = ')';
                $identifier .= $tok;
            }
            else {
                $id_scan_state = '';
                $i             = $i_save;
                last;
            }
        }
        elsif ( $id_scan_state eq ':' ) {    # looking for :: after alpha

            if ( $tok eq '::' ) {            # got it
                $identifier .= $tok;
                $id_scan_state = 'A';        # now require alpha
            }
            elsif ( $tok =~ /^[A-Za-z_]/ ) {    # more alphanumeric is ok here
                $identifier .= $tok;
                $id_scan_state = ':';           # now need ::
                $saw_alpha     = 1;
            }
            elsif ( $tok =~ /^[0-9]/ ) {        # numeric..see comments above
                $identifier .= $tok;
                $id_scan_state = ':';           # now need ::
                $saw_alpha     = 1;
            }
            elsif ( $tok eq "'" && $allow_tick ) {    # tick

                if ( $is_keyword{$identifier} ) {
                    $id_scan_state = '';              # that's all
                    $i             = $i_save;
                }
                else {
                    $identifier .= $tok;
                }
            }
            elsif ( ( $identifier =~ /^sub / ) && ( $tok =~ /^\s*$/ ) ) {
                $id_scan_state = '(';
                $identifier .= $tok;
            }
            elsif ( ( $identifier =~ /^sub / ) && ( $tok eq '(' ) ) {
                $id_scan_state = ')';
                $identifier .= $tok;
            }
            else {
                $id_scan_state = '';        # that's all
                $i             = $i_save;
                last;
            }
        }
        elsif ( $id_scan_state eq '(' ) {    # looking for ( of prototype

            if ( $tok eq '(' ) {             # got it
                $identifier .= $tok;
                $id_scan_state = ')';        # now find the end of it
            }
            elsif ( $tok =~ /^\s*$/ ) {      # blank - keep going
                $identifier .= $tok;
            }
            else {
                $id_scan_state = '';         # that's all - no prototype
                $i             = $i_save;
                last;
            }
        }
        elsif ( $id_scan_state eq ')' ) {    # looking for ) to end

            if ( $tok eq ')' ) {             # got it
                $identifier .= $tok;
                $id_scan_state = '';         # all done
                last;
            }
            elsif ( $tok =~ /^[\s\$\%\\\*\@\&\;]/ ) {
                $identifier .= $tok;
            }
            else {    # probable error in script, but keep going
                warning("Unexpected '$tok' while seeking end of prototype\n");
                $identifier .= $tok;
            }
        }
        else {        # can get here due to error in initialization
            $id_scan_state = '';
            $i             = $i_save;
            last;
        }
    }

    if ( $id_scan_state eq ')' ) {
        warning("Hit end of line while seeking ) to end prototype\n");
    }

    # once we enter the actual identifier, it may not extend beyond
    # the end of the current line
    if ( $id_scan_state =~ /^[A\:\(\)]/ ) {
        $id_scan_state = '';
    }
    if ( $i < 0 ) { $i = 0 }

    unless ($type) {

        if ($saw_type) {

            if ($saw_alpha) {
                if ( $identifier =~ /^->/ && $last_nonblank_type eq 'w' ) {
                    $type = 'w';
                }
                else { $type = 'i' }
            }
            elsif ( $identifier eq '->' ) {
                $type = '->';
            }
            elsif (
                ( length($identifier) > 1 )

                # In something like '@$=' we have an identifier '@$'
                # In something like '$${' we have type '$$' (and only
                # part of an identifier)
                && !( $identifier =~ /\$$/ && $tok eq '{' )
                && ( $identifier !~ /^(sub |package )$/ )
              )
            {
                $type = 'i';
            }
            else { $type = 't' }
        }
        elsif ($saw_alpha) {

            # type 'w' includes anything without leading type info
            # ($,%,@,*) including something like abc::def::ghi
            $type = 'w';
        }
        else {
            $type = '';
        }    # this can happen on a restart
    }

    if ($identifier) {
        $tok = $identifier;
        if ($message) { write_logfile_entry($message) }
    }
    else {
        $tok = $tok_begin;
        $i   = $i_begin;
    }

    TOKENIZER_DEBUG_FLAG_SCAN_ID && do {
        my ( $a, $b, $c ) = caller;
        print STDOUT
"SCANID: called from $a $b $c with tok, i, state, identifier =$tok_begin, $i_begin, $id_scan_state_begin, $identifier_begin\n";
        print STDOUT
"SCANID: returned with tok, i, state, identifier =$tok, $i, $id_scan_state, $identifier\n";
    };
    return ( $i, $tok, $type, $id_scan_state, $identifier );
}

{

    # saved package and subnames in case prototype is on separate line
    my ( $package_saved, $subname_saved );

    sub do_scan_sub {

        # do_scan_sub parses a sub name and prototype
        # it is called with $i_beg equal to the index of the first nonblank
        # token following a 'sub' token.

        # TODO: add future error checks to be sure we have a valid
        # sub name.  For example, 'sub &doit' is wrong.  Also, be sure
        # a name is given if and only if a non-anonymous sub is
        # appropriate.
        # USES GLOBAL VARS: $current_package, $last_nonblank_token,
        # $in_attribute_list, %saw_function_definition,
        # $statement_type

        my (
            $input_line, $i,             $i_beg,
            $tok,        $type,          $rtokens,
            $rtoken_map, $id_scan_state, $max_token_index
        ) = @_;
        $id_scan_state = "";    # normally we get everything in one call
        my $subname = undef;
        my $package = undef;
        my $proto   = undef;
        my $attrs   = undef;
        my $match;

        my $pos_beg = $rtoken_map->[$i_beg];
        pos($input_line) = $pos_beg;

        # Look for the sub NAME
        if (
            $input_line =~ m/\G\s*
        ((?:\w*(?:'|::))*)  # package - something that ends in :: or '
        (\w+)               # NAME    - required
        /gcx
          )
        {
            $match   = 1;
            $subname = $2;

            $package = ( defined($1) && $1 ) ? $1 : $current_package;
            $package =~ s/\'/::/g;
            if ( $package =~ /^\:/ ) { $package = 'main' . $package }
            $package =~ s/::$//;
            my $pos  = pos($input_line);
            my $numc = $pos - $pos_beg;
            $tok  = 'sub ' . substr( $input_line, $pos_beg, $numc );
            $type = 'i';
        }

        # Now look for PROTO ATTRS
        # Look for prototype/attributes which are usually on the same
        # line as the sub name but which might be on a separate line.
        # For example, we might have an anonymous sub with attributes,
        # or a prototype on a separate line from its sub name

        # NOTE: We only want to parse PROTOTYPES here. If we see anything that
        # does not look like a prototype, we assume it is a SIGNATURE and we
        # will stop and let the the standard tokenizer handle it.  In
        # particular, we stop if we see any nested parens, braces, or commas.
        my $saw_opening_paren = $input_line =~ /\G\s*\(/;
        if (
            $input_line =~ m/\G(\s*\([^\)\(\}\{\,]*\))?  # PROTO
            (\s*:)?                              # ATTRS leading ':'
            /gcx
            && ( $1 || $2 )
          )
        {
            $proto = $1;
            $attrs = $2;

            # If we also found the sub name on this call then append PROTO.
            # This is not necessary but for compatability with previous
            # versions when the -csc flag is used:
            if ( $match && $proto ) {
                $tok .= $proto;
            }
            $match ||= 1;

            # Handle prototype on separate line from subname
            if ($subname_saved) {
                $package = $package_saved;
                $subname = $subname_saved;
                $tok     = $last_nonblank_token;
            }
            $type = 'i';
        }

        if ($match) {

            # ATTRS: if there are attributes, back up and let the ':' be
            # found later by the scanner.
            my $pos = pos($input_line);
            if ($attrs) {
                $pos -= length($attrs);
            }

            my $next_nonblank_token = $tok;

            # catch case of line with leading ATTR ':' after anonymous sub
            if ( $pos == $pos_beg && $tok eq ':' ) {
                $type              = 'A';
                $in_attribute_list = 1;
            }

            # Otherwise, if we found a match we must convert back from
            # string position to the pre_token index for continued parsing.
            else {

                # I don't think an error flag can occur here ..but ?
                my $error;
                ( $i, $error ) = inverse_pretoken_map( $i, $pos, $rtoken_map,
                    $max_token_index );
                if ($error) { warning("Possibly invalid sub\n") }

                # check for multiple definitions of a sub
                ( $next_nonblank_token, my $i_next ) =
                  find_next_nonblank_token_on_this_line( $i, $rtokens,
                    $max_token_index );
            }

            if ( $next_nonblank_token =~ /^(\s*|#)$/ )
            {    # skip blank or side comment
                my ( $rpre_tokens, $rpre_types ) =
                  peek_ahead_for_n_nonblank_pre_tokens(1);
                if ( defined($rpre_tokens) && @{$rpre_tokens} ) {
                    $next_nonblank_token = $rpre_tokens->[0];
                }
                else {
                    $next_nonblank_token = '}';
                }
            }
            $package_saved = "";
            $subname_saved = "";

            # See what's next...
            if ( $next_nonblank_token eq '{' ) {
                if ($subname) {

                    # Check for multiple definitions of a sub, but
                    # it is ok to have multiple sub BEGIN, etc,
                    # so we do not complain if name is all caps
                    if (   $saw_function_definition{$package}{$subname}
                        && $subname !~ /^[A-Z]+$/ )
                    {
                        my $lno = $saw_function_definition{$package}{$subname};
                        warning(
"already saw definition of 'sub $subname' in package '$package' at line $lno\n"
                        );
                    }
                    $saw_function_definition{$package}{$subname} =
                      $tokenizer_self->{_last_line_number};
                }
            }
            elsif ( $next_nonblank_token eq ';' ) {
            }
            elsif ( $next_nonblank_token eq '}' ) {
            }

            # ATTRS - if an attribute list follows, remember the name
            # of the sub so the next opening brace can be labeled.
            # Setting 'statement_type' causes any ':'s to introduce
            # attributes.
            elsif ( $next_nonblank_token eq ':' ) {
                $statement_type = $tok;
            }

            # if we stopped before an open paren ...
            elsif ( $next_nonblank_token eq '(' ) {

                # If we DID NOT see this paren above then it must be on the
                # next line so we will set a flag to come back here and see if
                # it is a PROTOTYPE

                # Otherwise, we assume it is a SIGNATURE rather than a
                # PROTOTYPE and let the normal tokenizer handle it as a list
                if ( !$saw_opening_paren ) {
                    $id_scan_state = 'sub';     # we must come back to get proto
                    $package_saved = $package;
                    $subname_saved = $subname;
                }
                $statement_type = $tok;
            }
            elsif ($next_nonblank_token) {      # EOF technically ok
                warning(
"expecting ':' or ';' or '{' after definition or declaration of sub '$subname' but saw '$next_nonblank_token'\n"
                );
            }
            check_prototype( $proto, $package, $subname );
        }

        # no match but line not blank
        else {
        }
        return ( $i, $tok, $type, $id_scan_state );
    }
}

#########i###############################################################
# Tokenizer utility routines which may use CONSTANTS but no other GLOBALS
#########################################################################

sub find_next_nonblank_token {
    my ( $i, $rtokens, $max_token_index ) = @_;

    if ( $i >= $max_token_index ) {
        if ( !peeked_ahead() ) {
            peeked_ahead(1);
            $rtokens =
              peek_ahead_for_nonblank_token( $rtokens, $max_token_index );
        }
    }
    my $next_nonblank_token = $rtokens->[ ++$i ];

    if ( $next_nonblank_token =~ /^\s*$/ ) {
        $next_nonblank_token = $rtokens->[ ++$i ];
    }
    return ( $next_nonblank_token, $i );
}

sub numerator_expected {

    # this is a filter for a possible numerator, in support of guessing
    # for the / pattern delimiter token.
    # returns -
    #   1 - yes
    #   0 - can't tell
    #  -1 - no
    # Note: I am using the convention that variables ending in
    # _expected have these 3 possible values.
    my ( $i, $rtokens, $max_token_index ) = @_;
    my $numerator_expected = 0;

    my $next_token = $rtokens->[ $i + 1 ];
    if ( $next_token eq '=' ) { $i++; }    # handle /=
    my ( $next_nonblank_token, $i_next ) =
      find_next_nonblank_token( $i, $rtokens, $max_token_index );

    if ( $next_nonblank_token =~ /(\(|\$|\w|\.|\@)/ ) {
        $numerator_expected = 1;
    }
    else {

        if ( $next_nonblank_token =~ /^\s*$/ ) {
            $numerator_expected = 0;
        }
        else {
            $numerator_expected = -1;
        }
    }
    return $numerator_expected;
}

sub pattern_expected {

    # This is the start of a filter for a possible pattern.
    # It looks at the token after a possible pattern and tries to
    # determine if that token could end a pattern.
    # returns -
    #   1 - yes
    #   0 - can't tell
    #  -1 - no
    my ( $i, $rtokens, $max_token_index ) = @_;
    my $is_pattern = 0;

    my $next_token = $rtokens->[ $i + 1 ];
    if ( $next_token =~ /^[msixpodualgc]/ ) { $i++; }   # skip possible modifier
    my ( $next_nonblank_token, $i_next ) =
      find_next_nonblank_token( $i, $rtokens, $max_token_index );

    # list of tokens which may follow a pattern
    # (can probably be expanded)
    if ( $next_nonblank_token =~ /(\)|\}|\;|\&\&|\|\||and|or|while|if|unless)/ )
    {
        $is_pattern = 1;
    }
    else {

        if ( $next_nonblank_token =~ /^\s*$/ ) {
            $is_pattern = 0;
        }
        else {
            $is_pattern = -1;
        }
    }
    return $is_pattern;
}

sub find_next_nonblank_token_on_this_line {
    my ( $i, $rtokens, $max_token_index ) = @_;
    my $next_nonblank_token;

    if ( $i < $max_token_index ) {
        $next_nonblank_token = $rtokens->[ ++$i ];

        if ( $next_nonblank_token =~ /^\s*$/ ) {

            if ( $i < $max_token_index ) {
                $next_nonblank_token = $rtokens->[ ++$i ];
            }
        }
    }
    else {
        $next_nonblank_token = "";
    }
    return ( $next_nonblank_token, $i );
}

sub find_angle_operator_termination {

    # We are looking at a '<' and want to know if it is an angle operator.
    # We are to return:
    #   $i = pretoken index of ending '>' if found, current $i otherwise
    #   $type = 'Q' if found, '>' otherwise
    my ( $input_line, $i_beg, $rtoken_map, $expecting, $max_token_index ) = @_;
    my $i    = $i_beg;
    my $type = '<';
    pos($input_line) = 1 + $rtoken_map->[$i];

    my $filter;

    # we just have to find the next '>' if a term is expected
    if ( $expecting == TERM ) { $filter = '[\>]' }

    # we have to guess if we don't know what is expected
    elsif ( $expecting == UNKNOWN ) { $filter = '[\>\;\=\#\|\<]' }

    # shouldn't happen - we shouldn't be here if operator is expected
    else { warning("Program Bug in find_angle_operator_termination\n") }

    # To illustrate what we might be looking at, in case we are
    # guessing, here are some examples of valid angle operators
    # (or file globs):
    #  <tmp_imp/*>
    #  <FH>
    #  <$fh>
    #  <*.c *.h>
    #  <_>
    #  <jskdfjskdfj* op/* jskdjfjkosvk*> ( glob.t)
    #  <${PREFIX}*img*.$IMAGE_TYPE>
    #  <img*.$IMAGE_TYPE>
    #  <Timg*.$IMAGE_TYPE>
    #  <$LATEX2HTMLVERSIONS${dd}html[1-9].[0-9].pl>
    #
    # Here are some examples of lines which do not have angle operators:
    #  return undef unless $self->[2]++ < $#{$self->[1]};
    #  < 2  || @$t >
    #
    # the following line from dlister.pl caused trouble:
    #  print'~'x79,"\n",$D<1024?"0.$D":$D>>10,"K, $C files\n\n\n";
    #
    # If the '<' starts an angle operator, it must end on this line and
    # it must not have certain characters like ';' and '=' in it.  I use
    # this to limit the testing.  This filter should be improved if
    # possible.

    if ( $input_line =~ /($filter)/g ) {

        if ( $1 eq '>' ) {

            # We MAY have found an angle operator termination if we get
            # here, but we need to do more to be sure we haven't been
            # fooled.
            my $pos = pos($input_line);

            my $pos_beg = $rtoken_map->[$i];
            my $str     = substr( $input_line, $pos_beg, ( $pos - $pos_beg ) );

            # Reject if the closing '>' follows a '-' as in:
            # if ( VERSION < 5.009 && $op-> name eq 'assign' ) { }
            if ( $expecting eq UNKNOWN ) {
                my $check = substr( $input_line, $pos - 2, 1 );
                if ( $check eq '-' ) {
                    return ( $i, $type );
                }
            }

            ######################################debug#####
            #write_diagnostics( "ANGLE? :$str\n");
            #print "ANGLE: found $1 at pos=$pos str=$str check=$check\n";
            ######################################debug#####
            $type = 'Q';
            my $error;
            ( $i, $error ) =
              inverse_pretoken_map( $i, $pos, $rtoken_map, $max_token_index );

            # It may be possible that a quote ends midway in a pretoken.
            # If this happens, it may be necessary to split the pretoken.
            if ($error) {
                warning(
                    "Possible tokinization error..please check this line\n");
                report_possible_bug();
            }

            # Now let's see where we stand....
            # OK if math op not possible
            if ( $expecting == TERM ) {
            }

            # OK if there are no more than 2 pre-tokens inside
            # (not possible to write 2 token math between < and >)
            # This catches most common cases
            elsif ( $i <= $i_beg + 3 ) {
                write_diagnostics("ANGLE(1 or 2 tokens): $str\n");
            }

            # Not sure..
            else {

                # Let's try a Brace Test: any braces inside must balance
                my $br = 0;
                while ( $str =~ /\{/g ) { $br++ }
                while ( $str =~ /\}/g ) { $br-- }
                my $sb = 0;
                while ( $str =~ /\[/g ) { $sb++ }
                while ( $str =~ /\]/g ) { $sb-- }
                my $pr = 0;
                while ( $str =~ /\(/g ) { $pr++ }
                while ( $str =~ /\)/g ) { $pr-- }

                # if braces do not balance - not angle operator
                if ( $br || $sb || $pr ) {
                    $i    = $i_beg;
                    $type = '<';
                    write_diagnostics(
                        "NOT ANGLE (BRACE={$br ($pr [$sb ):$str\n");
                }

                # we should keep doing more checks here...to be continued
                # Tentatively accepting this as a valid angle operator.
                # There are lots more things that can be checked.
                else {
                    write_diagnostics(
                        "ANGLE-Guessing yes: $str expecting=$expecting\n");
                    write_logfile_entry("Guessing angle operator here: $str\n");
                }
            }
        }

        # didn't find ending >
        else {
            if ( $expecting == TERM ) {
                warning("No ending > for angle operator\n");
            }
        }
    }
    return ( $i, $type );
}

sub scan_number_do {

    #  scan a number in any of the formats that Perl accepts
    #  Underbars (_) are allowed in decimal numbers.
    #  input parameters -
    #      $input_line  - the string to scan
    #      $i           - pre_token index to start scanning
    #    $rtoken_map    - reference to the pre_token map giving starting
    #                    character position in $input_line of token $i
    #  output parameters -
    #    $i            - last pre_token index of the number just scanned
    #    number        - the number (characters); or undef if not a number

    my ( $input_line, $i, $rtoken_map, $input_type, $max_token_index ) = @_;
    my $pos_beg = $rtoken_map->[$i];
    my $pos;
    my $i_begin = $i;
    my $number  = undef;
    my $type    = $input_type;

    my $first_char = substr( $input_line, $pos_beg, 1 );

    # Look for bad starting characters; Shouldn't happen..
    if ( $first_char !~ /[\d\.\+\-Ee]/ ) {
        warning("Program bug - scan_number given character $first_char\n");
        report_definite_bug();
        return ( $i, $type, $number );
    }

    # handle v-string without leading 'v' character ('Two Dot' rule)
    # (vstring.t)
    # TODO: v-strings may contain underscores
    pos($input_line) = $pos_beg;
    if ( $input_line =~ /\G((\d+)?\.\d+(\.\d+)+)/g ) {
        $pos = pos($input_line);
        my $numc = $pos - $pos_beg;
        $number = substr( $input_line, $pos_beg, $numc );
        $type   = 'v';
        report_v_string($number);
    }

    # handle octal, hex, binary
    if ( !defined($number) ) {
        pos($input_line) = $pos_beg;
        if ( $input_line =~
            /\G[+-]?0(([xX][0-9a-fA-F_]+)|([0-7_]+)|([bB][01_]+))/g )
        {
            $pos = pos($input_line);
            my $numc = $pos - $pos_beg;
            $number = substr( $input_line, $pos_beg, $numc );
            $type   = 'n';
        }
    }

    # handle decimal
    if ( !defined($number) ) {
        pos($input_line) = $pos_beg;

        if ( $input_line =~ /\G([+-]?[\d_]*(\.[\d_]*)?([Ee][+-]?(\d+))?)/g ) {
            $pos = pos($input_line);

            # watch out for things like 0..40 which would give 0. by this;
            if (   ( substr( $input_line, $pos - 1, 1 ) eq '.' )
                && ( substr( $input_line, $pos, 1 ) eq '.' ) )
            {
                $pos--;
            }
            my $numc = $pos - $pos_beg;
            $number = substr( $input_line, $pos_beg, $numc );
            $type   = 'n';
        }
    }

    # filter out non-numbers like e + - . e2  .e3 +e6
    # the rule: at least one digit, and any 'e' must be preceded by a digit
    if (
        $number !~ /\d/    # no digits
        || (   $number =~ /^(.*)[eE]/
            && $1 !~ /\d/ )    # or no digits before the 'e'
      )
    {
        $number = undef;
        $type   = $input_type;
        return ( $i, $type, $number );
    }

    # Found a number; now we must convert back from character position
    # to pre_token index. An error here implies user syntax error.
    # An example would be an invalid octal number like '009'.
    my $error;
    ( $i, $error ) =
      inverse_pretoken_map( $i, $pos, $rtoken_map, $max_token_index );
    if ($error) { warning("Possibly invalid number\n") }

    return ( $i, $type, $number );
}

sub inverse_pretoken_map {

    # Starting with the current pre_token index $i, scan forward until
    # finding the index of the next pre_token whose position is $pos.
    my ( $i, $pos, $rtoken_map, $max_token_index ) = @_;
    my $error = 0;

    while ( ++$i <= $max_token_index ) {

        if ( $pos <= $rtoken_map->[$i] ) {

            # Let the calling routine handle errors in which we do not
            # land on a pre-token boundary.  It can happen by running
            # perltidy on some non-perl scripts, for example.
            if ( $pos < $rtoken_map->[$i] ) { $error = 1 }
            $i--;
            last;
        }
    }
    return ( $i, $error );
}

sub find_here_doc {

    # find the target of a here document, if any
    # input parameters:
    #   $i - token index of the second < of <<
    #   ($i must be less than the last token index if this is called)
    # output parameters:
    #   $found_target = 0 didn't find target; =1 found target
    #   HERE_TARGET - the target string (may be empty string)
    #   $i - unchanged if not here doc,
    #    or index of the last token of the here target
    #   $saw_error - flag noting unbalanced quote on here target
    my ( $expecting, $i, $rtokens, $rtoken_map, $max_token_index ) = @_;
    my $ibeg                 = $i;
    my $found_target         = 0;
    my $here_doc_target      = '';
    my $here_quote_character = '';
    my $saw_error            = 0;
    my ( $next_nonblank_token, $i_next_nonblank, $next_token );
    $next_token = $rtokens->[ $i + 1 ];

    # perl allows a backslash before the target string (heredoc.t)
    my $backslash = 0;
    if ( $next_token eq '\\' ) {
        $backslash  = 1;
        $next_token = $rtokens->[ $i + 2 ];
    }

    ( $next_nonblank_token, $i_next_nonblank ) =
      find_next_nonblank_token_on_this_line( $i, $rtokens, $max_token_index );

    if ( $next_nonblank_token =~ /[\'\"\`]/ ) {

        my $in_quote    = 1;
        my $quote_depth = 0;
        my $quote_pos   = 0;
        my $quoted_string;

        (
            $i, $in_quote, $here_quote_character, $quote_pos, $quote_depth,
            $quoted_string
          )
          = follow_quoted_string( $i_next_nonblank, $in_quote, $rtokens,
            $here_quote_character, $quote_pos, $quote_depth, $max_token_index );

        if ($in_quote) {    # didn't find end of quote, so no target found
            $i = $ibeg;
            if ( $expecting == TERM ) {
                warning(
"Did not find here-doc string terminator ($here_quote_character) before end of line \n"
                );
                $saw_error = 1;
            }
        }
        else {              # found ending quote
            ##my $j;
            $found_target = 1;

            my $tokj;
            foreach my $j ( $i_next_nonblank + 1 .. $i - 1 ) {
                $tokj = $rtokens->[$j];

                # we have to remove any backslash before the quote character
                # so that the here-doc-target exactly matches this string
                next
                  if ( $tokj eq "\\"
                    && $j < $i - 1
                    && $rtokens->[ $j + 1 ] eq $here_quote_character );
                $here_doc_target .= $tokj;
            }
        }
    }

    elsif ( ( $next_token =~ /^\s*$/ ) and ( $expecting == TERM ) ) {
        $found_target = 1;
        write_logfile_entry(
            "found blank here-target after <<; suggest using \"\"\n");
        $i = $ibeg;
    }
    elsif ( $next_token =~ /^\w/ ) {    # simple bareword or integer after <<

        my $here_doc_expected;
        if ( $expecting == UNKNOWN ) {
            $here_doc_expected = guess_if_here_doc($next_token);
        }
        else {
            $here_doc_expected = 1;
        }

        if ($here_doc_expected) {
            $found_target    = 1;
            $here_doc_target = $next_token;
            $i               = $ibeg + 1;
        }

    }
    else {

        if ( $expecting == TERM ) {
            $found_target = 1;
            write_logfile_entry("Note: bare here-doc operator <<\n");
        }
        else {
            $i = $ibeg;
        }
    }

    # patch to neglect any prepended backslash
    if ( $found_target && $backslash ) { $i++ }

    return ( $found_target, $here_doc_target, $here_quote_character, $i,
        $saw_error );
}

sub do_quote {

    # follow (or continue following) quoted string(s)
    # $in_quote return code:
    #   0 - ok, found end
    #   1 - still must find end of quote whose target is $quote_character
    #   2 - still looking for end of first of two quotes
    #
    # Returns updated strings:
    #  $quoted_string_1 = quoted string seen while in_quote=1
    #  $quoted_string_2 = quoted string seen while in_quote=2
    my (
        $i,               $in_quote,    $quote_character,
        $quote_pos,       $quote_depth, $quoted_string_1,
        $quoted_string_2, $rtokens,     $rtoken_map,
        $max_token_index
    ) = @_;

    my $in_quote_starting = $in_quote;

    my $quoted_string;
    if ( $in_quote == 2 ) {    # two quotes/quoted_string_1s to follow
        my $ibeg = $i;
        (
            $i, $in_quote, $quote_character, $quote_pos, $quote_depth,
            $quoted_string
          )
          = follow_quoted_string( $i, $in_quote, $rtokens, $quote_character,
            $quote_pos, $quote_depth, $max_token_index );
        $quoted_string_2 .= $quoted_string;
        if ( $in_quote == 1 ) {
            if ( $quote_character =~ /[\{\[\<\(]/ ) { $i++; }
            $quote_character = '';
        }
        else {
            $quoted_string_2 .= "\n";
        }
    }

    if ( $in_quote == 1 ) {    # one (more) quote to follow
        my $ibeg = $i;
        (
            $i, $in_quote, $quote_character, $quote_pos, $quote_depth,
            $quoted_string
          )
          = follow_quoted_string( $ibeg, $in_quote, $rtokens, $quote_character,
            $quote_pos, $quote_depth, $max_token_index );
        $quoted_string_1 .= $quoted_string;
        if ( $in_quote == 1 ) {
            $quoted_string_1 .= "\n";
        }
    }
    return ( $i, $in_quote, $quote_character, $quote_pos, $quote_depth,
        $quoted_string_1, $quoted_string_2 );
}

sub follow_quoted_string {

    # scan for a specific token, skipping escaped characters
    # if the quote character is blank, use the first non-blank character
    # input parameters:
    #   $rtokens = reference to the array of tokens
    #   $i = the token index of the first character to search
    #   $in_quote = number of quoted strings being followed
    #   $beginning_tok = the starting quote character
    #   $quote_pos = index to check next for alphanumeric delimiter
    # output parameters:
    #   $i = the token index of the ending quote character
    #   $in_quote = decremented if found end, unchanged if not
    #   $beginning_tok = the starting quote character
    #   $quote_pos = index to check next for alphanumeric delimiter
    #   $quote_depth = nesting depth, since delimiters '{ ( [ <' can be nested.
    #   $quoted_string = the text of the quote (without quotation tokens)
    my ( $i_beg, $in_quote, $rtokens, $beginning_tok, $quote_pos, $quote_depth,
        $max_token_index )
      = @_;
    my ( $tok, $end_tok );
    my $i             = $i_beg - 1;
    my $quoted_string = "";

    TOKENIZER_DEBUG_FLAG_QUOTE && do {
        print STDOUT
"QUOTE entering with quote_pos = $quote_pos i=$i beginning_tok =$beginning_tok\n";
    };

    # get the corresponding end token
    if ( $beginning_tok !~ /^\s*$/ ) {
        $end_tok = matching_end_token($beginning_tok);
    }

    # a blank token means we must find and use the first non-blank one
    else {
        my $allow_quote_comments = ( $i < 0 ) ? 1 : 0; # i<0 means we saw a <cr>

        while ( $i < $max_token_index ) {
            $tok = $rtokens->[ ++$i ];

            if ( $tok !~ /^\s*$/ ) {

                if ( ( $tok eq '#' ) && ($allow_quote_comments) ) {
                    $i = $max_token_index;
                }
                else {

                    if ( length($tok) > 1 ) {
                        if ( $quote_pos <= 0 ) { $quote_pos = 1 }
                        $beginning_tok = substr( $tok, $quote_pos - 1, 1 );
                    }
                    else {
                        $beginning_tok = $tok;
                        $quote_pos     = 0;
                    }
                    $end_tok     = matching_end_token($beginning_tok);
                    $quote_depth = 1;
                    last;
                }
            }
            else {
                $allow_quote_comments = 1;
            }
        }
    }

    # There are two different loops which search for the ending quote
    # character.  In the rare case of an alphanumeric quote delimiter, we
    # have to look through alphanumeric tokens character-by-character, since
    # the pre-tokenization process combines multiple alphanumeric
    # characters, whereas for a non-alphanumeric delimiter, only tokens of
    # length 1 can match.

    ###################################################################
    # Case 1 (rare): loop for case of alphanumeric quote delimiter..
    # "quote_pos" is the position the current word to begin searching
    ###################################################################
    if ( $beginning_tok =~ /\w/ ) {

        # Note this because it is not recommended practice except
        # for obfuscated perl contests
        if ( $in_quote == 1 ) {
            write_logfile_entry(
                "Note: alphanumeric quote delimiter ($beginning_tok) \n");
        }

        while ( $i < $max_token_index ) {

            if ( $quote_pos == 0 || ( $i < 0 ) ) {
                $tok = $rtokens->[ ++$i ];

                if ( $tok eq '\\' ) {

                    # retain backslash unless it hides the end token
                    $quoted_string .= $tok
                      unless $rtokens->[ $i + 1 ] eq $end_tok;
                    $quote_pos++;
                    last if ( $i >= $max_token_index );
                    $tok = $rtokens->[ ++$i ];
                }
            }
            my $old_pos = $quote_pos;

            unless ( defined($tok) && defined($end_tok) && defined($quote_pos) )
            {

            }
            $quote_pos = 1 + index( $tok, $end_tok, $quote_pos );

            if ( $quote_pos > 0 ) {

                $quoted_string .=
                  substr( $tok, $old_pos, $quote_pos - $old_pos - 1 );

                $quote_depth--;

                if ( $quote_depth == 0 ) {
                    $in_quote--;
                    last;
                }
            }
            else {
                $quoted_string .= substr( $tok, $old_pos );
            }
        }
    }

    ########################################################################
    # Case 2 (normal): loop for case of a non-alphanumeric quote delimiter..
    ########################################################################
    else {

        while ( $i < $max_token_index ) {
            $tok = $rtokens->[ ++$i ];

            if ( $tok eq $end_tok ) {
                $quote_depth--;

                if ( $quote_depth == 0 ) {
                    $in_quote--;
                    last;
                }
            }
            elsif ( $tok eq $beginning_tok ) {
                $quote_depth++;
            }
            elsif ( $tok eq '\\' ) {

                # retain backslash unless it hides the beginning or end token
                $tok = $rtokens->[ ++$i ];
                $quoted_string .= '\\'
                  unless ( $tok eq $end_tok || $tok eq $beginning_tok );
            }
            $quoted_string .= $tok;
        }
    }
    if ( $i > $max_token_index ) { $i = $max_token_index }
    return ( $i, $in_quote, $beginning_tok, $quote_pos, $quote_depth,
        $quoted_string );
}

sub indicate_error {
    my ( $msg, $line_number, $input_line, $pos, $carrat ) = @_;
    interrupt_logfile();
    warning($msg);
    write_error_indicator_pair( $line_number, $input_line, $pos, $carrat );
    resume_logfile();
    return;
}

sub write_error_indicator_pair {
    my ( $line_number, $input_line, $pos, $carrat ) = @_;
    my ( $offset, $numbered_line, $underline ) =
      make_numbered_line( $line_number, $input_line, $pos );
    $underline = write_on_underline( $underline, $pos - $offset, $carrat );
    warning( $numbered_line . "\n" );
    $underline =~ s/\s*$//;
    warning( $underline . "\n" );
    return;
}

sub make_numbered_line {

    #  Given an input line, its line number, and a character position of
    #  interest, create a string not longer than 80 characters of the form
    #     $lineno: sub_string
    #  such that the sub_string of $str contains the position of interest
    #
    #  Here is an example of what we want, in this case we add trailing
    #  '...' because the line is long.
    #
    # 2: (One of QAML 2.0's authors is a member of the World Wide Web Con ...
    #
    #  Here is another example, this time in which we used leading '...'
    #  because of excessive length:
    #
    # 2: ... er of the World Wide Web Consortium's
    #
    #  input parameters are:
    #   $lineno = line number
    #   $str = the text of the line
    #   $pos = position of interest (the error) : 0 = first character
    #
    #   We return :
    #     - $offset = an offset which corrects the position in case we only
    #       display part of a line, such that $pos-$offset is the effective
    #       position from the start of the displayed line.
    #     - $numbered_line = the numbered line as above,
    #     - $underline = a blank 'underline' which is all spaces with the same
    #       number of characters as the numbered line.

    my ( $lineno, $str, $pos ) = @_;
    my $offset = ( $pos < 60 ) ? 0 : $pos - 40;
    my $excess = length($str) - $offset - 68;
    my $numc   = ( $excess > 0 ) ? 68 : undef;

    if ( defined($numc) ) {
        if ( $offset == 0 ) {
            $str = substr( $str, $offset, $numc - 4 ) . " ...";
        }
        else {
            $str = "... " . substr( $str, $offset + 4, $numc - 4 ) . " ...";
        }
    }
    else {

        if ( $offset == 0 ) {
        }
        else {
            $str = "... " . substr( $str, $offset + 4 );
        }
    }

    my $numbered_line = sprintf( "%d: ", $lineno );
    $offset -= length($numbered_line);
    $numbered_line .= $str;
    my $underline = " " x length($numbered_line);
    return ( $offset, $numbered_line, $underline );
}

sub write_on_underline {

    # The "underline" is a string that shows where an error is; it starts
    # out as a string of blanks with the same length as the numbered line of
    # code above it, and we have to add marking to show where an error is.
    # In the example below, we want to write the string '--^' just below
    # the line of bad code:
    #
    # 2: (One of QAML 2.0's authors is a member of the World Wide Web Con ...
    #                 ---^
    # We are given the current underline string, plus a position and a
    # string to write on it.
    #
    # In the above example, there will be 2 calls to do this:
    # First call:  $pos=19, pos_chr=^
    # Second call: $pos=16, pos_chr=---
    #
    # This is a trivial thing to do with substr, but there is some
    # checking to do.

    my ( $underline, $pos, $pos_chr ) = @_;

    # check for error..shouldn't happen
    unless ( ( $pos >= 0 ) && ( $pos <= length($underline) ) ) {
        return $underline;
    }
    my $excess = length($pos_chr) + $pos - length($underline);
    if ( $excess > 0 ) {
        $pos_chr = substr( $pos_chr, 0, length($pos_chr) - $excess );
    }
    substr( $underline, $pos, length($pos_chr) ) = $pos_chr;
    return ($underline);
}

sub pre_tokenize {

    # Break a string, $str, into a sequence of preliminary tokens.  We
    # are interested in these types of tokens:
    #   words       (type='w'),            example: 'max_tokens_wanted'
    #   digits      (type = 'd'),          example: '0755'
    #   whitespace  (type = 'b'),          example: '   '
    #   any other single character (i.e. punct; type = the character itself).
    # We cannot do better than this yet because we might be in a quoted
    # string or pattern.  Caller sets $max_tokens_wanted to 0 to get all
    # tokens.
    my ( $str, $max_tokens_wanted ) = @_;

    # we return references to these 3 arrays:
    my @tokens    = ();     # array of the tokens themselves
    my @token_map = (0);    # string position of start of each token
    my @type      = ();     # 'b'=whitespace, 'd'=digits, 'w'=alpha, or punct

    do {

        # whitespace
        if ( $str =~ /\G(\s+)/gc ) { push @type, 'b'; }

        # numbers
        # note that this must come before words!
        elsif ( $str =~ /\G(\d+)/gc ) { push @type, 'd'; }

        # words
        elsif ( $str =~ /\G(\w+)/gc ) { push @type, 'w'; }

        # single-character punctuation
        elsif ( $str =~ /\G(\W)/gc ) { push @type, $1; }

        # that's all..
        else {
            return ( \@tokens, \@token_map, \@type );
        }

        push @tokens,    $1;
        push @token_map, pos($str);

    } while ( --$max_tokens_wanted != 0 );

    return ( \@tokens, \@token_map, \@type );
}

sub show_tokens {

    # this is an old debug routine
    # not called, but saved for reference
    my ( $rtokens, $rtoken_map ) = @_;
    my $num = scalar( @{$rtokens} );

    foreach my $i ( 0 .. $num - 1 ) {
        my $len = length( $rtokens->[$i] );
        print STDOUT "$i:$len:$rtoken_map->[$i]:$rtokens->[$i]:\n";
    }
    return;
}

{
    my %matching_end_token;

    BEGIN {
        %matching_end_token = (
            '{' => '}',
            '(' => ')',
            '[' => ']',
            '<' => '>',
        );
    }

    sub matching_end_token {

        # return closing character for a pattern
        my $beginning_token = shift;
        if ( $matching_end_token{$beginning_token} ) {
            return $matching_end_token{$beginning_token};
        }
        return ($beginning_token);
    }
}

sub dump_token_types {
    my ( $class, $fh ) = @_;

    # This should be the latest list of token types in use
    # adding NEW_TOKENS: add a comment here
    print $fh <<'END_OF_LIST';

Here is a list of the token types currently used for lines of type 'CODE'.  
For the following tokens, the "type" of a token is just the token itself.  

.. :: << >> ** && .. || // -> => += -= .= %= &= |= ^= *= <>
( ) <= >= == =~ !~ != ++ -- /= x=
... **= <<= >>= &&= ||= //= <=> 
, + - / * | % ! x ~ = \ ? : . < > ^ &

The following additional token types are defined:

 type    meaning
    b    blank (white space) 
    {    indent: opening structural curly brace or square bracket or paren
         (code block, anonymous hash reference, or anonymous array reference)
    }    outdent: right structural curly brace or square bracket or paren
    [    left non-structural square bracket (enclosing an array index)
    ]    right non-structural square bracket
    (    left non-structural paren (all but a list right of an =)
    )    right non-structural paren
    L    left non-structural curly brace (enclosing a key)
    R    right non-structural curly brace 
    ;    terminal semicolon
    f    indicates a semicolon in a "for" statement
    h    here_doc operator <<
    #    a comment
    Q    indicates a quote or pattern
    q    indicates a qw quote block
    k    a perl keyword
    C    user-defined constant or constant function (with void prototype = ())
    U    user-defined function taking parameters
    G    user-defined function taking block parameter (like grep/map/eval)
    M    (unused, but reserved for subroutine definition name)
    P    (unused, but -html uses it to label pod text)
    t    type indicater such as %,$,@,*,&,sub
    w    bare word (perhaps a subroutine call)
    i    identifier of some type (with leading %, $, @, *, &, sub, -> )
    n    a number
    v    a v-string
    F    a file test operator (like -e)
    Y    File handle
    Z    identifier in indirect object slot: may be file handle, object
    J    LABEL:  code block label
    j    LABEL after next, last, redo, goto
    p    unary +
    m    unary -
    pp   pre-increment operator ++
    mm   pre-decrement operator -- 
    A    : used as attribute separator
    
    Here are the '_line_type' codes used internally:
    SYSTEM         - system-specific code before hash-bang line
    CODE           - line of perl code (including comments)
    POD_START      - line starting pod, such as '=head'
    POD            - pod documentation text
    POD_END        - last line of pod section, '=cut'
    HERE           - text of here-document
    HERE_END       - last line of here-doc (target word)
    FORMAT         - format section
    FORMAT_END     - last line of format section, '.'
    DATA_START     - __DATA__ line
    DATA           - unidentified text following __DATA__
    END_START      - __END__ line
    END            - unidentified text following __END__
    ERROR          - we are in big trouble, probably not a perl script
END_OF_LIST

    return;
}

BEGIN {

    # These names are used in error messages
    @opening_brace_names = qw# '{' '[' '(' '?' #;
    @closing_brace_names = qw# '}' ']' ')' ':' #;

    my @q;

    my @digraphs = qw(
      .. :: << >> ** && .. || // -> => += -= .= %= &= |= ^= *= <>
      <= >= == =~ !~ != ++ -- /= x= ~~ ~. |. &. ^.
    );
    @is_digraph{@digraphs} = (1) x scalar(@digraphs);

    my @trigraphs = qw( ... **= <<= >>= &&= ||= //= <=> !~~ &.= |.= ^.= <<~);
    @is_trigraph{@trigraphs} = (1) x scalar(@trigraphs);

    my @tetragraphs = qw( <<>> );
    @is_tetragraph{@tetragraphs} = (1) x scalar(@tetragraphs);

    # make a hash of all valid token types for self-checking the tokenizer
    # (adding NEW_TOKENS : select a new character and add to this list)
    my @valid_token_types = qw#
      A b C G L R f h Q k t w i q n p m F pp mm U j J Y Z v
      { } ( ) [ ] ; + - / * | % ! x ~ = \ ? : . < > ^ &
      #;
    push( @valid_token_types, @digraphs );
    push( @valid_token_types, @trigraphs );
    push( @valid_token_types, @tetragraphs );
    push( @valid_token_types, ( '#', ',', 'CORE::' ) );
    @is_valid_token_type{@valid_token_types} = (1) x scalar(@valid_token_types);

    # a list of file test letters, as in -e (Table 3-4 of 'camel 3')
    my @file_test_operators =
      qw( A B C M O R S T W X b c d e f g k l o p r s t u w x z);
    @is_file_test_operator{@file_test_operators} =
      (1) x scalar(@file_test_operators);

    # these functions have prototypes of the form (&), so when they are
    # followed by a block, that block MAY BE followed by an operator.
    # Smartmatch operator ~~ may be followed by anonymous hash or array ref
    @q = qw( do eval );
    @is_block_operator{@q} = (1) x scalar(@q);

    # these functions allow an identifier in the indirect object slot
    @q = qw( print printf sort exec system say);
    @is_indirect_object_taker{@q} = (1) x scalar(@q);

    # These tokens may precede a code block
    # patched for SWITCH/CASE/CATCH.  Actually these could be removed
    # now and we could let the extended-syntax coding handle them
    @q =
      qw( BEGIN END CHECK INIT AUTOLOAD DESTROY UNITCHECK continue if elsif else
      unless do while until eval for foreach map grep sort
      switch case given when catch try finally);
    @is_code_block_token{@q} = (1) x scalar(@q);

    # I'll build the list of keywords incrementally
    my @Keywords = ();

    # keywords and tokens after which a value or pattern is expected,
    # but not an operator.  In other words, these should consume terms
    # to their right, or at least they are not expected to be followed
    # immediately by operators.
    my @value_requestor = qw(
      AUTOLOAD
      BEGIN
      CHECK
      DESTROY
      END
      EQ
      GE
      GT
      INIT
      LE
      LT
      NE
      UNITCHECK
      abs
      accept
      alarm
      and
      atan2
      bind
      binmode
      bless
      break
      caller
      chdir
      chmod
      chomp
      chop
      chown
      chr
      chroot
      close
      closedir
      cmp
      connect
      continue
      cos
      crypt
      dbmclose
      dbmopen
      defined
      delete
      die
      dump
      each
      else
      elsif
      eof
      eq
      exec
      exists
      exit
      exp
      fcntl
      fileno
      flock
      for
      foreach
      formline
      ge
      getc
      getgrgid
      getgrnam
      gethostbyaddr
      gethostbyname
      getnetbyaddr
      getnetbyname
      getpeername
      getpgrp
      getpriority
      getprotobyname
      getprotobynumber
      getpwnam
      getpwuid
      getservbyname
      getservbyport
      getsockname
      getsockopt
      glob
      gmtime
      goto
      grep
      gt
      hex
      if
      index
      int
      ioctl
      join
      keys
      kill
      last
      lc
      lcfirst
      le
      length
      link
      listen
      local
      localtime
      lock
      log
      lstat
      lt
      map
      mkdir
      msgctl
      msgget
      msgrcv
      msgsnd
      my
      ne
      next
      no
      not
      oct
      open
      opendir
      or
      ord
      our
      pack
      pipe
      pop
      pos
      print
      printf
      prototype
      push
      quotemeta
      rand
      read
      readdir
      readlink
      readline
      readpipe
      recv
      redo
      ref
      rename
      require
      reset
      return
      reverse
      rewinddir
      rindex
      rmdir
      scalar
      seek
      seekdir
      select
      semctl
      semget
      semop
      send
      sethostent
      setnetent
      setpgrp
      setpriority
      setprotoent
      setservent
      setsockopt
      shift
      shmctl
      shmget
      shmread
      shmwrite
      shutdown
      sin
      sleep
      socket
      socketpair
      sort
      splice
      split
      sprintf
      sqrt
      srand
      stat
      study
      substr
      symlink
      syscall
      sysopen
      sysread
      sysseek
      system
      syswrite
      tell
      telldir
      tie
      tied
      truncate
      uc
      ucfirst
      umask
      undef
      unless
      unlink
      unpack
      unshift
      untie
      until
      use
      utime
      values
      vec
      waitpid
      warn
      while
      write
      xor

      switch
      case
      given
      when
      err
      say

      catch
    );

    # patched above for SWITCH/CASE given/when err say
    # 'err' is a fairly safe addition.
    # TODO: 'default' still needed if appropriate
    # 'use feature' seen, but perltidy works ok without it.
    # Concerned that 'default' could break code.
    push( @Keywords, @value_requestor );

    # These are treated the same but are not keywords:
    my @extra_vr = qw(
      constant
      vars
    );
    push( @value_requestor, @extra_vr );

    @expecting_term_token{@value_requestor} = (1) x scalar(@value_requestor);

    # this list contains keywords which do not look for arguments,
    # so that they might be followed by an operator, or at least
    # not a term.
    my @operator_requestor = qw(
      endgrent
      endhostent
      endnetent
      endprotoent
      endpwent
      endservent
      fork
      getgrent
      gethostent
      getlogin
      getnetent
      getppid
      getprotoent
      getpwent
      getservent
      setgrent
      setpwent
      time
      times
      wait
      wantarray
    );

    push( @Keywords, @operator_requestor );

    # These are treated the same but are not considered keywords:
    my @extra_or = qw(
      STDERR
      STDIN
      STDOUT
    );

    push( @operator_requestor, @extra_or );

    @expecting_operator_token{@operator_requestor} =
      (1) x scalar(@operator_requestor);

    # these token TYPES expect trailing operator but not a term
    # note: ++ and -- are post-increment and decrement, 'C' = constant
    my @operator_requestor_types = qw( ++ -- C <> q );
    @expecting_operator_types{@operator_requestor_types} =
      (1) x scalar(@operator_requestor_types);

    # these token TYPES consume values (terms)
    # note: pp and mm are pre-increment and decrement
    # f=semicolon in for,  F=file test operator
    my @value_requestor_type = qw#
      L { ( [ ~ !~ =~ ; . .. ... A : && ! || // = + - x
      **= += -= .= /= *= %= x= &= |= ^= <<= >>= &&= ||= //=
      <= >= == != => \ > < % * / ? & | ** <=> ~~ !~~
      f F pp mm Y p m U J G j >> << ^ t
      ~. ^. |. &. ^.= |.= &.=
      #;
    push( @value_requestor_type, ',' )
      ;    # (perl doesn't like a ',' in a qw block)
    @expecting_term_types{@value_requestor_type} =
      (1) x scalar(@value_requestor_type);

    # Note: the following valid token types are not assigned here to
    # hashes requesting to be followed by values or terms, but are
    # instead currently hard-coded into sub operator_expected:
    # ) -> :: Q R Z ] b h i k n v w } #

    # For simple syntax checking, it is nice to have a list of operators which
    # will really be unhappy if not followed by a term.  This includes most
    # of the above...
    %really_want_term = %expecting_term_types;

    # with these exceptions...
    delete $really_want_term{'U'}; # user sub, depends on prototype
    delete $really_want_term{'F'}; # file test works on $_ if no following term
    delete $really_want_term{'Y'}; # indirect object, too risky to check syntax;
                                   # let perl do it

    @q = qw(q qq qw qx qr s y tr m);
    @is_q_qq_qw_qx_qr_s_y_tr_m{@q} = (1) x scalar(@q);

    # These keywords are handled specially in the tokenizer code:
    my @special_keywords = qw(
      do
      eval
      format
      m
      package
      q
      qq
      qr
      qw
      qx
      s
      sub
      tr
      y
    );
    push( @Keywords, @special_keywords );

    # Keywords after which list formatting may be used
    # WARNING: do not include |map|grep|eval or perl may die on
    # syntax errors (map1.t).
    my @keyword_taking_list = qw(
      and
      chmod
      chomp
      chop
      chown
      dbmopen
      die
      elsif
      exec
      fcntl
      for
      foreach
      formline
      getsockopt
      if
      index
      ioctl
      join
      kill
      local
      msgctl
      msgrcv
      msgsnd
      my
      open
      or
      our
      pack
      print
      printf
      push
      read
      readpipe
      recv
      return
      reverse
      rindex
      seek
      select
      semctl
      semget
      send
      setpriority
      setsockopt
      shmctl
      shmget
      shmread
      shmwrite
      socket
      socketpair
      sort
      splice
      split
      sprintf
      substr
      syscall
      sysopen
      sysread
      sysseek
      system
      syswrite
      tie
      unless
      unlink
      unpack
      unshift
      until
      vec
      warn
      while
      given
      when
    );
    @is_keyword_taking_list{@keyword_taking_list} =
      (1) x scalar(@keyword_taking_list);

    # perl functions which may be unary operators
    my @keyword_taking_optional_args = qw(
      chomp
      eof
      eval
      lc
      pop
      shift
      uc
      undef
    );
    @is_keyword_taking_optional_args{@keyword_taking_optional_args} =
      (1) x scalar(@keyword_taking_optional_args);

    # These are not used in any way yet
    #    my @unused_keywords = qw(
    #     __FILE__
    #     __LINE__
    #     __PACKAGE__
    #     );

    #  The list of keywords was originally extracted from function 'keyword' in
    #  perl file toke.c version 5.005.03, using this utility, plus a
    #  little editing: (file getkwd.pl):
    #  while (<>) { while (/\"(.*)\"/g) { print "$1\n"; } }
    #  Add 'get' prefix where necessary, then split into the above lists.
    #  This list should be updated as necessary.
    #  The list should not contain these special variables:
    #  ARGV DATA ENV SIG STDERR STDIN STDOUT
    #  __DATA__ __END__

    @is_keyword{@Keywords} = (1) x scalar(@Keywords);
}
1;

