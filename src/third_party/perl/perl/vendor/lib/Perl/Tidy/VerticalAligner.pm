package Perl::Tidy::VerticalAligner;
use strict;
use warnings;
our $VERSION = '20181120';

use Perl::Tidy::VerticalAligner::Alignment;
use Perl::Tidy::VerticalAligner::Line;

# The Perl::Tidy::VerticalAligner package collects output lines and
# attempts to line up certain common tokens, such as => and #, which are
# identified by the calling routine.
#
# There are two main routines: valign_input and flush.  Append acts as a
# storage buffer, collecting lines into a group which can be vertically
# aligned.  When alignment is no longer possible or desirable, it dumps
# the group to flush.
#
#     valign_input -----> flush
#
#     collects          writes
#     vertical          one
#     groups            group

BEGIN {

    # Caution: these debug flags produce a lot of output
    # They should all be 0 except when debugging small scripts

    use constant VALIGN_DEBUG_FLAG_APPEND  => 0;
    use constant VALIGN_DEBUG_FLAG_APPEND0 => 0;
    use constant VALIGN_DEBUG_FLAG_TERNARY => 0;
    use constant VALIGN_DEBUG_FLAG_TABS    => 0;

    my $debug_warning = sub {
        print STDOUT "VALIGN_DEBUGGING with key $_[0]\n";
        return;
    };

    VALIGN_DEBUG_FLAG_APPEND  && $debug_warning->('APPEND');
    VALIGN_DEBUG_FLAG_APPEND0 && $debug_warning->('APPEND0');
    VALIGN_DEBUG_FLAG_TERNARY && $debug_warning->('TERNARY');
    VALIGN_DEBUG_FLAG_TABS    && $debug_warning->('TABS');

}

use vars qw(
  $vertical_aligner_self
  $current_line
  $maximum_alignment_index
  $ralignment_list
  $maximum_jmax_seen
  $minimum_jmax_seen
  $previous_minimum_jmax_seen
  $previous_maximum_jmax_seen
  $maximum_line_index
  $group_level
  $group_type
  $group_maximum_gap
  $marginal_match
  $last_level_written
  $last_leading_space_count
  $extra_indent_ok
  $zero_count
  @group_lines
  $last_comment_column
  $last_side_comment_line_number
  $last_side_comment_length
  $last_side_comment_level
  $outdented_line_count
  $first_outdented_line_at
  $last_outdented_line_at
  $diagnostics_object
  $logger_object
  $file_writer_object
  @side_comment_history
  $comment_leading_space_count
  $is_matching_terminal_line
  $consecutive_block_comments

  $cached_line_text
  $cached_line_type
  $cached_line_flag
  $cached_seqno
  $cached_line_valid
  $cached_line_leading_space_count
  $cached_seqno_string

  $valign_buffer_filling
  @valign_buffer

  $seqno_string
  $last_nonblank_seqno_string

  $rOpts

  $rOpts_maximum_line_length
  $rOpts_variable_maximum_line_length
  $rOpts_continuation_indentation
  $rOpts_indent_columns
  $rOpts_tabs
  $rOpts_entab_leading_whitespace
  $rOpts_valign

  $rOpts_fixed_position_side_comment
  $rOpts_minimum_space_to_comment

);

sub initialize {

    (
        my $class, $rOpts, $file_writer_object, $logger_object,
        $diagnostics_object
    ) = @_;

    # variables describing the entire space group:
    $ralignment_list            = [];
    $group_level                = 0;
    $last_level_written         = -1;
    $extra_indent_ok            = 0;    # can we move all lines to the right?
    $last_side_comment_length   = 0;
    $maximum_jmax_seen          = 0;
    $minimum_jmax_seen          = 0;
    $previous_minimum_jmax_seen = 0;
    $previous_maximum_jmax_seen = 0;

    # variables describing each line of the group
    @group_lines = ();                  # list of all lines in group

    $outdented_line_count          = 0;
    $first_outdented_line_at       = 0;
    $last_outdented_line_at        = 0;
    $last_side_comment_line_number = 0;
    $last_side_comment_level       = -1;
    $is_matching_terminal_line     = 0;

    # most recent 3 side comments; [ line number, column ]
    $side_comment_history[0] = [ -300, 0 ];
    $side_comment_history[1] = [ -200, 0 ];
    $side_comment_history[2] = [ -100, 0 ];

    # valign_output_step_B cache:
    $cached_line_text                = "";
    $cached_line_type                = 0;
    $cached_line_flag                = 0;
    $cached_seqno                    = 0;
    $cached_line_valid               = 0;
    $cached_line_leading_space_count = 0;
    $cached_seqno_string             = "";

    # string of sequence numbers joined together
    $seqno_string               = "";
    $last_nonblank_seqno_string = "";

    # frequently used parameters
    $rOpts_indent_columns           = $rOpts->{'indent-columns'};
    $rOpts_tabs                     = $rOpts->{'tabs'};
    $rOpts_entab_leading_whitespace = $rOpts->{'entab-leading-whitespace'};
    $rOpts_fixed_position_side_comment =
      $rOpts->{'fixed-position-side-comment'};
    $rOpts_minimum_space_to_comment = $rOpts->{'minimum-space-to-comment'};
    $rOpts_maximum_line_length      = $rOpts->{'maximum-line-length'};
    $rOpts_variable_maximum_line_length =
      $rOpts->{'variable-maximum-line-length'};
    $rOpts_valign = $rOpts->{'valign'};

    $consecutive_block_comments = 0;
    forget_side_comment();

    initialize_for_new_group();

    $vertical_aligner_self = {};
    bless $vertical_aligner_self, $class;
    return $vertical_aligner_self;
}

sub initialize_for_new_group {
    $maximum_line_index      = -1;      # lines in the current group
    $maximum_alignment_index = -1;      # alignments in current group
    $zero_count              = 0;       # count consecutive lines without tokens
    $current_line            = undef;   # line being matched for alignment
    $group_maximum_gap       = 0;       # largest gap introduced
    $group_type              = "";
    $marginal_match          = 0;
    $comment_leading_space_count = 0;
    $last_leading_space_count    = 0;
    return;
}

# interface to Perl::Tidy::Diagnostics routines
sub write_diagnostics {
    my $msg = shift;
    if ($diagnostics_object) {
        $diagnostics_object->write_diagnostics($msg);
    }
    return;
}

# interface to Perl::Tidy::Logger routines
sub warning {
    my ($msg) = @_;
    if ($logger_object) {
        $logger_object->warning($msg);
    }
    return;
}

sub write_logfile_entry {
    my ($msg) = @_;
    if ($logger_object) {
        $logger_object->write_logfile_entry($msg);
    }
    return;
}

sub report_definite_bug {
    if ($logger_object) {
        $logger_object->report_definite_bug();
    }
    return;
}

sub get_spaces {

    # return the number of leading spaces associated with an indentation
    # variable $indentation is either a constant number of spaces or an
    # object with a get_spaces method.
    my $indentation = shift;
    return ref($indentation) ? $indentation->get_spaces() : $indentation;
}

sub get_recoverable_spaces {

    # return the number of spaces (+ means shift right, - means shift left)
    # that we would like to shift a group of lines with the same indentation
    # to get them to line up with their opening parens
    my $indentation = shift;
    return ref($indentation) ? $indentation->get_recoverable_spaces() : 0;
}

sub get_stack_depth {

    my $indentation = shift;
    return ref($indentation) ? $indentation->get_stack_depth() : 0;
}

sub make_alignment {
    my ( $col, $token ) = @_;

    # make one new alignment at column $col which aligns token $token
    ++$maximum_alignment_index;

    #my $alignment = new Perl::Tidy::VerticalAligner::Alignment(
    my $alignment = Perl::Tidy::VerticalAligner::Alignment->new(
        column          => $col,
        starting_column => $col,
        matching_token  => $token,
        starting_line   => $maximum_line_index,
        ending_line     => $maximum_line_index,
        serial_number   => $maximum_alignment_index,
    );
    $ralignment_list->[$maximum_alignment_index] = $alignment;
    return $alignment;
}

sub dump_alignments {
    print STDOUT
"Current Alignments:\ni\ttoken\tstarting_column\tcolumn\tstarting_line\tending_line\n";
    for my $i ( 0 .. $maximum_alignment_index ) {
        my $column          = $ralignment_list->[$i]->get_column();
        my $starting_column = $ralignment_list->[$i]->get_starting_column();
        my $matching_token  = $ralignment_list->[$i]->get_matching_token();
        my $starting_line   = $ralignment_list->[$i]->get_starting_line();
        my $ending_line     = $ralignment_list->[$i]->get_ending_line();
        print STDOUT
"$i\t$matching_token\t$starting_column\t$column\t$starting_line\t$ending_line\n";
    }
    return;
}

sub save_alignment_columns {
    for my $i ( 0 .. $maximum_alignment_index ) {
        $ralignment_list->[$i]->save_column();
    }
    return;
}

sub restore_alignment_columns {
    for my $i ( 0 .. $maximum_alignment_index ) {
        $ralignment_list->[$i]->restore_column();
    }
    return;
}

sub forget_side_comment {
    $last_comment_column = 0;
    return;
}

sub maximum_line_length_for_level {

    # return maximum line length for line starting with a given level
    my $maximum_line_length = $rOpts_maximum_line_length;
    if ($rOpts_variable_maximum_line_length) {
        my $level = shift;
        if ( $level < 0 ) { $level = 0 }
        $maximum_line_length += $level * $rOpts_indent_columns;
    }
    return $maximum_line_length;
}

