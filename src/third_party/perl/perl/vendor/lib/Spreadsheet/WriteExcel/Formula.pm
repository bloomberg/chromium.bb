package Spreadsheet::WriteExcel::Formula;

###############################################################################
#
# Formula - A class for generating Excel formulas.
#
#
# Used in conjunction with Spreadsheet::WriteExcel
#
# Copyright 2000-2010, John McNamara, jmcnamara@cpan.org
#
# Documentation after __END__
#

use Exporter;
use strict;
use Carp;






use vars qw($VERSION @ISA);
@ISA = qw(Exporter);

$VERSION = '2.40';

###############################################################################
#
# Class data.
#
my $parser;
my %ptg;
my %functions;


###############################################################################
#
# For debugging.
#
my $_debug = 0;


###############################################################################
#
# new()
#
# Constructor
#
sub new {

    my $class  = $_[0];

    my $self   = {
                    _byte_order     => $_[1],
                    _workbook       => "",
                    _ext_sheets     => {},
                    _ext_refs       => {},
                    _ext_ref_count  => 0,
                    _ext_names      => {},
                 };

    bless $self, $class;
    return $self;
}


###############################################################################
#
# _init_parser()
#
# There is a small overhead involved in generating the parser. Therefore, the
# initialisation is delayed until a formula is required.
# TODO: use a pre-compiled grammar.
#
# Porters take note, a recursive descent parser isn't mandatory. A future
# version of this module may use a YACC based parser instead.
#
sub _init_parser {

    my $self = shift;

    # Delay loading Parse::RecDescent to reduce the module dependencies.
    eval { require Parse::RecDescent };
    die  "The Parse::RecDescent module must be installed in order ".
         "to write an Excel formula\n" if $@;

    $self->_initialize_hashes();

    # The parsing grammar.
    #
    # TODO: Add support for international versions of Excel
    #
    $parser = Parse::RecDescent->new(<<'EndGrammar');

        expr:           list

        # Match arg lists such as SUM(1,2, 3)
        list:           <leftop: addition ',' addition>
                        { [ $item[1], '_arg', scalar @{$item[1]} ] }

        addition:       <leftop: multiplication add_op multiplication>

        # TODO: The add_op operators don't have equal precedence.
        add_op:         add |  sub | concat
                        | eq | ne | le | ge | lt | gt   # Order is important

        add:            '+'  { 'ptgAdd'    }
        sub:            '-'  { 'ptgSub'    }
        concat:         '&'  { 'ptgConcat' }
        eq:             '='  { 'ptgEQ'     }
        ne:             '<>' { 'ptgNE'     }
        le:             '<=' { 'ptgLE'     }
        ge:             '>=' { 'ptgGE'     }
        lt:             '<'  { 'ptgLT'     }
        gt:             '>'  { 'ptgGT'     }


        multiplication: <leftop: exponention mult_op exponention>

        mult_op:        mult  | div
        mult:           '*' { 'ptgMul' }
        div:            '/' { 'ptgDiv' }

        # Left associative (apparently)
        exponention:    <leftop: factor exp_op factor>

        exp_op:         '^' { 'ptgPower' }

        factor:         number       # Order is important
                        | string
                        | range2d
                        | range3d
                        | true
                        | false
                        | ref2d
                        | ref3d
                        | function
                        | name
                        | '(' expr ')'  { [$item[2], 'ptgParen'] }

        # Match a string.
        # Regex by merlyn. See http://www.perlmonks.org/index.pl?node_id=330280
        #
        string:           /"([^"]|"")*"/     #" For editors
                        { [ '_str', $item[1]] }

        # Match float or integer
        number:           /([+-]?)(?=\d|\.\d)\d*(\.\d*)?([Ee]([+-]?\d+))?/
                        { ['_num', $item[1]] }

        # Note: The highest column values is IV. The following regexes match
        # up to IZ. Out of range values are caught in the code.
        #
        # Note: sheetnames with whitespace, commas, or parentheses must be in
        # single quotes. Applies to ref3d and range3d
        #

        # Match A1, $A1, A$1 or $A$1.
        ref2d:            /\$?[A-I]?[A-Z]\$?\d+/
                        { ['_ref2d', $item[1]] }

        # Match an external sheet reference: Sheet1!A1 or 'Sheet (1)'!A1
        ref3d:            /[^!(,]+!\$?[A-I]?[A-Z]\$?\d+/
                        { ['_ref3d', $item[1]] }
                        | /'[^']+'!\$?[A-I]?[A-Z]\$?\d+/
                        { ['_ref3d', $item[1]] }

        # Match A1:C5, $A1:$C5 or A:C etc.
        range2d:          /\$?[A-I]?[A-Z]\$?(\d+)?:\$?[A-I]?[A-Z]\$?(\d+)?/
                        { ['_range2d', $item[1]] }

        # Match an external sheet range. 'Sheet 1:Sheet 2'!B2:C5
        range3d:          /[^!(,]+!\$?[A-I]?[A-Z]\$?(\d+)?:\$?[A-I]?[A-Z]\$?(\d+)?/
                        { ['_range3d', $item[1]] }
                        | /'[^']+'!\$?[A-I]?[A-Z]\$?(\d+)?:\$?[A-I]?[A-Z]\$?(\d+)?/
                        { ['_range3d', $item[1]] }

        # Match a function name.
        function:         /[A-Z0-9À-Ü_.]+/ '()'
                        { ['_funcV', $item[1]] }
                        | /[A-Z0-9À-Ü_.]+/ '(' expr ')'
                        { ['_class', $item[1], $item[3], '_funcV', $item[1]] }
                        | /[A-Z0-9À-Ü_.]+/ '(' list ')'
                        { ['_class', $item[1], $item[3], '_funcV', $item[1]] }

        # Match a defined name.
        name:           /[A-Za-z_]\w+/
                        { ['_name', $item[1]] }

        # Boolean values.
        true:           'TRUE'  { [ 'ptgBool', 1 ] }

        false:          'FALSE' { [ 'ptgBool', 0 ] }

EndGrammar

print "Init_parser.\n\n" if $_debug;
}



###############################################################################
#
# parse_formula()
#
# Takes a textual description of a formula and returns a RPN encoded byte
# string.
#
sub parse_formula {

    my $self= shift;

    # Initialise the parser if this is the first call
    $self->_init_parser() if not defined $parser;

    my $formula = shift @_;
    my $tokens;

    print $formula, "\n" if $_debug;

    # Build the parse tree for the formula
    my $parsetree =$parser->expr($formula);

    # Check if parsing worked.
    if (defined $parsetree) {
        my @tokens = $self->_reverse_tree(@$parsetree);

        # Add a volatile token if the formula contains a volatile function.
        # This must be the first token in the list
        #
        unshift @tokens, '_vol' if $self->_check_volatile(@tokens);

        # The return value depends on which Worksheet.pm method is the caller
        if (wantarray) {
            # Parse formula to see if it throws any errors and then
            # return raw tokens to Worksheet::store_formula()
            #
            $self->parse_tokens(@tokens);
            return @tokens;
        }
        else{
            # Return byte stream to Worksheet::write_formula()
            return $self->parse_tokens(@tokens);
        }
    }
    else {
        die "Couldn't parse formula: =$formula\n";
    }
}


