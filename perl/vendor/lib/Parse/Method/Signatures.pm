package Parse::Method::Signatures;

use Moose;
use MooseX::Types::Moose qw/
  ArrayRef HashRef ScalarRef CodeRef Int Str ClassName
/;

use Class::Load qw(load_class);
use PPI;
use Moose::Util::TypeConstraints;
use Parse::Method::Signatures::ParamCollection;
use Parse::Method::Signatures::Types qw/
  PositionalParam NamedParam UnpackedParam
/;

use Carp qw/croak/;

use namespace::clean -except => 'meta';
our $VERSION = '1.003019';
$VERSION = eval $VERSION;
our $ERROR_LEVEL = 0;
our %LEXTABLE;
our $DEBUG = $ENV{PMS_DEBUG} || 0;

# Setup what we need for specific PPI subclasses
@PPI::Token::EOF::ISA = 'PPI::Token';

class_type "PPI::Document";
class_type "PPI::Element";

has 'input' => (
    is       => 'ro',
    isa      => Str,
    required => 1
);

has 'offset' => (
    is      => 'rw',
    isa     => Int,
    default => 0,
);

has 'signature_class' => (
    is      => 'ro',
    isa     => Str,
    default => 'Parse::Method::Signatures::Sig',
);

has 'param_class' => (
    is      => 'ro',
    isa     => Str,
    default => 'Parse::Method::Signatures::Param',
);

has 'type_constraint_class' => (
    is      => 'ro',
    isa     => Str,
    default => 'Parse::Method::Signatures::TypeConstraint',
);

has 'type_constraint_callback' => (
    is        => 'ro',
    isa       => CodeRef,
    predicate => 'has_type_constraint_callback',
);

has 'from_namespace' => (
    is        => 'rw',
    isa       => ClassName,
    predicate => 'has_from_namespace'
);

has 'ppi_doc' => (
    is => 'ro',
    isa => 'PPI::Document',
    lazy_build => 1,
    builder => 'parse',
);

# A bit dirty, but we set this with local most of the time
has 'ppi' => (
    is => 'ro',
    isa => 'PPI::Element',
    lazy_build => 1,
    writer => '_set_ppi'
);

sub BUILD {
    my ($self) = @_;

    load_class($_)
        for map { $self->$_ } qw/
            signature_class
            param_class
            type_constraint_class
        /;

    my $ppi = $self->ppi;

    # Skip leading whitespace
    $self->consume_token
      unless $ppi->significant;
}

sub create_param {
    my ($self, $args) = @_;

    my @traits;
    push @traits, $args->{ variable_name } ? 'Bindable' : 'Placeholder'
        if !exists $args->{unpacking};
    push @traits, $args->{ named         } ? 'Named'    : 'Positional';
    push @traits, 'Unpacked::' . $args->{unpacking}
        if exists $args->{unpacking};

    return $self->param_class->new_with_traits(traits => \@traits, %{ $args });
}

override BUILDARGS => sub {
  my $class = shift;

  return { input => $_[0] } if @_ == 1 and !ref $_[0];

  return super();
};

sub parse {
  my ($self) = @_;
  
  my $input = substr($self->input, $self->offset);
  my $doc = PPI::Document->new(\$input);

  # Append the magic EOF Token
  $doc->add_element(PPI::Token::EOF->new(""));

  # Annoyingly "m($x)" gets treated as a regex operator. This isn't what we 
  # want. so replace it with a Word, then a list. The way we do this is by
  # taking the operator off the front, then reparsing the rest of the content
  # This will look the same (so wont affect anything in a code block) but is
  # just store different token wise.
  $self->_replace_regexps($doc);

  # ($, $x) parses the $, as a single var. not what we want. FIX UP
  # While we're att it lets fixup $: $? and $!
  $self->_replace_magic($doc);

  # (Str :$x) yields a label of "Str :"
  # (Foo Bar :$x) yields a label of "Bar :"
  $self->_replace_labels($doc);

  # This one is actually a bug in PPI, rather than just an oddity
  # (Str $x = 0xfF) parses as "Oxf" and a word of "F"
  $self->_fixup_hex($doc);

  return $doc;
}