sub valign_input {

    # Place one line in the current vertical group.
    #
    # The input parameters are:
    #     $level = indentation level of this line
    #     $rfields = reference to array of fields
    #     $rpatterns = reference to array of patterns, one per field
    #     $rtokens   = reference to array of tokens starting fields 1,2,..
    #
    # Here is an example of what this package does.  In this example,
    # we are trying to line up both the '=>' and the '#'.
    #
    #         '18' => 'grave',    #   \`
    #         '19' => 'acute',    #   `'
    #         '20' => 'caron',    #   \v
    # <-tabs-><f1-><--field 2 ---><-f3->
    # |            |              |    |
    # |            |              |    |
    # col1        col2         col3 col4
    #
    # The calling routine has already broken the entire line into 3 fields as
    # indicated.  (So the work of identifying promising common tokens has
    # already been done).
    #
    # In this example, there will be 2 tokens being matched: '=>' and '#'.
    # They are the leading parts of fields 2 and 3, but we do need to know
    # what they are so that we can dump a group of lines when these tokens
    # change.
    #
    # The fields contain the actual characters of each field.  The patterns
    # are like the fields, but they contain mainly token types instead
    # of tokens, so they have fewer characters.  They are used to be
    # sure we are matching fields of similar type.
    #
    # In this example, there will be 4 column indexes being adjusted.  The
    # first one is always at zero.  The interior columns are at the start of
    # the matching tokens, and the last one tracks the maximum line length.
    #
    # Each time a new line comes in, it joins the current vertical
    # group if possible.  Otherwise it causes the current group to be dumped
    # and a new group is started.
    #
    # For each new group member, the column locations are increased, as
    # necessary, to make room for the new fields.  When the group is finally
    # output, these column numbers are used to compute the amount of spaces of
    # padding needed for each field.
    #
    # Programming note: the fields are assumed not to have any tab characters.
    # Tabs have been previously removed except for tabs in quoted strings and
    # side comments.  Tabs in these fields can mess up the column counting.
    # The log file warns the user if there are any such tabs.

    my ( $rline_hash, $rfields, $rtokens, $rpatterns ) = @_;
    my $level                     = $rline_hash->{level};
    my $level_end                 = $rline_hash->{level_end};
    my $indentation               = $rline_hash->{indentation};
    my $is_forced_break           = $rline_hash->{is_forced_break};
    my $outdent_long_lines        = $rline_hash->{outdent_long_lines};
    my $is_terminal_ternary       = $rline_hash->{is_terminal_ternary};
    my $is_terminal_statement     = $rline_hash->{is_terminal_statement};
    my $do_not_pad                = $rline_hash->{do_not_pad};
    my $rvertical_tightness_flags = $rline_hash->{rvertical_tightness_flags};
    my $level_jump                = $rline_hash->{level_jump};

    # number of fields is $jmax
    # number of tokens between fields is $jmax-1
    my $jmax = $#{$rfields};

    my $leading_space_count = get_spaces($indentation);

    # set outdented flag to be sure we either align within statements or
    # across statement boundaries, but not both.
    my $is_outdented = $last_leading_space_count > $leading_space_count;
    $last_leading_space_count = $leading_space_count;

    # Patch: undo for hanging side comment
    my $is_hanging_side_comment =
      ( $jmax == 1 && $rtokens->[0] eq '#' && $rfields->[0] =~ /^\s*$/ );
    $is_outdented = 0 if $is_hanging_side_comment;

    # Forget side comment alignment after seeing 2 or more block comments
    my $is_block_comment = ( $jmax == 0 && $rfields->[0] =~ /^#/ );
    if ($is_block_comment) {
        $consecutive_block_comments++;
    }
    else {
        if ( $consecutive_block_comments > 1 ) { forget_side_comment() }
        $consecutive_block_comments = 0;
    }

    VALIGN_DEBUG_FLAG_APPEND0 && do {
        print STDOUT
"APPEND0: entering lines=$maximum_line_index new #fields= $jmax, leading_count=$leading_space_count last_cmt=$last_comment_column force=$is_forced_break\n";
    };

    # Validate cached line if necessary: If we can produce a container
    # with just 2 lines total by combining an existing cached opening
    # token with the closing token to follow, then we will mark both
    # cached flags as valid.
    if ($rvertical_tightness_flags) {
        if (   $maximum_line_index <= 0
            && $cached_line_type
            && $cached_seqno
            && $rvertical_tightness_flags->[2]
            && $rvertical_tightness_flags->[2] == $cached_seqno )
        {
            $rvertical_tightness_flags->[3] ||= 1;
            $cached_line_valid ||= 1;
        }
    }

    # do not join an opening block brace with an unbalanced line
    # unless requested with a flag value of 2
    if (   $cached_line_type == 3
        && $maximum_line_index < 0
        && $cached_line_flag < 2
        && $level_jump != 0 )
    {
        $cached_line_valid = 0;
    }

    # patch until new aligner is finished
    if ($do_not_pad) { my_flush() }

    # shouldn't happen:
    if ( $level < 0 ) { $level = 0 }

    # do not align code across indentation level changes
    # or if vertical alignment is turned off for debugging
    if ( $level != $group_level || $is_outdented || !$rOpts_valign ) {

        # we are allowed to shift a group of lines to the right if its
        # level is greater than the previous and next group
        $extra_indent_ok =
          ( $level < $group_level && $last_level_written < $group_level );

        my_flush();

        # If we know that this line will get flushed out by itself because
        # of level changes, we can leave the extra_indent_ok flag set.
        # That way, if we get an external flush call, we will still be
        # able to do some -lp alignment if necessary.
        $extra_indent_ok = ( $is_terminal_statement && $level > $group_level );

        $group_level = $level;

        # wait until after the above flush to get the leading space
        # count because it may have been changed if the -icp flag is in
        # effect
        $leading_space_count = get_spaces($indentation);

    }

    # --------------------------------------------------------------------
    # Patch to collect outdentable block COMMENTS
    # --------------------------------------------------------------------
    my $is_blank_line = "";
    if ( $group_type eq 'COMMENT' ) {
        if (
            (
                   $is_block_comment
                && $outdent_long_lines
                && $leading_space_count == $comment_leading_space_count
            )
            || $is_blank_line
          )
        {
            $group_lines[ ++$maximum_line_index ] = $rfields->[0];
            return;
        }
        else {
            my_flush();
        }
    }

    # --------------------------------------------------------------------
    # add dummy fields for terminal ternary
    # --------------------------------------------------------------------
    my $j_terminal_match;
    if ( $is_terminal_ternary && $current_line ) {
        $j_terminal_match =
          fix_terminal_ternary( $rfields, $rtokens, $rpatterns );
        $jmax = @{$rfields} - 1;
    }

    # --------------------------------------------------------------------
    # add dummy fields for else statement
    # --------------------------------------------------------------------
    if (   $rfields->[0] =~ /^else\s*$/
        && $current_line
        && $level_jump == 0 )
    {
        $j_terminal_match = fix_terminal_else( $rfields, $rtokens, $rpatterns );
        $jmax             = @{$rfields} - 1;
    }

    # --------------------------------------------------------------------
    # Step 1. Handle simple line of code with no fields to match.
    # --------------------------------------------------------------------
    if ( $jmax <= 0 ) {
        $zero_count++;

        if ( $maximum_line_index >= 0
            && !get_recoverable_spaces( $group_lines[0]->get_indentation() ) )
        {

            # flush the current group if it has some aligned columns..
            if ( $group_lines[0]->get_jmax() > 1 ) { my_flush() }

            # flush current group if we are just collecting side comments..
            elsif (

                # ...and we haven't seen a comment lately
                ( $zero_count > 3 )

                # ..or if this new line doesn't fit to the left of the comments
                || ( ( $leading_space_count + length( $rfields->[0] ) ) >
                    $group_lines[0]->get_column(0) )
              )
            {
                my_flush();
            }
        }

        # patch to start new COMMENT group if this comment may be outdented
        if (   $is_block_comment
            && $outdent_long_lines
            && $maximum_line_index < 0 )
        {
            $group_type                           = 'COMMENT';
            $comment_leading_space_count          = $leading_space_count;
            $group_lines[ ++$maximum_line_index ] = $rfields->[0];
            return;
        }

        # just write this line directly if no current group, no side comment,
        # and no space recovery is needed.
        if ( $maximum_line_index < 0 && !get_recoverable_spaces($indentation) )
        {
            valign_output_step_B( $leading_space_count, $rfields->[0], 0,
                $outdent_long_lines, $rvertical_tightness_flags, $level );
            return;
        }
    }
    else {
        $zero_count = 0;
    }

    # programming check: (shouldn't happen)
    # an error here implies an incorrect call was made
    if ( $jmax > 0 && ( $#{$rtokens} != ( $jmax - 1 ) ) ) {
        warning(
"Program bug in Perl::Tidy::VerticalAligner - number of tokens = $#{$rtokens} should be one less than number of fields: $#{$rfields})\n"
        );
        report_definite_bug();
    }

    # --------------------------------------------------------------------
    # create an object to hold this line
    # --------------------------------------------------------------------
    ##my $new_line = new Perl::Tidy::VerticalAligner::Line(
    my $new_line = Perl::Tidy::VerticalAligner::Line->new(
        jmax                      => $jmax,
        jmax_original_line        => $jmax,
        rtokens                   => $rtokens,
        rfields                   => $rfields,
        rpatterns                 => $rpatterns,
        indentation               => $indentation,
        leading_space_count       => $leading_space_count,
        outdent_long_lines        => $outdent_long_lines,
        list_type                 => "",
        is_hanging_side_comment   => $is_hanging_side_comment,
        maximum_line_length       => maximum_line_length_for_level($level),
        rvertical_tightness_flags => $rvertical_tightness_flags,
    );

    # Initialize a global flag saying if the last line of the group should
    # match end of group and also terminate the group.  There should be no
    # returns between here and where the flag is handled at the bottom.
    my $col_matching_terminal = 0;
    if ( defined($j_terminal_match) ) {

        # remember the column of the terminal ? or { to match with
        $col_matching_terminal = $current_line->get_column($j_terminal_match);

        # set global flag for sub decide_if_aligned
        $is_matching_terminal_line = 1;
    }

    # --------------------------------------------------------------------
    # It simplifies things to create a zero length side comment
    # if none exists.
    # --------------------------------------------------------------------
    make_side_comment( $new_line, $level_end );

    # --------------------------------------------------------------------
    # Decide if this is a simple list of items.
    # There are 3 list types: none, comma, comma-arrow.
    # We use this below to be less restrictive in deciding what to align.
    # --------------------------------------------------------------------
    if ($is_forced_break) {
        decide_if_list($new_line);
    }

    if ($current_line) {

        # --------------------------------------------------------------------
        # Allow hanging side comment to join current group, if any
        # This will help keep side comments aligned, because otherwise we
        # will have to start a new group, making alignment less likely.
        # --------------------------------------------------------------------
        join_hanging_comment( $new_line, $current_line )
          if $is_hanging_side_comment;

        # --------------------------------------------------------------------
        # If there is just one previous line, and it has more fields
        # than the new line, try to join fields together to get a match with
        # the new line.  At the present time, only a single leading '=' is
        # allowed to be compressed out.  This is useful in rare cases where
        # a table is forced to use old breakpoints because of side comments,
        # and the table starts out something like this:
        #   my %MonthChars = ('0', 'Jan',   # side comment
        #                     '1', 'Feb',
        #                     '2', 'Mar',
        # Eliminating the '=' field will allow the remaining fields to line up.
        # This situation does not occur if there are no side comments
        # because scan_list would put a break after the opening '('.
        # --------------------------------------------------------------------
        eliminate_old_fields( $new_line, $current_line );

        # --------------------------------------------------------------------
        # If the new line has more fields than the current group,
        # see if we can match the first fields and combine the remaining
        # fields of the new line.
        # --------------------------------------------------------------------
        eliminate_new_fields( $new_line, $current_line );

        # --------------------------------------------------------------------
        # Flush previous group unless all common tokens and patterns match..
        # --------------------------------------------------------------------
        check_match( $new_line, $current_line );

        # --------------------------------------------------------------------
        # See if there is space for this line in the current group (if any)
        # --------------------------------------------------------------------
        if ($current_line) {
            check_fit( $new_line, $current_line );
        }
    }

    # --------------------------------------------------------------------
    # Append this line to the current group (or start new group)
    # --------------------------------------------------------------------
    add_to_group($new_line);

    # Future update to allow this to vary:
    $current_line = $new_line if ( $maximum_line_index == 0 );

    # output this group if it ends in a terminal else or ternary line
    if ( defined($j_terminal_match) ) {

        # if there is only one line in the group (maybe due to failure to match
        # perfectly with previous lines), then align the ? or { of this
        # terminal line with the previous one unless that would make the line
        # too long
        if ( $maximum_line_index == 0 ) {
            my $col_now = $current_line->get_column($j_terminal_match);
            my $pad     = $col_matching_terminal - $col_now;
            my $padding_available =
              $current_line->get_available_space_on_right();
            if ( $pad > 0 && $pad <= $padding_available ) {
                $current_line->increase_field_width( $j_terminal_match, $pad );
            }
        }
        my_flush();
        $is_matching_terminal_line = 0;
    }

    # --------------------------------------------------------------------
    # Step 8. Some old debugging stuff
    # --------------------------------------------------------------------
    VALIGN_DEBUG_FLAG_APPEND && do {
        print STDOUT "APPEND fields:";
        dump_array( @{$rfields} );
        print STDOUT "APPEND tokens:";
        dump_array( @{$rtokens} );
        print STDOUT "APPEND patterns:";
        dump_array( @{$rpatterns} );
        dump_alignments();
    };

    return;
}

sub join_hanging_comment {

    my $line = shift;
    my $jmax = $line->get_jmax();
    return 0 unless $jmax == 1;    # must be 2 fields
    my $rtokens = $line->get_rtokens();
    return 0 unless $rtokens->[0] eq '#';    # the second field is a comment..
    my $rfields = $line->get_rfields();
    return 0 unless $rfields->[0] =~ /^\s*$/;    # the first field is empty...
    my $old_line            = shift;
    my $maximum_field_index = $old_line->get_jmax();
    return 0
      unless $maximum_field_index > $jmax;    # the current line has more fields
    my $rpatterns = $line->get_rpatterns();

    $line->set_is_hanging_side_comment(1);
    $jmax = $maximum_field_index;
    $line->set_jmax($jmax);
    $rfields->[$jmax]         = $rfields->[1];
    $rtokens->[ $jmax - 1 ]   = $rtokens->[0];
    $rpatterns->[ $jmax - 1 ] = $rpatterns->[0];
    foreach my $j ( 1 .. $jmax - 1 ) {
        $rfields->[$j]         = " "; # NOTE: caused glitch unless 1 blank, why?
        $rtokens->[ $j - 1 ]   = "";
        $rpatterns->[ $j - 1 ] = "";
    }
    return 1;
}

sub eliminate_old_fields {

    my $new_line = shift;
    my $jmax     = $new_line->get_jmax();
    if ( $jmax > $maximum_jmax_seen ) { $maximum_jmax_seen = $jmax }
    if ( $jmax < $minimum_jmax_seen ) { $minimum_jmax_seen = $jmax }

    # there must be one previous line
    return unless ( $maximum_line_index == 0 );

    my $old_line            = shift;
    my $maximum_field_index = $old_line->get_jmax();

    ###############################################
    # Moved below to allow new coding for => matches
    # return unless $maximum_field_index > $jmax;
    ###############################################

    # Identify specific cases where field elimination is allowed:
    # case=1: both lines have comma-separated lists, and the first
    #         line has an equals
    # case=2: both lines have leading equals

    # case 1 is the default
    my $case = 1;

    # See if case 2: both lines have leading '='
    # We'll require similar leading patterns in this case
    my $old_rtokens   = $old_line->get_rtokens();
    my $rtokens       = $new_line->get_rtokens();
    my $rpatterns     = $new_line->get_rpatterns();
    my $old_rpatterns = $old_line->get_rpatterns();
    if (   $rtokens->[0] =~ /^=>?\d*$/
        && $old_rtokens->[0] eq $rtokens->[0]
        && $old_rpatterns->[0] eq $rpatterns->[0] )
    {
        $case = 2;
    }

    # not too many fewer fields in new line for case 1
    return unless ( $case != 1 || $maximum_field_index - 2 <= $jmax );

    # case 1 must have side comment
    my $old_rfields = $old_line->get_rfields();
    return
      if ( $case == 1
        && length( $old_rfields->[$maximum_field_index] ) == 0 );

    my $rfields = $new_line->get_rfields();

    my $hid_equals = 0;

    my @new_alignments        = ();
    my @new_fields            = ();
    my @new_matching_patterns = ();
    my @new_matching_tokens   = ();

    my $j               = 0;
    my $current_field   = '';
    my $current_pattern = '';

    # loop over all old tokens
    my $in_match = 0;
    foreach my $k ( 0 .. $maximum_field_index - 1 ) {
        $current_field   .= $old_rfields->[$k];
        $current_pattern .= $old_rpatterns->[$k];
        last if ( $j > $jmax - 1 );

        if ( $old_rtokens->[$k] eq $rtokens->[$j] ) {
            $in_match                  = 1;
            $new_fields[$j]            = $current_field;
            $new_matching_patterns[$j] = $current_pattern;
            $current_field             = '';
            $current_pattern           = '';
            $new_matching_tokens[$j]   = $old_rtokens->[$k];
            $new_alignments[$j]        = $old_line->get_alignment($k);
            $j++;
        }
        else {

            if ( $old_rtokens->[$k] =~ /^\=\d*$/ ) {
                last if ( $case == 2 );    # avoid problems with stuff
                                           # like:   $a=$b=$c=$d;
                $hid_equals = 1;
            }
            last
              if ( $in_match && $case == 1 )
              ;    # disallow gaps in matching field types in case 1
        }
    }

    # Modify the current state if we are successful.
    # We must exactly reach the ends of the new list for success, and the old
    # pattern must have more fields. Here is an example where the first and
    # second lines have the same number, and we should not align:
    #  my @a = map chr, 0 .. 255;
    #  my @b = grep /\W/,    @a;
    #  my @c = grep /[^\w]/, @a;

    # Otherwise, we would get all of the commas aligned, which doesn't work as
    # well:
    #  my @a = map chr,      0 .. 255;
    #  my @b = grep /\W/,    @a;
    #  my @c = grep /[^\w]/, @a;

    if (   ( $j == $jmax )
        && ( $current_field eq '' )
        && ( $case != 1 || $hid_equals )
        && ( $maximum_field_index > $jmax ) )
    {
        my $k = $maximum_field_index;
        $current_field   .= $old_rfields->[$k];
        $current_pattern .= $old_rpatterns->[$k];
        $new_fields[$j]            = $current_field;
        $new_matching_patterns[$j] = $current_pattern;

        $new_alignments[$j] = $old_line->get_alignment($k);
        $maximum_field_index = $j;

        $old_line->set_alignments(@new_alignments);
        $old_line->set_jmax($jmax);
        $old_line->set_rtokens( \@new_matching_tokens );
        $old_line->set_rfields( \@new_fields );
        $old_line->set_rpatterns( \@{$rpatterns} );
    }

    # Dumb Down starting match if necessary:
    #
    # Consider the following two lines:
    #
    #  {
    #   $a => 20 > 3 ? 1 : 0,
    #   $xyz => 5,
    #  }

# We would like to get alignment regardless of the order of the two lines.
# If the lines come in in this order, then we will simplify the patterns of the first line
# in sub eliminate_new_fields.
# If the lines come in reverse order, then we achieve this with eliminate_new_fields.

    # This update is currently restricted to leading '=>' matches. Although we
    # could do this for both '=' and '=>', overall the results for '=' come out
    # better without this step because this step can eliminate some other good
    # matches.  For example, with the '=' we get:

#  my @disilva = ( "di Silva", "diSilva", "di Si\x{301}lva", "diSi\x{301}lva" );
#  my @dsf     = map "$_\x{FFFE}Fred", @disilva;
#  my @dsj     = map "$_\x{FFFE}John", @disilva;
#  my @dsJ     = map "$_ John", @disilva;

    # without including '=' we get:

#  my @disilva = ( "di Silva", "diSilva", "di Si\x{301}lva", "diSi\x{301}lva" );
#  my @dsf = map "$_\x{FFFE}Fred", @disilva;
#  my @dsj = map "$_\x{FFFE}John", @disilva;
#  my @dsJ = map "$_ John",        @disilva;
    elsif (
        $case == 2

        && @new_matching_tokens == 1
        ##&& $new_matching_tokens[0] =~ /^=/   # see note above
        && $new_matching_tokens[0] =~ /^=>/
        && $maximum_field_index > 2
      )
    {
        my $jmaxm             = $jmax - 1;
        my $kmaxm             = $maximum_field_index - 1;
        my $have_side_comment = $old_rtokens->[$kmaxm] eq '#';

        # We need to reduce the group pattern to be just two tokens,
        # the leading equality or => and the final side comment

        my $mid_field = join "",
          @{$old_rfields}[ 1 .. $maximum_field_index - 1 ];
        my $mid_patterns = join "",
          @{$old_rpatterns}[ 1 .. $maximum_field_index - 1 ];
        my @new_alignments = (
            $old_line->get_alignment(0),
            $old_line->get_alignment( $maximum_field_index - 1 )
        );
        my @new_tokens =
          ( $old_rtokens->[0], $old_rtokens->[ $maximum_field_index - 1 ] );
        my @new_fields = (
            $old_rfields->[0], $mid_field, $old_rfields->[$maximum_field_index]
        );
        my @new_patterns = (
            $old_rpatterns->[0], $mid_patterns,
            $old_rpatterns->[$maximum_field_index]
        );

        $maximum_field_index = 2;
        $old_line->set_jmax($maximum_field_index);
        $old_line->set_rtokens( \@new_tokens );
        $old_line->set_rfields( \@new_fields );
        $old_line->set_rpatterns( \@new_patterns );

        initialize_for_new_group();
        add_to_group($old_line);
        $current_line = $old_line;
    }
    return;
}

# create an empty side comment if none exists
sub make_side_comment {
    my ( $new_line, $level_end ) = @_;
    my $jmax    = $new_line->get_jmax();
    my $rtokens = $new_line->get_rtokens();

    # if line does not have a side comment...
    if ( ( $jmax == 0 ) || ( $rtokens->[ $jmax - 1 ] ne '#' ) ) {
        my $rfields   = $new_line->get_rfields();
        my $rpatterns = $new_line->get_rpatterns();
        $rtokens->[$jmax]     = '#';
        $rfields->[ ++$jmax ] = '';
        $rpatterns->[$jmax]   = '#';
        $new_line->set_jmax($jmax);
        $new_line->set_jmax_original_line($jmax);
    }

    # line has a side comment..
    else {

        # don't remember old side comment location for very long
        my $line_number = $vertical_aligner_self->get_output_line_number();
        my $rfields     = $new_line->get_rfields();
        if (
            $line_number - $last_side_comment_line_number > 12

            # and don't remember comment location across block level changes
            || (   $level_end < $last_side_comment_level
                && $rfields->[0] =~ /^}/ )
          )
        {
            forget_side_comment();
        }
        $last_side_comment_line_number = $line_number;
        $last_side_comment_level       = $level_end;
    }
    return;
}

sub decide_if_list {

    my $line = shift;

    # A list will be taken to be a line with a forced break in which all
    # of the field separators are commas or comma-arrows (except for the
    # trailing #)

    # List separator tokens are things like ',3'   or '=>2',
    # where the trailing digit is the nesting depth.  Allow braces
    # to allow nested list items.
    my $rtokens    = $line->get_rtokens();
    my $test_token = $rtokens->[0];
    if ( $test_token =~ /^(\,|=>)/ ) {
        my $list_type = $test_token;
        my $jmax      = $line->get_jmax();

        foreach ( 1 .. $jmax - 2 ) {
            if ( $rtokens->[$_] !~ /^(\,|=>|\{)/ ) {
                $list_type = "";
                last;
            }
        }
        $line->set_list_type($list_type);
    }
    return;
}

sub eliminate_new_fields {

    my ( $new_line, $old_line ) = @_;
    return unless ( $maximum_line_index >= 0 );
    my $jmax = $new_line->get_jmax();

    my $old_rtokens = $old_line->get_rtokens();
    my $rtokens     = $new_line->get_rtokens();
    my $is_assignment =
      ( $rtokens->[0] =~ /^=>?\d*$/ && ( $old_rtokens->[0] eq $rtokens->[0] ) );

    # must be monotonic variation
    return unless ( $is_assignment || $previous_maximum_jmax_seen <= $jmax );

    # must be more fields in the new line
    my $maximum_field_index = $old_line->get_jmax();
    return unless ( $maximum_field_index < $jmax );

    unless ($is_assignment) {
        return
          unless ( $old_line->get_jmax_original_line() == $minimum_jmax_seen )
          ;    # only if monotonic

        # never combine fields of a comma list
        return
          unless ( $maximum_field_index > 1 )
          && ( $new_line->get_list_type() !~ /^,/ );
    }

    my $rfields       = $new_line->get_rfields();
    my $rpatterns     = $new_line->get_rpatterns();
    my $old_rpatterns = $old_line->get_rpatterns();

    # loop over all OLD tokens except comment and check match
    my $match = 1;
    foreach my $k ( 0 .. $maximum_field_index - 2 ) {
        if (   ( $old_rtokens->[$k] ne $rtokens->[$k] )
            || ( $old_rpatterns->[$k] ne $rpatterns->[$k] ) )
        {
            $match = 0;
            last;
        }
    }

    # first tokens agree, so combine extra new tokens
    if ($match) {
        ##for my $k ( $maximum_field_index .. $jmax - 1 ) {
        foreach my $k ( $maximum_field_index .. $jmax - 1 ) {

            $rfields->[ $maximum_field_index - 1 ] .= $rfields->[$k];
            $rfields->[$k] = "";
            $rpatterns->[ $maximum_field_index - 1 ] .= $rpatterns->[$k];
            $rpatterns->[$k] = "";
        }

        $rtokens->[ $maximum_field_index - 1 ] = '#';
        $rfields->[$maximum_field_index]       = $rfields->[$jmax];
        $rpatterns->[$maximum_field_index]     = $rpatterns->[$jmax];
        $jmax                                  = $maximum_field_index;
    }
    $new_line->set_jmax($jmax);
    return;
}

sub fix_terminal_ternary {

    # Add empty fields as necessary to align a ternary term
    # like this:
    #
    #  my $leapyear =
    #      $year % 4   ? 0
    #    : $year % 100 ? 1
    #    : $year % 400 ? 0
    #    :               1;
    #
    # returns 1 if the terminal item should be indented

    my ( $rfields, $rtokens, $rpatterns ) = @_;

    my $jmax        = @{$rfields} - 1;
    my $old_line    = $group_lines[$maximum_line_index];
    my $rfields_old = $old_line->get_rfields();

    my $rpatterns_old       = $old_line->get_rpatterns();
    my $rtokens_old         = $old_line->get_rtokens();
    my $maximum_field_index = $old_line->get_jmax();

    # look for the question mark after the :
    my ($jquestion);
    my $depth_question;
    my $pad = "";
    foreach my $j ( 0 .. $maximum_field_index - 1 ) {
        my $tok = $rtokens_old->[$j];
        if ( $tok =~ /^\?(\d+)$/ ) {
            $depth_question = $1;

            # depth must be correct
            next unless ( $depth_question eq $group_level );

            $jquestion = $j;
            if ( $rfields_old->[ $j + 1 ] =~ /^(\?\s*)/ ) {
                $pad = " " x length($1);
            }
            else {
                return;    # shouldn't happen
            }
            last;
        }
    }
    return unless ( defined($jquestion) );    # shouldn't happen

    # Now splice the tokens and patterns of the previous line
    # into the else line to insure a match.  Add empty fields
    # as necessary.
    my $jadd = $jquestion;

    # Work on copies of the actual arrays in case we have
    # to return due to an error
    my @fields   = @{$rfields};
    my @patterns = @{$rpatterns};
    my @tokens   = @{$rtokens};

    VALIGN_DEBUG_FLAG_TERNARY && do {
        local $" = '><';
        print STDOUT "CURRENT FIELDS=<@{$rfields_old}>\n";
        print STDOUT "CURRENT TOKENS=<@{$rtokens_old}>\n";
        print STDOUT "CURRENT PATTERNS=<@{$rpatterns_old}>\n";
        print STDOUT "UNMODIFIED FIELDS=<@{$rfields}>\n";
        print STDOUT "UNMODIFIED TOKENS=<@{$rtokens}>\n";
        print STDOUT "UNMODIFIED PATTERNS=<@{$rpatterns}>\n";
    };

    # handle cases of leading colon on this line
    if ( $fields[0] =~ /^(:\s*)(.*)$/ ) {

        my ( $colon, $therest ) = ( $1, $2 );

        # Handle sub-case of first field with leading colon plus additional code
        # This is the usual situation as at the '1' below:
        #  ...
        #  : $year % 400 ? 0
        #  :               1;
        if ($therest) {

            # Split the first field after the leading colon and insert padding.
            # Note that this padding will remain even if the terminal value goes
            # out on a separate line.  This does not seem to look to bad, so no
            # mechanism has been included to undo it.
            my $field1 = shift @fields;
            unshift @fields, ( $colon, $pad . $therest );

            # change the leading pattern from : to ?
            return unless ( $patterns[0] =~ s/^\:/?/ );

            # install leading tokens and patterns of existing line
            unshift( @tokens,   @{$rtokens_old}[ 0 .. $jquestion ] );
            unshift( @patterns, @{$rpatterns_old}[ 0 .. $jquestion ] );

            # insert appropriate number of empty fields
            splice( @fields, 1, 0, ('') x $jadd ) if $jadd;
        }

        # handle sub-case of first field just equal to leading colon.
        # This can happen for example in the example below where
        # the leading '(' would create a new alignment token
        # : ( $name =~ /[]}]$/ ) ? ( $mname = $name )
        # :                        ( $mname = $name . '->' );
        else {

            return unless ( $jmax > 0 && $tokens[0] ne '#' ); # shouldn't happen

            # prepend a leading ? onto the second pattern
            $patterns[1] = "?b" . $patterns[1];

            # pad the second field
            $fields[1] = $pad . $fields[1];

            # install leading tokens and patterns of existing line, replacing
            # leading token and inserting appropriate number of empty fields
            splice( @tokens,   0, 1, @{$rtokens_old}[ 0 .. $jquestion ] );
            splice( @patterns, 1, 0, @{$rpatterns_old}[ 1 .. $jquestion ] );
            splice( @fields, 1, 0, ('') x $jadd ) if $jadd;
        }
    }

    # Handle case of no leading colon on this line.  This will
    # be the case when -wba=':' is used.  For example,
    #  $year % 400 ? 0 :
    #                1;
    else {

        # install leading tokens and patterns of existing line
        $patterns[0] = '?' . 'b' . $patterns[0];
        unshift( @tokens,   @{$rtokens_old}[ 0 .. $jquestion ] );
        unshift( @patterns, @{$rpatterns_old}[ 0 .. $jquestion ] );

        # insert appropriate number of empty fields
        $jadd = $jquestion + 1;
        $fields[0] = $pad . $fields[0];
        splice( @fields, 0, 0, ('') x $jadd ) if $jadd;
    }

    VALIGN_DEBUG_FLAG_TERNARY && do {
        local $" = '><';
        print STDOUT "MODIFIED TOKENS=<@tokens>\n";
        print STDOUT "MODIFIED PATTERNS=<@patterns>\n";
        print STDOUT "MODIFIED FIELDS=<@fields>\n";
    };

    # all ok .. update the arrays
    @{$rfields}   = @fields;
    @{$rtokens}   = @tokens;
    @{$rpatterns} = @patterns;

    # force a flush after this line
    return $jquestion;
}

sub fix_terminal_else {

    # Add empty fields as necessary to align a balanced terminal
    # else block to a previous if/elsif/unless block,
    # like this:
    #
    #  if   ( 1 || $x ) { print "ok 13\n"; }
    #  else             { print "not ok 13\n"; }
    #
    # returns 1 if the else block should be indented
    #
    my ( $rfields, $rtokens, $rpatterns ) = @_;
    my $jmax = @{$rfields} - 1;
    return unless ( $jmax > 0 );

    # check for balanced else block following if/elsif/unless
    my $rfields_old = $current_line->get_rfields();

    # TBD: add handling for 'case'
    return unless ( $rfields_old->[0] =~ /^(if|elsif|unless)\s*$/ );

    # look for the opening brace after the else, and extract the depth
    my $tok_brace = $rtokens->[0];
    my $depth_brace;
    if ( $tok_brace =~ /^\{(\d+)/ ) { $depth_brace = $1; }

    # probably:  "else # side_comment"
    else { return }

    my $rpatterns_old       = $current_line->get_rpatterns();
    my $rtokens_old         = $current_line->get_rtokens();
    my $maximum_field_index = $current_line->get_jmax();

    # be sure the previous if/elsif is followed by an opening paren
    my $jparen    = 0;
    my $tok_paren = '(' . $depth_brace;
    my $tok_test  = $rtokens_old->[$jparen];
    return unless ( $tok_test eq $tok_paren );    # shouldn't happen

    # Now find the opening block brace
    my ($jbrace);
    foreach my $j ( 1 .. $maximum_field_index - 1 ) {
        my $tok = $rtokens_old->[$j];
        if ( $tok eq $tok_brace ) {
            $jbrace = $j;
            last;
        }
    }
    return unless ( defined($jbrace) );           # shouldn't happen

    # Now splice the tokens and patterns of the previous line
    # into the else line to insure a match.  Add empty fields
    # as necessary.
    my $jadd = $jbrace - $jparen;
    splice( @{$rtokens},   0, 0, @{$rtokens_old}[ $jparen .. $jbrace - 1 ] );
    splice( @{$rpatterns}, 1, 0, @{$rpatterns_old}[ $jparen + 1 .. $jbrace ] );
    splice( @{$rfields}, 1, 0, ('') x $jadd );

    # force a flush after this line if it does not follow a case
    if   ( $rfields_old->[0] =~ /^case\s*$/ ) { return }
    else                                      { return $jbrace }
}

{    # sub check_match
    my %is_good_alignment;

    BEGIN {

        # Vertically aligning on certain "good" tokens is usually okay
        # so we can be less restrictive in marginal cases.
        my @q = qw( { ? => = );
        push @q, (',');
        @is_good_alignment{@q} = (1) x scalar(@q);
    }

    sub check_match {

        # See if the current line matches the current vertical alignment group.
        # If not, flush the current group.
        my ( $new_line, $old_line ) = @_;

        # uses global variables:
        #  $previous_minimum_jmax_seen
        #  $maximum_jmax_seen
        #  $maximum_line_index
        #  $marginal_match
        my $jmax                = $new_line->get_jmax();
        my $maximum_field_index = $old_line->get_jmax();

        # flush if this line has too many fields
        # variable $GoToLoc indicates goto branch point, for debugging
        my $GoToLoc = 1;
        if ( $jmax > $maximum_field_index ) { goto NO_MATCH }

        # flush if adding this line would make a non-monotonic field count
        if (
            ( $maximum_field_index > $jmax )    # this has too few fields
            && (
                ( $previous_minimum_jmax_seen <
                    $jmax )                     # and wouldn't be monotonic
                || ( $old_line->get_jmax_original_line() != $maximum_jmax_seen )
            )
          )
        {
            $GoToLoc = 2;
            goto NO_MATCH;
        }

        # otherwise see if this line matches the current group
        my $jmax_original_line      = $new_line->get_jmax_original_line();
        my $is_hanging_side_comment = $new_line->get_is_hanging_side_comment();
        my $rtokens                 = $new_line->get_rtokens();
        my $rfields                 = $new_line->get_rfields();
        my $rpatterns               = $new_line->get_rpatterns();
        my $list_type               = $new_line->get_list_type();

        my $group_list_type = $old_line->get_list_type();
        my $old_rpatterns   = $old_line->get_rpatterns();
        my $old_rtokens     = $old_line->get_rtokens();

        my $jlimit = $jmax - 1;
        if ( $maximum_field_index > $jmax ) {
            $jlimit = $jmax_original_line;
            --$jlimit unless ( length( $new_line->get_rfields()->[$jmax] ) );
        }

        # handle comma-separated lists ..
        if ( $group_list_type && ( $list_type eq $group_list_type ) ) {
            for my $j ( 0 .. $jlimit ) {
                my $old_tok = $old_rtokens->[$j];
                next unless $old_tok;
                my $new_tok = $rtokens->[$j];
                next unless $new_tok;

                # lists always match ...
                # unless they would align any '=>'s with ','s
                $GoToLoc = 3;
                goto NO_MATCH
                  if ( $old_tok =~ /^=>/ && $new_tok =~ /^,/
                    || $new_tok =~ /^=>/ && $old_tok =~ /^,/ );
            }
        }

        # do detailed check for everything else except hanging side comments
        elsif ( !$is_hanging_side_comment ) {

            my $leading_space_count = $new_line->get_leading_space_count();

            my $max_pad = 0;
            my $min_pad = 0;
            my $saw_good_alignment;

            for my $j ( 0 .. $jlimit ) {

                my $old_tok = $old_rtokens->[$j];
                my $new_tok = $rtokens->[$j];

                # Note on encoding used for alignment tokens:
                # -------------------------------------------
                # Tokens are "decorated" with information which can help
                # prevent unwanted alignments.  Consider for example the
                # following two lines:
                #   local ( $xn, $xd ) = split( '/', &'rnorm(@_) );
                #   local ( $i, $f ) = &'bdiv( $xn, $xd );
                # There are three alignment tokens in each line, a comma,
                # an =, and a comma.  In the first line these three tokens
                # are encoded as:
                #    ,4+local-18     =3      ,4+split-7
                # and in the second line they are encoded as
                #    ,4+local-18     =3      ,4+&'bdiv-8
                # Tokens always at least have token name and nesting
                # depth.  So in this example the ='s are at depth 3 and
                # the ,'s are at depth 4.  This prevents aligning tokens
                # of different depths.  Commas contain additional
                # information, as follows:
                # ,  {depth} + {container name} - {spaces to opening paren}
                # This allows us to reject matching the rightmost commas
                # in the above two lines, since they are for different
                # function calls.  This encoding is done in
                # 'sub send_lines_to_vertical_aligner'.

                # Pick off actual token.
                # Everything up to the first digit is the actual token.
                my $alignment_token = $new_tok;
                if ( $alignment_token =~ /^([^\d]+)/ ) { $alignment_token = $1 }

                # see if the decorated tokens match
                my $tokens_match = $new_tok eq $old_tok

                  # Exception for matching terminal : of ternary statement..
                  # consider containers prefixed by ? and : a match
                  || ( $new_tok =~ /^,\d*\+\:/ && $old_tok =~ /^,\d*\+\?/ );

                # No match if the alignment tokens differ...
                if ( !$tokens_match ) {

                    # ...Unless this is a side comment
                    if (
                        $j == $jlimit

                        # and there is either at least one alignment token
                        # or this is a single item following a list.  This
                        # latter rule is required for 'December' to join
                        # the following list:
                        # my (@months) = (
                        #     '',       'January',   'February', 'March',
                        #     'April',  'May',       'June',     'July',
                        #     'August', 'September', 'October',  'November',
                        #     'December'
                        # );
                        # If it doesn't then the -lp formatting will fail.
                        && ( $j > 0 || $old_tok =~ /^,/ )
                      )
                    {
                        $marginal_match = 1
                          if ( $marginal_match == 0
                            && $maximum_line_index == 0 );
                        last;
                    }

                    $GoToLoc = 4;
                    goto NO_MATCH;
                }

                # Calculate amount of padding required to fit this in.
                # $pad is the number of spaces by which we must increase
                # the current field to squeeze in this field.
                my $pad =
                  length( $rfields->[$j] ) - $old_line->current_field_width($j);
                if ( $j == 0 ) { $pad += $leading_space_count; }

                # remember max pads to limit marginal cases
                if ( $alignment_token ne '#' ) {
                    if ( $pad > $max_pad ) { $max_pad = $pad }
                    if ( $pad < $min_pad ) { $min_pad = $pad }
                }
                if ( $is_good_alignment{$alignment_token} ) {
                    $saw_good_alignment = 1;
                }

                # If patterns don't match, we have to be careful...
                if ( $old_rpatterns->[$j] ne $rpatterns->[$j] ) {

                    # flag this as a marginal match since patterns differ
                    $marginal_match = 1
                      if ( $marginal_match == 0 && $maximum_line_index == 0 );

                    # We have to be very careful about aligning commas
                    # when the pattern's don't match, because it can be
                    # worse to create an alignment where none is needed
                    # than to omit one.  Here's an example where the ','s
                    # are not in named containers.  The first line below
                    # should not match the next two:
                    #   ( $a, $b ) = ( $b, $r );
                    #   ( $x1, $x2 ) = ( $x2 - $q * $x1, $x1 );
                    #   ( $y1, $y2 ) = ( $y2 - $q * $y1, $y1 );
                    if ( $alignment_token eq ',' ) {

                       # do not align commas unless they are in named containers
                        $GoToLoc = 5;
                        goto NO_MATCH unless ( $new_tok =~ /[A-Za-z]/ );
                    }

                    # do not align parens unless patterns match;
                    # large ugly spaces can occur in math expressions.
                    elsif ( $alignment_token eq '(' ) {

                        # But we can allow a match if the parens don't
                        # require any padding.
                        $GoToLoc = 6;
                        if ( $pad != 0 ) { goto NO_MATCH }
                    }

                    # Handle an '=' alignment with different patterns to
                    # the left.
                    elsif ( $alignment_token eq '=' ) {

                        # It is best to be a little restrictive when
                        # aligning '=' tokens.  Here is an example of
                        # two lines that we will not align:
                        #       my $variable=6;
                        #       $bb=4;
                        # The problem is that one is a 'my' declaration,
                        # and the other isn't, so they're not very similar.
                        # We will filter these out by comparing the first
                        # letter of the pattern.  This is crude, but works
                        # well enough.
                        if (
                            substr( $old_rpatterns->[$j], 0, 1 ) ne
                            substr( $rpatterns->[$j],     0, 1 ) )
                        {
                            $GoToLoc = 7;
                            goto NO_MATCH;
                        }

                        # If we pass that test, we'll call it a marginal match.
                        # Here is an example of a marginal match:
                        #       $done{$$op} = 1;
                        #       $op         = compile_bblock($op);
                        # The left tokens are both identifiers, but
                        # one accesses a hash and the other doesn't.
                        # We'll let this be a tentative match and undo
                        # it later if we don't find more than 2 lines
                        # in the group.
                        elsif ( $maximum_line_index == 0 ) {
                            $marginal_match =
                              2;    # =2 prevents being undone below
                        }
                    }
                }

                # Don't let line with fewer fields increase column widths
                # ( align3.t )
                if ( $maximum_field_index > $jmax ) {

                    # Exception: suspend this rule to allow last lines to join
                    $GoToLoc = 8;
                    if ( $pad > 0 ) { goto NO_MATCH; }
                }
            } ## end for my $j ( 0 .. $jlimit)

            # Turn off the "marginal match" flag in some cases...
            # A "marginal match" occurs when the alignment tokens agree
            # but there are differences in the other tokens (patterns).
            # If we leave the marginal match flag set, then the rule is that we
            # will align only if there are more than two lines in the group.
            # We will turn of the flag if we almost have a match
            # and either we have seen a good alignment token or we
            # just need a small pad (2 spaces) to fit.  These rules are
            # the result of experimentation.  Tokens which misaligned by just
            # one or two characters are annoying.  On the other hand,
            # large gaps to less important alignment tokens are also annoying.
            if (   $marginal_match == 1
                && $jmax == $maximum_field_index
                && ( $saw_good_alignment || ( $max_pad < 3 && $min_pad > -3 ) )
              )
            {
                $marginal_match = 0;
            }
            ##print "marginal=$marginal_match saw=$saw_good_alignment jmax=$jmax max=$maximum_field_index maxpad=$max_pad minpad=$min_pad\n";
        }

        # We have a match (even if marginal).
        # If the current line has fewer fields than the current group
        # but otherwise matches, copy the remaining group fields to
        # make it a perfect match.
        if ( $maximum_field_index > $jmax ) {

            ##########################################################
            # FIXME: The previous version had a bug which made side comments
            # become regular fields, so for now the program does not allow a
            # line with side comment to match.  This should eventually be done.
            # The best test file for experimenting is 'lista.t'
            ##########################################################

            my $comment = $rfields->[$jmax];
            $GoToLoc = 9;
            goto NO_MATCH if ($comment);

            # Corrected loop
            for my $jj ( $jlimit .. $maximum_field_index ) {
                $rtokens->[$jj]         = $old_rtokens->[$jj];
                $rfields->[ $jj + 1 ]   = '';
                $rpatterns->[ $jj + 1 ] = $old_rpatterns->[ $jj + 1 ];
            }

##          THESE DO NOT GIVE CORRECT RESULTS
##          $rfields->[$jmax] = $comment;
##          $new_line->set_jmax($jmax);

        }
        return;

      NO_MATCH:

        # variable $GoToLoc is for debugging
        #print "no match from $GoToLoc\n";
        ##print "no match jmax=$jmax  max=$maximum_field_index $group_list_type lines=$maximum_line_index token=$old_rtokens->[0]\n";

        # Make one last effort to retain a match of certain statements
        my $match = salvage_equality_matches( $new_line, $old_line );
        my_flush() unless ($match);
        return;
    }
}

sub salvage_equality_matches {
    my ( $new_line, $old_line ) = @_;

    # Reduce the complexity of the two lines if it will allow us to retain
    # alignment of some common alignments, including '=' and '=>'.  We will
    # convert both lines to have just two matching tokens, the equality and the
    # side comment.

    # return 0 or undef if unsuccessful
    # return 1 if successful

    # Here is a very simple example of two lines where we could at least
    # align the equals:
    #  $x = $class->_sub( $x, $delta );
    #  $xpownm1 = $class->_pow( $class->_copy($x), $nm1 );    # x(i)^(n-1)

    # We will only do this if there is one old line (and one new line)
    return unless ( $maximum_line_index == 0 );
    return if ($is_matching_terminal_line);

    # We are only looking for equality type statements
    my $old_rtokens = $old_line->get_rtokens();
    my $rtokens     = $new_line->get_rtokens();
    my $is_equals =
      ( $rtokens->[0] =~ /=/ && ( $old_rtokens->[0] eq $rtokens->[0] ) );
    return unless ($is_equals);

    # The leading patterns must match
    my $old_rpatterns = $old_line->get_rpatterns();
    my $rpatterns     = $new_line->get_rpatterns();
    return if ( $old_rpatterns->[0] ne $rpatterns->[0] );

    # Both should have side comment fields (should always be true)
    my $jmax_old    = $old_line->get_jmax();
    my $jmax_new    = $new_line->get_jmax();
    my $end_tok_old = $old_rtokens->[ $jmax_old - 1 ];
    my $end_tok_new = $rtokens->[ $jmax_new - 1 ];
    my $have_side_comments =
         defined($end_tok_old)
      && $end_tok_old eq '#'
      && defined($end_tok_new)
      && $end_tok_new eq '#';
    if ( !$have_side_comments ) { return; }

    # Do not match if any remaining tokens in new line include '?', 'if',
    # 'unless','||', '&&'. The reason is that (1) this isn't a great match, and
    # (2) we will prevent possibly better matchs to follow.  Here is an
    # example.  The match of the first two lines is rejected, and this allows
    # the second and third lines to match.
    #   my $type = shift || "o";
    #   my $fname  = ( $type eq 'oo'               ? 'orte_city' : 'orte' );
    #   my $suffix = ( $coord_system eq 'standard' ? ''          : '-orig' );
    # This logic can cause some unwanted losses of alignments, but it can retain
    # long runs of multiple-token alignments, so overall it is worthwhile.
    # If we had a peek at the subsequent line we could make a much better
    # decision here, but for now this is not available.
    for ( my $j = 1 ; $j < $jmax_new - 1 ; $j++ ) {
        my $new_tok           = $rtokens->[$j];
        my $is_good_alignment = ( $new_tok =~ /^(=|\?|if|unless|\|\||\&\&)/ );
        return if ($is_good_alignment);
    }

    my $squeeze_line = sub {
        my ($line_obj) = @_;

        # reduce a line down to the three fields surrounding
        # the two tokens, an '=' of some sort and a '#' at the end

        my $jmax     = $line_obj->get_jmax();
        my $jmax_new = 2;
        return unless $jmax > $jmax_new;
        my $rfields     = $line_obj->get_rfields();
        my $rpatterns   = $line_obj->get_rpatterns();
        my $rtokens     = $line_obj->get_rtokens();
        my $rfields_new = [
            $rfields->[0], join( '', @{$rfields}[ 1 .. $jmax - 1 ] ),
            $rfields->[$jmax]
        ];
        my $rpatterns_new = [
            $rpatterns->[0], join( '', @{$rpatterns}[ 1 .. $jmax - 1 ] ),
            $rpatterns->[$jmax]
        ];
        my $rtokens_new = [ $rtokens->[0], $rtokens->[ $jmax - 1 ] ];
        $line_obj->{_rfields}   = $rfields_new;
        $line_obj->{_rpatterns} = $rpatterns_new;
        $line_obj->{_rtokens}   = $rtokens_new;
        $line_obj->set_jmax($jmax_new);
    };

    # Okay, we will force a match at the equals-like token.  We will fix both
    # lines to have just 2 tokens and 3 fields:
    $squeeze_line->($new_line);
    $squeeze_line->($old_line);

    # start over with a new group
    initialize_for_new_group();
    add_to_group($old_line);
    $current_line = $old_line;
    return 1;
}

sub check_fit {

    my ( $new_line, $old_line ) = @_;
    return unless ( $maximum_line_index >= 0 );

    my $jmax                    = $new_line->get_jmax();
    my $leading_space_count     = $new_line->get_leading_space_count();
    my $is_hanging_side_comment = $new_line->get_is_hanging_side_comment();
    my $rtokens                 = $new_line->get_rtokens();
    my $rfields                 = $new_line->get_rfields();
    my $rpatterns               = $new_line->get_rpatterns();

    my $group_list_type = $group_lines[0]->get_list_type();

    my $padding_so_far    = 0;
    my $padding_available = $old_line->get_available_space_on_right();

    # save current columns in case this doesn't work
    save_alignment_columns();

    my $maximum_field_index = $old_line->get_jmax();
    for my $j ( 0 .. $jmax ) {

        my $pad = length( $rfields->[$j] ) - $old_line->current_field_width($j);

        if ( $j == 0 ) {
            $pad += $leading_space_count;
        }

        # remember largest gap of the group, excluding gap to side comment
        if (   $pad < 0
            && $group_maximum_gap < -$pad
            && $j > 0
            && $j < $jmax - 1 )
        {
            $group_maximum_gap = -$pad;
        }

        next if $pad < 0;

        ## OLD NOTES:
        ## This patch helps sometimes, but it doesn't check to see if
        ## the line is too long even without the side comment.  It needs
        ## to be reworked.
        ##don't let a long token with no trailing side comment push
        ##side comments out, or end a group.  (sidecmt1.t)
        ##next if ($j==$jmax-1 && length($rfields->[$jmax])==0);

        # BEGIN PATCH for keith1.txt.
        # If the group began matching multiple tokens but later this got
        # reduced to a fewer number of matching tokens, then the fields
        # of the later lines will still have to fit into their corresponding
        # fields.  So a large later field will "push" the other fields to
        # the right, including previous side comments, and if there is no room
        # then there is no match.
        # For example, look at the last line in the following snippet:

 # my $b_prod_db = ( $ENV{ORACLE_SID} =~ m/p$/ && !$testing ) ? true    : false;
 # my $env       = ($b_prod_db)                               ? "prd"   : "val";
 # my $plant     = ( $OPT{p} )                                ? $OPT{p} : "STL";
 # my $task      = $OPT{t};
 # my $fnam      = "longggggggggggggggg.$record_created.$env.$plant.idash";

        # The long term will push the '?' to the right to fit in, and in this
        # case there is not enough room so it will not match the equals unless
        # we do something special.

        # Usually it looks good to keep an initial alignment of '=' going, and
        # we can do this if the long term can fit in the space taken up by the
        # remaining fields (the ? : fields here).

        # Allowing any matching token for now, but it could be restricted
        # to an '='-like token if necessary.

        if (
               $pad > $padding_available
            && $jmax == 2                        # matching one thing (plus #)
            && $j == $jmax - 1                   # at last field
            && $maximum_line_index > 0           # more than 1 line in group now
            && $jmax < $maximum_field_index      # other lines have more fields
            && length( $rfields->[$jmax] ) == 0  # no side comment

            # Uncomment to match only equals (but this does not seem necessary)
            # && $rtokens->[0] =~ /^=\d/           # matching an equals
          )
        {
            my $extra_padding = 0;
            foreach my $jj ( $j + 1 .. $maximum_field_index - 1 ) {
                $extra_padding += $old_line->current_field_width($jj);
            }

            next if ( $pad <= $padding_available + $extra_padding );
        }

        # END PATCH for keith1.pl

        # This line will need space; lets see if we want to accept it..
        if (

            # not if this won't fit
            ( $pad > $padding_available )

            # previously, there were upper bounds placed on padding here
            # (maximum_whitespace_columns), but they were not really helpful

          )
        {

            # revert to starting state then flush; things didn't work out
            restore_alignment_columns();
            my_flush();
            last;
        }

        # patch to avoid excessive gaps in previous lines,
        # due to a line of fewer fields.
        #   return join( ".",
        #       $self->{"dfi"},  $self->{"aa"}, $self->rsvd,     $self->{"rd"},
        #       $self->{"area"}, $self->{"id"}, $self->{"sel"} );
        next if ( $jmax < $maximum_field_index && $j == $jmax - 1 );

        # looks ok, squeeze this field in
        $old_line->increase_field_width( $j, $pad );
        $padding_available -= $pad;

        # remember largest gap of the group, excluding gap to side comment
        if ( $pad > $group_maximum_gap && $j > 0 && $j < $jmax - 1 ) {
            $group_maximum_gap = $pad;
        }
    }
    return;
}

sub add_to_group {

    # The current line either starts a new alignment group or is
    # accepted into the current alignment group.
    my $new_line = shift;
    $group_lines[ ++$maximum_line_index ] = $new_line;

    # initialize field lengths if starting new group
    if ( $maximum_line_index == 0 ) {

        my $jmax    = $new_line->get_jmax();
        my $rfields = $new_line->get_rfields();
        my $rtokens = $new_line->get_rtokens();
        my $col     = $new_line->get_leading_space_count();

        for my $j ( 0 .. $jmax ) {
            $col += length( $rfields->[$j] );

            # create initial alignments for the new group
            my $token = "";
            if ( $j < $jmax ) { $token = $rtokens->[$j] }
            my $alignment = make_alignment( $col, $token );
            $new_line->set_alignment( $j, $alignment );
        }

        $maximum_jmax_seen = $jmax;
        $minimum_jmax_seen = $jmax;
    }

    # use previous alignments otherwise
    else {
        my @new_alignments =
          $group_lines[ $maximum_line_index - 1 ]->get_alignments();
        $new_line->set_alignments(@new_alignments);
    }

    # remember group jmax extremes for next call to valign_input
    $previous_minimum_jmax_seen = $minimum_jmax_seen;
    $previous_maximum_jmax_seen = $maximum_jmax_seen;
    return;
}

sub dump_array {

    # debug routine to dump array contents
    local $" = ')(';
    print STDOUT "(@_)\n";
    return;
}

# flush() sends the current Perl::Tidy::VerticalAligner group down the
# pipeline to Perl::Tidy::FileWriter.

# This is the external flush, which also empties the buffer and cache
sub flush {

    # the buffer must be emptied first, then any cached text
    dump_valign_buffer();

    if ( $maximum_line_index < 0 ) {
        if ($cached_line_type) {
            $seqno_string = $cached_seqno_string;
            valign_output_step_C( $cached_line_text,
                $cached_line_leading_space_count,
                $last_level_written );
            $cached_line_type    = 0;
            $cached_line_text    = "";
            $cached_seqno_string = "";
        }
    }
    else {
        my_flush();
    }
    return;
}

sub reduce_valign_buffer_indentation {

    my ($diff) = @_;
    if ( $valign_buffer_filling && $diff ) {
        my $max_valign_buffer = @valign_buffer;
        foreach my $i ( 0 .. $max_valign_buffer - 1 ) {
            my ( $line, $leading_space_count, $level ) =
              @{ $valign_buffer[$i] };
            my $ws = substr( $line, 0, $diff );
            if ( ( length($ws) == $diff ) && $ws =~ /^\s+$/ ) {
                $line = substr( $line, $diff );
            }
            if ( $leading_space_count >= $diff ) {
                $leading_space_count -= $diff;
                $level = level_change( $leading_space_count, $diff, $level );
            }
            $valign_buffer[$i] = [ $line, $leading_space_count, $level ];
        }
    }
    return;
}

sub level_change {

    # compute decrease in level when we remove $diff spaces from the
    # leading spaces
    my ( $leading_space_count, $diff, $level ) = @_;
    if ($rOpts_indent_columns) {
        my $olev =
          int( ( $leading_space_count + $diff ) / $rOpts_indent_columns );
        my $nlev = int( $leading_space_count / $rOpts_indent_columns );
        $level -= ( $olev - $nlev );
        if ( $level < 0 ) { $level = 0 }
    }
    return $level;
}

sub dump_valign_buffer {
    if (@valign_buffer) {
        foreach (@valign_buffer) {
            valign_output_step_D( @{$_} );
        }
        @valign_buffer = ();
    }
    $valign_buffer_filling = "";
    return;
}

# This is the internal flush, which leaves the cache intact
sub my_flush {

    return if ( $maximum_line_index < 0 );

    # handle a group of comment lines
    if ( $group_type eq 'COMMENT' ) {

        VALIGN_DEBUG_FLAG_APPEND0 && do {
            my ( $a, $b, $c ) = caller();
            print STDOUT
"APPEND0: Flush called from $a $b $c for COMMENT group: lines=$maximum_line_index \n";

        };
        my $leading_space_count = $comment_leading_space_count;
        my $leading_string      = get_leading_string($leading_space_count);

        # zero leading space count if any lines are too long
        my $max_excess = 0;
        for my $i ( 0 .. $maximum_line_index ) {
            my $str = $group_lines[$i];
            my $excess =
              length($str) +
              $leading_space_count -
              maximum_line_length_for_level($group_level);
            if ( $excess > $max_excess ) {
                $max_excess = $excess;
            }
        }

        if ( $max_excess > 0 ) {
            $leading_space_count -= $max_excess;
            if ( $leading_space_count < 0 ) { $leading_space_count = 0 }
            $last_outdented_line_at =
              $file_writer_object->get_output_line_number();
            unless ($outdented_line_count) {
                $first_outdented_line_at = $last_outdented_line_at;
            }
            $outdented_line_count += ( $maximum_line_index + 1 );
        }

        # write the group of lines
        my $outdent_long_lines = 0;
        for my $i ( 0 .. $maximum_line_index ) {
            valign_output_step_B( $leading_space_count, $group_lines[$i], 0,
                $outdent_long_lines, "", $group_level );
        }
    }

    # handle a group of code lines
    else {

        VALIGN_DEBUG_FLAG_APPEND0 && do {
            my $group_list_type = $group_lines[0]->get_list_type();
            my ( $a, $b, $c ) = caller();
            my $maximum_field_index = $group_lines[0]->get_jmax();
            print STDOUT
"APPEND0: Flush called from $a $b $c fields=$maximum_field_index list=$group_list_type lines=$maximum_line_index extra=$extra_indent_ok\n";

        };

        # some small groups are best left unaligned
        my $do_not_align = decide_if_aligned();

        # optimize side comment location
        $do_not_align = adjust_side_comment($do_not_align);

        # recover spaces for -lp option if possible
        my $extra_leading_spaces = get_extra_leading_spaces();

        # all lines of this group have the same basic leading spacing
        my $group_leader_length = $group_lines[0]->get_leading_space_count();

        # add extra leading spaces if helpful
        # NOTE: Use zero; this did not work well
        my $min_ci_gap = 0;

        # loop to output all lines
        for my $i ( 0 .. $maximum_line_index ) {
            my $line = $group_lines[$i];
            valign_output_step_A( $line, $min_ci_gap, $do_not_align,
                $group_leader_length, $extra_leading_spaces );
        }
    }
    initialize_for_new_group();
    return;
}

sub decide_if_aligned {

    # Do not try to align two lines which are not really similar
    return unless $maximum_line_index == 1;
    return if ($is_matching_terminal_line);

    my $group_list_type = $group_lines[0]->get_list_type();

    my $rtokens        = $group_lines[0]->get_rtokens();
    my $leading_equals = ( $rtokens->[0] =~ /=/ );

   # A marginal match is a match which has different patterns. Normally, we
   # should not allow exactly two lines to match if marginal. But we will modify
   # this rule for two lines with a leading equals-like operator such that we
   # match if the patterns to the left of the equals are the same. So for
   # example the following two lines are a marginal match but have the same
   # left side patterns, so we will align the equals.
   #     my $orig = my $format = "^<<<<< ~~\n";
   #     my $abc  = "abc";
   # But these have a different left pattern so they will not be aligned
   #     $xmldoc .= $`;
   #     $self->{'leftovers'} .= "<bx-seq:seq" . $';
    my $is_marginal = $marginal_match;
    if ( $leading_equals && $is_marginal ) {
        my $rpatterns0 = $group_lines[0]->get_rpatterns();
        my $rpatterns1 = $group_lines[1]->get_rpatterns();
        my $pat0       = $rpatterns0->[0];
        my $pat1       = $rpatterns1->[0];
        $is_marginal = $pat0 ne $pat1;
    }

    my $do_not_align = (

        # always align lists
        !$group_list_type

          && (

            # don't align if it was just a marginal match
            $is_marginal    ##$marginal_match

            # don't align two lines with big gap
            # NOTE: I am not sure if this test is actually functional any longer
            || $group_maximum_gap > 12

            # or lines with differing number of alignment tokens
            || ( $previous_maximum_jmax_seen != $previous_minimum_jmax_seen
                && !$leading_equals )
          )
    );

    # But try to convert them into a simple comment group if the first line
    # a has side comment
    my $rfields             = $group_lines[0]->get_rfields();
    my $maximum_field_index = $group_lines[0]->get_jmax();
    if (   $do_not_align
        && ( $maximum_line_index > 0 )
        && ( length( $rfields->[$maximum_field_index] ) > 0 ) )
    {
        combine_fields();
        $do_not_align = 0;
    }
    return $do_not_align;
}

sub adjust_side_comment {

    my $do_not_align = shift;

    # let's see if we can move the side comment field out a little
    # to improve readability (the last field is always a side comment field)
    my $have_side_comment       = 0;
    my $first_side_comment_line = -1;
    my $maximum_field_index     = $group_lines[0]->get_jmax();
    for my $i ( 0 .. $maximum_line_index ) {
        my $line = $group_lines[$i];

        if ( length( $line->get_rfields()->[$maximum_field_index] ) ) {
            $have_side_comment       = 1;
            $first_side_comment_line = $i;
            last;
        }
    }

    my $kmax = $maximum_field_index + 1;

    if ($have_side_comment) {

        my $line = $group_lines[0];

        # the maximum space without exceeding the line length:
        my $avail = $line->get_available_space_on_right();

        # try to use the previous comment column
        my $side_comment_column = $line->get_column( $kmax - 2 );
        my $move                = $last_comment_column - $side_comment_column;

##        my $sc_line0 = $side_comment_history[0]->[0];
##        my $sc_col0  = $side_comment_history[0]->[1];
##        my $sc_line1 = $side_comment_history[1]->[0];
##        my $sc_col1  = $side_comment_history[1]->[1];
##        my $sc_line2 = $side_comment_history[2]->[0];
##        my $sc_col2  = $side_comment_history[2]->[1];
##
##        # FUTURE UPDATES:
##        # Be sure to ignore 'do not align' and  '} # end comments'
##        # Find first $move > 0 and $move <= $avail as follows:
##        # 1. try sc_col1 if sc_col1 == sc_col0 && (line-sc_line0) < 12
##        # 2. try sc_col2 if (line-sc_line2) < 12
##        # 3. try min possible space, plus up to 8,
##        # 4. try min possible space

        if ( $kmax > 0 && !$do_not_align ) {

            # but if this doesn't work, give up and use the minimum space
            if ( $move > $avail ) {
                $move = $rOpts_minimum_space_to_comment - 1;
            }

            # but we want some minimum space to the comment
            my $min_move = $rOpts_minimum_space_to_comment - 1;
            if (   $move >= 0
                && $last_side_comment_length > 0
                && ( $first_side_comment_line == 0 )
                && $group_level == $last_level_written )
            {
                $min_move = 0;
            }

            if ( $move < $min_move ) {
                $move = $min_move;
            }

            # previously, an upper bound was placed on $move here,
            # (maximum_space_to_comment), but it was not helpful

            # don't exceed the available space
            if ( $move > $avail ) { $move = $avail }

            # we can only increase space, never decrease
            if ( $move > 0 ) {
                $line->increase_field_width( $maximum_field_index - 1, $move );
            }

            # remember this column for the next group
            $last_comment_column = $line->get_column( $kmax - 2 );
        }
        else {

            # try to at least line up the existing side comment location
            if ( $kmax > 0 && $move > 0 && $move < $avail ) {
                $line->increase_field_width( $maximum_field_index - 1, $move );
                $do_not_align = 0;
            }

            # reset side comment column if we can't align
            else {
                forget_side_comment();
            }
        }
    }
    return $do_not_align;
}

sub valign_output_step_A {

    ###############################################################
    # This is Step A in writing vertically aligned lines.
    # The line is prepared according to the alignments which have
    # been found and shipped to the next step.
    ###############################################################

    my ( $line, $min_ci_gap, $do_not_align, $group_leader_length,
        $extra_leading_spaces )
      = @_;
    my $rfields                   = $line->get_rfields();
    my $leading_space_count       = $line->get_leading_space_count();
    my $outdent_long_lines        = $line->get_outdent_long_lines();
    my $maximum_field_index       = $line->get_jmax();
    my $rvertical_tightness_flags = $line->get_rvertical_tightness_flags();

    # add any extra spaces
    if ( $leading_space_count > $group_leader_length ) {
        $leading_space_count += $min_ci_gap;
    }

    my $str = $rfields->[0];

    # loop to concatenate all fields of this line and needed padding
    my $total_pad_count = 0;
    for my $j ( 1 .. $maximum_field_index ) {

        # skip zero-length side comments
        last
          if (
            ( $j == $maximum_field_index )
            && ( !defined( $rfields->[$j] )
                || ( length( $rfields->[$j] ) == 0 ) )
          );

        # compute spaces of padding before this field
        my $col = $line->get_column( $j - 1 );
        my $pad = $col - ( length($str) + $leading_space_count );

        if ($do_not_align) {
            $pad =
              ( $j < $maximum_field_index )
              ? 0
              : $rOpts_minimum_space_to_comment - 1;
        }

        # if the -fpsc flag is set, move the side comment to the selected
        # column if and only if it is possible, ignoring constraints on
        # line length and minimum space to comment
        if ( $rOpts_fixed_position_side_comment && $j == $maximum_field_index )
        {
            my $newpad = $pad + $rOpts_fixed_position_side_comment - $col - 1;
            if ( $newpad >= 0 ) { $pad = $newpad; }
        }

        # accumulate the padding
        if ( $pad > 0 ) { $total_pad_count += $pad; }

        # add this field
        if ( !defined $rfields->[$j] ) {
            write_diagnostics("UNDEFined field at j=$j\n");
        }

        # only add padding when we have a finite field;
        # this avoids extra terminal spaces if we have empty fields
        if ( length( $rfields->[$j] ) > 0 ) {
            $str .= ' ' x $total_pad_count;
            $total_pad_count = 0;
            $str .= $rfields->[$j];
        }
        else {
            $total_pad_count = 0;
        }

        # update side comment history buffer
        if ( $j == $maximum_field_index ) {
            my $lineno = $file_writer_object->get_output_line_number();
            shift @side_comment_history;
            push @side_comment_history, [ $lineno, $col ];
        }
    }

    my $side_comment_length = ( length( $rfields->[$maximum_field_index] ) );

    # ship this line off
    valign_output_step_B( $leading_space_count + $extra_leading_spaces,
        $str, $side_comment_length, $outdent_long_lines,
        $rvertical_tightness_flags, $group_level );
    return;
}

sub get_extra_leading_spaces {

    #----------------------------------------------------------
    # Define any extra indentation space (for the -lp option).
    # Here is why:
    # If a list has side comments, sub scan_list must dump the
    # list before it sees everything.  When this happens, it sets
    # the indentation to the standard scheme, but notes how
    # many spaces it would have liked to use.  We may be able
    # to recover that space here in the event that all of the
    # lines of a list are back together again.
    #----------------------------------------------------------

    my $extra_leading_spaces = 0;
    if ($extra_indent_ok) {
        my $object = $group_lines[0]->get_indentation();
        if ( ref($object) ) {
            my $extra_indentation_spaces_wanted =
              get_recoverable_spaces($object);

            # all indentation objects must be the same
            for my $i ( 1 .. $maximum_line_index ) {
                if ( $object != $group_lines[$i]->get_indentation() ) {
                    $extra_indentation_spaces_wanted = 0;
                    last;
                }
            }

            if ($extra_indentation_spaces_wanted) {

                # the maximum space without exceeding the line length:
                my $avail = $group_lines[0]->get_available_space_on_right();
                $extra_leading_spaces =
                  ( $avail > $extra_indentation_spaces_wanted )
                  ? $extra_indentation_spaces_wanted
                  : $avail;

                # update the indentation object because with -icp the terminal
                # ');' will use the same adjustment.
                $object->permanently_decrease_available_spaces(
                    -$extra_leading_spaces );
            }
        }
    }
    return $extra_leading_spaces;
}

sub combine_fields {

    # combine all fields except for the comment field  ( sidecmt.t )
    # Uses global variables:
    #  @group_lines
    #  $maximum_line_index
    my $maximum_field_index = $group_lines[0]->get_jmax();
    foreach my $j ( 0 .. $maximum_line_index ) {
        my $line    = $group_lines[$j];
        my $rfields = $line->get_rfields();
        foreach ( 1 .. $maximum_field_index - 1 ) {
            $rfields->[0] .= $rfields->[$_];
        }
        $rfields->[1] = $rfields->[$maximum_field_index];

        $line->set_jmax(1);
        $line->set_column( 0, 0 );
        $line->set_column( 1, 0 );

    }
    $maximum_field_index = 1;

    for my $j ( 0 .. $maximum_line_index ) {
        my $line    = $group_lines[$j];
        my $rfields = $line->get_rfields();
        for my $k ( 0 .. $maximum_field_index ) {
            my $pad = length( $rfields->[$k] ) - $line->current_field_width($k);
            if ( $k == 0 ) {
                $pad += $group_lines[$j]->get_leading_space_count();
            }

            if ( $pad > 0 ) { $line->increase_field_width( $k, $pad ) }

        }
    }
    return;
}

sub get_output_line_number {

    # the output line number reported to a caller is the number of items
    # written plus the number of items in the buffer
    my $self = shift;
    return 1 + $maximum_line_index +
      $file_writer_object->get_output_line_number();
}

sub valign_output_step_B {

    ###############################################################
    # This is Step B in writing vertically aligned lines.
    # Vertical tightness is applied according to preset flags.
    # In particular this routine handles stacking of opening
    # and closing tokens.
    ###############################################################

    my ( $leading_space_count, $str, $side_comment_length, $outdent_long_lines,
        $rvertical_tightness_flags, $level )
      = @_;

    # handle outdenting of long lines:
    if ($outdent_long_lines) {
        my $excess =
          length($str) -
          $side_comment_length +
          $leading_space_count -
          maximum_line_length_for_level($level);
        if ( $excess > 0 ) {
            $leading_space_count = 0;
            $last_outdented_line_at =
              $file_writer_object->get_output_line_number();

            unless ($outdented_line_count) {
                $first_outdented_line_at = $last_outdented_line_at;
            }
            $outdented_line_count++;
        }
    }

    # Make preliminary leading whitespace.  It could get changed
    # later by entabbing, so we have to keep track of any changes
    # to the leading_space_count from here on.
    my $leading_string =
      $leading_space_count > 0 ? ( ' ' x $leading_space_count ) : "";

    # Unpack any recombination data; it was packed by
    # sub send_lines_to_vertical_aligner. Contents:
    #
    #   [0] type: 1=opening non-block    2=closing non-block
    #             3=opening block brace  4=closing block brace
    #   [1] flag: if opening: 1=no multiple steps, 2=multiple steps ok
    #             if closing: spaces of padding to use
    #   [2] sequence number of container
    #   [3] valid flag: do not append if this flag is false
    #
    my ( $open_or_close, $tightness_flag, $seqno, $valid, $seqno_beg,
        $seqno_end );
    if ($rvertical_tightness_flags) {
        (
            $open_or_close, $tightness_flag, $seqno, $valid, $seqno_beg,
            $seqno_end
        ) = @{$rvertical_tightness_flags};
    }

    $seqno_string = $seqno_end;

    # handle any cached line ..
    # either append this line to it or write it out
    if ( length($cached_line_text) ) {

        # Dump an invalid cached line
        if ( !$cached_line_valid ) {
            valign_output_step_C( $cached_line_text,
                $cached_line_leading_space_count,
                $last_level_written );
        }

        # Handle cached line ending in OPENING tokens
        elsif ( $cached_line_type == 1 || $cached_line_type == 3 ) {

            my $gap = $leading_space_count - length($cached_line_text);

            # handle option of just one tight opening per line:
            if ( $cached_line_flag == 1 ) {
                if ( defined($open_or_close) && $open_or_close == 1 ) {
                    $gap = -1;
                }
            }

            if ( $gap >= 0 && defined($seqno_beg) ) {
                $leading_string      = $cached_line_text . ' ' x $gap;
                $leading_space_count = $cached_line_leading_space_count;
                $seqno_string        = $cached_seqno_string . ':' . $seqno_beg;
                $level               = $last_level_written;
            }
            else {
                valign_output_step_C( $cached_line_text,
                    $cached_line_leading_space_count,
                    $last_level_written );
            }
        }

        # Handle cached line ending in CLOSING tokens
        else {
            my $test_line = $cached_line_text . ' ' x $cached_line_flag . $str;
            if (

                # The new line must start with container
                $seqno_beg

                # The container combination must be okay..
                && (

                    # okay to combine like types
                    ( $open_or_close == $cached_line_type )

                    # closing block brace may append to non-block
                    || ( $cached_line_type == 2 && $open_or_close == 4 )

                    # something like ');'
                    || ( !$open_or_close && $cached_line_type == 2 )

                )

                # The combined line must fit
                && (
                    length($test_line) <=
                    maximum_line_length_for_level($last_level_written) )
              )
            {

                $seqno_string = $cached_seqno_string . ':' . $seqno_beg;

                # Patch to outdent closing tokens ending # in ');'
                # If we are joining a line like ');' to a previous stacked
                # set of closing tokens, then decide if we may outdent the
                # combined stack to the indentation of the ');'.  Since we
                # should not normally outdent any of the other tokens more than
                # the indentation of the lines that contained them, we will
                # only do this if all of the corresponding opening
                # tokens were on the same line.  This can happen with
                # -sot and -sct.  For example, it is ok here:
                #   __PACKAGE__->load_components( qw(
                #         PK::Auto
                #         Core
                #   ));
                #
                #   But, for example, we do not outdent in this example because
                #   that would put the closing sub brace out farther than the
                #   opening sub brace:
                #
                #   perltidy -sot -sct
                #   $c->Tk::bind(
                #       '<Control-f>' => sub {
                #           my ($c) = @_;
                #           my $e = $c->XEvent;
                #           itemsUnderArea $c;
                #       } );
                #
                if ( $str =~ /^\);/ && $cached_line_text =~ /^[\)\}\]\s]*$/ ) {

                    # The way to tell this is if the stacked sequence numbers
                    # of this output line are the reverse of the stacked
                    # sequence numbers of the previous non-blank line of
                    # sequence numbers.  So we can join if the previous
                    # nonblank string of tokens is the mirror image.  For
                    # example if stack )}] is 13:8:6 then we are looking for a
                    # leading stack like [{( which is 6:8:13 We only need to
                    # check the two ends, because the intermediate tokens must
                    # fall in order.  Note on speed: having to split on colons
                    # and eliminate multiple colons might appear to be slow,
                    # but it's not an issue because we almost never come
                    # through here.  In a typical file we don't.
                    $seqno_string               =~ s/^:+//;
                    $last_nonblank_seqno_string =~ s/^:+//;
                    $seqno_string               =~ s/:+/:/g;
                    $last_nonblank_seqno_string =~ s/:+/:/g;

                    # how many spaces can we outdent?
                    my $diff =
                      $cached_line_leading_space_count - $leading_space_count;
                    if (   $diff > 0
                        && length($seqno_string)
                        && length($last_nonblank_seqno_string) ==
                        length($seqno_string) )
                    {
                        my @seqno_last =
                          ( split /:/, $last_nonblank_seqno_string );
                        my @seqno_now = ( split /:/, $seqno_string );
                        if (   $seqno_now[-1] == $seqno_last[0]
                            && $seqno_now[0] == $seqno_last[-1] )
                        {

                            # OK to outdent ..
                            # for absolute safety, be sure we only remove
                            # whitespace
                            my $ws = substr( $test_line, 0, $diff );
                            if ( ( length($ws) == $diff ) && $ws =~ /^\s+$/ ) {

                                $test_line = substr( $test_line, $diff );
                                $cached_line_leading_space_count -= $diff;
                                $last_level_written =
                                  level_change(
                                    $cached_line_leading_space_count,
                                    $diff, $last_level_written );
                                reduce_valign_buffer_indentation($diff);
                            }

                            # shouldn't happen, but not critical:
                            ##else {
                            ## ERROR transferring indentation here
                            ##}
                        }
                    }
                }

                $str                 = $test_line;
                $leading_string      = "";
                $leading_space_count = $cached_line_leading_space_count;
                $level               = $last_level_written;
            }
            else {
                valign_output_step_C( $cached_line_text,
                    $cached_line_leading_space_count,
                    $last_level_written );
            }
        }
    }
    $cached_line_type = 0;
    $cached_line_text = "";

    # make the line to be written
    my $line = $leading_string . $str;

    # write or cache this line
    if ( !$open_or_close || $side_comment_length > 0 ) {
        valign_output_step_C( $line, $leading_space_count, $level );
    }
    else {
        $cached_line_text                = $line;
        $cached_line_type                = $open_or_close;
        $cached_line_flag                = $tightness_flag;
        $cached_seqno                    = $seqno;
        $cached_line_valid               = $valid;
        $cached_line_leading_space_count = $leading_space_count;
        $cached_seqno_string             = $seqno_string;
    }

    $last_level_written       = $level;
    $last_side_comment_length = $side_comment_length;
    $extra_indent_ok          = 0;
    return;
}