###############################################################################
#
# parse_tokens()
#
# Convert each token or token pair to its Excel 'ptg' equivalent.
#
sub parse_tokens {

    my $self        = shift;
    my $parse_str   = '';
    my $last_type   = '';
    my $modifier    = '';
    my $num_args    = 0;
    my $class       = 0;
    my @class       = 1;
    my @tokens      = @_;


    # A note about the class modifiers used below. In general the class,
    # "reference" or "value", of a function is applied to all of its operands.
    # However, in certain circumstances the operands can have mixed classes,
    # e.g. =VLOOKUP with external references. These will eventually be dealt
    # with by the parser. However, as a workaround the class type of a token
    # can be changed via the repeat_formula interface. Thus, a _ref2d token can
    # be changed by the user to _ref2dA or _ref2dR to change its token class.
    #
    while (@_) {
        my $token = shift @_;

        if ($token eq '_arg') {
            $num_args = shift @_;
        }
        elsif ($token eq '_class') {
            $token = shift @_;
            $class = $functions{$token}[2];
            # If $class is undef then it means that the function isn't valid.
            die "Unknown function $token() in formula\n" unless defined $class;
            push @class, $class;
        }
        elsif ($token eq '_vol') {
            $parse_str  .= $self->_convert_volatile();
        }
        elsif ($token eq 'ptgBool') {
            $token = shift @_;
            $parse_str .= $self->_convert_bool($token);
        }
        elsif ($token eq '_num') {
            $token = shift @_;
            $parse_str .= $self->_convert_number($token);
        }
        elsif ($token eq '_str') {
            $token = shift @_;
            $parse_str .= $self->_convert_string($token);
        }
        elsif ($token =~ /^_ref2d/) {
            ($modifier  = $token) =~ s/_ref2d//;
            $class      = $class[-1];
            $class      = 0 if $modifier eq 'R';
            $class      = 1 if $modifier eq 'V';
            $token      = shift @_;
            $parse_str .= $self->_convert_ref2d($token, $class);
        }
        elsif ($token =~ /^_ref3d/) {
            ($modifier  = $token) =~ s/_ref3d//;
            $class      = $class[-1];
            $class      = 0 if $modifier eq 'R';
            $class      = 1 if $modifier eq 'V';
            $token      = shift @_;
            $parse_str .= $self->_convert_ref3d($token, $class);
        }
        elsif ($token =~ /^_range2d/) {
            ($modifier  = $token) =~ s/_range2d//;
            $class      = $class[-1];
            $class      = 0 if $modifier eq 'R';
            $class      = 1 if $modifier eq 'V';
            $token      = shift @_;
            $parse_str .= $self->_convert_range2d($token, $class);
        }
        elsif ($token =~ /^_range3d/) {
            ($modifier  = $token) =~ s/_range3d//;
            $class      = $class[-1];
            $class      = 0 if $modifier eq 'R';
            $class      = 1 if $modifier eq 'V';
            $token      = shift @_;
            $parse_str .= $self->_convert_range3d($token, $class);
        }
        elsif ($token =~ /^_name/) {
            ($modifier  = $token) =~ s/_name//;
            $class      = $class[-1];
            $class      = 0 if $modifier eq 'R';
            $class      = 1 if $modifier eq 'V';
            $token = shift @_;
            $parse_str .= $self->_convert_name($token, $class);
        }
        elsif ($token eq '_funcV') {
            $token = shift @_;
            $parse_str .= $self->_convert_function($token, $num_args);
            pop @class;
            $num_args = 0; # Reset after use
        }
        elsif ($token eq '_func') {
            $token = shift @_;
            $parse_str .= $self->_convert_function($token, $num_args, 1);
            pop @class;
            $num_args = 0; # Reset after use
        }
        elsif (exists $ptg{$token}) {
            $parse_str .= pack("C", $ptg{$token});
        }
        else {
            # Unrecognised token
            return undef;
        }
    }


    if ($_debug) {
        print join(" ", map { sprintf "%02X", $_ } unpack("C*",$parse_str));
        print "\n\n";
        print join(" ", @tokens), "\n\n";
    }

    return $parse_str;
}


###############################################################################
#
#  _reverse_tree()
#
# This function descends recursively through the parse tree. At each level it
# swaps the order of an operator followed by an operand.
# For example, 1+2*3 would be converted in the following sequence:
#               1 + 2 * 3
#               1 + (2 * 3)
#               1 + (2 3 *)
#               1 (2 3 *) +
#               1 2 3 * +
#
sub _reverse_tree
{
    my $self = shift;

    my @tokens;
    my @expression = @_;
    my @stack;

    while (@expression) {
        my $token = shift @expression;

        # If the token is an operator swap it with the following operand
        if (    $token eq 'ptgAdd'      ||
                $token eq 'ptgSub'      ||
                $token eq 'ptgConcat'   ||
                $token eq 'ptgMul'      ||
                $token eq 'ptgDiv'      ||
                $token eq 'ptgPower'    ||
                $token eq 'ptgEQ'       ||
                $token eq 'ptgNE'       ||
                $token eq 'ptgLE'       ||
                $token eq 'ptgGE'       ||
                $token eq 'ptgLT'       ||
                $token eq 'ptgGT')
        {
            my $operand = shift @expression;
            push @stack, $operand;
        }

        push @stack, $token;
    }

    # Recurse through the parse tree
    foreach my $token (@stack) {
        if (ref($token)) {
            push @tokens, $self->_reverse_tree(@$token);
        }
        else {
            push @tokens, $token;
        }
    }

    return  @tokens;
}


###############################################################################
#
#  _check_volatile()
#
# Check if the formula contains a volatile function, i.e. a function that must
# be recalculated each time a cell is updated. These formulas require a ptgAttr
# with the volatile flag set as the first token in the parsed expression.
#
# Examples of volatile functions: RAND(), NOW(), TODAY()
#
sub _check_volatile {

    my $self     = shift;
    my @tokens   = @_;
    my $volatile = 0;

    for my $i (0..@tokens-1) {
        # If the next token is a function check if it is volatile.
        if ($tokens[$i] =~ m/^_func/ and $functions{$tokens[$i+1]}[3]) {
            $volatile = 1;
            last;
        }
    }

    return $volatile;
}