sub _replace_regexps {
  my ($self, $doc) = @_;

  REGEXP:
  foreach my $node ( @{ $doc->find('Token::Regexp') || [] } ) {
    my $str = $node->content;

    next REGEXP unless defined $node->{operator};

    # Rather annoyingly, there are *no* methods on Token::Regexp;
    my ($word, $rest) = $str =~ /^(\Q@{[$node->{operator}]}\E)(.*)$/s;

    my $subdoc = PPI::Document->new(\$rest);
    my @to_add = reverse map { $_->remove } $subdoc->children;
    push @to_add, new PPI::Token::Word($word);
    # insert_after restricts what you can insert.
    # $node->insert_after($_) for @to_add;
    $node->__insert_after($_) for @to_add;

    $node->delete;
  }
}


sub _replace_magic {
  my ($self, $doc) = @_;

  foreach my $node ( @{ $doc->find('Token::Magic') || [] } ) {
    my ($op) = $node->content =~ /^\$([,?:!)])$/ or next;

    $node->insert_after(new PPI::Token::Operator($op));
    $node->insert_after(new PPI::Token::Cast('$'));
    $node->delete;
  }
}

sub _replace_labels {
  my ($self, $doc) = @_;

  foreach my $node ( @{ $doc->find('Token::Label') || [] } ) {
    my ($word, $ws) = $node->content =~ /^(.*?)(\s+)?:$/s or next;

    $node->insert_after(new PPI::Token::Operator(':'));
    $node->insert_after(new PPI::Token::Whitespace($ws)) if defined $ws;
    $node->insert_after(new PPI::Token::Word($word));
    $node->delete;
  }
}

sub _fixup_hex {
  my ($self, $doc) = @_;

  foreach my $node ( @{ $doc->find('Token::Number::Hex') || [] } ) {
    my $next = $node->next_token;
    next unless $next->isa('PPI::Token::Word') 
             && $next->content =~ /^[0-9a-f]+$/i;

    $node->add_content($next->content);
    $next->delete;
  }
}

sub _build_ppi {
  my ($self) = @_;
  my $ppi = $self->ppi_doc->first_token;

  if ($ppi->class eq 'PPI::Token::Word' && exists $LEXTABLE{"$ppi"}) {
    bless $ppi, "PPI::Token::LexSymbol";
    $ppi->{lex} = $LEXTABLE{"$ppi"};
  }
  return $ppi;
}

# signature: O_PAREN
#            invocant
#            params
#            C_PAREN
#
# invocant: param ':'
#
# params: param COMMA params
#       | param
#       | /* NUL */
sub signature {
  my $self = shift;

  $self = $self->new(@_) unless blessed($self);

  $self->assert_token('(');

  my $args = {};
  my $params = [];

  my $param = $self->param;

  if ($param && $self->ppi->content eq ':') {
    # That param was actually the invocant
    $args->{invocant} = $param;
    croak "Invocant cannot be named"
      if NamedParam->check($param);
    croak "Invocant cannot be optional"
      if !$param->required;
    croak "Invocant cannot have a default value"
      if $param->has_default_value;

    croak "Invocant must be a simple scalar"
      if UnpackedParam->check($param) || $param->sigil ne '$';

    $self->consume_token;
    $param = $self->param;

  }

  if ($param) {
    push @$params, $param;

    my $greedy = $param->sigil ne '$' ? $param : undef;
    my $opt_pos_param = !$param->required;

    while ($self->ppi->content eq ',') {
      $self->consume_token;

      my $err_ctx = $self->ppi;
      $param = $self->param;
      $self->error($err_ctx, "Parameter expected")
        if !$param;

      my $is_named = NamedParam->check($param);
      if (!$is_named) {
        if ($param->required && $opt_pos_param) {
          $self->error($err_ctx, "Invalid: Required positional param " .
            " found after optional one");
        }
        if ($greedy) {
          croak "Invalid: Un-named parameter '" . $param->variable_name
            . "' after greedy '" 
            . $greedy->variable_name . "'\n";
        }
      }

      push @$params, $param;
      $opt_pos_param = $opt_pos_param || !$param->required;
      $greedy = $param->sigil ne '$' ? $param : undef;
    }
  }

  $self->assert_token(')');
  $args->{params} = $params;

  my $sig = $self->signature_class->new($args);

  return $sig;
}