sub valign_output_step_C {

    ###############################################################
    # This is Step C in writing vertically aligned lines.
    # Lines are either stored in a buffer or passed along to the next step.
    # The reason for storing lines is that we may later want to reduce their
    # indentation when -sot and -sct are both used.
    ###############################################################
    my @args = @_;

    # Dump any saved lines if we see a line with an unbalanced opening or
    # closing token.
    dump_valign_buffer() if ( $seqno_string && $valign_buffer_filling );

    # Either store or write this line
    if ($valign_buffer_filling) {
        push @valign_buffer, [@args];
    }
    else {
        valign_output_step_D(@args);
    }

    # For lines starting or ending with opening or closing tokens..
    if ($seqno_string) {
        $last_nonblank_seqno_string = $seqno_string;

        # Start storing lines when we see a line with multiple stacked opening
        # tokens.
        # patch for RT #94354, requested by Colin Williams
        if ( $seqno_string =~ /^\d+(\:+\d+)+$/ && $args[0] !~ /^[\}\)\]\:\?]/ )
        {

            # This test is efficient but a little subtle: The first test says
            # that we have multiple sequence numbers and hence multiple opening
            # or closing tokens in this line.  The second part of the test
            # rejects stacked closing and ternary tokens.  So if we get here
            # then we should have stacked unbalanced opening tokens.

            # Here is a complex example:

            # Foo($Bar[0], {  # (side comment)
            # 	baz => 1,
            # });

            # The first line has sequence 6::4.  It does not begin with
            # a closing token or ternary, so it passes the test and must be
            # stacked opening tokens.

            # The last line has sequence 4:6 but is a stack of closing tokens,
            # so it gets rejected.

            # Note that the sequence number of an opening token for a qw quote
            # is a negative number and will be rejected.
            # For example, for the following line:
            #    skip_symbols([qw(
            # $seqno_string='10:5:-1'.  It would be okay to accept it but
            # I decided not to do this after testing.

            $valign_buffer_filling = $seqno_string;

        }
    }
    return;
}