###############################################################################
#
# _convert_volatile()
#
# Convert _vol to a ptgAttr tag formatted to indicate that the formula contains
# a volatile function. See _check_volatile()
#
sub _convert_volatile {

    my $self = shift;

    # Set bitFattrSemi flag to indicate volatile function, "w" is set to zero.
    return pack("CCv", $ptg{ptgAttr}, 0x1, 0x0);
}


###############################################################################
#
# _convert_bool()
#
# Convert a boolean token to ptgBool
#
sub _convert_bool {

    my $self = shift;
    my $bool = shift;

    return pack("CC", $ptg{ptgBool}, $bool);
}


###############################################################################
#
# _convert_number()
#
# Convert a number token to ptgInt or ptgNum
#
sub _convert_number {

    my $self = shift;
    my $num  = shift;

    # Integer in the range 0..2**16-1
    if (($num =~ /^\d+$/) && ($num <= 65535)) {
        return pack("Cv", $ptg{ptgInt}, $num);
    }
    else { # A float
        $num = pack("d", $num);
        $num = reverse $num if $self->{_byte_order};
        return pack("C", $ptg{ptgNum}) . $num;
    }
}


###############################################################################
#
# _convert_string()
#
# Convert a string to a ptg Str.
#
sub _convert_string {

    my $self     = shift;
    my $str      = shift;
    my $encoding = 0;

    $str =~ s/^"//;   # Remove leading  "
    $str =~ s/"$//;   # Remove trailing "
    $str =~ s/""/"/g; # Substitute Excel's escaped double quote "" for "

    my $length = length($str);

    # Handle utf8 strings in perl 5.8.
    if ($] >= 5.008) {
        require Encode;

        if (Encode::is_utf8($str)) {
            $str = Encode::encode("UTF-16LE", $str);
            $encoding = 1;
        }
    }

    die "String in formula has more than 255 chars\n" if $length > 255;

    return pack("CCC", $ptg{ptgStr}, $length, $encoding) . $str;
}


###############################################################################
#
# _convert_ref2d()
#
# Convert an Excel reference such as A1, $B2, C$3 or $D$4 to a ptgRefV.
#
sub _convert_ref2d {

    my $self  = shift;
    my $cell  = shift;
    my $class = shift;
    my $ptgRef;

    # Convert the cell reference
    my ($row, $col) = $self->_cell_to_packed_rowcol($cell);

    # The ptg value depends on the class of the ptg.
    if    ($class == 0) {
        $ptgRef = pack("C", $ptg{ptgRef});
    }
    elsif ($class == 1) {
        $ptgRef = pack("C", $ptg{ptgRefV});
    }
    elsif ($class == 2) {
        $ptgRef = pack("C", $ptg{ptgRefA});
    }
    else{
        die "Unknown function class in formula\n";
    }

    return $ptgRef . $row . $col;
}


###############################################################################
#
# _convert_ref3d
#
# Convert an Excel 3d reference such as "Sheet1!A1" or "Sheet1:Sheet2!A1" to a
# ptgRef3dV.
#
sub _convert_ref3d {

    my $self  = shift;
    my $token = shift;
    my $class = shift;
    my $ptgRef;

    # Split the ref at the ! symbol
    my ($ext_ref, $cell) = split '!', $token;

    # Convert the external reference part
    $ext_ref = $self->_pack_ext_ref($ext_ref);

    # Convert the cell reference part
    my ($row, $col) = $self->_cell_to_packed_rowcol($cell);

    # The ptg value depends on the class of the ptg.
    if    ($class == 0) {
        $ptgRef = pack("C", $ptg{ptgRef3d});
    }
    elsif ($class == 1) {
        $ptgRef = pack("C", $ptg{ptgRef3dV});
    }
    elsif ($class == 2) {
        $ptgRef = pack("C", $ptg{ptgRef3dA});
    }
    else{
        die "Unknown function class in formula\n";
    }

    return $ptgRef . $ext_ref. $row . $col;
}


###############################################################################
#
# _convert_range2d()
#
# Convert an Excel range such as A1:D4 or A:D to a ptgRefV.
#
sub _convert_range2d {

    my $self  = shift;
    my $range = shift;
    my $class = shift;
    my $ptgArea;

    # Split the range into 2 cell refs
    my ($cell1, $cell2) = split ':', $range;

    # A range such as A:D is equivalent to A1:D65536, so add rows as required
    $cell1 .= '1'     if $cell1 !~ /\d/;
    $cell2 .= '65536' if $cell2 !~ /\d/;

    # Convert the cell references
    my ($row1, $col1) = $self->_cell_to_packed_rowcol($cell1);
    my ($row2, $col2) = $self->_cell_to_packed_rowcol($cell2);

    # The ptg value depends on the class of the ptg.
    if    ($class == 0) {
        $ptgArea = pack("C", $ptg{ptgArea});
    }
    elsif ($class == 1) {
        $ptgArea = pack("C", $ptg{ptgAreaV});
    }
    elsif ($class == 2) {
        $ptgArea = pack("C", $ptg{ptgAreaA});
    }
    else{
        die "Unknown function class in formula\n";
    }

    return $ptgArea . $row1 . $row2 . $col1. $col2;
}


###############################################################################
#
# _convert_range3d
#
# Convert an Excel 3d range such as "Sheet1!A1:D4" or "Sheet1:Sheet2!A1:D4" to
# a ptgArea3dV.
#
sub _convert_range3d {

    my $self      = shift;
    my $token     = shift;
    my $class = shift;
    my $ptgArea;

    # Split the ref at the ! symbol
    my ($ext_ref, $range) = split '!', $token;

    # Convert the external reference part
    $ext_ref = $self->_pack_ext_ref($ext_ref);

    # Split the range into 2 cell refs
    my ($cell1, $cell2) = split ':', $range;

    # A range such as A:D is equivalent to A1:D65536, so add rows as required
    $cell1 .= '1'     if $cell1 !~ /\d/;
    $cell2 .= '65536' if $cell2 !~ /\d/;

    # Convert the cell references
    my ($row1, $col1) = $self->_cell_to_packed_rowcol($cell1);
    my ($row2, $col2) = $self->_cell_to_packed_rowcol($cell2);

    # The ptg value depends on the class of the ptg.
    if    ($class == 0) {
        $ptgArea = pack("C", $ptg{ptgArea3d});
    }
    elsif ($class == 1) {
        $ptgArea = pack("C", $ptg{ptgArea3dV});
    }
    elsif ($class == 2) {
        $ptgArea = pack("C", $ptg{ptgArea3dA});
    }
    else{
        die "Unknown function class in formula\n";
    }

    return $ptgArea . $ext_ref . $row1 . $row2 . $col1. $col2;
}