# param: tc?
#        var
#        (OPTIONAL|REQUIRED)?
#        default?
#        where*
#        trait*
#
# where: WHERE <code block>
#
# trait: TRAIT class
#
# var : COLON label '(' var_or_unpack ')' # label is classish, with only /a-z0-9_/i allowed
#     | COLON VAR
#     | var_or_unpack
#
# var_or_unpack : '[' param* ']' # should all be required + un-named
#               | '{' param* '}' # Should all be named
#               | VAR
#
# OPTIONAL: '?'
# REQUIRED: '!'
sub param {
  my $self = shift;
  my $class_meth;
  unless (blessed($self)) {
    $self = $self->new(@_) unless blessed($self);
    $class_meth = 1;
  }

  # Also used to check if a anything has been consumed
  my $err_ctx = $self->ppi;

  my $param = {
    required => 1,
  };

  $self->_param_typed($param);

  $self->_param_opt_or_req(
    $self->_param_labeled($param)
      || $self->_param_named($param)
      || $self->_param_variable($param)
      || $self->_unpacked_param($param)
  ) or ($err_ctx == $self->ppi and return)
    or $self->error($err_ctx);

  $self->_param_default($param);
  $self->_param_constraint_or_traits($param);

  $param = $self->create_param($param);

  return !$class_meth
      ? $param
      : wantarray
      ? ($param, $self->remaining_input)
      : $param;
}

sub _param_opt_or_req {
  my ($self, $param) = @_;

  return unless $param;

  if ($self->ppi->class eq 'PPI::Token::Operator') {
    my $c = $self->ppi->content;
    if ($c eq '?') {
      $param->{required} = 0;
      $self->consume_token;
    } elsif ($c eq '!') {
      $param->{required} = 1;
      $self->consume_token;
    }
  }
  return $param;

}

sub _param_constraint_or_traits {
  my ($self, $param) = @_;

  while ($self->_param_where($param) ||
         $self->_param_traits($param) ) {
    # No op;

  }
  return $param;
}

sub _param_where {
  my ($self, $param) = @_;

  return unless $self->ppi->isa('PPI::Token::LexSymbol')
             && $self->ppi->lex eq 'WHERE';

  $self->consume_token;

  $param->{constraints} ||= [];

  my $ppi = $self->ppi;

  $self->error($ppi, "Block expected after where")
    unless $ppi->class eq 'PPI::Token::Structure'
        && $ppi->content eq '{';

  # Go from token to block
  $ppi = $ppi->parent;

  $ppi->finish or $self->error($ppi, 
    "Runaway '" . $ppi->braces . "' in " . $self->_parsing_area(1), 1);

  push @{$param->{constraints}}, $ppi->content;

  $self->_set_ppi($ppi->finish);
  $self->consume_token;
  return $param;
}

sub _param_traits {
  my ($self, $param) = @_;
  return unless $self->ppi->isa('PPI::Token::LexSymbol')
             && $self->ppi->lex eq 'TRAIT';

  my $op = $self->consume_token->content;

  $self->error($self->ppi, "Error parsing parameter trait")
    unless $self->ppi->isa('PPI::Token::Word');

  $param->{param_traits} ||= [];

  push @{$param->{param_traits}}, [$op, $self->consume_token->content];
  return $param;
}