sub valign_output_step_D {

    ###############################################################
    # This is Step D in writing vertically aligned lines.
    # Write one vertically aligned line of code to the output object.
    ###############################################################

    my ( $line, $leading_space_count, $level ) = @_;

    # The line is currently correct if there is no tabbing (recommended!)
    # We may have to lop off some leading spaces and replace with tabs.
    if ( $leading_space_count > 0 ) {

        # Nothing to do if no tabs
        if ( !( $rOpts_tabs || $rOpts_entab_leading_whitespace )
            || $rOpts_indent_columns <= 0 )
        {

            # nothing to do
        }

        # Handle entab option
        elsif ($rOpts_entab_leading_whitespace) {

         # Patch 12-nov-2018 based on report from Glenn. Extra padding was
         # not correctly entabbed, nor were side comments:
         # Increase leading space count for a padded line to get correct tabbing
            if ( $line =~ /^(\s+)(.*)$/ ) {
                my $spaces = length($1);
                if ( $spaces > $leading_space_count ) {
                    $leading_space_count = $spaces;
                }
            }

            my $space_count =
              $leading_space_count % $rOpts_entab_leading_whitespace;
            my $tab_count =
              int( $leading_space_count / $rOpts_entab_leading_whitespace );
            my $leading_string = "\t" x $tab_count . ' ' x $space_count;
            if ( $line =~ /^\s{$leading_space_count,$leading_space_count}/ ) {
                substr( $line, 0, $leading_space_count ) = $leading_string;
            }
            else {

                # shouldn't happen - program error counting whitespace
                # - skip entabbing
                VALIGN_DEBUG_FLAG_TABS
                  && warning(
"Error entabbing in valign_output_step_D: expected count=$leading_space_count\n"
                  );
            }
        }

        # Handle option of one tab per level
        else {
            my $leading_string = ( "\t" x $level );
            my $space_count =
              $leading_space_count - $level * $rOpts_indent_columns;

            # shouldn't happen:
            if ( $space_count < 0 ) {

                # But it could be an outdented comment
                if ( $line !~ /^\s*#/ ) {
                    VALIGN_DEBUG_FLAG_TABS
                      && warning(
"Error entabbing in valign_output_step_D: for level=$group_level count=$leading_space_count\n"
                      );
                }
                $leading_string = ( ' ' x $leading_space_count );
            }
            else {
                $leading_string .= ( ' ' x $space_count );
            }
            if ( $line =~ /^\s{$leading_space_count,$leading_space_count}/ ) {
                substr( $line, 0, $leading_space_count ) = $leading_string;
            }
            else {

                # shouldn't happen - program error counting whitespace
                # we'll skip entabbing
                VALIGN_DEBUG_FLAG_TABS
                  && warning(
"Error entabbing in valign_output_step_D: expected count=$leading_space_count\n"
                  );
            }
        }
    }
    $file_writer_object->write_code_line( $line . "\n" );
    return;
}