###############################################################################
#
# _pack_ext_ref()
#
# Convert the sheet name part of an external reference, for example "Sheet1" or
# "Sheet1:Sheet2", to a packed structure.
#
sub _pack_ext_ref {

    my $self    = shift;
    my $ext_ref = shift;
    my $sheet1;
    my $sheet2;

    $ext_ref =~ s/^'//;   # Remove leading  ' if any.
    $ext_ref =~ s/'$//;   # Remove trailing ' if any.

    # Check if there is a sheet range eg., Sheet1:Sheet2.
    if ($ext_ref =~ /:/) {
        ($sheet1, $sheet2) = split ':', $ext_ref;

        $sheet1 = $self->_get_sheet_index($sheet1);
        $sheet2 = $self->_get_sheet_index($sheet2);

        # Reverse max and min sheet numbers if necessary
        if ($sheet1 > $sheet2) {
            ($sheet1, $sheet2) = ($sheet2, $sheet1);
        }
    }
    else {
        # Single sheet name only.
        ($sheet1, $sheet2) = ($ext_ref, $ext_ref);

        $sheet1 = $self->_get_sheet_index($sheet1);
        $sheet2 = $sheet1;
    }

    my $key = "$sheet1:$sheet2";
    my $index;

    if (exists $self->{_ext_refs}->{$key}) {
        $index = $self->{_ext_refs}->{$key};
    }
    else {
        $index = $self->{_ext_ref_count};
        $self->{_ext_refs}->{$key} = $index;
        $self->{_ext_ref_count}++;
    }

    return pack("v",$index);
}


###############################################################################
#
# _get_sheet_index()
#
# Look up the index that corresponds to an external sheet name. The hash of
# sheet names is updated by the add_worksheet() method of the Workbook class.
#
sub _get_sheet_index {

    my $self        = shift;
    my $sheet_name  = shift;

    # Handle utf8 sheetnames in perl 5.8.
    if ($] >= 5.008) {
        require Encode;

        if (Encode::is_utf8($sheet_name)) {
            $sheet_name = Encode::encode("UTF-16BE", $sheet_name);
        }
    }


    if (not exists $self->{_ext_sheets}->{$sheet_name}) {
        die "Unknown sheet name $sheet_name in formula\n";
    }
    else {
        return $self->{_ext_sheets}->{$sheet_name};
    }
}


###############################################################################
#
# set_ext_sheets()
#
# This semi-public method is used to update the hash of sheet names. It is
# updated by the add_worksheet() method of the Workbook class.
#
sub set_ext_sheets {

    my $self        = shift;
    my $worksheet   = shift;
    my $index       = shift;

    # The _ext_sheets hash is used to translate between worksheet names
    # and their index
    $self->{_ext_sheets}->{$worksheet} = $index;

}


###############################################################################
#
# get_ext_sheets()
#
# This semi-public method is used to get the worksheet references that were
# used in formulas for inclusion in the EXTERNSHEET Workbook record.
#
sub get_ext_sheets {

    my $self  = shift;

    return %{$self->{_ext_refs}};
}


###############################################################################
#
# get_ext_ref_count()
#
# This semi-public method is used to update the hash of sheet names. It is
# updated by the add_worksheet() method of the Workbook class.
#
sub get_ext_ref_count {

    my $self  = shift;

    return $self->{_ext_ref_count};
}


###############################################################################
#
# _get_name_index()
#
# Look up the index that corresponds to an external defined name. The hash of
# defined names is updated by the define_name() method in the Workbook class.
#
sub _get_name_index {

    my $self        = shift;
    my $name        = shift;

    if (not exists $self->{_ext_names}->{$name}) {
        die "Unknown defined name $name in formula\n";
    }
    else {
        return $self->{_ext_names}->{$name};
    }
}


###############################################################################
#
# set_ext_name()
#
# This semi-public method is used to update the hash of defined names.
#
sub set_ext_name {

    my $self        = shift;
    my $name        = shift;
    my $index       = shift;

    $self->{_ext_names}->{$name} = $index;
}


###############################################################################
#
# _convert_function()
#
# Convert a function to a ptgFuncV or ptgFuncVarV depending on the number of
# args that it takes.
#
sub _convert_function {

    my $self     = shift;
    my $token    = shift;
    my $num_args = shift;
    my $non_var  = shift;

    die "Unknown function $token() in formula\n"
        unless defined $functions{$token}[0];

    my $args = $functions{$token}[1];

    # Fixed number of args eg. TIME($i,$j,$k).
    if ($args >= 0) {
        # Check that the number of args is valid.
        if ($args != $num_args) {
            die "Incorrect number of arguments for $token() in formula\n";
        }
        else {
            if ($non_var) {
                return pack("Cv", $ptg{ptgFunc},  $functions{$token}[0]);
            }
            else {
                return pack("Cv", $ptg{ptgFuncV}, $functions{$token}[0]);
            }
        }
    }

    # Variable number of args eg. SUM($i,$j,$k, ..).
    if ($args == -1) {
        if ($non_var) {
            return pack "CCv", $ptg{ptgFuncVar},
                                $num_args, $functions{$token}[0];
        }
        else {
            return pack "CCv", $ptg{ptgFuncVarV},
                                $num_args, $functions{$token}[0];
        }
    }
}


###############################################################################
#
# _convert_name()
#
# Convert a symbolic name into a name reference.
#
sub _convert_name {

    my $self     = shift;
    my $name     = shift;
    my $class    = shift;

    my $ptgName;

    my $name_index = $self->_get_name_index($name);

    # The ptg value depends on the class of the ptg.
    if    ($class == 0) {
        $ptgName = $ptg{ptgName};
    }
    elsif ($class == 1) {
        $ptgName = $ptg{ptgNameV};
    }
    elsif ($class == 2) {
        $ptgName = $ptg{ptgNameA};
    }


    return pack 'CV', $ptgName, $name_index;
}


###############################################################################
#
# _cell_to_rowcol($cell_ref)
#
# Convert an Excel cell reference such as A1 or $B2 or C$3 or $D$4 to a zero
# indexed row and column number. Also returns two boolean values to indicate
# whether the row or column are relative references.
# TODO use function in Utility.pm
#
sub _cell_to_rowcol {

    my $self = shift;
    my $cell = shift;

    $cell =~ /(\$?)([A-I]?[A-Z])(\$?)(\d+)/;

    my $col_rel = $1 eq "" ? 1 : 0;
    my $col     = $2;
    my $row_rel = $3 eq "" ? 1 : 0;
    my $row     = $4;

    # Convert base26 column string to a number.
    # All your Base are belong to us.
    my @chars  = split //, $col;
    my $expn   = 0;
    $col       = 0;

    while (@chars) {
        my $char = pop(@chars); # LS char first
        $col += (ord($char) - ord('A') + 1) * (26**$expn);
        $expn++;
    }

    # Convert 1-index to zero-index
    $row--;
    $col--;

    return $row, $col, $row_rel, $col_rel;
}