sub _param_labeled {
  my ($self, $param) = @_;

  return unless 
    $self->ppi->content eq ':' &&
    $self->ppi->next_token->isa('PPI::Token::Word');

  $self->consume_token;

  $self->error($self->ppi, "Invalid label")
    if $self->ppi->content =~ /[^-\w]/;

  $param->{named} = 1;
  $param->{required} = 0;
  $param->{label} = $self->consume_token->content;

  $self->assert_token('(');
  $self->_unpacked_param($param) 
    || $self->_param_variable($param)
    || $self->error($self->ppi);

  $self->assert_token(')');

  return $param;
}

sub _unpacked_param {
  my ($self, $param) = @_;

  return $self->bracketed('[', \&unpacked_array, $param) ||
         $self->bracketed('{', \&unpacked_hash, $param);
}

sub _param_named {
  my ($self, $param) = @_;

  return unless
    $self->ppi->content eq ':' &&
    $self->ppi->next_token->isa('PPI::Token::Symbol');

  $param->{required} = 0;
  $param->{named} = 1;
  $self->consume_token;

  my $err_ctx = $self->ppi;
  $param = $self->_param_variable($param);

  $self->error($err_ctx, "Arrays or hashes cannot be named")
    if $param->{sigil} ne '$';

  return $param;
}

sub _param_typed {
  my ($self, $param) = @_;

  my $tc = $self->tc
    or return;


  $tc = $self->type_constraint_class->new(
    ppi  => $tc,
    ( $self->has_type_constraint_callback
      ? (tc_callback => $self->type_constraint_callback)
      : ()
    ),
    ( $self->has_from_namespace
      ? ( from_namespace => $self->from_namespace )
      : ()
    ),
  );
  $param->{type_constraints} = $tc;

  return $param;
}
 
sub _param_default {
  my ($self, $param) = @_;

  return unless $self->ppi->content eq '=';

  $self->consume_token;

  $param->{default_value} =
    $self->_consume_if_isa(qw/
      PPI::Token::QuoteLike
      PPI::Token::Number
      PPI::Token::Quote
      PPI::Token::Symbol
      PPI::Token::Magic
      PPI::Token::ArrayIndex
    /) ||
    $self->bracketed('[') ||
    $self->bracketed('{') 
  or $self->error($self->ppi);
    
  $param->{default_value} = $param->{default_value}->content;
}


sub _param_variable {
  my ($self, $param) = @_;

  my $ppi = $self->ppi;
  my $class = $ppi->class;
  return unless $class eq 'PPI::Token::Symbol'
             || $class eq 'PPI::Token::Cast';

  if ($class eq 'PPI::Token::Symbol') {
    $ppi->symbol_type eq $ppi->raw_type or $self->error($ppi);

    $param->{sigil} = $ppi->raw_type;
    $param->{variable_name} = $self->consume_token->content;
  } else {
    $param->{sigil} = $self->consume_token->content;
  }

  return $param;
}

sub unpacked_hash {
  my ($self, $list, $param) = @_;

  my $params = [];
  while ($self->ppi->content ne '}') {
    my $errctx = $self->ppi;
    my $p = $self->param
      or $self->error($self->ppi);

    $self->error($errctx, "Cannot have positional parameters in an unpacked-array")
      if $p->sigil eq '$' && PositionalParam->check($p);
    push @$params, $p;

    last if $self->ppi->content eq '}';
    $self->assert_token(',');
  }
  $param->{params} = $params;
  $param->{sigil} = '$';
  $param->{unpacking} = 'Hash';
  return $param;
}

sub unpacked_array {
  my ($self, $list, $param) = @_;

  my $params = [];
  while ($self->ppi->content ne ']') {
    my $watermark = $self->ppi;
    my $param = $self->param
      or $self->error($self->ppi);

    $self->error($watermark, "Cannot have named parameters in an unpacked-array")
      if NamedParam->check($param);

    $self->error($watermark, "Cannot have optional parameters in an unpacked-array")
      unless $param->required;

    push @$params, $param;

    last if $self->ppi->content eq ']';
    $self->assert_token(',');
  }
  $param->{params} = $params;
  $param->{sigil} = '$';
  $param->{unpacking} = 'Array';
  return $param;
}