{    # begin get_leading_string

    my @leading_string_cache;

    sub get_leading_string {

        # define the leading whitespace string for this line..
        my $leading_whitespace_count = shift;

        # Handle case of zero whitespace, which includes multi-line quotes
        # (which may have a finite level; this prevents tab problems)
        if ( $leading_whitespace_count <= 0 ) {
            return "";
        }

        # look for previous result
        elsif ( $leading_string_cache[$leading_whitespace_count] ) {
            return $leading_string_cache[$leading_whitespace_count];
        }

        # must compute a string for this number of spaces
        my $leading_string;

        # Handle simple case of no tabs
        if ( !( $rOpts_tabs || $rOpts_entab_leading_whitespace )
            || $rOpts_indent_columns <= 0 )
        {
            $leading_string = ( ' ' x $leading_whitespace_count );
        }

        # Handle entab option
        elsif ($rOpts_entab_leading_whitespace) {
            my $space_count =
              $leading_whitespace_count % $rOpts_entab_leading_whitespace;
            my $tab_count = int(
                $leading_whitespace_count / $rOpts_entab_leading_whitespace );
            $leading_string = "\t" x $tab_count . ' ' x $space_count;
        }

        # Handle option of one tab per level
        else {
            $leading_string = ( "\t" x $group_level );
            my $space_count =
              $leading_whitespace_count - $group_level * $rOpts_indent_columns;

            # shouldn't happen:
            if ( $space_count < 0 ) {
                VALIGN_DEBUG_FLAG_TABS
                  && warning(
"Error in get_leading_string: for level=$group_level count=$leading_whitespace_count\n"
                  );

                # -- skip entabbing
                $leading_string = ( ' ' x $leading_whitespace_count );
            }
            else {
                $leading_string .= ( ' ' x $space_count );
            }
        }
        $leading_string_cache[$leading_whitespace_count] = $leading_string;
        return $leading_string;
    }
}    # end get_leading_string

sub report_anything_unusual {
    my $self = shift;
    if ( $outdented_line_count > 0 ) {
        write_logfile_entry(
            "$outdented_line_count long lines were outdented:\n");
        write_logfile_entry(
            "  First at output line $first_outdented_line_at\n");

        if ( $outdented_line_count > 1 ) {
            write_logfile_entry(
                "   Last at output line $last_outdented_line_at\n");
        }
        write_logfile_entry(
            "  use -noll to prevent outdenting, -l=n to increase line length\n"
        );
        write_logfile_entry("\n");
    }
    return;
}
1;