###############################################################################
#
# _cell_to_packed_rowcol($row, $col, $row_rel, $col_rel)
#
# pack() row and column into the required 3 byte format.
#
sub _cell_to_packed_rowcol {

    use integer;    # Avoid << shift bug in Perl 5.6.0 on HP-UX

    my $self = shift;
    my $cell = shift;

    my ($row, $col, $row_rel, $col_rel) = $self->_cell_to_rowcol($cell);

    die "Column $cell greater than IV in formula\n" if $col >= 256;
    die "Row $cell greater than 65536 in formula\n" if $row >= 65536;

    # Set the high bits to indicate if row or col are relative.
    $col    |= $col_rel << 14;
    $col    |= $row_rel << 15;

    $row     = pack('v', $row);
    $col     = pack('v', $col);

    return ($row, $col);
}


###############################################################################
#
# _initialize_hashes()
#
sub _initialize_hashes {

    # The Excel ptg indices
    %ptg = (
        'ptgExp'            => 0x01,
        'ptgTbl'            => 0x02,
        'ptgAdd'            => 0x03,
        'ptgSub'            => 0x04,
        'ptgMul'            => 0x05,
        'ptgDiv'            => 0x06,
        'ptgPower'          => 0x07,
        'ptgConcat'         => 0x08,
        'ptgLT'             => 0x09,
        'ptgLE'             => 0x0A,
        'ptgEQ'             => 0x0B,
        'ptgGE'             => 0x0C,
        'ptgGT'             => 0x0D,
        'ptgNE'             => 0x0E,
        'ptgIsect'          => 0x0F,
        'ptgUnion'          => 0x10,
        'ptgRange'          => 0x11,
        'ptgUplus'          => 0x12,
        'ptgUminus'         => 0x13,
        'ptgPercent'        => 0x14,
        'ptgParen'          => 0x15,
        'ptgMissArg'        => 0x16,
        'ptgStr'            => 0x17,
        'ptgAttr'           => 0x19,
        'ptgSheet'          => 0x1A,
        'ptgEndSheet'       => 0x1B,
        'ptgErr'            => 0x1C,
        'ptgBool'           => 0x1D,
        'ptgInt'            => 0x1E,
        'ptgNum'            => 0x1F,
        'ptgArray'          => 0x20,
        'ptgFunc'           => 0x21,
        'ptgFuncVar'        => 0x22,
        'ptgName'           => 0x23,
        'ptgRef'            => 0x24,
        'ptgArea'           => 0x25,
        'ptgMemArea'        => 0x26,
        'ptgMemErr'         => 0x27,
        'ptgMemNoMem'       => 0x28,
        'ptgMemFunc'        => 0x29,
        'ptgRefErr'         => 0x2A,
        'ptgAreaErr'        => 0x2B,
        'ptgRefN'           => 0x2C,
        'ptgAreaN'          => 0x2D,
        'ptgMemAreaN'       => 0x2E,
        'ptgMemNoMemN'      => 0x2F,
        'ptgNameX'          => 0x39,
        'ptgRef3d'          => 0x3A,
        'ptgArea3d'         => 0x3B,
        'ptgRefErr3d'       => 0x3C,
        'ptgAreaErr3d'      => 0x3D,
        'ptgArrayV'         => 0x40,
        'ptgFuncV'          => 0x41,
        'ptgFuncVarV'       => 0x42,
        'ptgNameV'          => 0x43,
        'ptgRefV'           => 0x44,
        'ptgAreaV'          => 0x45,
        'ptgMemAreaV'       => 0x46,
        'ptgMemErrV'        => 0x47,
        'ptgMemNoMemV'      => 0x48,
        'ptgMemFuncV'       => 0x49,
        'ptgRefErrV'        => 0x4A,
        'ptgAreaErrV'       => 0x4B,
        'ptgRefNV'          => 0x4C,
        'ptgAreaNV'         => 0x4D,
        'ptgMemAreaNV'      => 0x4E,
        'ptgMemNoMemN'      => 0x4F,
        'ptgFuncCEV'        => 0x58,
        'ptgNameXV'         => 0x59,
        'ptgRef3dV'         => 0x5A,
        'ptgArea3dV'        => 0x5B,
        'ptgRefErr3dV'      => 0x5C,
        'ptgAreaErr3d'      => 0x5D,
        'ptgArrayA'         => 0x60,
        'ptgFuncA'          => 0x61,
        'ptgFuncVarA'       => 0x62,
        'ptgNameA'          => 0x63,
        'ptgRefA'           => 0x64,
        'ptgAreaA'          => 0x65,
        'ptgMemAreaA'       => 0x66,
        'ptgMemErrA'        => 0x67,
        'ptgMemNoMemA'      => 0x68,
        'ptgMemFuncA'       => 0x69,
        'ptgRefErrA'        => 0x6A,
        'ptgAreaErrA'       => 0x6B,
        'ptgRefNA'          => 0x6C,
        'ptgAreaNA'         => 0x6D,
        'ptgMemAreaNA'      => 0x6E,
        'ptgMemNoMemN'      => 0x6F,
        'ptgFuncCEA'        => 0x78,
        'ptgNameXA'         => 0x79,
        'ptgRef3dA'         => 0x7A,
        'ptgArea3dA'        => 0x7B,
        'ptgRefErr3dA'      => 0x7C,
        'ptgAreaErr3d'      => 0x7D,
    );

    # Thanks to Michael Meeks and Gnumeric for the initial arg values.
    #
    # The following hash was generated by "function_locale.pl" in the distro.
    # Refer to function_locale.pl for non-English function names.
    #
    # The array elements are as follow:
    # ptg:   The Excel function ptg code.
    # args:  The number of arguments that the function takes:
    #           >=0 is a fixed number of arguments.
    #           -1  is a variable  number of arguments.
    # class: The reference, value or array class of the function args.
    # vol:   The function is volatile.
    #
    %functions  = (
        #                                     ptg  args  class  vol
        'COUNT'                         => [   0,   -1,    0,    0 ],
        'IF'                            => [   1,   -1,    1,    0 ],
        'ISNA'                          => [   2,    1,    1,    0 ],
        'ISERROR'                       => [   3,    1,    1,    0 ],
        'SUM'                           => [   4,   -1,    0,    0 ],
        'AVERAGE'                       => [   5,   -1,    0,    0 ],
        'MIN'                           => [   6,   -1,    0,    0 ],
        'MAX'                           => [   7,   -1,    0,    0 ],
        'ROW'                           => [   8,   -1,    0,    0 ],
        'COLUMN'                        => [   9,   -1,    0,    0 ],
        'NA'                            => [  10,    0,    0,    0 ],
        'NPV'                           => [  11,   -1,    1,    0 ],
        'STDEV'                         => [  12,   -1,    0,    0 ],
        'DOLLAR'                        => [  13,   -1,    1,    0 ],
        'FIXED'                         => [  14,   -1,    1,    0 ],
        'SIN'                           => [  15,    1,    1,    0 ],
        'COS'                           => [  16,    1,    1,    0 ],
        'TAN'                           => [  17,    1,    1,    0 ],
        'ATAN'                          => [  18,    1,    1,    0 ],
        'PI'                            => [  19,    0,    1,    0 ],
        'SQRT'                          => [  20,    1,    1,    0 ],
        'EXP'                           => [  21,    1,    1,    0 ],
        'LN'                            => [  22,    1,    1,    0 ],
        'LOG10'                         => [  23,    1,    1,    0 ],
        'ABS'                           => [  24,    1,    1,    0 ],
        'INT'                           => [  25,    1,    1,    0 ],
        'SIGN'                          => [  26,    1,    1,    0 ],
        'ROUND'                         => [  27,    2,    1,    0 ],
        'LOOKUP'                        => [  28,   -1,    0,    0 ],
        'INDEX'                         => [  29,   -1,    0,    1 ],
        'REPT'                          => [  30,    2,    1,    0 ],
        'MID'                           => [  31,    3,    1,    0 ],
        'LEN'                           => [  32,    1,    1,    0 ],
        'VALUE'                         => [  33,    1,    1,    0 ],
        'TRUE'                          => [  34,    0,    1,    0 ],
        'FALSE'                         => [  35,    0,    1,    0 ],
        'AND'                           => [  36,   -1,    1,    0 ],
        'OR'                            => [  37,   -1,    1,    0 ],
        'NOT'                           => [  38,    1,    1,    0 ],
        'MOD'                           => [  39,    2,    1,    0 ],
        'DCOUNT'                        => [  40,    3,    0,    0 ],
        'DSUM'                          => [  41,    3,    0,    0 ],
        'DAVERAGE'                      => [  42,    3,    0,    0 ],
        'DMIN'                          => [  43,    3,    0,    0 ],
        'DMAX'                          => [  44,    3,    0,    0 ],
        'DSTDEV'                        => [  45,    3,    0,    0 ],
        'VAR'                           => [  46,   -1,    0,    0 ],
        'DVAR'                          => [  47,    3,    0,    0 ],
        'TEXT'                          => [  48,    2,    1,    0 ],
        'LINEST'                        => [  49,   -1,    0,    0 ],
        'TREND'                         => [  50,   -1,    0,    0 ],
        'LOGEST'                        => [  51,   -1,    0,    0 ],
        'GROWTH'                        => [  52,   -1,    0,    0 ],
        'PV'                            => [  56,   -1,    1,    0 ],
        'FV'                            => [  57,   -1,    1,    0 ],
        'NPER'                          => [  58,   -1,    1,    0 ],
        'PMT'                           => [  59,   -1,    1,    0 ],
        'RATE'                          => [  60,   -1,    1,    0 ],
        'MIRR'                          => [  61,    3,    0,    0 ],
        'IRR'                           => [  62,   -1,    0,    0 ],
        'RAND'                          => [  63,    0,    1,    1 ],
        'MATCH'                         => [  64,   -1,    0,    0 ],
        'DATE'                          => [  65,    3,    1,    0 ],
        'TIME'                          => [  66,    3,    1,    0 ],
        'DAY'                           => [  67,    1,    1,    0 ],
        'MONTH'                         => [  68,    1,    1,    0 ],
        'YEAR'                          => [  69,    1,    1,    0 ],
        'WEEKDAY'                       => [  70,   -1,    1,    0 ],
        'HOUR'                          => [  71,    1,    1,    0 ],
        'MINUTE'                        => [  72,    1,    1,    0 ],
        'SECOND'                        => [  73,    1,    1,    0 ],
        'NOW'                           => [  74,    0,    1,    1 ],
        'AREAS'                         => [  75,    1,    0,    1 ],
        'ROWS'                          => [  76,    1,    0,    1 ],
        'COLUMNS'                       => [  77,    1,    0,    1 ],
        'OFFSET'                        => [  78,   -1,    0,    1 ],
        'SEARCH'                        => [  82,   -1,    1,    0 ],
        'TRANSPOSE'                     => [  83,    1,    1,    0 ],
        'TYPE'                          => [  86,    1,    1,    0 ],
        'ATAN2'                         => [  97,    2,    1,    0 ],
        'ASIN'                          => [  98,    1,    1,    0 ],
        'ACOS'                          => [  99,    1,    1,    0 ],
        'CHOOSE'                        => [ 100,   -1,    1,    0 ],
        'HLOOKUP'                       => [ 101,   -1,    0,    0 ],
        'VLOOKUP'                       => [ 102,   -1,    0,    0 ],
        'ISREF'                         => [ 105,    1,    0,    0 ],
        'LOG'                           => [ 109,   -1,    1,    0 ],
        'CHAR'                          => [ 111,    1,    1,    0 ],
        'LOWER'                         => [ 112,    1,    1,    0 ],
        'UPPER'                         => [ 113,    1,    1,    0 ],
        'PROPER'                        => [ 114,    1,    1,    0 ],
        'LEFT'                          => [ 115,   -1,    1,    0 ],
        'RIGHT'                         => [ 116,   -1,    1,    0 ],
        'EXACT'                         => [ 117,    2,    1,    0 ],
        'TRIM'                          => [ 118,    1,    1,    0 ],
        'REPLACE'                       => [ 119,    4,    1,    0 ],
        'SUBSTITUTE'                    => [ 120,   -1,    1,    0 ],
        'CODE'                          => [ 121,    1,    1,    0 ],
        'FIND'                          => [ 124,   -1,    1,    0 ],
        'CELL'                          => [ 125,   -1,    0,    1 ],
        'ISERR'                         => [ 126,    1,    1,    0 ],
        'ISTEXT'                        => [ 127,    1,    1,    0 ],
        'ISNUMBER'                      => [ 128,    1,    1,    0 ],
        'ISBLANK'                       => [ 129,    1,    1,    0 ],
        'T'                             => [ 130,    1,    0,    0 ],
        'N'                             => [ 131,    1,    0,    0 ],
        'DATEVALUE'                     => [ 140,    1,    1,    0 ],
        'TIMEVALUE'                     => [ 141,    1,    1,    0 ],
        'SLN'                           => [ 142,    3,    1,    0 ],
        'SYD'                           => [ 143,    4,    1,    0 ],
        'DDB'                           => [ 144,   -1,    1,    0 ],
        'INDIRECT'                      => [ 148,   -1,    1,    1 ],
        'CALL'                          => [ 150,   -1,    1,    0 ],
        'CLEAN'                         => [ 162,    1,    1,    0 ],
        'MDETERM'                       => [ 163,    1,    2,    0 ],
        'MINVERSE'                      => [ 164,    1,    2,    0 ],
        'MMULT'                         => [ 165,    2,    2,    0 ],
        'IPMT'                          => [ 167,   -1,    1,    0 ],
        'PPMT'                          => [ 168,   -1,    1,    0 ],
        'COUNTA'                        => [ 169,   -1,    0,    0 ],
        'PRODUCT'                       => [ 183,   -1,    0,    0 ],
        'FACT'                          => [ 184,    1,    1,    0 ],
        'DPRODUCT'                      => [ 189,    3,    0,    0 ],
        'ISNONTEXT'                     => [ 190,    1,    1,    0 ],
        'STDEVP'                        => [ 193,   -1,    0,    0 ],
        'VARP'                          => [ 194,   -1,    0,    0 ],
        'DSTDEVP'                       => [ 195,    3,    0,    0 ],
        'DVARP'                         => [ 196,    3,    0,    0 ],
        'TRUNC'                         => [ 197,   -1,    1,    0 ],
        'ISLOGICAL'                     => [ 198,    1,    1,    0 ],
        'DCOUNTA'                       => [ 199,    3,    0,    0 ],
        'ROUNDUP'                       => [ 212,    2,    1,    0 ],
        'ROUNDDOWN'                     => [ 213,    2,    1,    0 ],
        'RANK'                          => [ 216,   -1,    0,    0 ],
        'ADDRESS'                       => [ 219,   -1,    1,    0 ],
        'DAYS360'                       => [ 220,   -1,    1,    0 ],
        'TODAY'                         => [ 221,    0,    1,    1 ],
        'VDB'                           => [ 222,   -1,    1,    0 ],
        'MEDIAN'                        => [ 227,   -1,    0,    0 ],
        'SUMPRODUCT'                    => [ 228,   -1,    2,    0 ],
        'SINH'                          => [ 229,    1,    1,    0 ],
        'COSH'                          => [ 230,    1,    1,    0 ],
        'TANH'                          => [ 231,    1,    1,    0 ],
        'ASINH'                         => [ 232,    1,    1,    0 ],
        'ACOSH'                         => [ 233,    1,    1,    0 ],
        'ATANH'                         => [ 234,    1,    1,    0 ],
        'DGET'                          => [ 235,    3,    0,    0 ],
        'INFO'                          => [ 244,    1,    1,    1 ],
        'DB'                            => [ 247,   -1,    1,    0 ],
        'FREQUENCY'                     => [ 252,    2,    0,    0 ],
        'ERROR.TYPE'                    => [ 261,    1,    1,    0 ],
        'REGISTER.ID'                   => [ 267,   -1,    1,    0 ],
        'AVEDEV'                        => [ 269,   -1,    0,    0 ],
        'BETADIST'                      => [ 270,   -1,    1,    0 ],
        'GAMMALN'                       => [ 271,    1,    1,    0 ],
        'BETAINV'                       => [ 272,   -1,    1,    0 ],
        'BINOMDIST'                     => [ 273,    4,    1,    0 ],
        'CHIDIST'                       => [ 274,    2,    1,    0 ],
        'CHIINV'                        => [ 275,    2,    1,    0 ],
        'COMBIN'                        => [ 276,    2,    1,    0 ],
        'CONFIDENCE'                    => [ 277,    3,    1,    0 ],
        'CRITBINOM'                     => [ 278,    3,    1,    0 ],
        'EVEN'                          => [ 279,    1,    1,    0 ],
        'EXPONDIST'                     => [ 280,    3,    1,    0 ],
        'FDIST'                         => [ 281,    3,    1,    0 ],
        'FINV'                          => [ 282,    3,    1,    0 ],
        'FISHER'                        => [ 283,    1,    1,    0 ],
        'FISHERINV'                     => [ 284,    1,    1,    0 ],
        'FLOOR'                         => [ 285,    2,    1,    0 ],
        'GAMMADIST'                     => [ 286,    4,    1,    0 ],
        'GAMMAINV'                      => [ 287,    3,    1,    0 ],
        'CEILING'                       => [ 288,    2,    1,    0 ],
        'HYPGEOMDIST'                   => [ 289,    4,    1,    0 ],
        'LOGNORMDIST'                   => [ 290,    3,    1,    0 ],
        'LOGINV'                        => [ 291,    3,    1,    0 ],
        'NEGBINOMDIST'                  => [ 292,    3,    1,    0 ],
        'NORMDIST'                      => [ 293,    4,    1,    0 ],
        'NORMSDIST'                     => [ 294,    1,    1,    0 ],
        'NORMINV'                       => [ 295,    3,    1,    0 ],
        'NORMSINV'                      => [ 296,    1,    1,    0 ],
        'STANDARDIZE'                   => [ 297,    3,    1,    0 ],
        'ODD'                           => [ 298,    1,    1,    0 ],
        'PERMUT'                        => [ 299,    2,    1,    0 ],
        'POISSON'                       => [ 300,    3,    1,    0 ],
        'TDIST'                         => [ 301,    3,    1,    0 ],
        'WEIBULL'                       => [ 302,    4,    1,    0 ],
        'SUMXMY2'                       => [ 303,    2,    2,    0 ],
        'SUMX2MY2'                      => [ 304,    2,    2,    0 ],
        'SUMX2PY2'                      => [ 305,    2,    2,    0 ],
        'CHITEST'                       => [ 306,    2,    2,    0 ],
        'CORREL'                        => [ 307,    2,    2,    0 ],
        'COVAR'                         => [ 308,    2,    2,    0 ],
        'FORECAST'                      => [ 309,    3,    2,    0 ],
        'FTEST'                         => [ 310,    2,    2,    0 ],
        'INTERCEPT'                     => [ 311,    2,    2,    0 ],
        'PEARSON'                       => [ 312,    2,    2,    0 ],
        'RSQ'                           => [ 313,    2,    2,    0 ],
        'STEYX'                         => [ 314,    2,    2,    0 ],
        'SLOPE'                         => [ 315,    2,    2,    0 ],
        'TTEST'                         => [ 316,    4,    2,    0 ],
        'PROB'                          => [ 317,   -1,    2,    0 ],
        'DEVSQ'                         => [ 318,   -1,    0,    0 ],
        'GEOMEAN'                       => [ 319,   -1,    0,    0 ],
        'HARMEAN'                       => [ 320,   -1,    0,    0 ],
        'SUMSQ'                         => [ 321,   -1,    0,    0 ],
        'KURT'                          => [ 322,   -1,    0,    0 ],
        'SKEW'                          => [ 323,   -1,    0,    0 ],
        'ZTEST'                         => [ 324,   -1,    0,    0 ],
        'LARGE'                         => [ 325,    2,    0,    0 ],
        'SMALL'                         => [ 326,    2,    0,    0 ],
        'QUARTILE'                      => [ 327,    2,    0,    0 ],
        'PERCENTILE'                    => [ 328,    2,    0,    0 ],
        'PERCENTRANK'                   => [ 329,   -1,    0,    0 ],
        'MODE'                          => [ 330,   -1,    2,    0 ],
        'TRIMMEAN'                      => [ 331,    2,    0,    0 ],
        'TINV'                          => [ 332,    2,    1,    0 ],
        'CONCATENATE'                   => [ 336,   -1,    1,    0 ],
        'POWER'                         => [ 337,    2,    1,    0 ],
        'RADIANS'                       => [ 342,    1,    1,    0 ],
        'DEGREES'                       => [ 343,    1,    1,    0 ],
        'SUBTOTAL'                      => [ 344,   -1,    0,    0 ],
        'SUMIF'                         => [ 345,   -1,    0,    0 ],
        'COUNTIF'                       => [ 346,    2,    0,    0 ],
        'COUNTBLANK'                    => [ 347,    1,    0,    0 ],
        'ROMAN'                         => [ 354,   -1,    1,    0 ],
    );

}