sub tc {
  my ($self, $required) = @_;

  my $ident = $self->_ident;

  $ident or ($required and $self->error($self->ppi)) or return;

  return $self->_tc_union(
    $self->bracketed('[', \&_tc_params, $ident)
      || $ident->clone
  );
}

# Handle parameterized TCs. e.g.:
# ArrayRef[Str]
# Dict[Str => Str]
# Dict["foo bar", Baz]
sub _tc_params {
  my ($self, $list, $tc) = @_;

  my $new = PPI::Statement::Expression::TCParams->new($tc->clone);

  return $new if $self->ppi->content eq ']';

  $new->add_element($self->_tc_param);

  while ($self->ppi->content =~ /^,|=>$/ ) {

    my $op = $self->consume_token;
    $self->_stringify_last($new) if $op->content eq '=>';

    $new->add_element($self->tc(1));
  }

  return $new;
}

# Valid token for individual component of parameterized TC
sub _tc_param {
  my ($self) = @_;

  (my $class = $self->ppi->class) =~ s/^PPI:://;
  return $self->consume_token->clone
      if $class eq 'Token::Number' ||
         $class =~ /^Token::Quote::(?:Single|Double|Literal|Interpolate)/;

  return $self->tc(1);
}

sub _tc_union {
  my ($self, $tc) = @_;
  
  return $tc unless $self->ppi->content eq '|';

  my $union = PPI::Statement::Expression::TCUnion->new;
  $union->add_element($tc);
  while ( $self->ppi->content eq '|' ) {
   
    $self->consume_token;
    $union->add_element($self->tc(1));
  }

  return $union;
}

# Stringify LHS of fat comma
sub _stringify_last {
  my ($self, $list) = @_;
  my $last = $list->last_token;
  return unless $last->isa('PPI::Token::Word');

  # Is this conditional on the content of the word?
  bless $last, "PPI::Token::StringifiedWord";
  return $list;
}

# Handle the boring bits of bracketed product, then call $code->($self, ...) 
sub bracketed {
  my ($self, $type, $code, @args) = @_;

  local $ERROR_LEVEL = $ERROR_LEVEL + 1;
  my $ppi = $self->ppi;
  return unless $ppi->content eq $type;

  $self->consume_token; # consume '[';

  # Get from the '[' token the to Strucure::Constructor 
  $ppi = $ppi->parent;

  $ppi->finish or $self->error($ppi, 
    "Runaway '" . $ppi->braces . "' in " . $self->_parsing_area(1), 1);


  my $ret;
  if ($code) {
    my $list = PPI::Structure::Constructor->new($ppi->start->clone);
    $ret = $code->($self, $list, @args);

    $self->error($self->ppi)
      if $self->ppi != $ppi->finish;

    # There is no public way to do this as of PPI 1.204_06. I'll add one to the
    # next release, 1.205 (or so)
    $list->{finish} = $self->consume_token->clone;
  } else {
    # Just clone the entire [] or {}
    $ret = $ppi->clone;
    $self->_set_ppi($ppi->finish);
    $self->consume_token;
  }

  return $ret;
}

# Work out what sort of production we are in for sane default error messages
sub _parsing_area { 
  shift;
  my $height = shift || 0;
  my (undef, undef, undef, $sub) = caller($height+$ERROR_LEVEL);

  return "type constraint" if $sub =~ /(?:\b|_)tc(?:\b|_)/;
  return "unpacked parameter"      
                           if $sub =~ /(?:\b|_)unpacked(?:\b|_)/;
  return "parameter"       if $sub =~ /(?:\b|_)param(?:\b|_)/;
  return "signature"       if $sub =~ /(?:\b|_)signature(?:\b|_)/;

  " unknown production ($sub)";
}

