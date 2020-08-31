package SQL::Abstract::Tree;

use Moo;
no warnings 'qw';

use Carp;
use Sub::Quote 'quote_sub';

my $op_look_ahead = '(?: (?= [\s\)\(\;] ) | \z)';
my $op_look_behind = '(?: (?<= [\,\s\)\(] ) | \A )';

my $quote_left = qr/[\`\'\"\[]/;
my $quote_right = qr/[\`\'\"\]]/;

my $placeholder_re = qr/(?: \? | \$\d+ )/x;

# These SQL keywords always signal end of the current expression (except inside
# of a parenthesized subexpression).
# Format: A list of strings that will be compiled to extended syntax ie.
# /.../x) regexes, without capturing parentheses. They will be automatically
# anchored to op boundaries (excluding quotes) to match the whole token.
my @expression_start_keywords = (
  'SELECT',
  'UPDATE',
  'SET',
  'INSERT \s+ INTO',
  'DELETE \s+ FROM',
  'FROM',
  '(?:
    (?:
        (?: (?: LEFT | RIGHT | FULL ) \s+ )?
        (?: (?: CROSS | INNER | OUTER ) \s+ )?
    )?
    JOIN
  )',
  'ON',
  'WHERE',
  '(?: DEFAULT \s+ )? VALUES',
  'GROUP \s+ BY',
  'HAVING',
  'ORDER \s+ BY',
  'SKIP',
  'FETCH',
  'FIRST',
  'LIMIT',
  'OFFSET',
  'FOR',
  'UNION',
  'INTERSECT',
  'EXCEPT',
  'BEGIN \s+ WORK',
  'COMMIT',
  'ROLLBACK \s+ TO \s+ SAVEPOINT',
  'ROLLBACK',
  'SAVEPOINT',
  'RELEASE \s+ SAVEPOINT',
  'RETURNING',
);

my $expr_start_re = join ("\n\t|\n", @expression_start_keywords );
$expr_start_re = qr/ $op_look_behind (?i: $expr_start_re ) $op_look_ahead /x;

# These are binary operator keywords always a single LHS and RHS
# * AND/OR are handled separately as they are N-ary
# * so is NOT as being unary
# * BETWEEN without parentheses around the ANDed arguments (which
#   makes it a non-binary op) is detected and accommodated in
#   _recurse_parse()
# * AS is not really an operator but is handled here as it's also LHS/RHS

# this will be included in the $binary_op_re, the distinction is interesting during
# testing as one is tighter than the other, plus alphanum cmp ops have different
# look ahead/behind (e.g. "x"="y" )
my @alphanum_cmp_op_keywords = (qw/< > != <> = <= >= /);
my $alphanum_cmp_op_re = join ("\n\t|\n", map
  { "(?: (?<= [\\w\\s] | $quote_right ) | \\A )"  . quotemeta ($_) . "(?: (?= [\\w\\s] | $quote_left ) | \\z )" }
  @alphanum_cmp_op_keywords
);
$alphanum_cmp_op_re = qr/$alphanum_cmp_op_re/x;

my $binary_op_re = '(?: NOT \s+)? (?:' . join ('|', qw/IN BETWEEN [RI]?LIKE REGEXP/) . ')';
$binary_op_re = join "\n\t|\n",
  "$op_look_behind (?i: $binary_op_re | AS ) $op_look_ahead",
  $alphanum_cmp_op_re,
  $op_look_behind . 'IS (?:\s+ NOT)?' . "(?= \\s+ NULL \\b | $op_look_ahead )",
;
$binary_op_re = qr/$binary_op_re/x;

my $rno_re = qr/ROW_NUMBER \s* \( \s* \) \s+ OVER/ix;

my $unary_op_re = 'NOT \s+ EXISTS | NOT | ' . $rno_re;
$unary_op_re = join "\n\t|\n",
  "$op_look_behind (?i: $unary_op_re ) $op_look_ahead",
;
$unary_op_re = qr/$unary_op_re/x;

my $asc_desc_re = qr/$op_look_behind (?i: ASC | DESC ) $op_look_ahead /x;
my $and_or_re = qr/$op_look_behind (?i: AND | OR ) $op_look_ahead /x;

my $tokenizer_re = join("\n\t|\n",
  $expr_start_re,
  $binary_op_re,
  $unary_op_re,
  $asc_desc_re,
  $and_or_re,
  $op_look_behind . ' \* ' . $op_look_ahead,
  (map { quotemeta $_ } qw/, ( )/),
  $placeholder_re,
);

# this one *is* capturing for the split below
# splits on whitespace if all else fails
# has to happen before the composing qr's are anchored (below)
$tokenizer_re = qr/ \s* ( $tokenizer_re ) \s* | \s+ /x;

# Parser states for _recurse_parse()
use constant PARSE_TOP_LEVEL => 0;
use constant PARSE_IN_EXPR => 1;
use constant PARSE_IN_PARENS => 2;
use constant PARSE_IN_FUNC => 3;
use constant PARSE_RHS => 4;
use constant PARSE_LIST_ELT => 5;

my $expr_term_re = qr/$expr_start_re | \)/x;
my $rhs_term_re = qr/ $expr_term_re | $binary_op_re | $unary_op_re | $asc_desc_re | $and_or_re | \, /x;
my $all_std_keywords_re = qr/ $rhs_term_re | \( | $placeholder_re /x;

# anchor everything - even though keywords are separated by the tokenizer, leakage may occur
for (
  $quote_left,
  $quote_right,
  $placeholder_re,
  $expr_start_re,
  $alphanum_cmp_op_re,
  $binary_op_re,
  $unary_op_re,
  $asc_desc_re,
  $and_or_re,
  $expr_term_re,
  $rhs_term_re,
  $all_std_keywords_re,
) {
  $_ = qr/ \A $_ \z /x;
}

# what can be bunched together under one MISC in an AST
my $compressable_node_re = qr/^ \- (?: MISC | LITERAL | PLACEHOLDER ) $/x;

my %indents = (
   select        => 0,
   update        => 0,
   'insert into' => 0,
   'delete from' => 0,
   from          => 1,
   where         => 0,
   join          => 1,
   'left join'   => 1,
   on            => 2,
   having        => 0,
   'group by'    => 0,
   'order by'    => 0,
   set           => 1,
   into          => 1,
   values        => 1,
   limit         => 1,
   offset        => 1,
   skip          => 1,
   first         => 1,
);


has [qw(
  newline indent_string indent_amount fill_in_placeholders placeholder_surround
)] => (is => 'ro');

has [qw( indentmap colormap )] => ( is => 'ro', default => quote_sub('{}') );

# class global is in fact desired
my $merger;

sub BUILDARGS {
  my $class = shift;
  my $args = ref $_[0] eq 'HASH' ? $_[0] : {@_};

  if (my $p = delete $args->{profile}) {
    my %extra_args;
    if ($p eq 'console') {
      %extra_args = (
        fill_in_placeholders => 1,
        placeholder_surround => ['?/', ''],
        indent_string => ' ',
        indent_amount => 2,
        newline       => "\n",
        colormap      => {},
        indentmap     => \%indents,

        ! ( eval { require Term::ANSIColor } ) ? () : do {
          my $c = \&Term::ANSIColor::color;

          my $red     = [$c->('red')    , $c->('reset')];
          my $cyan    = [$c->('cyan')   , $c->('reset')];
          my $green   = [$c->('green')  , $c->('reset')];
          my $yellow  = [$c->('yellow') , $c->('reset')];
          my $blue    = [$c->('blue')   , $c->('reset')];
          my $magenta = [$c->('magenta'), $c->('reset')];
          my $b_o_w   = [$c->('black on_white'), $c->('reset')];
          (
            placeholder_surround => [$c->('black on_magenta'), $c->('reset')],
            colormap => {
              'begin work'            => $b_o_w,
              commit                  => $b_o_w,
              rollback                => $b_o_w,
              savepoint               => $b_o_w,
              'rollback to savepoint' => $b_o_w,
              'release savepoint'     => $b_o_w,

              select                  => $red,
              'insert into'           => $red,
              update                  => $red,
              'delete from'           => $red,

              set                     => $cyan,
              from                    => $cyan,

              where                   => $green,
              values                  => $yellow,

              join                    => $magenta,
              'left join'             => $magenta,
              on                      => $blue,

              'group by'              => $yellow,
              having                  => $yellow,
              'order by'              => $yellow,

              skip                    => $green,
              first                   => $green,
              limit                   => $green,
              offset                  => $green,
            }
          );
        },
      );
    }
    elsif ($p eq 'console_monochrome') {
      %extra_args = (
        fill_in_placeholders => 1,
        placeholder_surround => ['?/', ''],
        indent_string => ' ',
        indent_amount => 2,
        newline       => "\n",
        indentmap     => \%indents,
      );
    }
    elsif ($p eq 'html') {
      %extra_args = (
        fill_in_placeholders => 1,
        placeholder_surround => ['<span class="placeholder">', '</span>'],
        indent_string => '&nbsp;',
        indent_amount => 2,
        newline       => "<br />\n",
        colormap      => { map {
          (my $class = $_) =~ s/\s+/-/g;
          ( $_ => [ qq|<span class="$class">|, '</span>' ] )
        } (
          keys %indents,
          qw(commit rollback savepoint),
          'begin work', 'rollback to savepoint', 'release savepoint',
        ) },
        indentmap     => \%indents,
      );
    }
    elsif ($p eq 'none') {
      # nada
    }
    else {
      croak "No such profile '$p'";
    }

    # see if we got any duplicates and merge if needed
    if (scalar grep { exists $args->{$_} } keys %extra_args) {
      # heavy-duty merge
      $args = ($merger ||= do {
        require Hash::Merge;
        my $m = Hash::Merge->new;

        $m->specify_behavior({
          SCALAR => {
            SCALAR => sub { $_[1] },
            ARRAY  => sub { [ $_[0], @{$_[1]} ] },
            HASH   => sub { $_[1] },
          },
          ARRAY => {
            SCALAR => sub { $_[1] },
            ARRAY  => sub { $_[1] },
            HASH   => sub { $_[1] },
          },
          HASH => {
            SCALAR => sub { $_[1] },
            ARRAY  => sub { [ values %{$_[0]}, @{$_[1]} ] },
            HASH   => sub { Hash::Merge::_merge_hashes( $_[0], $_[1] ) },
          },
        }, 'SQLA::Tree Behavior' );

        $m;
      })->merge(\%extra_args, $args );

    }
    else {
      $args = { %extra_args, %$args };
    }
  }

  $args;
}

sub parse {
  my ($self, $s) = @_;

  return [] unless defined $s;

  # tokenize string, and remove all optional whitespace
  my $tokens = [];
  foreach my $token (split $tokenizer_re, $s) {
    push @$tokens, $token if (
      defined $token
        and
      length $token
        and
      $token =~ /\S/
    );
  }

  return [ $self->_recurse_parse($tokens, PARSE_TOP_LEVEL) ];
}

sub _recurse_parse {
  my ($self, $tokens, $state) = @_;

  my @left;
  while (1) { # left-associative parsing

    if (! @$tokens
          or
        ($state == PARSE_IN_PARENS && $tokens->[0] eq ')')
          or
        ($state == PARSE_IN_EXPR && $tokens->[0] =~ $expr_term_re )
          or
        ($state == PARSE_RHS && $tokens->[0] =~ $rhs_term_re )
          or
        ($state == PARSE_LIST_ELT && ( $tokens->[0] eq ',' or $tokens->[0] =~ $expr_term_re ) )
    ) {
      return @left;
    }

    my $token = shift @$tokens;

    # nested expression in ()
    if ($token eq '(' ) {
      my @right = $self->_recurse_parse($tokens, PARSE_IN_PARENS);
      $token = shift @$tokens   or croak "missing closing ')' around block " . $self->unparse(\@right);
      $token eq ')'             or croak "unexpected token '$token' terminating block " . $self->unparse(\@right);

      push @left, [ '-PAREN' => \@right ];
    }

    # AND/OR
    elsif ($token =~ $and_or_re) {
      my $op = uc $token;

      my @right = $self->_recurse_parse($tokens, PARSE_IN_EXPR);

      # Merge chunks if "logic" matches
      @left = [ $op => [ @left, (@right and $op eq $right[0][0])
        ? @{ $right[0][1] }
        : @right
      ] ];
    }

    # LIST (,)
    elsif ($token eq ',') {

      my @right = $self->_recurse_parse($tokens, PARSE_LIST_ELT);

      # deal with malformed lists ( foo, bar, , baz )
      @right = [] unless @right;

      @right = [ -MISC => [ @right ] ] if @right > 1;

      if (!@left) {
        @left = [ -LIST => [ [], @right ] ];
      }
      elsif ($left[0][0] eq '-LIST') {
        push @{$left[0][1]}, (@{$right[0]} and  $right[0][0] eq '-LIST')
          ? @{$right[0][1]}
          : @right
        ;
      }
      else {
        @left = [ -LIST => [ @left, @right ] ];
      }
    }

    # binary operator keywords
    elsif ($token =~ $binary_op_re) {
      my $op = uc $token;

      my @right = $self->_recurse_parse($tokens, PARSE_RHS);

      # A between with a simple LITERAL for a 1st RHS argument needs a
      # rerun of the search to (hopefully) find the proper AND construct
      if ($op eq 'BETWEEN' and $right[0] eq '-LITERAL') {
        unshift @$tokens, $right[1][0];
        @right = $self->_recurse_parse($tokens, PARSE_IN_EXPR);
      }

      push @left, [$op => [ (@left ? pop @left : ''), @right ]];
    }

    # unary op keywords
    elsif ($token =~ $unary_op_re) {
      my $op = uc $token;

      # normalize RNO explicitly
      $op = 'ROW_NUMBER() OVER' if $op =~ /^$rno_re$/;

      my @right = $self->_recurse_parse($tokens, PARSE_RHS);

      push @left, [ $op => \@right ];
    }

    # expression terminator keywords
    elsif ($token =~ $expr_start_re) {
      my $op = uc $token;
      my @right = $self->_recurse_parse($tokens, PARSE_IN_EXPR);

      push @left, [ $op => \@right ];
    }

    # a '?'
    elsif ($token =~ $placeholder_re) {
      push @left, [ -PLACEHOLDER => [ $token ] ];
    }

    # check if the current token is an unknown op-start
    elsif (@$tokens and ($tokens->[0] eq '(' or $tokens->[0] =~ $placeholder_re ) ) {
      push @left, [ $token => [ $self->_recurse_parse($tokens, PARSE_RHS) ] ];
    }

    # we're now in "unknown token" land - start eating tokens until
    # we see something familiar, OR in the case of RHS (binop) stop
    # after the first token
    # Also stop processing when we could end up with an unknown func
    else {
      my @lits = [ -LITERAL => [$token] ];

      unshift @lits, pop @left if @left == 1;

      unless ( $state == PARSE_RHS ) {
        while (
          @$tokens
            and
          $tokens->[0] !~ $all_std_keywords_re
            and
          ! (@$tokens > 1 and $tokens->[1] eq '(')
        ) {
          push @lits, [ -LITERAL => [ shift @$tokens ] ];
        }
      }

      @lits = [ -MISC => [ @lits ] ] if @lits > 1;

      push @left, @lits;
    }

    # compress -LITERAL -MISC and -PLACEHOLDER pieces into a single
    # -MISC container
    if (@left > 1) {
      my $i = 0;
      while ($#left > $i) {
        if ($left[$i][0] =~ $compressable_node_re and $left[$i+1][0] =~ $compressable_node_re) {
          splice @left, $i, 2, [ -MISC => [
            map { $_->[0] eq '-MISC' ? @{$_->[1]} : $_ } (@left[$i, $i+1])
          ]];
        }
        else {
          $i++;
        }
      }
    }

    return @left if $state == PARSE_RHS;

    # deal with post-fix operators
    if (@$tokens) {
      # asc/desc
      if ($tokens->[0] =~ $asc_desc_re) {
        @left = [ ('-' . uc (shift @$tokens)) => [ @left ] ];
      }
    }
  }
}

sub format_keyword {
  my ($self, $keyword) = @_;

  if (my $around = $self->colormap->{lc $keyword}) {
     $keyword = "$around->[0]$keyword$around->[1]";
  }

  return $keyword
}

my %starters = (
   select        => 1,
   update        => 1,
   'insert into' => 1,
   'delete from' => 1,
);

sub pad_keyword {
   my ($self, $keyword, $depth) = @_;

   my $before = '';
   if (defined $self->indentmap->{lc $keyword}) {
      $before = $self->newline . $self->indent($depth + $self->indentmap->{lc $keyword});
   }
   $before = '' if $depth == 0 and defined $starters{lc $keyword};
   return [$before, ''];
}

sub indent { ($_[0]->indent_string||'') x ( ( $_[0]->indent_amount || 0 ) * $_[1] ) }

sub _is_key {
   my ($self, $tree) = @_;
   $tree = $tree->[0] while ref $tree;

   defined $tree && defined $self->indentmap->{lc $tree};
}

sub fill_in_placeholder {
   my ($self, $bindargs) = @_;

   if ($self->fill_in_placeholders) {
      my $val = shift @{$bindargs} || '';
      my $quoted = $val =~ s/^(['"])(.*)\1$/$2/;
      my ($left, $right) = @{$self->placeholder_surround};
      $val =~ s/\\/\\\\/g;
      $val =~ s/'/\\'/g;
      $val = qq('$val') if $quoted;
      return qq($left$val$right)
   }
   return '?'
}

# FIXME - terrible name for a user facing API
sub unparse {
  my ($self, $tree, $bindargs) = @_;
  $self->_unparse($tree, [@{$bindargs||[]}], 0);
}

sub _unparse {
  my ($self, $tree, $bindargs, $depth) = @_;

  if (not $tree or not @$tree) {
    return '';
  }

  # FIXME - needs a config switch to disable
  $self->_parenthesis_unroll($tree);

  my ($op, $args) = @{$tree}[0,1];

  if (! defined $op or (! ref $op and ! defined $args) ) {
    require Data::Dumper;
    Carp::confess( sprintf ( "Internal error - malformed branch at depth $depth:\n%s",
      Data::Dumper::Dumper($tree)
    ) );
  }

  if (ref $op) {
    return join (' ', map $self->_unparse($_, $bindargs, $depth), @$tree);
  }
  elsif ($op eq '-LITERAL') { # literal has different sig
    return $args->[0];
  }
  elsif ($op eq '-PLACEHOLDER') {
    return $self->fill_in_placeholder($bindargs);
  }
  elsif ($op eq '-PAREN') {
    return sprintf ('( %s )',
      join (' ', map { $self->_unparse($_, $bindargs, $depth + 2) } @{$args} )
        .
      ($self->_is_key($args)
        ? ( $self->newline||'' ) . $self->indent($depth + 1)
        : ''
      )
    );
  }
  elsif ($op eq 'AND' or $op eq 'OR' or $op =~ $binary_op_re ) {
    return join (" $op ", map $self->_unparse($_, $bindargs, $depth), @{$args});
  }
  elsif ($op eq '-LIST' ) {
    return join (', ', map $self->_unparse($_, $bindargs, $depth), @{$args});
  }
  elsif ($op eq '-MISC' ) {
    return join (' ', map $self->_unparse($_, $bindargs, $depth), @{$args});
  }
  elsif ($op =~ qr/^-(ASC|DESC)$/ ) {
    my $dir = $1;
    return join (' ', (map $self->_unparse($_, $bindargs, $depth), @{$args}), $dir);
  }
  else {
    my ($l, $r) = @{$self->pad_keyword($op, $depth)};

    my $rhs = $self->_unparse($args, $bindargs, $depth);

    return sprintf "$l%s$r", join(
      ( ref $args eq 'ARRAY' and @{$args} == 1 and $args->[0][0] eq '-PAREN' )
        ? ''    # mysql--
        : ' '
      ,
      $self->format_keyword($op),
      (length $rhs ? $rhs : () ),
    );
  }
}

# All of these keywords allow their parameters to be specified with or without parenthesis without changing the semantics
my @unrollable_ops = (
  'ON',
  'WHERE',
  'GROUP \s+ BY',
  'HAVING',
  'ORDER \s+ BY',
  'I?LIKE',
);
my $unrollable_ops_re = join ' | ', @unrollable_ops;
$unrollable_ops_re = qr/$unrollable_ops_re/xi;

sub _parenthesis_unroll {
  my $self = shift;
  my $ast = shift;

  return unless (ref $ast and ref $ast->[1]);

  my $changes;
  do {
    my @children;
    $changes = 0;

    for my $child (@{$ast->[1]}) {

      # the current node in this loop is *always* a PAREN
      if (! ref $child or ! @$child or $child->[0] ne '-PAREN') {
        push @children, $child;
        next;
      }

      my $parent_op = $ast->[0];

      # unroll nested parenthesis
      while ( $parent_op ne 'IN' and @{$child->[1]} == 1 and $child->[1][0][0] eq '-PAREN') {
        $child = $child->[1][0];
        $changes++;
      }

      # set to CHILD in the case of PARENT ( CHILD )
      # but NOT in the case of PARENT( CHILD1, CHILD2 )
      my $single_child_op = (@{$child->[1]} == 1) ? $child->[1][0][0] : '';

      my $child_op_argc = $single_child_op ? scalar @{$child->[1][0][1]} : undef;

      my $single_grandchild_op
        = ( $child_op_argc||0 == 1 and ref $child->[1][0][1][0] eq 'ARRAY' )
            ? $child->[1][0][1][0][0]
            : ''
      ;

      # if the parent operator explicitly allows it AND the child isn't a subselect
      # nuke the parenthesis
      if ($parent_op =~ $unrollable_ops_re and $single_child_op ne 'SELECT') {
        push @children, @{$child->[1]};
        $changes++;
      }

      # if the parenthesis are wrapped around an AND/OR matching the parent AND/OR - open the parenthesis up and merge the list
      elsif (
        $single_child_op eq $parent_op
          and
        ( $parent_op eq 'AND' or $parent_op eq 'OR')
      ) {
        push @children, @{$child->[1][0][1]};
        $changes++;
      }

      # only *ONE* LITERAL or placeholder element
      # as an AND/OR/NOT argument
      elsif (
        ( $single_child_op eq '-LITERAL' or $single_child_op eq '-PLACEHOLDER' )
          and
        ( $parent_op eq 'AND' or $parent_op eq 'OR' or $parent_op eq 'NOT' )
      ) {
        push @children, @{$child->[1]};
        $changes++;
      }

      # an AND/OR expression with only one binop in the parenthesis
      # with exactly two grandchildren
      # the only time when we can *not* unroll this is when both
      # the parent and the child are mathops (in which case we'll
      # break precedence) or when the child is BETWEEN (special
      # case)
      elsif (
        ($parent_op eq 'AND' or $parent_op eq 'OR')
          and
        $single_child_op =~ $binary_op_re
          and
        $single_child_op ne 'BETWEEN'
          and
        $child_op_argc == 2
          and
        ! (
          $single_child_op =~ $alphanum_cmp_op_re
            and
          $parent_op =~ $alphanum_cmp_op_re
        )
      ) {
        push @children, @{$child->[1]};
        $changes++;
      }

      # a function binds tighter than a mathop - see if our ancestor is a
      # mathop, and our content is:
      # a single non-mathop child with a single PAREN grandchild which
      # would indicate mathop ( nonmathop ( ... ) )
      # or a single non-mathop with a single LITERAL ( nonmathop foo )
      # or a single non-mathop with a single PLACEHOLDER ( nonmathop ? )
      elsif (
        $single_child_op
          and
        $parent_op =~ $alphanum_cmp_op_re
          and
        $single_child_op !~ $alphanum_cmp_op_re
          and
        $child_op_argc == 1
          and
        (
          $single_grandchild_op eq '-PAREN'
            or
          $single_grandchild_op eq '-LITERAL'
            or
          $single_grandchild_op eq '-PLACEHOLDER'
        )
      ) {
        push @children, @{$child->[1]};
        $changes++;
      }

      # a construct of ... ( somefunc ( ... ) ) ... can safely lose the outer parens
      # except for the case of ( NOT ( ... ) ) which has already been handled earlier
      # and except for the case of RNO, where the double are explicit syntax
      elsif (
        $parent_op ne 'ROW_NUMBER() OVER'
          and
        $single_child_op
          and
        $single_child_op ne 'NOT'
          and
        $child_op_argc == 1
          and
        $single_grandchild_op eq '-PAREN'
      ) {
        push @children, @{$child->[1]};
        $changes++;
      }


      # otherwise no more mucking for this pass
      else {
        push @children, $child;
      }
    }

    $ast->[1] = \@children;

  } while ($changes);
}

sub _strip_asc_from_order_by {
  my ($self, $ast) = @_;

  return $ast if (
    ref $ast ne 'ARRAY'
      or
    $ast->[0] ne 'ORDER BY'
  );


  my $to_replace;

  if (@{$ast->[1]} == 1 and $ast->[1][0][0] eq '-ASC') {
    $to_replace = [ $ast->[1][0] ];
  }
  elsif (@{$ast->[1]} == 1 and $ast->[1][0][0] eq '-LIST') {
    $to_replace = [ grep { $_->[0] eq '-ASC' } @{$ast->[1][0][1]} ];
  }

  @$_ = @{$_->[1][0]} for @$to_replace;

  $ast;
}

sub format { my $self = shift; $self->unparse($self->parse($_[0]), $_[1]) }

1;

=pod

=head1 NAME

SQL::Abstract::Tree - Represent SQL as an AST

=head1 SYNOPSIS

 my $sqla_tree = SQL::Abstract::Tree->new({ profile => 'console' });

 print $sqla_tree->format('SELECT * FROM foo WHERE foo.a > 2');

 # SELECT *
 #   FROM foo
 #   WHERE foo.a > 2

=head1 METHODS

=head2 new

 my $sqla_tree = SQL::Abstract::Tree->new({ profile => 'console' });

 $args = {
   profile => 'console',      # predefined profile to use (default: 'none')
   fill_in_placeholders => 1, # true for placeholder population
   placeholder_surround =>    # The strings that will be wrapped around
              [GREEN, RESET], # populated placeholders if the above is set
   indent_string => ' ',      # the string used when indenting
   indent_amount => 2,        # how many of above string to use for a single
                              # indent level
   newline       => "\n",     # string for newline
   colormap      => {
     select => [RED, RESET], # a pair of strings defining what to surround
                             # the keyword with for colorization
     # ...
   },
   indentmap     => {
     select        => 0,     # A zero means that the keyword will start on
                             # a new line
     from          => 1,     # Any other positive integer means that after
     on            => 2,     # said newline it will get that many indents
     # ...
   },
 }

Returns a new SQL::Abstract::Tree object.  All arguments are optional.

=head3 profiles

There are four predefined profiles, C<none>, C<console>, C<console_monochrome>,
and C<html>.  Typically a user will probably just use C<console> or
C<console_monochrome>, but if something about a profile bothers you, merely
use the profile and override the parts that you don't like.

=head2 format

 $sqlat->format('SELECT * FROM bar WHERE x = ?', [1])

Takes C<$sql> and C<\@bindargs>.

Returns a formatting string based on the string passed in

=head2 parse

 $sqlat->parse('SELECT * FROM bar WHERE x = ?')

Returns a "tree" representing passed in SQL.  Please do not depend on the
structure of the returned tree.  It may be stable at some point, but not yet.

=head2 unparse

 $sqlat->unparse($tree_structure, \@bindargs)

Transform "tree" into SQL, applying various transforms on the way.

=head2 format_keyword

 $sqlat->format_keyword('SELECT')

Currently this just takes a keyword and puts the C<colormap> stuff around it.
Later on it may do more and allow for coderef based transforms.

=head2 pad_keyword

 my ($before, $after) = @{$sqlat->pad_keyword('SELECT')};

Returns whitespace to be inserted around a keyword.

=head2 fill_in_placeholder

 my $value = $sqlat->fill_in_placeholder(\@bindargs)

Removes last arg from passed arrayref and returns it, surrounded with
the values in placeholder_surround, and then surrounded with single quotes.

=head2 indent

Returns as many indent strings as indent amounts times the first argument.

=head1 ACCESSORS

=head2 colormap

See L</new>

=head2 fill_in_placeholders

See L</new>

=head2 indent_amount

See L</new>

=head2 indent_string

See L</new>

=head2 indentmap

See L</new>

=head2 newline

See L</new>

=head2 placeholder_surround

See L</new>