1;


__END__

=encoding latin1

=head1 NAME

Formula - A class for generating Excel formulas

=head1 SYNOPSIS

See the documentation for Spreadsheet::WriteExcel

=head1 DESCRIPTION

This module is used by Spreadsheet::WriteExcel. You do not need to use it directly.


=head1 NOTES

The following notes are to help developers and maintainers understand the sequence of operation. They are also intended as a pro-memoria for the author. ;-)

Spreadsheet::WriteExcel::Formula converts a textual representation of a formula into the pre-parsed binary format that Excel uses to store formulas. For example C<1+2*3> is stored as follows: C<1E 01 00 1E 02 00 1E 03 00 05 03>.

This string is comprised of operators and operands arranged in a reverse-Polish format. The meaning of the tokens in the above example is shown in the following table:

    Token   Name        Value
    1E      ptgInt      0001   (stored as 01 00)
    1E      ptgInt      0002   (stored as 02 00)
    1E      ptgInt      0003   (stored as 03 00)
    05      ptgMul
    03      ptgAdd

The tokens and token names are defined in the "Excel Developer's Kit" from Microsoft Press. C<ptg> stands for Parse ThinG (as in "That lexer can't grok it, it's a parse thang.")

In general the tokens fall into two categories: operators such as C<ptgMul> and operands such as C<ptgInt>. When the formula is evaluated by Excel the operand tokens push values onto a stack. The operator tokens then pop the required number of operands off of the stack, perform an operation and push the resulting value back onto the stack. This methodology is similar to the basic operation of a reverse-Polish (RPN) calculator.