# error(PPI::Token $token, Str $msg?, Bool $no_in = 0)
sub error {
  my ($self, $token, $msg, $no_in) = @_;

  $msg = "Error parsing " . $self->_parsing_area(2)
    unless ($msg);


  $msg = $msg . " near '$token'" . 
        ($no_in ? ""
                : " in '" . $token->statement . "'" 
        );

  if ($DEBUG) {
    Carp::confess($msg);
  } else {
    Carp::croak($msg);
  }
}

sub assert_token {
  my ($self, $need, $msg) = @_;

  if ($self->ppi->content ne $need) {
    $self->error($self->ppi, "'$need' expected whilst parsing " . $self->_parsing_area(2));
  }
  return $self->consume_token;
}


%LEXTABLE = (
  where => 'WHERE',
  is    => 'TRAIT',
  does  => 'TRAIT',
);

sub _ident {
  my ($self) = @_;

  my $ppi = $self->ppi;
  return $self->consume_token
    if $ppi->class eq 'PPI::Token::Word';
  return undef;
}

sub _consume_if_isa {
  my ($self, @classes) = @_;

  for (@classes) {
    return $self->consume_token
      if $self->ppi->isa($_);
  }

}

sub consume_token {
  my ($self) = @_;

  my $ppi = $self->ppi;
  my $ret = $ppi;

  while (!$ppi->isa('PPI::Token::EOF') ) {
    $ppi = $ppi->next_token;
    last if $ppi->significant;
  }

  if ($ppi->class eq 'PPI::Token::Word' && exists $LEXTABLE{"$ppi"}) {
    bless $ppi, "PPI::Token::LexSymbol";
    $ppi->{lex} = $LEXTABLE{"$ppi"};
  }
  $self->_set_ppi( $ppi );
  return $ret;
}

sub remaining_input {
  my $tok = $_[0]->ppi;
  my $buff;

  while ( !$tok->isa('PPI::Token::EOF') ) {
    $buff .= $tok->content;
    $tok = $tok->next_token;
  }
  return $buff;
}

__PACKAGE__->meta->make_immutable;


# Extra PPI classes to represent what we want.
{ package 
    PPI::Statement::Expression::TCUnion;
  use base 'PPI::Statement::Expression';

  sub content {
    join('|', $_[0]->children );
  }
}

{ package 
    PPI::Statement::Expression::TCParams;
    
  use base 'PPI::Statement::Expression';
  use Moose;

  # $self->children stores everything so PPI can track parents
  # params just contains the keywords (not commas) inside the []
  has type => ( is => 'ro');
  has params => ( 
    is => 'ro',
    default => sub { [] },
  );

  sub new {
    my ($class, $type) = @_;

    return $class->meta->new_object(
      __INSTANCE__ => $class->SUPER::new($type),
      type => $type
    );
  };

  override add_element => sub {
    my ($self, $ele) = @_;
    super();
    push @{$self->params}, $ele;
  };

  sub content { 
    $_[0]->type->content . '[' . join(',', @{$_[0]->params}) . ']'
  }

  no Moose;
}

{ package 
    PPI::Token::LexSymbol;
  use base 'PPI::Token::Word';

  sub lex {
    my ($self) = @_;
    return $self->{lex}
  }
}

# Used for LHS of fat comma
{ package
    PPI::Token::StringifiedWord;
  use base 'PPI::Token::Word'; 

  use Moose;
  override content => sub {
    return '"' . super() . '"';
  };

  sub string {
    return $_[0]->PPI::Token::Word::content();
  }
  no Moose;
}

1;

__END__

=head1 NAME

Parse::Method::Signatures - Perl6 like method signature parser

=head1 DESCRIPTION

Inspired by L<Perl6::Signature> but streamlined to just support the subset
deemed useful for L<TryCatch> and L<MooseX::Method::Signatures>.

=head1 TODO