Spreadsheet::WriteExcel::Formula parses a formula using a C<Parse::RecDescent> parser (at a later stage it may use a C<Parse::Yapp> parser or C<Parse::FastDescent>).

The parser converts the textual representation of a formula into a parse tree. Thus, C<1+2*3> is converted into something like the following, C<e> stands for expression:

             e
           / | \
         1   +   e
               / | \
             2   *   3


The function C<_reverse_tree()> recurses down through this structure swapping the order of operators followed by operands to produce a reverse-Polish tree. In other words the formula is converted from in-fix notation to post-fix. Following the above example the resulting tree would look like this:


             e
           / | \
         1   e   +
           / | \
         2   3   *

The result of the recursion is a single array of tokens. In our example the simplified form would look like the following:

    (1, 2, 3, *, +)

The actual return value contains some additional information to help in the secondary parsing stage:

    (_num, 1, _num, 2, _num, 3, ptgMul, ptgAdd, _arg, 1)

The additional tokens are:

    Token       Meaning
    _num        The next token is a number
    _str        The next token is a string
    _ref2d      The next token is a 2d cell reference
    _ref3d      The next token is a 3d cell reference
    _range2d    The next token is a 2d range
    _range3d    The next token is a 3d range
    _funcV       The next token is a function
    _arg        The next token is the number of args for a function
    _class      The next token is a function name
    _vol        The formula contains a voltile function

The C<_arg> token is generated for all lists but is only used for functions that take a variable number of arguments.

The C<_class> token indicates the start of the arguments to a function. This allows the post-processor to decide the "class" of the ref and range arguments that the function takes. The class can be reference, value or array. Since function calls can be nested, the class variable is stored on a stack in the C<@class> array. The class of the ref or range is then read as the top element of the stack C<$class[-1]>. When a C<_funcV> is read it pops the class value.

Certain Excel functions such as RAND() and NOW() are designated as volatile and must be recalculated by Excel every time that a cell is updated. Any formulas that contain one of these functions has a specially formatted C<ptgAttr> tag prepended to it to indicate that it is volatile.

A secondary parsing stage is carried out by C<parse_tokens()> which converts these tokens into a binary string. For the C<1+2*3> example this would give:

    1E 01 00 1E 02 00 1E 03 00 05 03

This two-pass method could probably have been reduced to a single pass through the C<Parse::RecDescent> parser. However, it was easier to develop and debug this way.

The token values and formula values are stored in the C<%ptg> and C<%functions> hashes. These hashes and the parser object C<$parser> are exposed as global data. This breaks the OO encapsulation, but means that they can be shared by several instances of Spreadsheet::WriteExcel called from the same program.

Non-English function names can be added to the C<%functions> hash using the C<function_locale.pl> program in the C<examples> directory of the distro. The supported languages are: German, French, Spanish, Portuguese, Dutch, Finnish, Italian and Swedish. These languages are not added by default because there are conflicts between functions names in different languages.

The parser is initialised by C<_init_parser()>. The initialisation is delayed until the first formula is parsed. This eliminates the overhead of generating the parser in programs that are not processing formulas. (The parser should really be pre-compiled, this is to-do when the grammar stabilises).




=head1 AUTHOR

John McNamara jmcnamara@cpan.org

=head1 COPYRIGHT

Copyright MM-MMX, John McNamara.

All Rights Reserved. This module is free software. It may be used, redistributed and/or modified under the same terms as Perl itself.