=over

=item * Document the parameter return types.

=item * Probably lots of other things

=back

=head1 METHODS

There are only two public methods to this module, both of which should be
called as class methods. Both methods accept  either a single (non-ref) scalar
as the value for the L</input> attribute, or normal new style arguments (hash
or hash-ref).

=head2 signature

 my $sig = Parse::Method::Signatures->signature( '(Str $foo)' )

Attempts to parse the (bracketed) method signature. Returns a value or croaks
on error.

=head2 param

  my $param = Parse::Method::Signatures->param( 'Str $foo where { length($_) < 10 }')

Attempts to parse the specification for a single parameter. Returns value or
croaks on error.

=head1 ATTRIBUTES

All the attributes on this class are read-only.

=head2 input

B<Type:> Str

The string to parse.

=head2 offset

B<Type:> Int

Offset into L</input> at which to start parsing. Useful for using with
Devel::Declare linestring

=head2 signature_class

B<Default:> Parse::Method::Signatures::Sig

B<Type:> Str (loaded on demand class name)

=head2 param_class

B<Default:> Parse::Method::Signatures::Param

B<Type:> Str (loaded on demand class name)

=head2 type_constraint_class

B<Default:> L<Parse::Method::Signatures::TypeConstraint>

B<Type:> Str (loaded on demand class name)

Class that is used to turn the parsed type constraint into an actual
L<Moose::Meta::TypeConstraint> object.

=head2 from_namespace

B<Type:> ClassName

Let this module know which package it is parsing signatures form. This is
entirely optional, and the only effect is has is on parsing type constraints.

If this attribute is set it is passed to L</type_constraint_class> which can
use it to introspect the package (commonly for L<MooseX::Types> exported
types). See
L<Parse::Method::Signature::TypeConstraints/find_registered_constraint> for
more details.

=head2 type_constraint_callback

B<Type:> CodeRef

Passed to the constructor of L</type_constraint_class>. Default implementation
of this callback asks Moose for a type constrain matching the name passed in.
If you have more complex requirements, such as parsing types created by
L<MooseX::Types> then you will want a callback similar to this:

 # my $target_package defined elsewhere.
 my $tc_cb = sub {
   my ($pms_tc, $name) = @_;
   my $code = $target_package->can($name);
   $code ? eval { $code->() } 
         : $pms_tc->find_registered_constraint($name);
 }

Note that the above example is better provided by providing the
L</from_namespace> attribute.

=head1 CAVEATS

Like Perl6::Signature, the parsing of certain constructs is currently only a
'best effort' - specifically default values and where code blocks might not
successfully for certain complex cases. Patches/Failing tests welcome.

Additionally, default value specifications are not evaluated which means that
no such lexical or similar errors will not be produced by this module.
Constant folding will also not be performed.

There are certain constructs that are simply too much hassle to avoid when the
work around is simple. Currently the only cases that are known to parse wrong
are when using anonymous variables (i.e. just sigils) in unpacked arrays. Take
the following example:

 method foo (ArrayRef [$, $], $some_value_we_care_about) {

In this case the C<$]> is treated as one of perl's magic variables
(specifically, the patch level of the Perl interpreter) rather than a C<$>
followed by a C<]> as was almost certainly intended. The work around for this
is simple: introduce a space between the characters:

 method foo (ArrayRef [ $, $ ], $some_value_we_care_about) {

The same applies

=head1 AUTHOR

Ash Berlin <ash@cpan.org>.

Thanks to Florian Ragwitz <rafl@debian.org>.

Many thanks to Piers Cawley to showing me the way to refactor my spaghetti
code into something more manageable.

=head1 SEE ALSO

L<Devel::Declare> which is used by most modules that use this (currently by
all modules known to the author.)

L<http://github.com/ashb/trycatch/tree>.

=head1 LICENSE

Licensed under the same terms as Perl itself.

This distribution copyright 2008-2009, Ash Berlin <ash@cpan.org>

