# GENERATE RECURSIVE DESCENT PARSER OBJECTS FROM A GRAMMAR

use 5.006;
use strict;

package Parse::RecDescent;

use Text::Balanced qw ( extract_codeblock extract_bracketed extract_quotelike extract_delimited );

use vars qw ( $skip );

   *defskip  = \ '\s*'; # DEFAULT SEPARATOR IS OPTIONAL WHITESPACE
   $skip  = '\s*';      # UNIVERSAL SEPARATOR IS OPTIONAL WHITESPACE
my $MAXREP  = 100_000_000;  # REPETITIONS MATCH AT MOST 100,000,000 TIMES


#ifndef RUNTIME
sub import  # IMPLEMENT PRECOMPILER BEHAVIOUR UNDER:
        #    perl -MParse::RecDescent - <grammarfile> <classname> [runtimeclassname]
{
    local *_die = sub { print @_, "\n"; exit };

    my ($package, $file, $line) = caller;

    if ($file eq '-' && $line == 0)
    {
        _die("Usage: perl -MLocalTest - <grammarfile> <classname>")
            unless @ARGV >= 2 and $ARGV <= 3;

        my ($sourcefile, $class, $runtime_class) = @ARGV;

        local *IN;
        open IN, $sourcefile
            or _die(qq{Can't open grammar file "$sourcefile"});
        local $/; #
        my $grammar = <IN>;
        close IN;

        Parse::RecDescent->Precompile({ -runtime_class => $runtime_class },
                                      $grammar, $class, $sourcefile);
        exit;
    }
}

sub Save
{
    my $self = shift;
    my %opt;
    if ('HASH' eq ref $_[0]) {
        %opt = (%opt, %{$_[0]});
        shift;
    }
    my ($class) = @_;
    $self->{saving} = 1;
    $self->Precompile(undef,$class);
    $self->{saving} = 0;
}

sub PrecompiledRuntime
{
    my ($self, $class) = @_;
    my $opt = {
        -standalone => 1,
        -runtime_class => $class,
    };
    $self->Precompile($opt, '', $class);
}

sub Precompile
{
    my $self = shift;
    my %opt = ( -standalone => 0,
            );
    if ('HASH' eq ref $_[0]) {
        %opt = (%opt, %{$_[0]});
        shift;
    }
    my ($grammar, $class, $sourcefile) = @_;

    $class =~ /^(\w+::)*\w+$/ or croak("Bad class name: $class");

    my $modulefile = $class;
    $modulefile =~ s/.*:://;
    $modulefile .= ".pm";

    my $code = '';

    local *OUT;
    open OUT, ">", $modulefile
      or croak("Can't write to new module file '$modulefile'");

    print OUT "#\n",
      "# This parser was generated with\n",
      "# Parse::RecDescent version $Parse::RecDescent::VERSION\n",
      "#\n\n";

    print STDERR "precompiling grammar from file '$sourcefile'\n",
      "to class $class in module file '$modulefile'\n"
      if $grammar && $sourcefile;

    if ($grammar) {
        $self = Parse::RecDescent->new($grammar,  # $grammar
                                       1,         # $compiling
                                       $class     # $namespace
                                 )
          || croak("Can't compile bad grammar")
          if $grammar;

        # Do not allow &DESTROY to remove the precompiled namespace
        delete $self->{_not_precompiled};

        foreach ( keys %{$self->{rules}} ) {
            $self->{rules}{$_}{changed} = 1;
        }

        $code = $self->_code();
    }

    # If a name for the runtime package was not provided,
    # generate one based on the module output name and the generated
    # code
    if (not defined($opt{-runtime_class})) {
        if ($opt{-standalone}) {
            my $basename = $class . '::_Runtime';

            my $name = $basename;

            for (my $i = 0; $code =~ /$basename/; ++$i) {
                $name = sprintf("%s%06d", $basename, $i);
            }

            $opt{-runtime_class} = $name;
        } else {
            my $package = ref $self;
            local $::RD_HINT = defined $::RD_HINT ? $::RD_HINT : 1;
            _hint(<<EOWARNING);
The precompiled grammar did not specify the -runtime_class
option. The resulting parser will "use $package". Future changes to
$package may cause $class to stop working.

Consider building a -standalone parser, or providing the
-runtime_class option as described in Parse::RecDescent's POD.

Use \$::RD_HINT = 0 to disable this message.
EOWARNING
            $opt{-runtime_class} = $package;
        }
    }

    $code =~ s/Parse::RecDescent/$opt{-runtime_class}/gs;

    # Make the resulting pre-compiled parser stand-alone by including
    # the contents of Parse::RecDescent as -runtime_class in the
    # resulting precompiled parser.
    if ($opt{-standalone}) {
        local *IN;
        open IN, '<', $Parse::RecDescent::_FILENAME
          or croak("Can't open $Parse::RecDescent::_FILENAME for standalone pre-compilation: $!\n");
        my $exclude = 0;
        print OUT "{\n";
        while (<IN>) {
            if ($_ =~ /^\s*#\s*ifndef\s+RUNTIME\s*$/) {
                ++$exclude;
            }
            if ($exclude) {
                if ($_ =~ /^\s*#\s*endif\s$/) {
                    --$exclude;
                }
            } else {
                if ($_ =~ m/^__END__/) {
                    last;
                }

                # Standalone parsers shouldn't trigger the CPAN
                # indexer to index the runtime, as it shouldn't be
                # exposed as a user-consumable package.
                #
                # Trick the indexer by including a newline in the package declarations
                s/^package /package # this should not be indexed by CPAN\n/gs;
                s/Parse::RecDescent/$opt{-runtime_class}/gs;
                print OUT $_;
            }
        }
        close IN;
        print OUT "}\n";
    }

    if ($grammar) {
        print OUT "package $class;\n";
    }

    if (not $opt{-standalone}) {
        print OUT "use $opt{-runtime_class};\n";
    }

    if ($grammar) {
        print OUT "{ my \$ERRORS;\n\n";

        print OUT $code;

        print OUT "}\npackage $class; sub new { ";
        print OUT "my ";

        $code = $self->_dump([$self], [qw(self)]);
        $code =~ s/Parse::RecDescent/$opt{-runtime_class}/gs;

        print OUT $code;

        print OUT "}";
    }

    close OUT
      or croak("Can't write to new module file '$modulefile'");
}
#endif

package Parse::RecDescent::LineCounter;


sub TIESCALAR   # ($classname, \$text, $thisparser, $prevflag)
{
    bless {
        text    => $_[1],
        parser  => $_[2],
        prev    => $_[3]?1:0,
          }, $_[0];
}

sub FETCH
{
    my $parser = $_[0]->{parser};
    my $cache = $parser->{linecounter_cache};
    my $from = $parser->{fulltextlen}-length(${$_[0]->{text}})-$_[0]->{prev}
;

    unless (exists $cache->{$from})
    {
        $parser->{lastlinenum} = $parser->{offsetlinenum}
          - Parse::RecDescent::_linecount(substr($parser->{fulltext},$from))
          + 1;
        $cache->{$from} = $parser->{lastlinenum};
    }
    return $cache->{$from};
}

sub STORE
{
    my $parser = $_[0]->{parser};
    $parser->{offsetlinenum} -= $parser->{lastlinenum} - $_[1];
    return undef;
}

sub resync   # ($linecounter)
{
    my $self = tied($_[0]);
    die "Tried to alter something other than a LineCounter\n"
        unless $self =~ /Parse::RecDescent::LineCounter/;

    my $parser = $self->{parser};
    my $apparently = $parser->{offsetlinenum}
             - Parse::RecDescent::_linecount(${$self->{text}})
             + 1;

    $parser->{offsetlinenum} += $parser->{lastlinenum} - $apparently;
    return 1;
}

package Parse::RecDescent::ColCounter;

sub TIESCALAR   # ($classname, \$text, $thisparser, $prevflag)
{
    bless {
        text    => $_[1],
        parser  => $_[2],
        prev    => $_[3]?1:0,
          }, $_[0];
}

sub FETCH
{
    my $parser = $_[0]->{parser};
    my $missing = $parser->{fulltextlen}-length(${$_[0]->{text}})-$_[0]->{prev}+1;
    substr($parser->{fulltext},0,$missing) =~ m/^(.*)\Z/m;
    return length($1);
}

sub STORE
{
    die "Can't set column number via \$thiscolumn\n";
}


package Parse::RecDescent::OffsetCounter;

sub TIESCALAR   # ($classname, \$text, $thisparser, $prev)
{
    bless {
        text    => $_[1],
        parser  => $_[2],
        prev    => $_[3]?-1:0,
          }, $_[0];
}

sub FETCH
{
    my $parser = $_[0]->{parser};
    return $parser->{fulltextlen}-length(${$_[0]->{text}})+$_[0]->{prev};
}

sub STORE
{
    die "Can't set current offset via \$thisoffset or \$prevoffset\n";
}



package Parse::RecDescent::Rule;

sub new ($$$$$)
{
    my $class = ref($_[0]) || $_[0];
    my $name  = $_[1];
    my $owner = $_[2];
    my $line  = $_[3];
    my $replace = $_[4];

    if (defined $owner->{"rules"}{$name})
    {
        my $self = $owner->{"rules"}{$name};
        if ($replace && !$self->{"changed"})
        {
            $self->reset;
        }
        return $self;
    }
    else
    {
        return $owner->{"rules"}{$name} =
            bless
            {
                "name"     => $name,
                "prods"    => [],
                "calls"    => [],
                "changed"  => 0,
                "line"     => $line,
                "impcount" => 0,
                "opcount"  => 0,
                "vars"     => "",
            }, $class;
    }
}

sub reset($)
{
    @{$_[0]->{"prods"}} = ();
    @{$_[0]->{"calls"}} = ();
    $_[0]->{"changed"}  = 0;
    $_[0]->{"impcount"}  = 0;
    $_[0]->{"opcount"}  = 0;
    $_[0]->{"vars"}  = "";
}

sub DESTROY {}

sub hasleftmost($$)
{
    my ($self, $ref) = @_;

    my $prod;
    foreach $prod ( @{$self->{"prods"}} )
    {
        return 1 if $prod->hasleftmost($ref);
    }

    return 0;
}

sub leftmostsubrules($)
{
    my $self = shift;
    my @subrules = ();

    my $prod;
    foreach $prod ( @{$self->{"prods"}} )
    {
        push @subrules, $prod->leftmostsubrule();
    }

    return @subrules;
}

sub expected($)
{
    my $self = shift;
    my @expected = ();

    my $prod;
    foreach $prod ( @{$self->{"prods"}} )
    {
        my $next = $prod->expected();
        unless (! $next or _contains($next,@expected) )
        {
            push @expected, $next;
        }
    }

    return join ', or ', @expected;
}

sub _contains($@)
{
    my $target = shift;
    my $item;
    foreach $item ( @_ ) { return 1 if $target eq $item; }
    return 0;
}

sub addcall($$)
{
    my ( $self, $subrule ) = @_;
    unless ( _contains($subrule, @{$self->{"calls"}}) )
    {
        push @{$self->{"calls"}}, $subrule;
    }
}

sub addprod($$)
{
    my ( $self, $prod ) = @_;
    push @{$self->{"prods"}}, $prod;
    $self->{"changed"} = 1;
    $self->{"impcount"} = 0;
    $self->{"opcount"} = 0;
    $prod->{"number"} = $#{$self->{"prods"}};
    return $prod;
}

sub addvar
{
    my ( $self, $var, $parser ) = @_;
    if ($var =~ /\A\s*local\s+([%@\$]\w+)/)
    {
        $parser->{localvars} .= " $1";
        $self->{"vars"} .= "$var;\n" }
    else
        { $self->{"vars"} .= "my $var;\n" }
    $self->{"changed"} = 1;
    return 1;
}

sub addautoscore
{
    my ( $self, $code ) = @_;
    $self->{"autoscore"} = $code;
    $self->{"changed"} = 1;
    return 1;
}

sub nextoperator($)
{
    my $self = shift;
    my $prodcount = scalar @{$self->{"prods"}};
    my $opcount = ++$self->{"opcount"};
    return "_operator_${opcount}_of_production_${prodcount}_of_rule_$self->{name}";
}

sub nextimplicit($)
{
    my $self = shift;
    my $prodcount = scalar @{$self->{"prods"}};
    my $impcount = ++$self->{"impcount"};
    return "_alternation_${impcount}_of_production_${prodcount}_of_rule_$self->{name}";
}


sub code
{
    my ($self, $namespace, $parser, $check) = @_;

eval 'undef &' . $namespace . '::' . $self->{"name"} unless $parser->{saving};

    my $code =
'
# ARGS ARE: ($parser, $text; $repeating, $_noactions, \@args, $_itempos)
sub ' . $namespace . '::' . $self->{"name"} .  '
{
	my $thisparser = $_[0];
	use vars q{$tracelevel};
	local $tracelevel = ($tracelevel||0)+1;
	$ERRORS = 0;
    my $thisrule = $thisparser->{"rules"}{"' . $self->{"name"} . '"};

    Parse::RecDescent::_trace(q{Trying rule: [' . $self->{"name"} . ']},
                  Parse::RecDescent::_tracefirst($_[1]),
                  q{' . $self->{"name"} . '},
                  $tracelevel)
                    if defined $::RD_TRACE;

    ' . ($parser->{deferrable}
        ? 'my $def_at = @{$thisparser->{deferred}};'
        : '') .
    '
    my $err_at = @{$thisparser->{errors}};

    my $score;
    my $score_return;
    my $_tok;
    my $return = undef;
    my $_matched=0;
    my $commit=0;
    my @item = ();
    my %item = ();
    my $repeating =  $_[2];
    my $_noactions = $_[3];
    my @arg =    defined $_[4] ? @{ &{$_[4]} } : ();
    my $_itempos = $_[5];
    my %arg =    ($#arg & 01) ? @arg : (@arg, undef);
    my $text;
    my $lastsep;
    my $current_match;
    my $expectation = new Parse::RecDescent::Expectation(q{' . $self->expected() . '});
    $expectation->at($_[1]);
    '. ($parser->{_check}{thisoffset}?'
    my $thisoffset;
    tie $thisoffset, q{Parse::RecDescent::OffsetCounter}, \$text, $thisparser;
    ':'') . ($parser->{_check}{prevoffset}?'
    my $prevoffset;
    tie $prevoffset, q{Parse::RecDescent::OffsetCounter}, \$text, $thisparser, 1;
    ':'') . ($parser->{_check}{thiscolumn}?'
    my $thiscolumn;
    tie $thiscolumn, q{Parse::RecDescent::ColCounter}, \$text, $thisparser;
    ':'') . ($parser->{_check}{prevcolumn}?'
    my $prevcolumn;
    tie $prevcolumn, q{Parse::RecDescent::ColCounter}, \$text, $thisparser, 1;
    ':'') . ($parser->{_check}{prevline}?'
    my $prevline;
    tie $prevline, q{Parse::RecDescent::LineCounter}, \$text, $thisparser, 1;
    ':'') . '
    my $thisline;
    tie $thisline, q{Parse::RecDescent::LineCounter}, \$text, $thisparser;

    '. $self->{vars} .'
';

    my $prod;
    foreach $prod ( @{$self->{"prods"}} )
    {
        $prod->addscore($self->{autoscore},0,0) if $self->{autoscore};
        next unless $prod->checkleftmost();
        $code .= $prod->code($namespace,$self,$parser);

        $code .= $parser->{deferrable}
                ? '     splice
                @{$thisparser->{deferred}}, $def_at unless $_matched;
                  '
                : '';
    }

    $code .=
'
    unless ( $_matched || defined($score) )
    {
        ' .($parser->{deferrable}
            ? '     splice @{$thisparser->{deferred}}, $def_at;
              '
            : '') . '

        $_[1] = $text;  # NOT SURE THIS IS NEEDED
        Parse::RecDescent::_trace(q{<<'.Parse::RecDescent::_matchtracemessage($self,1).' rule>>},
                     Parse::RecDescent::_tracefirst($_[1]),
                     q{' . $self->{"name"} .'},
                     $tracelevel)
                    if defined $::RD_TRACE;
        return undef;
    }
    if (!defined($return) && defined($score))
    {
        Parse::RecDescent::_trace(q{>>Accepted scored production<<}, "",
                      q{' . $self->{"name"} .'},
                      $tracelevel)
                        if defined $::RD_TRACE;
        $return = $score_return;
    }
    splice @{$thisparser->{errors}}, $err_at;
    $return = $item[$#item] unless defined $return;
    if (defined $::RD_TRACE)
    {
        Parse::RecDescent::_trace(q{>>'.Parse::RecDescent::_matchtracemessage($self).' rule<< (return value: [} .
                      $return . q{])}, "",
                      q{' . $self->{"name"} .'},
                      $tracelevel);
        Parse::RecDescent::_trace(q{(consumed: [} .
                      Parse::RecDescent::_tracemax(substr($_[1],0,-length($text))) . q{])},
                      Parse::RecDescent::_tracefirst($text),
                      , q{' . $self->{"name"} .'},
                      $tracelevel)
    }
    $_[1] = $text;
    return $return;
}
';

    return $code;
}

my @left;
sub isleftrec($$)
{
    my ($self, $rules) = @_;
    my $root = $self->{"name"};
    @left = $self->leftmostsubrules();
    my $next;
    foreach $next ( @left )
    {
        next unless defined $rules->{$next}; # SKIP NON-EXISTENT RULES
        return 1 if $next eq $root;
        my $child;
        foreach $child ( $rules->{$next}->leftmostsubrules() )
        {
            push(@left, $child)
            if ! _contains($child, @left) ;
        }
    }
    return 0;
}

package Parse::RecDescent::Production;

sub describe ($;$)
{
    return join ' ', map { $_->describe($_[1]) or () } @{$_[0]->{items}};
}

sub new ($$;$$)
{
    my ($self, $line, $uncommit, $error) = @_;
    my $class = ref($self) || $self;

    bless
    {
        "items"    => [],
        "uncommit" => $uncommit,
        "error"    => $error,
        "line"     => $line,
        strcount   => 0,
        patcount   => 0,
        dircount   => 0,
        actcount   => 0,
    }, $class;
}

sub expected ($)
{
    my $itemcount = scalar @{$_[0]->{"items"}};
    return ($itemcount) ? $_[0]->{"items"}[0]->describe(1) : '';
}

sub hasleftmost ($$)
{
    my ($self, $ref) = @_;
    return ${$self->{"items"}}[0] eq $ref  if scalar @{$self->{"items"}};
    return 0;
}

sub isempty($)
{
    my $self = shift;
    return 0 == @{$self->{"items"}};
}

sub leftmostsubrule($)
{
    my $self = shift;

    if ( $#{$self->{"items"}} >= 0 )
    {
        my $subrule = $self->{"items"}[0]->issubrule();
        return $subrule if defined $subrule;
    }

    return ();
}

sub checkleftmost($)
{
    my @items = @{$_[0]->{"items"}};
    if (@items==1 && ref($items[0]) =~ /\AParse::RecDescent::Error/
        && $items[0]->{commitonly} )
    {
        Parse::RecDescent::_warn(2,"Lone <error?> in production treated
                        as <error?> <reject>");
        Parse::RecDescent::_hint("A production consisting of a single
                      conditional <error?> directive would
                      normally succeed (with the value zero) if the
                      rule is not 'commited' when it is
                      tried. Since you almost certainly wanted
                      '<error?> <reject>' Parse::RecDescent
                      supplied it for you.");
        push @{$_[0]->{items}},
            Parse::RecDescent::UncondReject->new(0,0,'<reject>');
    }
    elsif (@items==1 && ($items[0]->describe||"") =~ /<rulevar|<autoscore/)
    {
        # Do nothing
    }
    elsif (@items &&
        ( ref($items[0]) =~ /\AParse::RecDescent::UncondReject/
        || ($items[0]->describe||"") =~ /<autoscore/
        ))
    {
        Parse::RecDescent::_warn(1,"Optimizing away production: [". $_[0]->describe ."]");
        my $what = $items[0]->describe =~ /<rulevar/
                ? "a <rulevar> (which acts like an unconditional <reject> during parsing)"
             : $items[0]->describe =~ /<autoscore/
                ? "an <autoscore> (which acts like an unconditional <reject> during parsing)"
                : "an unconditional <reject>";
        my $caveat = $items[0]->describe =~ /<rulevar/
                ? " after the specified variable was set up"
                : "";
        my $advice = @items > 1
                ? "However, there were also other (useless) items after the leading "
                  . $items[0]->describe
                  . ", so you may have been expecting some other behaviour."
                : "You can safely ignore this message.";
        Parse::RecDescent::_hint("The production starts with $what. That means that the
                      production can never successfully match, so it was
                      optimized out of the final parser$caveat. $advice");
        return 0;
    }
    return 1;
}

sub changesskip($)
{
    my $item;
    foreach $item (@{$_[0]->{"items"}})
    {
        if (ref($item) =~ /Parse::RecDescent::(Action|Directive)/)
        {
            return 1 if $item->{code} =~ /\$skip\s*=/;
        }
    }
    return 0;
}

sub adddirective
{
    my ( $self, $whichop, $line, $name ) = @_;
    push @{$self->{op}},
        { type=>$whichop, line=>$line, name=>$name,
          offset=> scalar(@{$self->{items}}) };
}

sub addscore
{
    my ( $self, $code, $lookahead, $line ) = @_;
    $self->additem(Parse::RecDescent::Directive->new(
                  "local \$^W;
                   my \$thisscore = do { $code } + 0;
                   if (!defined(\$score) || \$thisscore>\$score)
                    { \$score=\$thisscore; \$score_return=\$item[-1]; }
                   undef;", $lookahead, $line,"<score: $code>") )
        unless $self->{items}[-1]->describe =~ /<score/;
    return 1;
}

sub check_pending
{
    my ( $self, $line ) = @_;
    if ($self->{op})
    {
        while (my $next = pop @{$self->{op}})
        {
        Parse::RecDescent::_error("Incomplete <$next->{type}op:...>.", $line);
        Parse::RecDescent::_hint(
            "The current production ended without completing the
             <$next->{type}op:...> directive that started near line
             $next->{line}. Did you forget the closing '>'?");
        }
    }
    return 1;
}

sub enddirective
{
    my ( $self, $line, $minrep, $maxrep ) = @_;
    unless ($self->{op})
    {
        Parse::RecDescent::_error("Unmatched > found.", $line);
        Parse::RecDescent::_hint(
            "A '>' angle bracket was encountered, which typically
             indicates the end of a directive. However no suitable
             preceding directive was encountered. Typically this
             indicates either a extra '>' in the grammar, or a
             problem inside the previous directive.");
        return;
    }
    my $op = pop @{$self->{op}};
    my $span = @{$self->{items}} - $op->{offset};
    if ($op->{type} =~ /left|right/)
    {
        if ($span != 3)
        {
        Parse::RecDescent::_error(
            "Incorrect <$op->{type}op:...> specification:
             expected 3 args, but found $span instead", $line);
        Parse::RecDescent::_hint(
            "The <$op->{type}op:...> directive requires a
             sequence of exactly three elements. For example:
             <$op->{type}op:leftarg /op/ rightarg>");
        }
        else
        {
        push @{$self->{items}},
            Parse::RecDescent::Operator->new(
                $op->{type}, $minrep, $maxrep, splice(@{$self->{"items"}}, -3));
        $self->{items}[-1]->sethashname($self);
        $self->{items}[-1]{name} = $op->{name};
        }
    }
}

sub prevwasreturn
{
    my ( $self, $line ) = @_;
    unless (@{$self->{items}})
    {
        Parse::RecDescent::_error(
            "Incorrect <return:...> specification:
            expected item missing", $line);
        Parse::RecDescent::_hint(
            "The <return:...> directive requires a
            sequence of at least one item. For example:
            <return: list>");
        return;
    }
    push @{$self->{items}},
        Parse::RecDescent::Result->new();
}

sub additem
{
    my ( $self, $item ) = @_;
    $item->sethashname($self);
    push @{$self->{"items"}}, $item;
    return $item;
}

sub _duplicate_itempos
{
    my ($src) = @_;
    my $dst = {};

    foreach (keys %$src)
    {
        %{$dst->{$_}} = %{$src->{$_}};
    }
    $dst;
}

sub _update_itempos
{
    my ($dst, $src, $typekeys, $poskeys) = @_;

    my @typekeys = 'ARRAY' eq ref $typekeys ?
      @$typekeys :
      keys %$src;

    foreach my $k (keys %$src)
    {
        if ('ARRAY' eq ref $poskeys)
        {
            @{$dst->{$k}}{@$poskeys} = @{$src->{$k}}{@$poskeys};
        }
        else
        {
            %{$dst->{$k}} = %{$src->{$k}};
        }
    }
}

sub preitempos
{
    return q
    {
        push @itempos, {'offset' => {'from'=>$thisoffset, 'to'=>undef},
                        'line'   => {'from'=>$thisline,   'to'=>undef},
                        'column' => {'from'=>$thiscolumn, 'to'=>undef} };
    }
}

sub incitempos
{
    return q
    {
        $itempos[$#itempos]{'offset'}{'from'} += length($lastsep);
        $itempos[$#itempos]{'line'}{'from'}   = $thisline;
        $itempos[$#itempos]{'column'}{'from'} = $thiscolumn;
    }
}

sub unincitempos
{
    # the next incitempos will properly set these two fields, but
    # {'offset'}{'from'} needs to be decreased by length($lastsep)
    # $itempos[$#itempos]{'line'}{'from'}
    # $itempos[$#itempos]{'column'}{'from'}
    return q
    {
        $itempos[$#itempos]{'offset'}{'from'} -= length($lastsep) if defined $lastsep;
    }
}

sub postitempos
{
    return q
    {
        $itempos[$#itempos]{'offset'}{'to'} = $prevoffset;
        $itempos[$#itempos]{'line'}{'to'}   = $prevline;
        $itempos[$#itempos]{'column'}{'to'} = $prevcolumn;
    }
}

sub code($$$$)
{
    my ($self,$namespace,$rule,$parser) = @_;
    my $code =
'
    while (!$_matched'
    . (defined $self->{"uncommit"} ? '' : ' && !$commit')
    . ')
    {
        ' .
        ($self->changesskip()
            ? 'local $skip = defined($skip) ? $skip : $Parse::RecDescent::skip;'
            : '') .'
        Parse::RecDescent::_trace(q{Trying production: ['
                      . $self->describe . ']},
                      Parse::RecDescent::_tracefirst($_[1]),
                      q{' . $rule ->{name}. '},
                      $tracelevel)
                        if defined $::RD_TRACE;
        my $thisprod = $thisrule->{"prods"}[' . $self->{"number"} . '];
        ' . (defined $self->{"error"} ? '' : '$text = $_[1];' ) . '
        my $_savetext;
        @item = (q{' . $rule->{"name"} . '});
        %item = (__RULE__ => q{' . $rule->{"name"} . '});
        my $repcount = 0;

';
    $code .=
'        my @itempos = ({});
'           if $parser->{_check}{itempos};

    my $item;
    my $i;

    for ($i = 0; $i < @{$self->{"items"}}; $i++)
    {
        $item = ${$self->{items}}[$i];

        $code .= preitempos() if $parser->{_check}{itempos};

        $code .= $item->code($namespace,$rule,$parser->{_check});

        $code .= postitempos() if $parser->{_check}{itempos};

    }

    if ($parser->{_AUTOACTION} && defined($item) && !$item->isa("Parse::RecDescent::Action"))
    {
        $code .= $parser->{_AUTOACTION}->code($namespace,$rule);
        Parse::RecDescent::_warn(1,"Autogenerating action in rule
                       \"$rule->{name}\":
                        $parser->{_AUTOACTION}{code}")
        and
        Parse::RecDescent::_hint("The \$::RD_AUTOACTION was defined,
                      so any production not ending in an
                      explicit action has the specified
                      \"auto-action\" automatically
                      appended.");
    }
    elsif ($parser->{_AUTOTREE} && defined($item) && !$item->isa("Parse::RecDescent::Action"))
    {
        if ($i==1 && $item->isterminal)
        {
            $code .= $parser->{_AUTOTREE}{TERMINAL}->code($namespace,$rule);
        }
        else
        {
            $code .= $parser->{_AUTOTREE}{NODE}->code($namespace,$rule);
        }
        Parse::RecDescent::_warn(1,"Autogenerating tree-building action in rule
                       \"$rule->{name}\"")
        and
        Parse::RecDescent::_hint("The directive <autotree> was specified,
                      so any production not ending
                      in an explicit action has
                      some parse-tree building code
                      automatically appended.");
    }

    $code .=
'
        Parse::RecDescent::_trace(q{>>'.Parse::RecDescent::_matchtracemessage($self).' production: ['
                      . $self->describe . ']<<},
                      Parse::RecDescent::_tracefirst($text),
                      q{' . $rule->{name} . '},
                      $tracelevel)
                        if defined $::RD_TRACE;

' . ( $parser->{_check}{itempos} ? '
        if ( defined($_itempos) )
        {
            Parse::RecDescent::Production::_update_itempos($_itempos, $itempos[ 1], undef, [qw(from)]);
            Parse::RecDescent::Production::_update_itempos($_itempos, $itempos[-1], undef, [qw(to)]);
        }
' : '' ) . '

        $_matched = 1;
        last;
    }

';
    return $code;
}

1;

package Parse::RecDescent::Action;

sub describe { undef }

sub sethashname { $_[0]->{hashname} = '__ACTION' . ++$_[1]->{actcount} .'__'; }

sub new
{
    my $class = ref($_[0]) || $_[0];
    bless
    {
        "code"      => $_[1],
        "lookahead" => $_[2],
        "line"      => $_[3],
    }, $class;
}

sub issubrule { undef }
sub isterminal { 0 }

sub code($$$$)
{
    my ($self, $namespace, $rule) = @_;

'
        Parse::RecDescent::_trace(q{Trying action},
                      Parse::RecDescent::_tracefirst($text),
                      q{' . $rule->{name} . '},
                      $tracelevel)
                        if defined $::RD_TRACE;
        ' . ($self->{"lookahead"} ? '$_savetext = $text;' : '' ) .'

        $_tok = ($_noactions) ? 0 : do ' . $self->{"code"} . ';
        ' . ($self->{"lookahead"}<0?'if':'unless') . ' (defined $_tok)
        {
            Parse::RecDescent::_trace(q{<<'.Parse::RecDescent::_matchtracemessage($self,1).' action>> (return value: [undef])})
                    if defined $::RD_TRACE;
            last;
        }
        Parse::RecDescent::_trace(q{>>'.Parse::RecDescent::_matchtracemessage($self).' action<< (return value: [}
                      . $_tok . q{])},
                      Parse::RecDescent::_tracefirst($text))
                        if defined $::RD_TRACE;
        push @item, $_tok;
        ' . ($self->{line}>=0 ? '$item{'. $self->{hashname} .'}=$_tok;' : '' ) .'
        ' . ($self->{"lookahead"} ? '$text = $_savetext;' : '' ) .'
'
}


1;

package Parse::RecDescent::Directive;

sub sethashname { $_[0]->{hashname} = '__DIRECTIVE' . ++$_[1]->{dircount} .  '__'; }

sub issubrule { undef }
sub isterminal { 0 }
sub describe { $_[1] ? '' : $_[0]->{name} }

sub new ($$$$$)
{
    my $class = ref($_[0]) || $_[0];
    bless
    {
        "code"      => $_[1],
        "lookahead" => $_[2],
        "line"      => $_[3],
        "name"      => $_[4],
    }, $class;
}

sub code($$$$)
{
    my ($self, $namespace, $rule) = @_;

'
        ' . ($self->{"lookahead"} ? '$_savetext = $text;' : '' ) .'

        Parse::RecDescent::_trace(q{Trying directive: ['
                    . $self->describe . ']},
                    Parse::RecDescent::_tracefirst($text),
                      q{' . $rule->{name} . '},
                      $tracelevel)
                        if defined $::RD_TRACE; ' .'
        $_tok = do { ' . $self->{"code"} . ' };
        if (defined($_tok))
        {
            Parse::RecDescent::_trace(q{>>'.Parse::RecDescent::_matchtracemessage($self).' directive<< (return value: [}
                        . $_tok . q{])},
                        Parse::RecDescent::_tracefirst($text))
                            if defined $::RD_TRACE;
        }
        else
        {
            Parse::RecDescent::_trace(q{<<'.Parse::RecDescent::_matchtracemessage($self,1).' directive>>},
                        Parse::RecDescent::_tracefirst($text))
                            if defined $::RD_TRACE;
        }
        ' . ($self->{"lookahead"} ? '$text = $_savetext and ' : '' ) .'
        last '
        . ($self->{"lookahead"}<0?'if':'unless') . ' defined $_tok;
        push @item, $item{'.$self->{hashname}.'}=$_tok;
        ' . ($self->{"lookahead"} ? '$text = $_savetext;' : '' ) .'
'
}

1;

package Parse::RecDescent::UncondReject;

sub issubrule { undef }
sub isterminal { 0 }
sub describe { $_[1] ? '' : $_[0]->{name} }
sub sethashname { $_[0]->{hashname} = '__DIRECTIVE' . ++$_[1]->{dircount} .  '__'; }

sub new ($$$;$)
{
    my $class = ref($_[0]) || $_[0];
    bless
    {
        "lookahead" => $_[1],
        "line"      => $_[2],
        "name"      => $_[3],
    }, $class;
}

# MARK, YOU MAY WANT TO OPTIMIZE THIS.


sub code($$$$)
{
    my ($self, $namespace, $rule) = @_;

'
        Parse::RecDescent::_trace(q{>>Rejecting production<< (found '
                     . $self->describe . ')},
                     Parse::RecDescent::_tracefirst($text),
                      q{' . $rule->{name} . '},
                      $tracelevel)
                        if defined $::RD_TRACE;
        undef $return;
        ' . ($self->{"lookahead"} ? '$_savetext = $text;' : '' ) .'

        $_tok = undef;
        ' . ($self->{"lookahead"} ? '$text = $_savetext and ' : '' ) .'
        last '
        . ($self->{"lookahead"}<0?'if':'unless') . ' defined $_tok;
'
}

1;

package Parse::RecDescent::Error;

sub issubrule { undef }
sub isterminal { 0 }
sub describe { $_[1] ? '' : $_[0]->{commitonly} ? '<error?:...>' : '<error...>' }
sub sethashname { $_[0]->{hashname} = '__DIRECTIVE' . ++$_[1]->{dircount} .  '__'; }

sub new ($$$$$)
{
    my $class = ref($_[0]) || $_[0];
    bless
    {
        "msg"        => $_[1],
        "lookahead"  => $_[2],
        "commitonly" => $_[3],
        "line"       => $_[4],
    }, $class;
}

sub code($$$$)
{
    my ($self, $namespace, $rule) = @_;

    my $action = '';

    if ($self->{"msg"})  # ERROR MESSAGE SUPPLIED
    {
        #WAS: $action .= "Parse::RecDescent::_error(qq{$self->{msg}}" .  ',$thisline);';
        $action .= 'push @{$thisparser->{errors}}, [qq{'.$self->{msg}.'},$thisline];';

    }
    else      # GENERATE ERROR MESSAGE DURING PARSE
    {
        $action .= '
        my $rule = $item[0];
           $rule =~ s/_/ /g;
        #WAS: Parse::RecDescent::_error("Invalid $rule: " . $expectation->message() ,$thisline);
        push @{$thisparser->{errors}}, ["Invalid $rule: " . $expectation->message() ,$thisline];
        ';
    }

    my $dir =
          new Parse::RecDescent::Directive('if (' .
        ($self->{"commitonly"} ? '$commit' : '1') .
        ") { do {$action} unless ".' $_noactions; undef } else {0}',
                    $self->{"lookahead"},0,$self->describe);
    $dir->{hashname} = $self->{hashname};
    return $dir->code($namespace, $rule, 0);
}

1;

package Parse::RecDescent::Token;

sub sethashname { $_[0]->{hashname} = '__PATTERN' . ++$_[1]->{patcount} . '__'; }

sub issubrule { undef }
sub isterminal { 1 }
sub describe ($) { shift->{'description'}}


# ARGS ARE: $self, $pattern, $left_delim, $modifiers, $lookahead, $linenum
sub new ($$$$$$)
{
    my $class = ref($_[0]) || $_[0];
    my $pattern = $_[1];
    my $pat = $_[1];
    my $ldel = $_[2];
    my $rdel = $ldel;
    $rdel =~ tr/{[(</}])>/;

    my $mod = $_[3];

    my $desc;

    if ($ldel eq '/') { $desc = "$ldel$pattern$rdel$mod" }
    else          { $desc = "m$ldel$pattern$rdel$mod" }
    $desc =~ s/\\/\\\\/g;
    $desc =~ s/\$$/\\\$/g;
    $desc =~ s/}/\\}/g;
    $desc =~ s/{/\\{/g;

    if (!eval "no strict;
           local \$SIG{__WARN__} = sub {0};
           '' =~ m$ldel$pattern$rdel$mod" and $@)
    {
        Parse::RecDescent::_warn(3, "Token pattern \"m$ldel$pattern$rdel$mod\"
                         may not be a valid regular expression",
                       $_[5]);
        $@ =~ s/ at \(eval.*/./;
        Parse::RecDescent::_hint($@);
    }

    # QUIETLY PREVENT (WELL-INTENTIONED) CALAMITY
    $mod =~ s/[gc]//g;
    $pattern =~ s/(\A|[^\\])\\G/$1/g;

    bless
    {
        "pattern"   => $pattern,
        "ldelim"      => $ldel,
        "rdelim"      => $rdel,
        "mod"         => $mod,
        "lookahead"   => $_[4],
        "line"        => $_[5],
        "description" => $desc,
    }, $class;
}


sub code($$$$$)
{
    my ($self, $namespace, $rule, $check) = @_;
    my $ldel = $self->{"ldelim"};
    my $rdel = $self->{"rdelim"};
    my $sdel = $ldel;
    my $mod  = $self->{"mod"};

    $sdel =~ s/[[{(<]/{}/;

my $code = '
        Parse::RecDescent::_trace(q{Trying terminal: [' . $self->describe
                      . ']}, Parse::RecDescent::_tracefirst($text),
                      q{' . $rule->{name} . '},
                      $tracelevel)
                        if defined $::RD_TRACE;
        undef $lastsep;
        $expectation->is(q{' . ($rule->hasleftmost($self) ? ''
                : $self->describe ) . '})->at($text);
        ' . ($self->{"lookahead"} ? '$_savetext = $text;' : '' ) . '

        ' . ($self->{"lookahead"}<0?'if':'unless')
        . ' ($text =~ s/\A($skip)/$lastsep=$1 and ""/e and '
        . ($check->{itempos}? 'do {'.Parse::RecDescent::Production::incitempos().' 1} and ' : '')
        . '  $text =~ m' . $ldel . '\A(?:' . $self->{"pattern"} . ')' . $rdel . $mod . ')
        {
            '.($self->{"lookahead"} ? '$text = $_savetext;' : '$text = $lastsep . $text if defined $lastsep;') .
            ($check->{itempos} ? Parse::RecDescent::Production::unincitempos() : '') . '
            $expectation->failed();
            Parse::RecDescent::_trace(q{<<Didn\'t match terminal>>},
                          Parse::RecDescent::_tracefirst($text))
                    if defined $::RD_TRACE;

            last;
        }
        $current_match = substr($text, $-[0], $+[0] - $-[0]);
        substr($text,0,length($current_match),q{});
        Parse::RecDescent::_trace(q{>>Matched terminal<< (return value: [}
                        . $current_match . q{])},
                          Parse::RecDescent::_tracefirst($text))
                    if defined $::RD_TRACE;
        push @item, $item{'.$self->{hashname}.'}=$current_match;
        ' . ($self->{"lookahead"} ? '$text = $_savetext;' : '' ) .'
';

    return $code;
}

1;

package Parse::RecDescent::Literal;

sub sethashname { $_[0]->{hashname} = '__STRING' . ++$_[1]->{strcount} . '__'; }

sub issubrule { undef }
sub isterminal { 1 }
sub describe ($) { shift->{'description'} }

sub new ($$$$)
{
    my $class = ref($_[0]) || $_[0];

    my $pattern = $_[1];

    my $desc = $pattern;
    $desc=~s/\\/\\\\/g;
    $desc=~s/}/\\}/g;
    $desc=~s/{/\\{/g;

    bless
    {
        "pattern"     => $pattern,
        "lookahead"   => $_[2],
        "line"        => $_[3],
        "description" => "'$desc'",
    }, $class;
}


sub code($$$$)
{
    my ($self, $namespace, $rule, $check) = @_;

my $code = '
        Parse::RecDescent::_trace(q{Trying terminal: [' . $self->describe
                      . ']},
                      Parse::RecDescent::_tracefirst($text),
                      q{' . $rule->{name} . '},
                      $tracelevel)
                        if defined $::RD_TRACE;
        undef $lastsep;
        $expectation->is(q{' . ($rule->hasleftmost($self) ? ''
                : $self->describe ) . '})->at($text);
        ' . ($self->{"lookahead"} ? '$_savetext = $text;' : '' ) . '

        ' . ($self->{"lookahead"}<0?'if':'unless')
        . ' ($text =~ s/\A($skip)/$lastsep=$1 and ""/e and '
        . ($check->{itempos}? 'do {'.Parse::RecDescent::Production::incitempos().' 1} and ' : '')
        . '  $text =~ m/\A' . quotemeta($self->{"pattern"}) . '/)
        {
            '.($self->{"lookahead"} ? '$text = $_savetext;' : '$text = $lastsep . $text if defined $lastsep;').'
            '. ($check->{itempos} ? Parse::RecDescent::Production::unincitempos() : '') . '
            $expectation->failed();
            Parse::RecDescent::_trace(qq{<<Didn\'t match terminal>>},
                          Parse::RecDescent::_tracefirst($text))
                            if defined $::RD_TRACE;
            last;
        }
        $current_match = substr($text, $-[0], $+[0] - $-[0]);
        substr($text,0,length($current_match),q{});
        Parse::RecDescent::_trace(q{>>Matched terminal<< (return value: [}
                        . $current_match . q{])},
                          Parse::RecDescent::_tracefirst($text))
                            if defined $::RD_TRACE;
        push @item, $item{'.$self->{hashname}.'}=$current_match;
        ' . ($self->{"lookahead"} ? '$text = $_savetext;' : '' ) .'
';

    return $code;
}

1;

package Parse::RecDescent::InterpLit;

sub sethashname { $_[0]->{hashname} = '__STRING' . ++$_[1]->{strcount} . '__'; }

sub issubrule { undef }
sub isterminal { 1 }
sub describe ($) { shift->{'description'} }

sub new ($$$$)
{
    my $class = ref($_[0]) || $_[0];

    my $pattern = $_[1];
    $pattern =~ s#/#\\/#g;

    my $desc = $pattern;
    $desc=~s/\\/\\\\/g;
    $desc=~s/}/\\}/g;
    $desc=~s/{/\\{/g;

    bless
    {
        "pattern"   => $pattern,
        "lookahead" => $_[2],
        "line"      => $_[3],
        "description" => "'$desc'",
    }, $class;
}

sub code($$$$)
{
    my ($self, $namespace, $rule, $check) = @_;

my $code = '
        Parse::RecDescent::_trace(q{Trying terminal: [' . $self->describe
                      . ']},
                      Parse::RecDescent::_tracefirst($text),
                      q{' . $rule->{name} . '},
                      $tracelevel)
                        if defined $::RD_TRACE;
        undef $lastsep;
        $expectation->is(q{' . ($rule->hasleftmost($self) ? ''
                : $self->describe ) . '})->at($text);
        ' . ($self->{"lookahead"} ? '$_savetext = $text;' : '' ) . '

        ' . ($self->{"lookahead"}<0?'if':'unless')
        . ' ($text =~ s/\A($skip)/$lastsep=$1 and ""/e and '
        . ($check->{itempos}? 'do {'.Parse::RecDescent::Production::incitempos().' 1} and ' : '')
        . '  do { $_tok = "' . $self->{"pattern"} . '"; 1 } and
             substr($text,0,length($_tok)) eq $_tok and
             do { substr($text,0,length($_tok)) = ""; 1; }
        )
        {
            '.($self->{"lookahead"} ? '$text = $_savetext;' : '$text = $lastsep . $text if defined $lastsep;').'
            '. ($check->{itempos} ? Parse::RecDescent::Production::unincitempos() : '') . '
            $expectation->failed();
            Parse::RecDescent::_trace(q{<<Didn\'t match terminal>>},
                          Parse::RecDescent::_tracefirst($text))
                            if defined $::RD_TRACE;
            last;
        }
        Parse::RecDescent::_trace(q{>>Matched terminal<< (return value: [}
                        . $_tok . q{])},
                          Parse::RecDescent::_tracefirst($text))
                            if defined $::RD_TRACE;
        push @item, $item{'.$self->{hashname}.'}=$_tok;
        ' . ($self->{"lookahead"} ? '$text = $_savetext;' : '' ) .'
';

    return $code;
}

1;

package Parse::RecDescent::Subrule;

sub issubrule ($) { return $_[0]->{"subrule"} }
sub isterminal { 0 }
sub sethashname {}

sub describe ($)
{
    my $desc = $_[0]->{"implicit"} || $_[0]->{"subrule"};
    $desc = "<matchrule:$desc>" if $_[0]->{"matchrule"};
    return $desc;
}

sub callsyntax($$)
{
    if ($_[0]->{"matchrule"})
    {
        return "&{'$_[1]'.qq{$_[0]->{subrule}}}";
    }
    else
    {
        return $_[1].$_[0]->{"subrule"};
    }
}

sub new ($$$$;$$$)
{
    my $class = ref($_[0]) || $_[0];
    bless
    {
        "subrule"   => $_[1],
        "lookahead" => $_[2],
        "line"      => $_[3],
        "implicit"  => $_[4] || undef,
        "matchrule" => $_[5],
        "argcode"   => $_[6] || undef,
    }, $class;
}


sub code($$$$)
{
    my ($self, $namespace, $rule, $check) = @_;

'
        Parse::RecDescent::_trace(q{Trying subrule: [' . $self->{"subrule"} . ']},
                  Parse::RecDescent::_tracefirst($text),
                  q{' . $rule->{"name"} . '},
                  $tracelevel)
                    if defined $::RD_TRACE;
        if (1) { no strict qw{refs};
        $expectation->is(' . ($rule->hasleftmost($self) ? 'q{}'
                # WAS : 'qq{'.$self->describe.'}' ) . ')->at($text);
                : 'q{'.$self->describe.'}' ) . ')->at($text);
        ' . ($self->{"lookahead"} ? '$_savetext = $text;' : '' )
        . ($self->{"lookahead"}<0?'if':'unless')
        . ' (defined ($_tok = '
        . $self->callsyntax($namespace.'::')
        . '($thisparser,$text,$repeating,'
        . ($self->{"lookahead"}?'1':'$_noactions')
        . ($self->{argcode} ? ",sub { return $self->{argcode} }"
                   : ',sub { \\@arg }')
        . ($check->{"itempos"}?',$itempos[$#itempos]':',undef')
        . ')))
        {
            '.($self->{"lookahead"} ? '$text = $_savetext;' : '').'
            Parse::RecDescent::_trace(q{<<'.Parse::RecDescent::_matchtracemessage($self,1).' subrule: ['
            . $self->{subrule} . ']>>},
                          Parse::RecDescent::_tracefirst($text),
                          q{' . $rule->{"name"} .'},
                          $tracelevel)
                            if defined $::RD_TRACE;
            $expectation->failed();
            last;
        }
        Parse::RecDescent::_trace(q{>>'.Parse::RecDescent::_matchtracemessage($self).' subrule: ['
                    . $self->{subrule} . ']<< (return value: [}
                    . $_tok . q{]},

                      Parse::RecDescent::_tracefirst($text),
                      q{' . $rule->{"name"} .'},
                      $tracelevel)
                        if defined $::RD_TRACE;
        $item{q{' . $self->{subrule} . '}} = $_tok;
        push @item, $_tok;
        ' . ($self->{"lookahead"} ? '$text = $_savetext;' : '' ) .'
        }
'
}

package Parse::RecDescent::Repetition;

sub issubrule ($) { return $_[0]->{"subrule"} }
sub isterminal { 0 }
sub sethashname {  }

sub describe ($)
{
    my $desc = $_[0]->{"expected"} || $_[0]->{"subrule"};
    $desc = "<matchrule:$desc>" if $_[0]->{"matchrule"};
    return $desc;
}

sub callsyntax($$)
{
    if ($_[0]->{matchrule})
        { return "sub { goto &{''.qq{$_[1]$_[0]->{subrule}}} }"; }
    else
        { return "\\&$_[1]$_[0]->{subrule}"; }
}

sub new ($$$$$$$$$$)
{
    my ($self, $subrule, $repspec, $min, $max, $lookahead, $line, $parser, $matchrule, $argcode) = @_;
    my $class = ref($self) || $self;
    ($max, $min) = ( $min, $max) if ($max<$min);

    my $desc;
    if ($subrule=~/\A_alternation_\d+_of_production_\d+_of_rule/)
        { $desc = $parser->{"rules"}{$subrule}->expected }

    if ($lookahead)
    {
        if ($min>0)
        {
           return new Parse::RecDescent::Subrule($subrule,$lookahead,$line,$desc,$matchrule,$argcode);
        }
        else
        {
            Parse::RecDescent::_error("Not symbol (\"!\") before
                        \"$subrule\" doesn't make
                        sense.",$line);
            Parse::RecDescent::_hint("Lookahead for negated optional
                       repetitions (such as
                       \"!$subrule($repspec)\" can never
                       succeed, since optional items always
                       match (zero times at worst).
                       Did you mean a single \"!$subrule\",
                       instead?");
        }
    }
    bless
    {
        "subrule"   => $subrule,
        "repspec"   => $repspec,
        "min"       => $min,
        "max"       => $max,
        "lookahead" => $lookahead,
        "line"      => $line,
        "expected"  => $desc,
        "argcode"   => $argcode || undef,
        "matchrule" => $matchrule,
    }, $class;
}

sub code($$$$)
{
    my ($self, $namespace, $rule, $check) = @_;

    my ($subrule, $repspec, $min, $max, $lookahead) =
        @{$self}{ qw{subrule repspec min max lookahead} };

'
        Parse::RecDescent::_trace(q{Trying repeated subrule: [' . $self->describe . ']},
                  Parse::RecDescent::_tracefirst($text),
                  q{' . $rule->{"name"} . '},
                  $tracelevel)
                    if defined $::RD_TRACE;
        $expectation->is(' . ($rule->hasleftmost($self) ? 'q{}'
                # WAS : 'qq{'.$self->describe.'}' ) . ')->at($text);
                : 'q{'.$self->describe.'}' ) . ')->at($text);
        ' . ($self->{"lookahead"} ? '$_savetext = $text;' : '' ) .'
        unless (defined ($_tok = $thisparser->_parserepeat($text, '
        . $self->callsyntax($namespace.'::')
        . ', ' . $min . ', ' . $max . ', '
        . ($self->{"lookahead"}?'1':'$_noactions')
        . ',$expectation,'
        . ($self->{argcode} ? "sub { return $self->{argcode} }"
                        : 'sub { \\@arg }')
        . ($check->{"itempos"}?',$itempos[$#itempos]':',undef')
        . ')))
        {
            Parse::RecDescent::_trace(q{<<'.Parse::RecDescent::_matchtracemessage($self,1).' repeated subrule: ['
            . $self->describe . ']>>},
                          Parse::RecDescent::_tracefirst($text),
                          q{' . $rule->{"name"} .'},
                          $tracelevel)
                            if defined $::RD_TRACE;
            last;
        }
        Parse::RecDescent::_trace(q{>>'.Parse::RecDescent::_matchtracemessage($self).' repeated subrule: ['
                    . $self->{subrule} . ']<< (}
                    . @$_tok . q{ times)},

                      Parse::RecDescent::_tracefirst($text),
                      q{' . $rule->{"name"} .'},
                      $tracelevel)
                        if defined $::RD_TRACE;
        $item{q{' . "$self->{subrule}($self->{repspec})" . '}} = $_tok;
        push @item, $_tok;
        ' . ($self->{"lookahead"} ? '$text = $_savetext;' : '' ) .'

'
}

package Parse::RecDescent::Result;

sub issubrule { 0 }
sub isterminal { 0 }
sub describe { '' }

sub new
{
    my ($class, $pos) = @_;

    bless {}, $class;
}

sub code($$$$)
{
    my ($self, $namespace, $rule) = @_;

    '
        $return = $item[-1];
    ';
}

package Parse::RecDescent::Operator;

my @opertype = ( " non-optional", "n optional" );

sub issubrule { 0 }
sub isterminal { 0 }

sub describe { $_[0]->{"expected"} }
sub sethashname { $_[0]->{hashname} = '__DIRECTIVE' . ++$_[1]->{dircount} .  '__'; }


sub new
{
    my ($class, $type, $minrep, $maxrep, $leftarg, $op, $rightarg) = @_;

    bless
    {
        "type"      => "${type}op",
        "leftarg"   => $leftarg,
        "op"        => $op,
        "min"       => $minrep,
        "max"       => $maxrep,
        "rightarg"  => $rightarg,
        "expected"  => "<${type}op: ".$leftarg->describe." ".$op->describe." ".$rightarg->describe.">",
    }, $class;
}

sub code($$$$)
{
    my ($self, $namespace, $rule, $check) = @_;

    my @codeargs = @_[1..$#_];

    my ($leftarg, $op, $rightarg) =
        @{$self}{ qw{leftarg op rightarg} };

    my $code = '
        Parse::RecDescent::_trace(q{Trying operator: [' . $self->describe . ']},
                  Parse::RecDescent::_tracefirst($text),
                  q{' . $rule->{"name"} . '},
                  $tracelevel)
                    if defined $::RD_TRACE;
        $expectation->is(' . ($rule->hasleftmost($self) ? 'q{}'
                # WAS : 'qq{'.$self->describe.'}' ) . ')->at($text);
                : 'q{'.$self->describe.'}' ) . ')->at($text);

        $_tok = undef;
        OPLOOP: while (1)
        {
          $repcount = 0;
          my @item;
          my %item;
';

    $code .= '
          my  $_itempos = $itempos[-1];
          my  $itemposfirst;
' if $check->{itempos};

    if ($self->{type} eq "leftop" )
    {
        $code .= '
          # MATCH LEFTARG
          ' . $leftarg->code(@codeargs) . '

';

        $code .= '
          if (defined($_itempos) and !defined($itemposfirst))
          {
              $itemposfirst = Parse::RecDescent::Production::_duplicate_itempos($_itempos);
          }
' if $check->{itempos};

        $code .= '
          $repcount++;

          my $savetext = $text;
          my $backtrack;

          # MATCH (OP RIGHTARG)(s)
          while ($repcount < ' . $self->{max} . ')
          {
            $backtrack = 0;
            ' . $op->code(@codeargs) . '
            ' . ($op->isterminal() ? 'pop @item;' : '$backtrack=1;' ) . '
            ' . (ref($op) eq 'Parse::RecDescent::Token'
                ? 'if (defined $1) {push @item, $item{'.($self->{name}||$self->{hashname}).'}=$1; $backtrack=1;}'
                : "" ) . '
            ' . $rightarg->code(@codeargs) . '
            $savetext = $text;
            $repcount++;
          }
          $text = $savetext;
          pop @item if $backtrack;

          ';
    }
    else
    {
        $code .= '
          my $savetext = $text;
          my $backtrack;
          # MATCH (LEFTARG OP)(s)
          while ($repcount < ' . $self->{max} . ')
          {
            $backtrack = 0;
            ' . $leftarg->code(@codeargs) . '
';
        $code .= '
            if (defined($_itempos) and !defined($itemposfirst))
            {
                $itemposfirst = Parse::RecDescent::Production::_duplicate_itempos($_itempos);
            }
' if $check->{itempos};

        $code .= '
            $repcount++;
            $backtrack = 1;
            ' . $op->code(@codeargs) . '
            $savetext = $text;
            ' . ($op->isterminal() ? 'pop @item;' : "" ) . '
            ' . (ref($op) eq 'Parse::RecDescent::Token' ? 'do { push @item, $item{'.($self->{name}||$self->{hashname}).'}=$1; } if defined $1;' : "" ) . '
          }
          $text = $savetext;
          pop @item if $backtrack;

          # MATCH RIGHTARG
          ' . $rightarg->code(@codeargs) . '
          $repcount++;
          ';
    }

    $code .= 'unless (@item) { undef $_tok; last }' unless $self->{min}==0;

    $code .= '
          $_tok = [ @item ];
';


    $code .= '
          if (defined $itemposfirst)
          {
              Parse::RecDescent::Production::_update_itempos(
                  $_itempos, $itemposfirst, undef, [qw(from)]);
          }
' if $check->{itempos};

    $code .= '
          last;
        } # end of OPLOOP
';

    $code .= '
        unless ($repcount>='.$self->{min}.')
        {
            Parse::RecDescent::_trace(q{<<'.Parse::RecDescent::_matchtracemessage($self,1).' operator: ['
                          . $self->describe
                          . ']>>},
                          Parse::RecDescent::_tracefirst($text),
                          q{' . $rule->{"name"} .'},
                          $tracelevel)
                            if defined $::RD_TRACE;
            $expectation->failed();
            last;
        }
        Parse::RecDescent::_trace(q{>>'.Parse::RecDescent::_matchtracemessage($self).' operator: ['
                      . $self->describe
                      . ']<< (return value: [}
                      . qq{@{$_tok||[]}} . q{]},
                      Parse::RecDescent::_tracefirst($text),
                      q{' . $rule->{"name"} .'},
                      $tracelevel)
                        if defined $::RD_TRACE;

        push @item, $item{'.($self->{name}||$self->{hashname}).'}=$_tok||[];
';

    return $code;
}


package Parse::RecDescent::Expectation;

sub new ($)
{
    bless {
        "failed"      => 0,
        "expected"    => "",
        "unexpected"      => "",
        "lastexpected"    => "",
        "lastunexpected"  => "",
        "defexpected"     => $_[1],
          };
}

sub is ($$)
{
    $_[0]->{lastexpected} = $_[1]; return $_[0];
}

sub at ($$)
{
    $_[0]->{lastunexpected} = $_[1]; return $_[0];
}

sub failed ($)
{
    return unless $_[0]->{lastexpected};
    $_[0]->{expected}   = $_[0]->{lastexpected}   unless $_[0]->{failed};
    $_[0]->{unexpected} = $_[0]->{lastunexpected} unless $_[0]->{failed};
    $_[0]->{failed} = 1;
}

sub message ($)
{
    my ($self) = @_;
    $self->{expected} = $self->{defexpected} unless $self->{expected};
    $self->{expected} =~ s/_/ /g;
    if (!$self->{unexpected} || $self->{unexpected} =~ /\A\s*\Z/s)
    {
        return "Was expecting $self->{expected}";
    }
    else
    {
        $self->{unexpected} =~ /\s*(.*)/;
        return "Was expecting $self->{expected} but found \"$1\" instead";
    }
}

1;

package Parse::RecDescent;

use Carp;
use vars qw ( $AUTOLOAD $VERSION $_FILENAME);

my $ERRORS = 0;

our $VERSION = '1.967015';
$VERSION = eval $VERSION;
$_FILENAME=__FILE__;

# BUILDING A PARSER

my $nextnamespace = "namespace000001";

sub _nextnamespace()
{
    return "Parse::RecDescent::" . $nextnamespace++;
}

# ARGS ARE: $class, $grammar, $compiling, $namespace
sub new ($$$$)
{
    my $class = ref($_[0]) || $_[0];
    local $Parse::RecDescent::compiling = $_[2];
    my $name_space_name = defined $_[3]
        ? "Parse::RecDescent::".$_[3]
        : _nextnamespace();
    my $self =
    {
        "rules"     => {},
        "namespace" => $name_space_name,
        "startcode" => '',
        "localvars" => '',
        "_AUTOACTION" => undef,
        "_AUTOTREE"   => undef,

        # Precompiled parsers used to set _precompiled, but that
        # wasn't present in some versions of Parse::RecDescent used to
        # build precompiled parsers.  Instead, set a new
        # _not_precompiled flag, which is remove from future
        # Precompiled parsers at build time.
        "_not_precompiled" => 1,
    };


    if ($::RD_AUTOACTION) {
        my $sourcecode = $::RD_AUTOACTION;
        $sourcecode = "{ $sourcecode }"
            unless $sourcecode =~ /\A\s*\{.*\}\s*\Z/;
        $self->{_check}{itempos} =
            $sourcecode =~ /\@itempos\b|\$itempos\s*\[/;
        $self->{_AUTOACTION}
            = new Parse::RecDescent::Action($sourcecode,0,-1)
    }

    bless $self, $class;
    return $self->Replace($_[1])
}

sub Compile($$$$) {
    die "Compilation of Parse::RecDescent grammars not yet implemented\n";
}

sub DESTROY {
    my ($self) = @_;
    my $namespace = $self->{namespace};
    $namespace =~ s/Parse::RecDescent:://;
    if ($self->{_not_precompiled}) {
        # BEGIN WORKAROUND
        # Perl has a bug that creates a circular reference between
        # @ISA and that variable's stash:
        #   https://rt.perl.org/rt3/Ticket/Display.html?id=92708
        # Emptying the array before deleting the stash seems to
        # prevent the leak.  Once the ticket above has been resolved,
        # these two lines can be removed.
        no strict 'refs';
        @{$self->{namespace} . '::ISA'} = ();
        # END WORKAROUND

        # Some grammars may contain circular references between rules,
        # such as:
        #   a: 'ID' | b
        #   b: '(' a ')'
        # Unless these references are broken, the subs stay around on
        # stash deletion below.  Iterate through the stash entries and
        # for each defined code reference, set it to reference sub {}
        # instead.
        {
            local $^W; # avoid 'sub redefined' warnings.
            my $blank_sub = sub {};
            while (my ($name, $glob) = each %{"Parse::RecDescent::$namespace\::"}) {
                *$glob = $blank_sub if defined &$glob;
            }
        }

        # Delete the namespace's stash
        delete $Parse::RecDescent::{$namespace.'::'};
    }
}

# BUILDING A GRAMMAR....

# ARGS ARE: $self, $grammar, $isimplicit, $isleftop
sub Replace ($$)
{
    # set $replace = 1 for _generate
    splice(@_, 2, 0, 1);

    return _generate(@_);
}

# ARGS ARE: $self, $grammar, $isimplicit, $isleftop
sub Extend ($$)
{
    # set $replace = 0 for _generate
    splice(@_, 2, 0, 0);

    return _generate(@_);
}

sub _no_rule ($$;$)
{
    _error("Ruleless $_[0] at start of grammar.",$_[1]);
    my $desc = $_[2] ? "\"$_[2]\"" : "";
    _hint("You need to define a rule for the $_[0] $desc
           to be part of.");
}

my $NEGLOOKAHEAD    =  '\G(\s*\.\.\.\!)';
my $POSLOOKAHEAD    =  '\G(\s*\.\.\.)';
my $RULE            =  '\G\s*(\w+)[ \t]*:';
my $PROD            =  '\G\s*([|])';
my $TOKEN           = q{\G\s*/((\\\\/|\\\\\\\\|[^/])*)/([cgimsox]*)};
my $MTOKEN          = q{\G\s*(m\s*[^\w\s])};
my $LITERAL         = q{\G\s*'((\\\\['\\\\]|[^'])*)'};
my $INTERPLIT       = q{\G\s*"((\\\\["\\\\]|[^"])*)"};
my $SUBRULE         =  '\G\s*(\w+)';
my $MATCHRULE       =  '\G(\s*<matchrule:)';
my $SIMPLEPAT       =  '((\\s+/[^/\\\\]*(?:\\\\.[^/\\\\]*)*/)?)';
my $OPTIONAL        =  '\G\((\?)'.$SIMPLEPAT.'\)';
my $ANY             =  '\G\((s\?)'.$SIMPLEPAT.'\)';
my $MANY            =  '\G\((s|\.\.)'.$SIMPLEPAT.'\)';
my $EXACTLY         =  '\G\(([1-9]\d*)'.$SIMPLEPAT.'\)';
my $BETWEEN         =  '\G\((\d+)\.\.([1-9]\d*)'.$SIMPLEPAT.'\)';
my $ATLEAST         =  '\G\((\d+)\.\.'.$SIMPLEPAT.'\)';
my $ATMOST          =  '\G\(\.\.([1-9]\d*)'.$SIMPLEPAT.'\)';
my $BADREP          =  '\G\((-?\d+)?\.\.(-?\d+)?'.$SIMPLEPAT.'\)';
my $ACTION          =  '\G\s*\{';
my $IMPLICITSUBRULE =  '\G\s*\(';
my $COMMENT         =  '\G\s*(#.*)';
my $COMMITMK        =  '\G\s*<commit>';
my $UNCOMMITMK      =  '\G\s*<uncommit>';
my $QUOTELIKEMK     =  '\G\s*<perl_quotelike>';
my $CODEBLOCKMK     =  '\G\s*<perl_codeblock(?:\s+([][()<>{}]+))?>';
my $VARIABLEMK      =  '\G\s*<perl_variable>';
my $NOCHECKMK       =  '\G\s*<nocheck>';
my $AUTOACTIONPATMK =  '\G\s*<autoaction:';
my $AUTOTREEMK      =  '\G\s*<autotree(?::\s*([\w:]+)\s*)?>';
my $AUTOSTUBMK      =  '\G\s*<autostub>';
my $AUTORULEMK      =  '\G\s*<autorule:(.*?)>';
my $REJECTMK        =  '\G\s*<reject>';
my $CONDREJECTMK    =  '\G\s*<reject:';
my $SCOREMK         =  '\G\s*<score:';
my $AUTOSCOREMK     =  '\G\s*<autoscore:';
my $SKIPMK          =  '\G\s*<skip:';
my $OPMK            =  '\G\s*<(left|right)op(?:=(\'.*?\'))?:';
my $ENDDIRECTIVEMK  =  '\G\s*>';
my $RESYNCMK        =  '\G\s*<resync>';
my $RESYNCPATMK     =  '\G\s*<resync:';
my $RULEVARPATMK    =  '\G\s*<rulevar:';
my $DEFERPATMK      =  '\G\s*<defer:';
my $TOKENPATMK      =  '\G\s*<token:';
my $AUTOERRORMK     =  '\G\s*<error(\??)>';
my $MSGERRORMK      =  '\G\s*<error(\??):';
my $NOCHECK         =  '\G\s*<nocheck>';
my $WARNMK          =  '\G\s*<warn((?::\s*(\d+)\s*)?)>';
my $HINTMK          =  '\G\s*<hint>';
my $TRACEBUILDMK    =  '\G\s*<trace_build((?::\s*(\d+)\s*)?)>';
my $TRACEPARSEMK    =  '\G\s*<trace_parse((?::\s*(\d+)\s*)?)>';
my $UNCOMMITPROD    = $PROD.'\s*<uncommit';
my $ERRORPROD       = $PROD.'\s*<error';
my $LONECOLON       =  '\G\s*:';
my $OTHER           =  '\G\s*([^\s]+)';

my @lines = 0;

sub _generate
{
    my ($self, $grammar, $replace, $isimplicit, $isleftop) = (@_, 0);

    my $aftererror = 0;
    my $lookahead = 0;
    my $lookaheadspec = "";
    my $must_pop_lines;
    if (! $lines[-1]) {
        push @lines, _linecount($grammar) ;
        $must_pop_lines = 1;
    }
    $self->{_check}{itempos} = ($grammar =~ /\@itempos\b|\$itempos\s*\[/)
        unless $self->{_check}{itempos};
    for (qw(thisoffset thiscolumn prevline prevoffset prevcolumn))
    {
        $self->{_check}{$_} =
            ($grammar =~ /\$$_/) || $self->{_check}{itempos}
                unless $self->{_check}{$_};
    }
    my $line;

    my $rule = undef;
    my $prod = undef;
    my $item = undef;
    my $lastgreedy = '';
    pos $grammar = 0;
    study $grammar;

    local $::RD_HINT  = $::RD_HINT;
    local $::RD_WARN  = $::RD_WARN;
    local $::RD_TRACE = $::RD_TRACE;
    local $::RD_CHECK = $::RD_CHECK;

    while (pos $grammar < length $grammar)
    {
        $line = $lines[-1] - _linecount($grammar) + 1;
        my $commitonly;
        my $code = "";
        my @components = ();
        if ($grammar =~ m/$COMMENT/gco)
        {
            _parse("a comment",0,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
            next;
        }
        elsif ($grammar =~ m/$NEGLOOKAHEAD/gco)
        {
            _parse("a negative lookahead",$aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
            $lookahead = $lookahead ? -$lookahead : -1;
            $lookaheadspec .= $1;
            next;   # SKIP LOOKAHEAD RESET AT END OF while LOOP
        }
        elsif ($grammar =~ m/$POSLOOKAHEAD/gco)
        {
            _parse("a positive lookahead",$aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
            $lookahead = $lookahead ? $lookahead : 1;
            $lookaheadspec .= $1;
            next;   # SKIP LOOKAHEAD RESET AT END OF while LOOP
        }
        elsif ($grammar =~ m/(?=$ACTION)/gco
            and do { ($code) = extract_codeblock($grammar); $code })
        {
            _parse("an action", $aftererror, $line, $code);
            $item = new Parse::RecDescent::Action($code,$lookahead,$line);
            $prod and $prod->additem($item)
                  or  $self->_addstartcode($code);
        }
        elsif ($grammar =~ m/(?=$IMPLICITSUBRULE)/gco
            and do { ($code) = extract_codeblock($grammar,'{([',undef,'(',1);
                $code })
        {
            $code =~ s/\A\s*\(|\)\Z//g;
            _parse("an implicit subrule", $aftererror, $line,
                "( $code )");
            my $implicit = $rule->nextimplicit;
            return undef
                if !$self->_generate("$implicit : $code",$replace,1);
            my $pos = pos $grammar;
            substr($grammar,$pos,0,$implicit);
            pos $grammar = $pos;;
        }
        elsif ($grammar =~ m/$ENDDIRECTIVEMK/gco)
        {

        # EXTRACT TRAILING REPETITION SPECIFIER (IF ANY)

            my ($minrep,$maxrep) = (1,$MAXREP);
            if ($grammar =~ m/\G[(]/gc)
            {
                pos($grammar)--;

                if ($grammar =~ m/$OPTIONAL/gco)
                    { ($minrep, $maxrep) = (0,1) }
                elsif ($grammar =~ m/$ANY/gco)
                    { $minrep = 0 }
                elsif ($grammar =~ m/$EXACTLY/gco)
                    { ($minrep, $maxrep) = ($1,$1) }
                elsif ($grammar =~ m/$BETWEEN/gco)
                    { ($minrep, $maxrep) = ($1,$2) }
                elsif ($grammar =~ m/$ATLEAST/gco)
                    { $minrep = $1 }
                elsif ($grammar =~ m/$ATMOST/gco)
                    { $maxrep = $1 }
                elsif ($grammar =~ m/$MANY/gco)
                    { }
                elsif ($grammar =~ m/$BADREP/gco)
                {
                    _parse("an invalid repetition specifier", 0,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
                    _error("Incorrect specification of a repeated directive",
                           $line);
                    _hint("Repeated directives cannot have
                           a maximum repetition of zero, nor can they have
                           negative components in their ranges.");
                }
            }

            $prod && $prod->enddirective($line,$minrep,$maxrep);
        }
        elsif ($grammar =~ m/\G\s*<[^m]/gc)
        {
            pos($grammar)-=2;

            if ($grammar =~ m/$OPMK/gco)
            {
                # $DB::single=1;
                _parse("a $1-associative operator directive", $aftererror, $line, "<$1op:...>");
                $prod->adddirective($1, $line,$2||'');
            }
            elsif ($grammar =~ m/$UNCOMMITMK/gco)
            {
                _parse("an uncommit marker", $aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
                $item = new Parse::RecDescent::Directive('$commit=0;1',
                                  $lookahead,$line,"<uncommit>");
                $prod and $prod->additem($item)
                      or  _no_rule("<uncommit>",$line);
            }
            elsif ($grammar =~ m/$QUOTELIKEMK/gco)
            {
                _parse("an perl quotelike marker", $aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
                $item = new Parse::RecDescent::Directive(
                    'my ($match,@res);
                     ($match,$text,undef,@res) =
                          Text::Balanced::extract_quotelike($text,$skip);
                      $match ? \@res : undef;
                    ', $lookahead,$line,"<perl_quotelike>");
                $prod and $prod->additem($item)
                      or  _no_rule("<perl_quotelike>",$line);
            }
            elsif ($grammar =~ m/$CODEBLOCKMK/gco)
            {
                my $outer = $1||"{}";
                _parse("an perl codeblock marker", $aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
                $item = new Parse::RecDescent::Directive(
                    'Text::Balanced::extract_codeblock($text,undef,$skip,\''.$outer.'\');
                    ', $lookahead,$line,"<perl_codeblock>");
                $prod and $prod->additem($item)
                      or  _no_rule("<perl_codeblock>",$line);
            }
            elsif ($grammar =~ m/$VARIABLEMK/gco)
            {
                _parse("an perl variable marker", $aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
                $item = new Parse::RecDescent::Directive(
                    'Text::Balanced::extract_variable($text,$skip);
                    ', $lookahead,$line,"<perl_variable>");
                $prod and $prod->additem($item)
                      or  _no_rule("<perl_variable>",$line);
            }
            elsif ($grammar =~ m/$NOCHECKMK/gco)
            {
                _parse("a disable checking marker", $aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
                if ($rule)
                {
                    _error("<nocheck> directive not at start of grammar", $line);
                    _hint("The <nocheck> directive can only
                           be specified at the start of a
                           grammar (before the first rule
                           is defined.");
                }
                else
                {
                    local $::RD_CHECK = 1;
                }
            }
            elsif ($grammar =~ m/$AUTOSTUBMK/gco)
            {
                _parse("an autostub marker", $aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
                $::RD_AUTOSTUB = "";
            }
            elsif ($grammar =~ m/$AUTORULEMK/gco)
            {
                _parse("an autorule marker", $aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
                $::RD_AUTOSTUB = $1;
            }
            elsif ($grammar =~ m/$AUTOTREEMK/gco)
            {
                my $base = defined($1) ? $1 : "";
                my $current_match = substr($grammar, $-[0], $+[0] - $-[0]);
                $base .= "::" if $base && $base !~ /::$/;
                _parse("an autotree marker", $aftererror,$line, $current_match);
                if ($rule)
                {
                    _error("<autotree> directive not at start of grammar", $line);
                    _hint("The <autotree> directive can only
                           be specified at the start of a
                           grammar (before the first rule
                           is defined.");
                }
                else
                {
                    undef $self->{_AUTOACTION};
                    $self->{_AUTOTREE}{NODE}
                        = new Parse::RecDescent::Action(q({bless \%item, ').$base.q('.$item[0]}),0,-1);
                    $self->{_AUTOTREE}{TERMINAL}
                        = new Parse::RecDescent::Action(q({bless {__VALUE__=>$item[1]}, ').$base.q('.$item[0]}),0,-1);
                }
            }

            elsif ($grammar =~ m/$REJECTMK/gco)
            {
                _parse("an reject marker", $aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
                $item = new Parse::RecDescent::UncondReject($lookahead,$line,"<reject>");
                $prod and $prod->additem($item)
                      or  _no_rule("<reject>",$line);
            }
            elsif ($grammar =~ m/(?=$CONDREJECTMK)/gco
                and do { ($code) = extract_codeblock($grammar,'{',undef,'<');
                      $code })
            {
                _parse("a (conditional) reject marker", $aftererror,$line, $code );
                $code =~ /\A\s*<reject:(.*)>\Z/s;
                my $cond = $1;
                $item = new Parse::RecDescent::Directive(
                          "($1) ? undef : 1", $lookahead,$line,"<reject:$cond>");
                $prod and $prod->additem($item)
                      or  _no_rule("<reject:$cond>",$line);
            }
            elsif ($grammar =~ m/(?=$SCOREMK)/gco
                and do { ($code) = extract_codeblock($grammar,'{',undef,'<');
                      $code })
            {
                _parse("a score marker", $aftererror,$line, $code );
                $code =~ /\A\s*<score:(.*)>\Z/s;
                $prod and $prod->addscore($1, $lookahead, $line)
                      or  _no_rule($code,$line);
            }
            elsif ($grammar =~ m/(?=$AUTOSCOREMK)/gco
                and do { ($code) = extract_codeblock($grammar,'{',undef,'<');
                     $code;
                       } )
            {
                _parse("an autoscore specifier", $aftererror,$line,$code);
                $code =~ /\A\s*<autoscore:(.*)>\Z/s;

                $rule and $rule->addautoscore($1,$self)
                      or  _no_rule($code,$line);

                $item = new Parse::RecDescent::UncondReject($lookahead,$line,$code);
                $prod and $prod->additem($item)
                      or  _no_rule($code,$line);
            }
            elsif ($grammar =~ m/$RESYNCMK/gco)
            {
                _parse("a resync to newline marker", $aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
                $item = new Parse::RecDescent::Directive(
                          'if ($text =~ s/(\A[^\n]*\n)//) { $return = 0; $1; } else { undef }',
                          $lookahead,$line,"<resync>");
                $prod and $prod->additem($item)
                      or  _no_rule("<resync>",$line);
            }
            elsif ($grammar =~ m/(?=$RESYNCPATMK)/gco
                and do { ($code) = extract_bracketed($grammar,'<');
                      $code })
            {
                _parse("a resync with pattern marker", $aftererror,$line, $code );
                $code =~ /\A\s*<resync:(.*)>\Z/s;
                $item = new Parse::RecDescent::Directive(
                          'if ($text =~ s/(\A'.$1.')//) { $return = 0; $1; } else { undef }',
                          $lookahead,$line,$code);
                $prod and $prod->additem($item)
                      or  _no_rule($code,$line);
            }
            elsif ($grammar =~ m/(?=$SKIPMK)/gco
                and do { ($code) = extract_codeblock($grammar,'<');
                      $code })
            {
                _parse("a skip marker", $aftererror,$line, $code );
                $code =~ /\A\s*<skip:(.*)>\Z/s;
                if ($rule) {
                    $item = new Parse::RecDescent::Directive(
                        'my $oldskip = $skip; $skip='.$1.'; $oldskip',
                        $lookahead,$line,$code);
                    $prod and $prod->additem($item)
                      or  _no_rule($code,$line);
                } else {
                    #global <skip> directive
                    $self->{skip} = $1;
                }
            }
            elsif ($grammar =~ m/(?=$RULEVARPATMK)/gco
                and do { ($code) = extract_codeblock($grammar,'{',undef,'<');
                     $code;
                       } )
            {
                _parse("a rule variable specifier", $aftererror,$line,$code);
                $code =~ /\A\s*<rulevar:(.*)>\Z/s;

                $rule and $rule->addvar($1,$self)
                      or  _no_rule($code,$line);

                $item = new Parse::RecDescent::UncondReject($lookahead,$line,$code);
                $prod and $prod->additem($item)
                      or  _no_rule($code,$line);
            }
            elsif ($grammar =~ m/(?=$AUTOACTIONPATMK)/gco
                and do { ($code) = extract_codeblock($grammar,'{',undef,'<');
                     $code;
                       } )
            {
                _parse("an autoaction specifier", $aftererror,$line,$code);
                $code =~ s/\A\s*<autoaction:(.*)>\Z/$1/s;
                if ($code =~ /\A\s*[^{]|[^}]\s*\Z/) {
                    $code = "{ $code }"
                }
        $self->{_check}{itempos} =
            $code =~ /\@itempos\b|\$itempos\s*\[/;
        $self->{_AUTOACTION}
            = new Parse::RecDescent::Action($code,0,-$line)
            }
            elsif ($grammar =~ m/(?=$DEFERPATMK)/gco
                and do { ($code) = extract_codeblock($grammar,'{',undef,'<');
                     $code;
                       } )
            {
                _parse("a deferred action specifier", $aftererror,$line,$code);
                $code =~ s/\A\s*<defer:(.*)>\Z/$1/s;
                if ($code =~ /\A\s*[^{]|[^}]\s*\Z/)
                {
                    $code = "{ $code }"
                }

                $item = new Parse::RecDescent::Directive(
                          "push \@{\$thisparser->{deferred}}, sub $code;",
                          $lookahead,$line,"<defer:$code>");
                $prod and $prod->additem($item)
                      or  _no_rule("<defer:$code>",$line);

                $self->{deferrable} = 1;
            }
            elsif ($grammar =~ m/(?=$TOKENPATMK)/gco
                and do { ($code) = extract_codeblock($grammar,'{',undef,'<');
                     $code;
                       } )
            {
                _parse("a token constructor", $aftererror,$line,$code);
                $code =~ s/\A\s*<token:(.*)>\Z/$1/s;

                my $types = eval 'no strict; local $SIG{__WARN__} = sub {0}; my @arr=('.$code.'); @arr' || ();
                if (!$types)
                {
                    _error("Incorrect token specification: \"$@\"", $line);
                    _hint("The <token:...> directive requires a list
                           of one or more strings representing possible
                           types of the specified token. For example:
                           <token:NOUN,VERB>");
                }
                else
                {
                    $item = new Parse::RecDescent::Directive(
                              'no strict;
                               $return = { text => $item[-1] };
                               @{$return->{type}}{'.$code.'} = (1..'.$types.');',
                              $lookahead,$line,"<token:$code>");
                    $prod and $prod->additem($item)
                          or  _no_rule("<token:$code>",$line);
                }
            }
            elsif ($grammar =~ m/$COMMITMK/gco)
            {
                _parse("an commit marker", $aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
                $item = new Parse::RecDescent::Directive('$commit = 1',
                                  $lookahead,$line,"<commit>");
                $prod and $prod->additem($item)
                      or  _no_rule("<commit>",$line);
            }
            elsif ($grammar =~ m/$NOCHECKMK/gco) {
                _parse("an hint request", $aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
        $::RD_CHECK = 0;
        }
            elsif ($grammar =~ m/$HINTMK/gco) {
                _parse("an hint request", $aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
        $::RD_HINT = $self->{__HINT__} = 1;
        }
            elsif ($grammar =~ m/$WARNMK/gco) {
                _parse("an warning request", $aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
        $::RD_WARN = $self->{__WARN__} = $1 ? $2+0 : 1;
        }
            elsif ($grammar =~ m/$TRACEBUILDMK/gco) {
                _parse("an grammar build trace request", $aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
        $::RD_TRACE = $1 ? $2+0 : 1;
        }
            elsif ($grammar =~ m/$TRACEPARSEMK/gco) {
                _parse("an parse trace request", $aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
        $self->{__TRACE__} = $1 ? $2+0 : 1;
        }
            elsif ($grammar =~ m/$AUTOERRORMK/gco)
            {
                $commitonly = $1;
                _parse("an error marker", $aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
                $item = new Parse::RecDescent::Error('',$lookahead,$1,$line);
                $prod and $prod->additem($item)
                      or  _no_rule("<error>",$line);
                $aftererror = !$commitonly;
            }
            elsif ($grammar =~ m/(?=$MSGERRORMK)/gco
                and do { $commitonly = $1;
                     ($code) = extract_bracketed($grammar,'<');
                    $code })
            {
                _parse("an error marker", $aftererror,$line,$code);
                $code =~ /\A\s*<error\??:(.*)>\Z/s;
                $item = new Parse::RecDescent::Error($1,$lookahead,$commitonly,$line);
                $prod and $prod->additem($item)
                      or  _no_rule("$code",$line);
                $aftererror = !$commitonly;
            }
            elsif (do { $commitonly = $1;
                     ($code) = extract_bracketed($grammar,'<');
                    $code })
            {
                if ($code =~ /^<[A-Z_]+>$/)
                {
                    _error("Token items are not yet
                    supported: \"$code\"",
                           $line);
                    _hint("Items like $code that consist of angle
                    brackets enclosing a sequence of
                    uppercase characters will eventually
                    be used to specify pre-lexed tokens
                    in a grammar. That functionality is not
                    yet implemented. Or did you misspell
                    \"$code\"?");
                }
                else
                {
                    _error("Untranslatable item encountered: \"$code\"",
                           $line);
                    _hint("Did you misspell \"$code\"
                           or forget to comment it out?");
                }
            }
        }
        elsif ($grammar =~ m/$RULE/gco)
        {
            _parseunneg("a rule declaration", 0,
                    $lookahead,$line, substr($grammar, $-[0], $+[0] - $-[0]) ) or next;
            my $rulename = $1;
            if ($rulename =~ /Replace|Extend|Precompile|PrecompiledRuntime|Save/ )
            {
                _warn(2,"Rule \"$rulename\" hidden by method
                       Parse::RecDescent::$rulename",$line)
                and
                _hint("The rule named \"$rulename\" cannot be directly
                       called through the Parse::RecDescent object
                       for this grammar (although it may still
                       be used as a subrule of other rules).
                       It can't be directly called because
                       Parse::RecDescent::$rulename is already defined (it
                       is the standard method of all
                       parsers).");
            }
            $rule = new Parse::RecDescent::Rule($rulename,$self,$line,$replace);
            $prod->check_pending($line) if $prod;
            $prod = $rule->addprod( new Parse::RecDescent::Production );
            $aftererror = 0;
        }
        elsif ($grammar =~ m/$UNCOMMITPROD/gco)
        {
            pos($grammar)-=9;
            _parseunneg("a new (uncommitted) production",
                    0, $lookahead, $line, substr($grammar, $-[0], $+[0] - $-[0]) ) or next;

            $prod->check_pending($line) if $prod;
            $prod = new Parse::RecDescent::Production($line,1);
            $rule and $rule->addprod($prod)
                  or  _no_rule("<uncommit>",$line);
            $aftererror = 0;
        }
        elsif ($grammar =~ m/$ERRORPROD/gco)
        {
            pos($grammar)-=6;
            _parseunneg("a new (error) production", $aftererror,
                    $lookahead,$line, substr($grammar, $-[0], $+[0] - $-[0]) ) or next;
            $prod->check_pending($line) if $prod;
            $prod = new Parse::RecDescent::Production($line,0,1);
            $rule and $rule->addprod($prod)
                  or  _no_rule("<error>",$line);
            $aftererror = 0;
        }
        elsif ($grammar =~ m/$PROD/gco)
        {
            _parseunneg("a new production", 0,
                    $lookahead,$line, substr($grammar, $-[0], $+[0] - $-[0]) ) or next;
            $rule
              and (!$prod || $prod->check_pending($line))
              and $prod = $rule->addprod(new Parse::RecDescent::Production($line))
            or  _no_rule("production",$line);
            $aftererror = 0;
        }
        elsif ($grammar =~ m/$LITERAL/gco)
        {
            my $literal = $1;
            ($code = $literal) =~ s/\\\\/\\/g;
            _parse("a literal terminal", $aftererror,$line,$literal);
            $item = new Parse::RecDescent::Literal($code,$lookahead,$line);
            $prod and $prod->additem($item)
                  or  _no_rule("literal terminal",$line,"'$literal'");
        }
        elsif ($grammar =~ m/$INTERPLIT/gco)
        {
            _parse("an interpolated literal terminal", $aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
            $item = new Parse::RecDescent::InterpLit($1,$lookahead,$line);
            $prod and $prod->additem($item)
                  or  _no_rule("interpolated literal terminal",$line,"'$1'");
        }
        elsif ($grammar =~ m/$TOKEN/gco)
        {
            _parse("a /../ pattern terminal", $aftererror,$line, substr($grammar, $-[0], $+[0] - $-[0]) );
            $item = new Parse::RecDescent::Token($1,'/',$3?$3:'',$lookahead,$line);
            $prod and $prod->additem($item)
                  or  _no_rule("pattern terminal",$line,"/$1/");
        }
        elsif ($grammar =~ m/(?=$MTOKEN)/gco
            and do { ($code, undef, @components)
                    = extract_quotelike($grammar);
                 $code }
              )

        {
            _parse("an m/../ pattern terminal", $aftererror,$line,$code);
            $item = new Parse::RecDescent::Token(@components[3,2,8],
                                 $lookahead,$line);
            $prod and $prod->additem($item)
                  or  _no_rule("pattern terminal",$line,$code);
        }
        elsif ($grammar =~ m/(?=$MATCHRULE)/gco
                and do { ($code) = extract_bracketed($grammar,'<');
                     $code
                       }
               or $grammar =~ m/$SUBRULE/gco
                and $code = $1)
        {
            my $name = $code;
            my $matchrule = 0;
            if (substr($name,0,1) eq '<')
            {
                $name =~ s/$MATCHRULE\s*//;
                $name =~ s/\s*>\Z//;
                $matchrule = 1;
            }

        # EXTRACT TRAILING ARG LIST (IF ANY)

            my ($argcode) = extract_codeblock($grammar, "[]",'') || '';

        # EXTRACT TRAILING REPETITION SPECIFIER (IF ANY)

            if ($grammar =~ m/\G[(]/gc)
            {
                pos($grammar)--;

                if ($grammar =~ m/$OPTIONAL/gco)
                {
                    _parse("an zero-or-one subrule match", $aftererror,$line,"$code$argcode($1)");
                    $item = new Parse::RecDescent::Repetition($name,$1,0,1,
                                       $lookahead,$line,
                                       $self,
                                       $matchrule,
                                       $argcode);
                    $prod and $prod->additem($item)
                          or  _no_rule("repetition",$line,"$code$argcode($1)");

                    !$matchrule and $rule and $rule->addcall($name);
                }
                elsif ($grammar =~ m/$ANY/gco)
                {
                    _parse("a zero-or-more subrule match", $aftererror,$line,"$code$argcode($1)");
                    if ($2)
                    {
                        my $pos = pos $grammar;
                        substr($grammar,$pos,0,
                               "<leftop='$name(s?)': $name $2 $name>(s?) ");

                        pos $grammar = $pos;
                    }
                    else
                    {
                        $item = new Parse::RecDescent::Repetition($name,$1,0,$MAXREP,
                                           $lookahead,$line,
                                           $self,
                                           $matchrule,
                                           $argcode);
                        $prod and $prod->additem($item)
                              or  _no_rule("repetition",$line,"$code$argcode($1)");

                        !$matchrule and $rule and $rule->addcall($name);

                        _check_insatiable($name,$1,$grammar,$line) if $::RD_CHECK;
                    }
                }
                elsif ($grammar =~ m/$MANY/gco)
                {
                    _parse("a one-or-more subrule match", $aftererror,$line,"$code$argcode($1)");
                    if ($2)
                    {
                        # $DB::single=1;
                        my $pos = pos $grammar;
                        substr($grammar,$pos,0,
                               "<leftop='$name(s)': $name $2 $name> ");

                        pos $grammar = $pos;
                    }
                    else
                    {
                        $item = new Parse::RecDescent::Repetition($name,$1,1,$MAXREP,
                                           $lookahead,$line,
                                           $self,
                                           $matchrule,
                                           $argcode);

                        $prod and $prod->additem($item)
                              or  _no_rule("repetition",$line,"$code$argcode($1)");

                        !$matchrule and $rule and $rule->addcall($name);

                        _check_insatiable($name,$1,$grammar,$line) if $::RD_CHECK;
                    }
                }
                elsif ($grammar =~ m/$EXACTLY/gco)
                {
                    _parse("an exactly-$1-times subrule match", $aftererror,$line,"$code$argcode($1)");
                    if ($2)
                    {
                        my $pos = pos $grammar;
                        substr($grammar,$pos,0,
                               "<leftop='$name($1)': $name $2 $name>($1) ");

                        pos $grammar = $pos;
                    }
                    else
                    {
                        $item = new Parse::RecDescent::Repetition($name,$1,$1,$1,
                                           $lookahead,$line,
                                           $self,
                                           $matchrule,
                                           $argcode);
                        $prod and $prod->additem($item)
                              or  _no_rule("repetition",$line,"$code$argcode($1)");

                        !$matchrule and $rule and $rule->addcall($name);
                    }
                }
                elsif ($grammar =~ m/$BETWEEN/gco)
                {
                    _parse("a $1-to-$2 subrule match", $aftererror,$line,"$code$argcode($1..$2)");
                    if ($3)
                    {
                        my $pos = pos $grammar;
                        substr($grammar,$pos,0,
                               "<leftop='$name($1..$2)': $name $3 $name>($1..$2) ");

                        pos $grammar = $pos;
                    }
                    else
                    {
                        $item = new Parse::RecDescent::Repetition($name,"$1..$2",$1,$2,
                                           $lookahead,$line,
                                           $self,
                                           $matchrule,
                                           $argcode);
                        $prod and $prod->additem($item)
                              or  _no_rule("repetition",$line,"$code$argcode($1..$2)");

                        !$matchrule and $rule and $rule->addcall($name);
                    }
                }
                elsif ($grammar =~ m/$ATLEAST/gco)
                {
                    _parse("a $1-or-more subrule match", $aftererror,$line,"$code$argcode($1..)");
                    if ($2)
                    {
                        my $pos = pos $grammar;
                        substr($grammar,$pos,0,
                               "<leftop='$name($1..)': $name $2 $name>($1..) ");

                        pos $grammar = $pos;
                    }
                    else
                    {
                        $item = new Parse::RecDescent::Repetition($name,"$1..",$1,$MAXREP,
                                           $lookahead,$line,
                                           $self,
                                           $matchrule,
                                           $argcode);
                        $prod and $prod->additem($item)
                              or  _no_rule("repetition",$line,"$code$argcode($1..)");

                        !$matchrule and $rule and $rule->addcall($name);
                        _check_insatiable($name,"$1..",$grammar,$line) if $::RD_CHECK;
                    }
                }
                elsif ($grammar =~ m/$ATMOST/gco)
                {
                    _parse("a one-to-$1 subrule match", $aftererror,$line,"$code$argcode(..$1)");
                    if ($2)
                    {
                        my $pos = pos $grammar;
                        substr($grammar,$pos,0,
                               "<leftop='$name(..$1)': $name $2 $name>(..$1) ");

                        pos $grammar = $pos;
                    }
                    else
                    {
                        $item = new Parse::RecDescent::Repetition($name,"..$1",1,$1,
                                           $lookahead,$line,
                                           $self,
                                           $matchrule,
                                           $argcode);
                        $prod and $prod->additem($item)
                              or  _no_rule("repetition",$line,"$code$argcode(..$1)");

                        !$matchrule and $rule and $rule->addcall($name);
                    }
                }
                elsif ($grammar =~ m/$BADREP/gco)
                {
                    my $current_match = substr($grammar, $-[0], $+[0] - $-[0]);
                    _parse("an subrule match with invalid repetition specifier", 0,$line, $current_match);
                    _error("Incorrect specification of a repeated subrule",
                           $line);
                    _hint("Repeated subrules like \"$code$argcode$current_match\" cannot have
                           a maximum repetition of zero, nor can they have
                           negative components in their ranges.");
                }
            }
            else
            {
                _parse("a subrule match", $aftererror,$line,$code);
                my $desc;
                if ($name=~/\A_alternation_\d+_of_production_\d+_of_rule/)
                    { $desc = $self->{"rules"}{$name}->expected }
                $item = new Parse::RecDescent::Subrule($name,
                                       $lookahead,
                                       $line,
                                       $desc,
                                       $matchrule,
                                       $argcode);

                $prod and $prod->additem($item)
                      or  _no_rule("(sub)rule",$line,$name);

                !$matchrule and $rule and $rule->addcall($name);
            }
        }
        elsif ($grammar =~ m/$LONECOLON/gco   )
        {
            _error("Unexpected colon encountered", $line);
            _hint("Did you mean \"|\" (to start a new production)?
                   Or perhaps you forgot that the colon
                   in a rule definition must be
                   on the same line as the rule name?");
        }
        elsif ($grammar =~ m/$ACTION/gco   ) # BAD ACTION, ALREADY FAILED
        {
            _error("Malformed action encountered",
                   $line);
            _hint("Did you forget the closing curly bracket
                   or is there a syntax error in the action?");
        }
        elsif ($grammar =~ m/$OTHER/gco   )
        {
            _error("Untranslatable item encountered: \"$1\"",
                   $line);
            _hint("Did you misspell \"$1\"
                   or forget to comment it out?");
        }

        if ($lookaheadspec =~ tr /././ > 3)
        {
            $lookaheadspec =~ s/\A\s+//;
            $lookahead = $lookahead<0
                    ? 'a negative lookahead ("...!")'
                    : 'a positive lookahead ("...")' ;
            _warn(1,"Found two or more lookahead specifiers in a
                   row.",$line)
            and
            _hint("Multiple positive and/or negative lookaheads
                   are simply multiplied together to produce a
                   single positive or negative lookahead
                   specification. In this case the sequence
                   \"$lookaheadspec\" was reduced to $lookahead.
                   Was this your intention?");
        }
        $lookahead = 0;
        $lookaheadspec = "";

        $grammar =~ m/\G\s+/gc;
    }

    if ($must_pop_lines) {
        pop @lines;
    }

    unless ($ERRORS or $isimplicit or !$::RD_CHECK)
    {
        $self->_check_grammar();
    }

    unless ($ERRORS or $isimplicit or $Parse::RecDescent::compiling)
    {
        my $code = $self->_code();
        if (defined $::RD_TRACE)
        {
            my $mode = ($nextnamespace eq "namespace000002") ? '>' : '>>';
            print STDERR "printing code (", length($code),") to RD_TRACE\n";
            local *TRACE_FILE;
            open TRACE_FILE, $mode, "RD_TRACE"
            and print TRACE_FILE "my \$ERRORS;\n$code"
            and close TRACE_FILE;
        }

        unless ( eval "$code 1" )
        {
            _error("Internal error in generated parser code!");
            $@ =~ s/at grammar/in grammar at/;
            _hint($@);
        }
    }

    if ($ERRORS and !_verbosity("HINT"))
    {
        local $::RD_HINT = defined $::RD_HINT ? $::RD_HINT : 1;
        _hint('Set $::RD_HINT (or -RD_HINT if you\'re using "perl -s")
               for hints on fixing these problems.  Use $::RD_HINT = 0
               to disable this message.');
    }
    if ($ERRORS) { $ERRORS=0; return }
    return $self;
}


sub _addstartcode($$)
{
    my ($self, $code) = @_;
    $code =~ s/\A\s*\{(.*)\}\Z/$1/s;

    $self->{"startcode"} .= "$code;\n";
}

# CHECK FOR GRAMMAR PROBLEMS....

sub _check_insatiable($$$$)
{
    my ($subrule,$repspec,$grammar,$line) = @_;
    pos($grammar)=pos($_[2]);
    return if $grammar =~ m/$OPTIONAL/gco || $grammar =~ m/$ANY/gco;
    my $min = 1;
    if ( $grammar =~ m/$MANY/gco
      || $grammar =~ m/$EXACTLY/gco
      || $grammar =~ m/$ATMOST/gco
      || $grammar =~ m/$BETWEEN/gco && do { $min=$2; 1 }
      || $grammar =~ m/$ATLEAST/gco && do { $min=$2; 1 }
      || $grammar =~ m/$SUBRULE(?!\s*:)/gco
       )
    {
        return unless $1 eq $subrule && $min > 0;
        my $current_match = substr($grammar, $-[0], $+[0] - $-[0]);
        _warn(3,"Subrule sequence \"$subrule($repspec) $current_match\" will
               (almost certainly) fail.",$line)
        and
        _hint("Unless subrule \"$subrule\" performs some cunning
               lookahead, the repetition \"$subrule($repspec)\" will
               insatiably consume as many matches of \"$subrule\" as it
               can, leaving none to match the \"$current_match\" that follows.");
    }
}

sub _check_grammar ($)
{
    my $self = shift;
    my $rules = $self->{"rules"};
    my $rule;
    foreach $rule ( values %$rules )
    {
        next if ! $rule->{"changed"};

    # CHECK FOR UNDEFINED RULES

        my $call;
        foreach $call ( @{$rule->{"calls"}} )
        {
            if (!defined ${$rules}{$call}
              &&!defined &{"Parse::RecDescent::$call"})
            {
                if (!defined $::RD_AUTOSTUB)
                {
                    _warn(3,"Undefined (sub)rule \"$call\"
                          used in a production.")
                    and
                    _hint("Will you be providing this rule
                           later, or did you perhaps
                           misspell \"$call\"? Otherwise
                           it will be treated as an
                           immediate <reject>.");
                    eval "sub $self->{namespace}::$call {undef}";
                }
                else    # EXPERIMENTAL
                {
                    my $rule = qq{'$call'};
                    if ($::RD_AUTOSTUB and $::RD_AUTOSTUB ne "1") {
                        $rule = $::RD_AUTOSTUB;
                    }
                    _warn(1,"Autogenerating rule: $call")
                    and
                    _hint("A call was made to a subrule
                           named \"$call\", but no such
                           rule was specified. However,
                           since \$::RD_AUTOSTUB
                           was defined, a rule stub
                           ($call : $rule) was
                           automatically created.");

                    $self->_generate("$call: $rule",0,1);
                }
            }
        }

    # CHECK FOR LEFT RECURSION

        if ($rule->isleftrec($rules))
        {
            _error("Rule \"$rule->{name}\" is left-recursive.");
            _hint("Redesign the grammar so it's not left-recursive.
                   That will probably mean you need to re-implement
                   repetitions using the '(s)' notation.
                   For example: \"$rule->{name}(s)\".");
            next;
        }

    # CHECK FOR PRODUCTIONS FOLLOWING EMPTY PRODUCTIONS
      {
          my $hasempty;
          my $prod;
          foreach $prod ( @{$rule->{"prods"}} ) {
              if ($hasempty) {
                  _error("Production " . $prod->describe . " for \"$rule->{name}\"
                         will never be reached (preceding empty production will
                         always match first).");
                  _hint("Reorder the grammar so that the empty production
                         is last in the list or productions.");
                  last;
              }
              $hasempty ||= $prod->isempty();
          }
      }
    }
}

# GENERATE ACTUAL PARSER CODE

sub _code($)
{
    my $self = shift;
    my $initial_skip = defined($self->{skip}) ?
      '$skip = ' . $self->{skip} . ';' :
      $self->_dump([$skip],[qw(skip)]);

    my $code = qq!
package $self->{namespace};
use strict;
use vars qw(\$skip \$AUTOLOAD $self->{localvars} );
\@$self->{namespace}\::ISA = ();
$initial_skip
$self->{startcode}

{
local \$SIG{__WARN__} = sub {0};
# PRETEND TO BE IN Parse::RecDescent NAMESPACE
*$self->{namespace}::AUTOLOAD   = sub
{
    no strict 'refs';
!
# This generated code uses ${"AUTOLOAD"} rather than $AUTOLOAD in
# order to avoid the circular reference documented here:
#    https://rt.perl.org/rt3/Public/Bug/Display.html?id=110248
# As a result of the investigation of
#    https://rt.cpan.org/Ticket/Display.html?id=53710
. qq!
    \${"AUTOLOAD"} =~ s/^$self->{namespace}/Parse::RecDescent/;
    goto &{\${"AUTOLOAD"}};
}
}

!;
    $code .= "push \@$self->{namespace}\::ISA, 'Parse::RecDescent';";
    $self->{"startcode"} = '';

    my $rule;
    # sort the rules to ensure the output is reproducible
    foreach $rule ( sort { $a->{name} cmp $b->{name} }
                    values %{$self->{"rules"}} )
    {
        if ($rule->{"changed"})
        {
            $code .= $rule->code($self->{"namespace"},$self);
            $rule->{"changed"} = 0;
        }
    }

    return $code;
}

# A wrapper for Data::Dumper->Dump, which localizes some variables to
# keep the output in a form suitable for Parse::RecDescent.
#
# List of variables and their defaults taken from
# $Data::Dumper::VERSION == 2.158

sub _dump {
	require Data::Dumper;

	#
	# Allow the user's settings to persist for some features in case
	# RD_TRACE is set.  These shouldn't affect the eval()-ability of
	# the resulting parser.
	#

	#local $Data::Dumper::Indent = 2;
	#local $Data::Dumper::Useqq      = 0;
	#local $Data::Dumper::Quotekeys  = 1;
	#local $Data::Dumper::Useperl = 0;

	#
	# These may affect whether the output is valid perl code for
	# eval(), and must be controlled. Set them to their default
	# values.
	#

	local $Data::Dumper::Purity     = 0;
	local $Data::Dumper::Pad        = "";
	local $Data::Dumper::Varname    = "VAR";
	local $Data::Dumper::Terse      = 0;
	local $Data::Dumper::Freezer    = "";
	local $Data::Dumper::Toaster    = "";
	local $Data::Dumper::Deepcopy   = 0;
	local $Data::Dumper::Bless      = "bless";
	local $Data::Dumper::Maxdepth   = 0;
	local $Data::Dumper::Pair       = ' => ';
	local $Data::Dumper::Deparse    = 0;
	local $Data::Dumper::Sparseseen = 0;

	#
	# Modify the below options from their defaults.
	#

	# Sort the keys to ensure the output is reproducible
	local $Data::Dumper::Sortkeys   = 1;

	# Don't stop recursing
	local $Data::Dumper::Maxrecurse = 0;

	return Data::Dumper->Dump(@_[1..$#_]);
}

# EXECUTING A PARSE....

sub AUTOLOAD    # ($parser, $text; $linenum, @args)
{
    croak "Could not find method: $AUTOLOAD\n" unless ref $_[0];
    my $class = ref($_[0]) || $_[0];
    my $text = ref($_[1]) eq 'SCALAR' ? ${$_[1]} : "$_[1]";
    $_[0]->{lastlinenum} = _linecount($text);
    $_[0]->{lastlinenum} += ($_[2]||0) if @_ > 2;
    $_[0]->{offsetlinenum} = $_[0]->{lastlinenum};
    $_[0]->{fulltext} = $text;
    $_[0]->{fulltextlen} = length $text;
    $_[0]->{linecounter_cache} = {};
    $_[0]->{deferred} = [];
    $_[0]->{errors} = [];
    my @args = @_[3..$#_];
    my $args = sub { [ @args ] };

    $AUTOLOAD =~ s/$class/$_[0]->{namespace}/;
    no strict "refs";

    local $::RD_WARN  = $::RD_WARN  || $_[0]->{__WARN__};
    local $::RD_HINT  = $::RD_HINT  || $_[0]->{__HINT__};
    local $::RD_TRACE = $::RD_TRACE || $_[0]->{__TRACE__};

    croak "Unknown starting rule ($AUTOLOAD) called\n"
        unless defined &$AUTOLOAD;
    my $retval = &{$AUTOLOAD}(
        $_[0], # $parser
        $text, # $text
        undef, # $repeating
        undef, # $_noactions
        $args, # \@args
        undef, # $_itempos
    );


    if (defined $retval)
    {
        foreach ( @{$_[0]->{deferred}} ) { &$_; }
    }
    else
    {
        foreach ( @{$_[0]->{errors}} ) { _error(@$_); }
    }

    if (ref $_[1] eq 'SCALAR') { ${$_[1]} = $text }

    $ERRORS = 0;
    return $retval;
}

sub _parserepeat($$$$$$$$$)    # RETURNS A REF TO AN ARRAY OF MATCHES
{
    my ($parser, $text, $prod, $min, $max, $_noactions, $expectation, $argcode, $_itempos) = @_;
    my @tokens = ();

    my $itemposfirst;
    my $reps;
    for ($reps=0; $reps<$max;)
    {
        $expectation->at($text);
        my $_savetext = $text;
        my $prevtextlen = length $text;
        my $_tok;
        if (! defined ($_tok = &$prod($parser,$text,1,$_noactions,$argcode,$_itempos)))
        {
            $text = $_savetext;
            last;
        }

        if (defined($_itempos) and !defined($itemposfirst))
        {
            $itemposfirst = Parse::RecDescent::Production::_duplicate_itempos($_itempos);
        }

        push @tokens, $_tok if defined $_tok;
        last if ++$reps >= $min and $prevtextlen == length $text;
    }

    do { $expectation->failed(); return undef} if $reps<$min;

    if (defined $itemposfirst)
    {
        Parse::RecDescent::Production::_update_itempos($_itempos, $itemposfirst, undef, [qw(from)]);
    }

    $_[1] = $text;
    return [@tokens];
}

sub set_autoflush {
    my $orig_selected = select $_[0];
    $| = 1;
    select $orig_selected;
    return;
}

# ERROR REPORTING....

sub _write_ERROR {
    my ($errorprefix, $errortext) = @_;
    return if $errortext !~ /\S/;
    $errorprefix =~ s/\s+\Z//;
    local $^A = q{};

    formline(<<'END_FORMAT', $errorprefix, $errortext);
@>>>>>>>>>>>>>>>>>>>>: ^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
END_FORMAT
    formline(<<'END_FORMAT', $errortext);
~~                     ^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
END_FORMAT
    print {*STDERR} $^A;
}

# TRACING

my $TRACE_FORMAT = <<'END_FORMAT';
@>|@|||||||||@^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<|
  | ~~       |^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<|
END_FORMAT

my $TRACECONTEXT_FORMAT = <<'END_FORMAT';
@>|@|||||||||@                                      |^<<<<<<<<<<<<<<<<<<<<<<<<<<<
  | ~~       |                                      |^<<<<<<<<<<<<<<<<<<<<<<<<<<<
END_FORMAT

sub _write_TRACE {
    my ($tracelevel, $tracerulename, $tracemsg) = @_;
    return if $tracemsg !~ /\S/;
    $tracemsg =~ s/\s*\Z//;
    local $^A = q{};
    my $bar = '|';
    formline($TRACE_FORMAT, $tracelevel, $tracerulename, $bar, $tracemsg, $tracemsg);
    print {*STDERR} $^A;
}

sub _write_TRACECONTEXT {
    my ($tracelevel, $tracerulename, $tracecontext) = @_;
    return if $tracecontext !~ /\S/;
    $tracecontext =~ s/\s*\Z//;
    local $^A = q{};
    my $bar = '|';
    formline($TRACECONTEXT_FORMAT, $tracelevel, $tracerulename, $bar, $tracecontext, $tracecontext);
    print {*STDERR} $^A;
}

sub _verbosity($)
{
       defined $::RD_TRACE
    or defined $::RD_HINT    and  $::RD_HINT   and $_[0] =~ /ERRORS|WARN|HINT/
    or defined $::RD_WARN    and  $::RD_WARN   and $_[0] =~ /ERRORS|WARN/
    or defined $::RD_ERRORS  and  $::RD_ERRORS and $_[0] =~ /ERRORS/
}

sub _error($;$)
{
    $ERRORS++;
    return 0 if ! _verbosity("ERRORS");
    my $errortext   = $_[0];
    my $errorprefix = "ERROR" .  ($_[1] ? " (line $_[1])" : "");
    $errortext =~ s/\s+/ /g;
    print {*STDERR} "\n" if _verbosity("WARN");
    _write_ERROR($errorprefix, $errortext);
    return 1;
}

sub _warn($$;$)
{
    return 0 unless _verbosity("WARN") && ($::RD_HINT || $_[0] >= ($::RD_WARN||1));
    my $errortext   = $_[1];
    my $errorprefix = "Warning" .  ($_[2] ? " (line $_[2])" : "");
    print {*STDERR} "\n" if _verbosity("HINT");
    $errortext =~ s/\s+/ /g;
    _write_ERROR($errorprefix, $errortext);
    return 1;
}

sub _hint($)
{
    return 0 unless $::RD_HINT;
    my $errortext = $_[0];
    my $errorprefix = "Hint" .  ($_[1] ? " (line $_[1])" : "");
    $errortext =~ s/\s+/ /g;
    _write_ERROR($errorprefix, $errortext);
    return 1;
}

sub _tracemax($)
{
    if (defined $::RD_TRACE
        && $::RD_TRACE =~ /\d+/
        && $::RD_TRACE>1
        && $::RD_TRACE+10<length($_[0]))
    {
        my $count = length($_[0]) - $::RD_TRACE;
        return substr($_[0],0,$::RD_TRACE/2)
            . "...<$count>..."
            . substr($_[0],-$::RD_TRACE/2);
    }
    else
    {
        return substr($_[0],0,500);
    }
}

sub _tracefirst($)
{
    if (defined $::RD_TRACE
        && $::RD_TRACE =~ /\d+/
        && $::RD_TRACE>1
        && $::RD_TRACE+10<length($_[0]))
    {
        my $count = length($_[0]) - $::RD_TRACE;
        return substr($_[0],0,$::RD_TRACE) . "...<+$count>";
    }
    else
    {
        return substr($_[0],0,500);
    }
}

my $lastcontext = '';
my $lastrulename = '';
my $lastlevel = '';

sub _trace($;$$$)
{
    my $tracemsg      = $_[0];
    my $tracecontext  = $_[1]||$lastcontext;
    my $tracerulename = $_[2]||$lastrulename;
    my $tracelevel    = $_[3]||$lastlevel;
    if ($tracerulename) { $lastrulename = $tracerulename }
    if ($tracelevel)    { $lastlevel = $tracelevel }

    $tracecontext =~ s/\n/\\n/g;
    $tracecontext =~ s/\s+/ /g;
    $tracerulename = qq{$tracerulename};
    _write_TRACE($tracelevel, $tracerulename, $tracemsg);
    if ($tracecontext ne $lastcontext)
    {
        if ($tracecontext)
        {
            $lastcontext = _tracefirst($tracecontext);
            $tracecontext = qq{"$tracecontext"};
        }
        else
        {
            $tracecontext = qq{<NO TEXT LEFT>};
        }
        _write_TRACECONTEXT($tracelevel, $tracerulename, $tracecontext);
    }
}

sub _matchtracemessage
{
    my ($self, $reject) = @_;

    my $prefix = '';
    my $postfix = '';
    my $matched = not $reject;
    my @t = ("Matched", "Didn't match");
    if (exists $self->{lookahead} and $self->{lookahead})
    {
        $postfix = $reject ? "(reject)" : "(keep)";
        $prefix = "...";
        if ($self->{lookahead} < 0)
        {
            $prefix .= '!';
            $matched = not $matched;
        }
    }
    $prefix . ($matched ? $t[0] : $t[1]) . $postfix;
}

sub _parseunneg($$$$$)
{
    _parse($_[0],$_[1],$_[3],$_[4]);
    if ($_[2]<0)
    {
        _error("Can't negate \"$_[4]\".",$_[3]);
        _hint("You can't negate $_[0]. Remove the \"...!\" before
               \"$_[4]\".");
        return 0;
    }
    return 1;
}

sub _parse($$$$)
{
    my $what = $_[3];
       $what =~ s/^\s+//;
    if ($_[1])
    {
        _warn(3,"Found $_[0] ($what) after an unconditional <error>",$_[2])
        and
        _hint("An unconditional <error> always causes the
               production containing it to immediately fail.
               \u$_[0] that follows an <error>
               will never be reached.  Did you mean to use
               <error?> instead?");
    }

    return if ! _verbosity("TRACE");
    my $errortext = "Treating \"$what\" as $_[0]";
    my $errorprefix = "Parse::RecDescent";
    $errortext =~ s/\s+/ /g;
    _write_ERROR($errorprefix, $errortext);
}

sub _linecount($) {
    scalar substr($_[0], pos $_[0]||0) =~ tr/\n//
}


package main;

use vars qw ( $RD_ERRORS $RD_WARN $RD_HINT $RD_TRACE $RD_CHECK );
$::RD_CHECK = 1;
$::RD_ERRORS = 1;
$::RD_WARN = 3;

1;

__END__

=head1 NAME

Parse::RecDescent - Generate Recursive-Descent Parsers

=head1 VERSION

This document describes version 1.967015 of Parse::RecDescent
released April 4th, 2017.

=head1 SYNOPSIS

 use Parse::RecDescent;

 # Generate a parser from the specification in $grammar:

     $parser = new Parse::RecDescent ($grammar);

 # Generate a parser from the specification in $othergrammar

     $anotherparser = new Parse::RecDescent ($othergrammar);


 # Parse $text using rule 'startrule' (which must be
 # defined in $grammar):

    $parser->startrule($text);


 # Parse $text using rule 'otherrule' (which must also
 # be defined in $grammar):

     $parser->otherrule($text);


 # Change the universal token prefix pattern
 # before building a grammar
 # (the default is: '\s*'):

    $Parse::RecDescent::skip = '[ \t]+';


 # Replace productions of existing rules (or create new ones)
 # with the productions defined in $newgrammar:

    $parser->Replace($newgrammar);


 # Extend existing rules (or create new ones)
 # by adding extra productions defined in $moregrammar:

    $parser->Extend($moregrammar);


 # Global flags (useful as command line arguments under -s):

    $::RD_ERRORS       # unless undefined, report fatal errors
    $::RD_WARN         # unless undefined, also report non-fatal problems
    $::RD_HINT         # if defined, also suggestion remedies
    $::RD_TRACE        # if defined, also trace parsers' behaviour
    $::RD_AUTOSTUB     # if defined, generates "stubs" for undefined rules
    $::RD_AUTOACTION   # if defined, appends specified action to productions


=head1 DESCRIPTION

=head2 Overview

Parse::RecDescent incrementally generates top-down recursive-descent text
parsers from simple I<yacc>-like grammar specifications. It provides:

=over 4

=item *

Regular expressions or literal strings as terminals (tokens),

=item *

Multiple (non-contiguous) productions for any rule,

=item *

Repeated and optional subrules within productions,

=item *

Full access to Perl within actions specified as part of the grammar,

=item *

Simple automated error reporting during parser generation and parsing,

=item *

The ability to commit to, uncommit to, or reject particular
productions during a parse,

=item *

The ability to pass data up and down the parse tree ("down" via subrule
argument lists, "up" via subrule return values)

=item *

Incremental extension of the parsing grammar (even during a parse),

=item *

Precompilation of parser objects,

=item *

User-definable reduce-reduce conflict resolution via
"scoring" of matching productions.

=back

=head2 Using C<Parse::RecDescent>

Parser objects are created by calling C<Parse::RecDescent::new>, passing in a
grammar specification (see the following subsections). If the grammar is
correct, C<new> returns a blessed reference which can then be used to initiate
parsing through any rule specified in the original grammar. A typical sequence
looks like this:

    $grammar = q {
        # GRAMMAR SPECIFICATION HERE
         };

    $parser = new Parse::RecDescent ($grammar) or die "Bad grammar!\n";

    # acquire $text

    defined $parser->startrule($text) or print "Bad text!\n";

The rule through which parsing is initiated must be explicitly defined
in the grammar (i.e. for the above example, the grammar must include a
rule of the form: "startrule: <subrules>".

If the starting rule succeeds, its value (see below)
is returned. Failure to generate the original parser or failure to match a text
is indicated by returning C<undef>. Note that it's easy to set up grammars
that can succeed, but which return a value of 0, "0", or "".  So don't be
tempted to write:

    $parser->startrule($text) or print "Bad text!\n";

Normally, the parser has no effect on the original text. So in the
previous example the value of $text would be unchanged after having
been parsed.

If, however, the text to be matched is passed by reference:

    $parser->startrule(\$text)

then any text which was consumed during the match will be removed from the
start of $text.


=head2 Rules

In the grammar from which the parser is built, rules are specified by
giving an identifier (which must satisfy /[A-Za-z]\w*/), followed by a
colon I<on the same line>, followed by one or more productions,
separated by single vertical bars. The layout of the productions
is entirely free-format:

    rule1:  production1
     |  production2 |
    production3 | production4

At any point in the grammar previously defined rules may be extended with
additional productions. This is achieved by redeclaring the rule with the new
productions. Thus:

    rule1: a | b | c
    rule2: d | e | f
    rule1: g | h

is exactly equivalent to:

    rule1: a | b | c | g | h
    rule2: d | e | f

Each production in a rule consists of zero or more items, each of which
may be either: the name of another rule to be matched (a "subrule"),
a pattern or string literal to be matched directly (a "token"), a
block of Perl code to be executed (an "action"), a special instruction
to the parser (a "directive"), or a standard Perl comment (which is
ignored).

A rule matches a text if one of its productions matches. A production
matches if each of its items match consecutive substrings of the
text. The productions of a rule being matched are tried in the same
order that they appear in the original grammar, and the first matching
production terminates the match attempt (successfully). If all
productions are tried and none matches, the match attempt fails.

Note that this behaviour is quite different from the "prefer the longer match"
behaviour of I<yacc>. For example, if I<yacc> were parsing the rule:

    seq : 'A' 'B'
    | 'A' 'B' 'C'

upon matching "AB" it would look ahead to see if a 'C' is next and, if
so, will match the second production in preference to the first. In
other words, I<yacc> effectively tries all the productions of a rule
breadth-first in parallel, and selects the "best" match, where "best"
means longest (note that this is a gross simplification of the true
behaviour of I<yacc> but it will do for our purposes).

In contrast, C<Parse::RecDescent> tries each production depth-first in
sequence, and selects the "best" match, where "best" means first. This is
the fundamental difference between "bottom-up" and "recursive descent"
parsing.

Each successfully matched item in a production is assigned a value,
which can be accessed in subsequent actions within the same
production (or, in some cases, as the return value of a successful
subrule call). Unsuccessful items don't have an associated value,
since the failure of an item causes the entire surrounding production
to immediately fail. The following sections describe the various types
of items and their success values.


=head2 Subrules

A subrule which appears in a production is an instruction to the parser to
attempt to match the named rule at that point in the text being
parsed. If the named subrule is not defined when requested the
production containing it immediately fails (unless it was "autostubbed" - see
L<Autostubbing>).

A rule may (recursively) call itself as a subrule, but I<not> as the
left-most item in any of its productions (since such recursions are usually
non-terminating).

The value associated with a subrule is the value associated with its
C<$return> variable (see L<"Actions"> below), or with the last successfully
matched item in the subrule match.

Subrules may also be specified with a trailing repetition specifier,
indicating that they are to be (greedily) matched the specified number
of times. The available specifiers are:

    subrule(?)  # Match one-or-zero times
    subrule(s)  # Match one-or-more times
    subrule(s?) # Match zero-or-more times
    subrule(N)  # Match exactly N times for integer N > 0
    subrule(N..M)   # Match between N and M times
    subrule(..M)    # Match between 1 and M times
    subrule(N..)    # Match at least N times

Repeated subrules keep matching until either the subrule fails to
match, or it has matched the minimal number of times but fails to
consume any of the parsed text (this second condition prevents the
subrule matching forever in some cases).

Since a repeated subrule may match many instances of the subrule itself, the
value associated with it is not a simple scalar, but rather a reference to a
list of scalars, each of which is the value associated with one of the
individual subrule matches. In other words in the rule:

    program: statement(s)

the value associated with the repeated subrule "statement(s)" is a reference
to an array containing the values matched by each call to the individual
subrule "statement".

Repetition modifiers may include a separator pattern:

    program: statement(s /;/)

specifying some sequence of characters to be skipped between each repetition.
This is really just a shorthand for the E<lt>leftop:...E<gt> directive
(see below).

=head2 Tokens

If a quote-delimited string or a Perl regex appears in a production,
the parser attempts to match that string or pattern at that point in
the text. For example:

    typedef: "typedef" typename identifier ';'

    identifier: /[A-Za-z_][A-Za-z0-9_]*/

As in regular Perl, a single quoted string is uninterpolated, whilst
a double-quoted string or a pattern is interpolated (at the time
of matching, I<not> when the parser is constructed). Hence, it is
possible to define rules in which tokens can be set at run-time:

    typedef: "$::typedefkeyword" typename identifier ';'

    identifier: /$::identpat/

Note that, since each rule is implemented inside a special namespace
belonging to its parser, it is necessary to explicitly quantify
variables from the main package.

Regex tokens can be specified using just slashes as delimiters
or with the explicit C<mE<lt>delimiterE<gt>......E<lt>delimiterE<gt>> syntax:

    typedef: "typedef" typename identifier ';'

    typename: /[A-Za-z_][A-Za-z0-9_]*/

    identifier: m{[A-Za-z_][A-Za-z0-9_]*}

A regex of either type can also have any valid trailing parameter(s)
(that is, any of [cgimsox]):

    typedef: "typedef" typename identifier ';'

    identifier: / [a-z_]        # LEADING ALPHA OR UNDERSCORE
          [a-z0-9_]*    # THEN DIGITS ALSO ALLOWED
        /ix     # CASE/SPACE/COMMENT INSENSITIVE

The value associated with any successfully matched token is a string
containing the actual text which was matched by the token.

It is important to remember that, since each grammar is specified in a
Perl string, all instances of the universal escape character '\' within
a grammar must be "doubled", so that they interpolate to single '\'s when
the string is compiled. For example, to use the grammar:

    word:       /\S+/ | backslash
    line:       prefix word(s) "\n"
    backslash:  '\\'

the following code is required:

    $parser = new Parse::RecDescent (q{

        word:   /\\S+/ | backslash
        line:   prefix word(s) "\\n"
        backslash:  '\\\\'

    });

=head2 Anonymous subrules

Parentheses introduce a nested scope that is very like a call to an anonymous
subrule. Hence they are useful for "in-lining" subroutine calls, and other
kinds of grouping behaviour. For example, instead of:

    word:       /\S+/ | backslash
    line:       prefix word(s) "\n"

you could write:

    line:       prefix ( /\S+/ | backslash )(s) "\n"

and get exactly the same effects.

Parentheses are also use for collecting unrepeated alternations within a
single production.

    secret_identity: "Mr" ("Incredible"|"Fantastic"|"Sheen") ", Esq."


=head2 Terminal Separators

For the purpose of matching, each terminal in a production is considered
to be preceded by a "prefix" - a pattern which must be
matched before a token match is attempted. By default, the
prefix is optional whitespace (which always matches, at
least trivially), but this default may be reset in any production.

The variable C<$Parse::RecDescent::skip> stores the universal
prefix, which is the default for all terminal matches in all parsers
built with C<Parse::RecDescent>.

If you want to change the universal prefix using
C<$Parse::RecDescent::skip>, be careful to set it I<before> creating
the grammar object, because it is applied statically (when a grammar
is built) rather than dynamically (when the grammar is used).
Alternatively you can provide a global C<E<lt>skip:...E<gt>> directive
in your grammar before any rules (described later).

The prefix for an individual production can be altered
by using the C<E<lt>skip:...E<gt>> directive (described later).
Setting this directive in the top-level rule is an alternative approach
to setting C<$Parse::RecDescent::skip> before creating the object, but
in this case you don't get the intended skipping behaviour if you
directly invoke methods different from the top-level rule.


=head2 Actions

An action is a block of Perl code which is to be executed (as the
block of a C<do> statement) when the parser reaches that point in a
production. The action executes within a special namespace belonging to
the active parser, so care must be taken in correctly qualifying variable
names (see also L<Start-up Actions> below).

The action is considered to succeed if the final value of the block
is defined (that is, if the implied C<do> statement evaluates to a
defined value - I<even one which would be treated as "false">). Note
that the value associated with a successful action is also the final
value in the block.

An action will I<fail> if its last evaluated value is C<undef>. This is
surprisingly easy to accomplish by accident. For instance, here's an
infuriating case of an action that makes its production fail, but only
when debugging I<isn't> activated:

    description: name rank serial_number
        { print "Got $item[2] $item[1] ($item[3])\n"
        if $::debugging
        }

If C<$debugging> is false, no statement in the block is executed, so
the final value is C<undef>, and the entire production fails. The solution is:

    description: name rank serial_number
        { print "Got $item[2] $item[1] ($item[3])\n"
        if $::debugging;
          1;
        }

Within an action, a number of useful parse-time variables are
available in the special parser namespace (there are other variables
also accessible, but meddling with them will probably just break your
parser. As a general rule, if you avoid referring to unqualified
variables - especially those starting with an underscore - inside an action,
things should be okay):

=over 4

=item C<@item> and C<%item>

The array slice C<@item[1..$#item]> stores the value associated with each item
(that is, each subrule, token, or action) in the current production. The
analogy is to C<$1>, C<$2>, etc. in a I<yacc> grammar.
Note that, for obvious reasons, C<@item> only contains the
values of items I<before> the current point in the production.

The first element (C<$item[0]>) stores the name of the current rule
being matched.

C<@item> is a standard Perl array, so it can also be indexed with negative
numbers, representing the number of items I<back> from the current position in
the parse:

    stuff: /various/ bits 'and' pieces "then" data 'end'
        { print $item[-2] }  # PRINTS data
             # (EASIER THAN: $item[6])

The C<%item> hash complements the <@item> array, providing named
access to the same item values:

    stuff: /various/ bits 'and' pieces "then" data 'end'
        { print $item{data}  # PRINTS data
             # (EVEN EASIER THAN USING @item)


The results of named subrules are stored in the hash under each
subrule's name (including the repetition specifier, if any),
whilst all other items are stored under a "named
positional" key that indicates their ordinal position within their item
type: __STRINGI<n>__, __PATTERNI<n>__, __DIRECTIVEI<n>__, __ACTIONI<n>__:

    stuff: /various/ bits 'and' pieces "then" data 'end' { save }
        { print $item{__PATTERN1__}, # PRINTS 'various'
        $item{__STRING2__},  # PRINTS 'then'
        $item{__ACTION1__},  # PRINTS RETURN
                 # VALUE OF save
        }


If you want proper I<named> access to patterns or literals, you need to turn
them into separate rules:

    stuff: various bits 'and' pieces "then" data 'end'
        { print $item{various}  # PRINTS various
        }

    various: /various/


The special entry C<$item{__RULE__}> stores the name of the current
rule (i.e. the same value as C<$item[0]>.

The advantage of using C<%item>, instead of C<@items> is that it
removes the need to track items positions that may change as a grammar
evolves. For example, adding an interim C<E<lt>skipE<gt>> directive
of action can silently ruin a trailing action, by moving an C<@item>
element "down" the array one place. In contrast, the named entry
of C<%item> is unaffected by such an insertion.

A limitation of the C<%item> hash is that it only records the I<last>
value of a particular subrule. For example:

    range: '(' number '..' number )'
        { $return = $item{number} }

will return only the value corresponding to the I<second> match of the
C<number> subrule. In other words, successive calls to a subrule
overwrite the corresponding entry in C<%item>. Once again, the
solution is to rename each subrule in its own rule:

    range: '(' from_num '..' to_num ')'
        { $return = $item{from_num} }

    from_num: number
    to_num:   number



=item C<@arg> and C<%arg>

The array C<@arg> and the hash C<%arg> store any arguments passed to
the rule from some other rule (see L<Subrule argument lists>). Changes
to the elements of either variable do not propagate back to the calling
rule (data can be passed back from a subrule via the C<$return>
variable - see next item).


=item C<$return>

If a value is assigned to C<$return> within an action, that value is
returned if the production containing the action eventually matches
successfully. Note that setting C<$return> I<doesn't> cause the current
production to succeed. It merely tells it what to return if it I<does> succeed.
Hence C<$return> is analogous to C<$$> in a I<yacc> grammar.

If C<$return> is not assigned within a production, the value of the
last component of the production (namely: C<$item[$#item]>) is
returned if the production succeeds.


=item C<$commit>

The current state of commitment to the current production (see L<"Directives">
below).

=item C<$skip>

The current terminal prefix (see L<"Directives"> below).

=item C<$text>

The remaining (unparsed) text. Changes to C<$text> I<do not
propagate> out of unsuccessful productions, but I<do> survive
successful productions. Hence it is possible to dynamically alter the
text being parsed - for example, to provide a C<#include>-like facility:

    hash_include: '#include' filename
        { $text = ::loadfile($item[2]) . $text }

    filename: '<' /[a-z0-9._-]+/i '>'  { $return = $item[2] }
    | '"' /[a-z0-9._-]+/i '"'  { $return = $item[2] }


=item C<$thisline> and C<$prevline>

C<$thisline> stores the current line number within the current parse
(starting from 1). C<$prevline> stores the line number for the last
character which was already successfully parsed (this will be different from
C<$thisline> at the end of each line).

For efficiency, C<$thisline> and C<$prevline> are actually tied
hashes, and only recompute the required line number when the variable's
value is used.

Assignment to C<$thisline> adjusts the line number calculator, so that
it believes that the current line number is the value being assigned. Note
that this adjustment will be reflected in all subsequent line numbers
calculations.

Modifying the value of the variable C<$text> (as in the previous
C<hash_include> example, for instance) will confuse the line
counting mechanism. To prevent this, you should call
C<Parse::RecDescent::LineCounter::resync($thisline)> I<immediately>
after any assignment to the variable C<$text> (or, at least, before the
next attempt to use C<$thisline>).

Note that if a production fails after assigning to or
resync'ing C<$thisline>, the parser's line counter mechanism will
usually be corrupted.

Also see the entry for C<@itempos>.

The line number can be set to values other than 1, by calling the start
rule with a second argument. For example:

    $parser = new Parse::RecDescent ($grammar);

    $parser->input($text, 10);  # START LINE NUMBERS AT 10


=item C<$thiscolumn> and C<$prevcolumn>

C<$thiscolumn> stores the current column number within the current line
being parsed (starting from 1). C<$prevcolumn> stores the column number
of the last character which was actually successfully parsed. Usually
C<$prevcolumn == $thiscolumn-1>, but not at the end of lines.

For efficiency, C<$thiscolumn> and C<$prevcolumn> are
actually tied hashes, and only recompute the required column number
when the variable's value is used.

Assignment to C<$thiscolumn> or C<$prevcolumn> is a fatal error.

Modifying the value of the variable C<$text> (as in the previous
C<hash_include> example, for instance) may confuse the column
counting mechanism.

Note that C<$thiscolumn> reports the column number I<before> any
whitespace that might be skipped before reading a token. Hence
if you wish to know where a token started (and ended) use something like this:

    rule: token1 token2 startcol token3 endcol token4
        { print "token3: columns $item[3] to $item[5]"; }

    startcol: '' { $thiscolumn }    # NEED THE '' TO STEP PAST TOKEN SEP
    endcol:  { $prevcolumn }

Also see the entry for C<@itempos>.

=item C<$thisoffset> and C<$prevoffset>

C<$thisoffset> stores the offset of the current parsing position
within the complete text
being parsed (starting from 0). C<$prevoffset> stores the offset
of the last character which was actually successfully parsed. In all
cases C<$prevoffset == $thisoffset-1>.

For efficiency, C<$thisoffset> and C<$prevoffset> are
actually tied hashes, and only recompute the required offset
when the variable's value is used.

Assignment to C<$thisoffset> or <$prevoffset> is a fatal error.

Modifying the value of the variable C<$text> will I<not> affect the
offset counting mechanism.

Also see the entry for C<@itempos>.

=item C<@itempos>

The array C<@itempos> stores a hash reference corresponding to
each element of C<@item>. The elements of the hash provide the
following:

    $itempos[$n]{offset}{from}  # VALUE OF $thisoffset BEFORE $item[$n]
    $itempos[$n]{offset}{to}    # VALUE OF $prevoffset AFTER $item[$n]
    $itempos[$n]{line}{from}    # VALUE OF $thisline BEFORE $item[$n]
    $itempos[$n]{line}{to}  # VALUE OF $prevline AFTER $item[$n]
    $itempos[$n]{column}{from}  # VALUE OF $thiscolumn BEFORE $item[$n]
    $itempos[$n]{column}{to}    # VALUE OF $prevcolumn AFTER $item[$n]

Note that the various C<$itempos[$n]...{from}> values record the
appropriate value I<after> any token prefix has been skipped.

Hence, instead of the somewhat tedious and error-prone:

    rule: startcol token1 endcol
      startcol token2 endcol
      startcol token3 endcol
        { print "token1: columns $item[1]
              to $item[3]
         token2: columns $item[4]
              to $item[6]
         token3: columns $item[7]
              to $item[9]" }

    startcol: '' { $thiscolumn }    # NEED THE '' TO STEP PAST TOKEN SEP
    endcol:  { $prevcolumn }

it is possible to write:

    rule: token1 token2 token3
        { print "token1: columns $itempos[1]{column}{from}
              to $itempos[1]{column}{to}
         token2: columns $itempos[2]{column}{from}
              to $itempos[2]{column}{to}
         token3: columns $itempos[3]{column}{from}
              to $itempos[3]{column}{to}" }

Note however that (in the current implementation) the use of C<@itempos>
anywhere in a grammar implies that item positioning information is
collected I<everywhere> during the parse. Depending on the grammar
and the size of the text to be parsed, this may be prohibitively
expensive and the explicit use of C<$thisline>, C<$thiscolumn>, etc. may
be a better choice.


=item C<$thisparser>

A reference to the S<C<Parse::RecDescent>> object through which
parsing was initiated.

The value of C<$thisparser> propagates down the subrules of a parse
but not back up. Hence, you can invoke subrules from another parser
for the scope of the current rule as follows:

    rule: subrule1 subrule2
    | { $thisparser = $::otherparser } <reject>
    | subrule3 subrule4
    | subrule5

The result is that the production calls "subrule1" and "subrule2" of
the current parser, and the remaining productions call the named subrules
from C<$::otherparser>. Note, however that "Bad Things" will happen if
C<::otherparser> isn't a blessed reference and/or doesn't have methods
with the same names as the required subrules!

=item C<$thisrule>

A reference to the S<C<Parse::RecDescent::Rule>> object corresponding to the
rule currently being matched.

=item C<$thisprod>

A reference to the S<C<Parse::RecDescent::Production>> object
corresponding to the production currently being matched.

=item C<$score> and C<$score_return>

$score stores the best production score to date, as specified by
an earlier C<E<lt>score:...E<gt>> directive. $score_return stores
the corresponding return value for the successful production.

See L<Scored productions>.

=back

B<Warning:> the parser relies on the information in the various C<this...>
objects in some non-obvious ways. Tinkering with the other members of
these objects will probably cause Bad Things to happen, unless you
I<really> know what you're doing. The only exception to this advice is
that the use of C<$this...-E<gt>{local}> is always safe.


=head2 Start-up Actions

Any actions which appear I<before> the first rule definition in a
grammar are treated as "start-up" actions. Each such action is
stripped of its outermost brackets and then evaluated (in the parser's
special namespace) just before the rules of the grammar are first
compiled.

The main use of start-up actions is to declare local variables within the
parser's special namespace:

    { my $lastitem = '???'; }

    list: item(s)   { $return = $lastitem }

    item: book  { $lastitem = 'book'; }
      bell  { $lastitem = 'bell'; }
      candle    { $lastitem = 'candle'; }

but start-up actions can be used to execute I<any> valid Perl code
within a parser's special namespace.

Start-up actions can appear within a grammar extension or replacement
(that is, a partial grammar installed via C<Parse::RecDescent::Extend()> or
C<Parse::RecDescent::Replace()> - see L<Incremental Parsing>), and will be
executed before the new grammar is installed. Note, however, that a
particular start-up action is only ever executed once.


=head2 Autoactions

It is sometimes desirable to be able to specify a default action to be
taken at the end of every production (for example, in order to easily
build a parse tree). If the variable C<$::RD_AUTOACTION> is defined
when C<Parse::RecDescent::new()> is called, the contents of that
variable are treated as a specification of an action which is to appended
to each production in the corresponding grammar.

Alternatively, you can hard-code the autoaction within a grammar, using the
C<< <autoaction:...> >> directive.

So, for example, to construct a simple parse tree you could write:

    $::RD_AUTOACTION = q { [@item] };

    parser = Parse::RecDescent->new(q{
    expression: and_expr '||' expression | and_expr
    and_expr:   not_expr '&&' and_expr   | not_expr
    not_expr:   '!' brack_expr       | brack_expr
    brack_expr: '(' expression ')'       | identifier
    identifier: /[a-z]+/i
    });

or:

    parser = Parse::RecDescent->new(q{
    <autoaction: { [@item] } >

    expression: and_expr '||' expression | and_expr
    and_expr:   not_expr '&&' and_expr   | not_expr
    not_expr:   '!' brack_expr       | brack_expr
    brack_expr: '(' expression ')'       | identifier
    identifier: /[a-z]+/i
    });

Either of these is equivalent to:

    parser = new Parse::RecDescent (q{
    expression: and_expr '||' expression
        { [@item] }
      | and_expr
        { [@item] }

    and_expr:   not_expr '&&' and_expr
        { [@item] }
    |   not_expr
        { [@item] }

    not_expr:   '!' brack_expr
        { [@item] }
    |   brack_expr
        { [@item] }

    brack_expr: '(' expression ')'
        { [@item] }
      | identifier
        { [@item] }

    identifier: /[a-z]+/i
        { [@item] }
    });

Alternatively, we could take an object-oriented approach, use different
classes for each node (and also eliminating redundant intermediate nodes):

    $::RD_AUTOACTION = q
      { $#item==1 ? $item[1] : "$item[0]_node"->new(@item[1..$#item]) };

    parser = Parse::RecDescent->new(q{
        expression: and_expr '||' expression | and_expr
        and_expr:   not_expr '&&' and_expr   | not_expr
        not_expr:   '!' brack_expr           | brack_expr
        brack_expr: '(' expression ')'       | identifier
        identifier: /[a-z]+/i
    });

or:

    parser = Parse::RecDescent->new(q{
        <autoaction:
          $#item==1 ? $item[1] : "$item[0]_node"->new(@item[1..$#item])
        >

        expression: and_expr '||' expression | and_expr
        and_expr:   not_expr '&&' and_expr   | not_expr
        not_expr:   '!' brack_expr           | brack_expr
        brack_expr: '(' expression ')'       | identifier
        identifier: /[a-z]+/i
    });

which are equivalent to:

    parser = Parse::RecDescent->new(q{
        expression: and_expr '||' expression
            { "expression_node"->new(@item[1..3]) }
        | and_expr

        and_expr:   not_expr '&&' and_expr
            { "and_expr_node"->new(@item[1..3]) }
        |   not_expr

        not_expr:   '!' brack_expr
            { "not_expr_node"->new(@item[1..2]) }
        |   brack_expr

        brack_expr: '(' expression ')'
            { "brack_expr_node"->new(@item[1..3]) }
        | identifier

        identifier: /[a-z]+/i
            { "identifer_node"->new(@item[1]) }
    });

Note that, if a production already ends in an action, no autoaction is appended
to it. For example, in this version:

    $::RD_AUTOACTION = q
      { $#item==1 ? $item[1] : "$item[0]_node"->new(@item[1..$#item]) };

    parser = Parse::RecDescent->new(q{
        expression: and_expr '&&' expression | and_expr
        and_expr:   not_expr '&&' and_expr   | not_expr
        not_expr:   '!' brack_expr           | brack_expr
        brack_expr: '(' expression ')'       | identifier
        identifier: /[a-z]+/i
            { 'terminal_node'->new($item[1]) }
    });

each C<identifier> match produces a C<terminal_node> object, I<not> an
C<identifier_node> object.

A level 1 warning is issued each time an "autoaction" is added to
some production.


=head2 Autotrees

A commonly needed autoaction is one that builds a parse-tree. It is moderately
tricky to set up such an action (which must treat terminals differently from
non-terminals), so Parse::RecDescent simplifies the process by providing the
C<E<lt>autotreeE<gt>> directive.

If this directive appears at the start of grammar, it causes
Parse::RecDescent to insert autoactions at the end of any rule except
those which already end in an action. The action inserted depends on whether
the production is an intermediate rule (two or more items), or a terminal
of the grammar (i.e. a single pattern or string item).

So, for example, the following grammar:

    <autotree>

    file    : command(s)
    command : get | set | vet
    get : 'get' ident ';'
    set : 'set' ident 'to' value ';'
    vet : 'check' ident 'is' value ';'
    ident   : /\w+/
    value   : /\d+/

is equivalent to:

    file    : command(s)        { bless \%item, $item[0] }
    command : get       { bless \%item, $item[0] }
    | set           { bless \%item, $item[0] }
    | vet           { bless \%item, $item[0] }
    get : 'get' ident ';'   { bless \%item, $item[0] }
    set : 'set' ident 'to' value ';'    { bless \%item, $item[0] }
    vet : 'check' ident 'is' value ';'  { bless \%item, $item[0] }

    ident   : /\w+/  { bless {__VALUE__=>$item[1]}, $item[0] }
    value   : /\d+/  { bless {__VALUE__=>$item[1]}, $item[0] }

Note that each node in the tree is blessed into a class of the same name
as the rule itself. This makes it easy to build object-oriented
processors for the parse-trees that the grammar produces. Note too that
the last two rules produce special objects with the single attribute
'__VALUE__'. This is because they consist solely of a single terminal.

This autoaction-ed grammar would then produce a parse tree in a data
structure like this:

    {
      file => {
        command => {
         [ get => {
            identifier => { __VALUE__ => 'a' },
              },
           set => {
            identifier => { __VALUE__ => 'b' },
            value      => { __VALUE__ => '7' },
              },
           vet => {
            identifier => { __VALUE__ => 'b' },
            value      => { __VALUE__ => '7' },
              },
          ],
           },
      }
    }

(except, of course, that each nested hash would also be blessed into
the appropriate class).

You can also specify a base class for the C<E<lt>autotreeE<gt>> directive.
The supplied prefix will be prepended to the rule names when creating
tree nodes.  The following are equivalent:

    <autotree:MyBase::Class>
    <autotree:MyBase::Class::>

And will produce a root node blessed into the C<MyBase::Class::file>
package in the example above.


=head2 Autostubbing

Normally, if a subrule appears in some production, but no rule of that
name is ever defined in the grammar, the production which refers to the
non-existent subrule fails immediately. This typically occurs as a
result of misspellings, and is a sufficiently common occurrence that a
warning is generated for such situations.

However, when prototyping a grammar it is sometimes useful to be
able to use subrules before a proper specification of them is
really possible.  For example, a grammar might include a section like:

    function_call: identifier '(' arg(s?) ')'

    identifier: /[a-z]\w*/i

where the possible format of an argument is sufficiently complex that
it is not worth specifying in full until the general function call
syntax has been debugged. In this situation it is convenient to leave
the real rule C<arg> undefined and just slip in a placeholder (or
"stub"):

    arg: 'arg'

so that the function call syntax can be tested with dummy input such as:

    f0()
    f1(arg)
    f2(arg arg)
    f3(arg arg arg)

et cetera.

Early in prototyping, many such "stubs" may be required, so
C<Parse::RecDescent> provides a means of automating their definition.
If the variable C<$::RD_AUTOSTUB> is defined when a parser is built, a
subrule reference to any non-existent rule (say, C<subrule>), will
cause a "stub" rule to be automatically defined in the generated
parser.  If C<$::RD_AUTOSTUB eq '1'> or is false, a stub rule of the
form:

    subrule: 'subrule'

will be generated.  The special-case for a value of C<'1'> is to allow
the use of the B<perl -s> with B<-RD_AUTOSTUB> without generating
C<subrule: '1'> per below. If C<$::RD_AUTOSTUB> is true, a stub rule
of the form:

    subrule: $::RD_AUTOSTUB

will be generated.  C<$::RD_AUTOSTUB> must contain a valid production
item, no checking is performed.  No lazy evaluation of
C<$::RD_AUTOSTUB> is performed, it is evaluated at the time the Parser
is generated.

Hence, with C<$::RD_AUTOSTUB> defined, it is possible to only
partially specify a grammar, and then "fake" matches of the
unspecified (sub)rules by just typing in their name, or a literal
value that was assigned to C<$::RD_AUTOSTUB>.



=head2 Look-ahead

If a subrule, token, or action is prefixed by "...", then it is
treated as a "look-ahead" request. That means that the current production can
(as usual) only succeed if the specified item is matched, but that the matching
I<does not consume any of the text being parsed>. This is very similar to the
C</(?=...)/> look-ahead construct in Perl patterns. Thus, the rule:

    inner_word: word ...word

will match whatever the subrule "word" matches, provided that match is followed
by some more text which subrule "word" would also match (although this
second substring is not actually consumed by "inner_word")

Likewise, a "...!" prefix, causes the following item to succeed (without
consuming any text) if and only if it would normally fail. Hence, a
rule such as:

    identifier: ...!keyword ...!'_' /[A-Za-z_]\w*/

matches a string of characters which satisfies the pattern
C</[A-Za-z_]\w*/>, but only if the same sequence of characters would
not match either subrule "keyword" or the literal token '_'.

Sequences of look-ahead prefixes accumulate, multiplying their positive and/or
negative senses. Hence:

    inner_word: word ...!......!word

is exactly equivalent to the original example above (a warning is issued in
cases like these, since they often indicate something left out, or
misunderstood).

Note that actions can also be treated as look-aheads. In such cases,
the state of the parser text (in the local variable C<$text>)
I<after> the look-ahead action is guaranteed to be identical to its
state I<before> the action, regardless of how it's changed I<within>
the action (unless you actually undefine C<$text>, in which case you
get the disaster you deserve :-).


=head2 Directives

Directives are special pre-defined actions which may be used to alter
the behaviour of the parser. There are currently twenty-three directives:
C<E<lt>commitE<gt>>,
C<E<lt>uncommitE<gt>>,
C<E<lt>rejectE<gt>>,
C<E<lt>scoreE<gt>>,
C<E<lt>autoscoreE<gt>>,
C<E<lt>skipE<gt>>,
C<E<lt>resyncE<gt>>,
C<E<lt>errorE<gt>>,
C<E<lt>warnE<gt>>,
C<E<lt>hintE<gt>>,
C<E<lt>trace_buildE<gt>>,
C<E<lt>trace_parseE<gt>>,
C<E<lt>nocheckE<gt>>,
C<E<lt>rulevarE<gt>>,
C<E<lt>matchruleE<gt>>,
C<E<lt>leftopE<gt>>,
C<E<lt>rightopE<gt>>,
C<E<lt>deferE<gt>>,
C<E<lt>nocheckE<gt>>,
C<E<lt>perl_quotelikeE<gt>>,
C<E<lt>perl_codeblockE<gt>>,
C<E<lt>perl_variableE<gt>>,
and C<E<lt>tokenE<gt>>.

=over 4

=item Committing and uncommitting

The C<E<lt>commitE<gt>> and C<E<lt>uncommitE<gt>> directives permit the recursive
descent of the parse tree to be pruned (or "cut") for efficiency.
Within a rule, a C<E<lt>commitE<gt>> directive instructs the rule to ignore subsequent
productions if the current production fails. For example:

    command: 'find' <commit> filename
       | 'open' <commit> filename
       | 'move' filename filename

Clearly, if the leading token 'find' is matched in the first production but that
production fails for some other reason, then the remaining
productions cannot possibly match. The presence of the
C<E<lt>commitE<gt>> causes the "command" rule to fail immediately if
an invalid "find" command is found, and likewise if an invalid "open"
command is encountered.

It is also possible to revoke a previous commitment. For example:

    if_statement: 'if' <commit> condition
        'then' block <uncommit>
        'else' block
        | 'if' <commit> condition
        'then' block

In this case, a failure to find an "else" block in the first
production shouldn't preclude trying the second production, but a
failure to find a "condition" certainly should.

As a special case, any production in which the I<first> item is an
C<E<lt>uncommitE<gt>> immediately revokes a preceding C<E<lt>commitE<gt>>
(even though the production would not otherwise have been tried). For
example, in the rule:

    request: 'explain' expression
           | 'explain' <commit> keyword
           | 'save'
           | 'quit'
           | <uncommit> term '?'

if the text being matched was "explain?", and the first two
productions failed, then the C<E<lt>commitE<gt>> in production two would cause
productions three and four to be skipped, but the leading
C<E<lt>uncommitE<gt>> in the production five would allow that production to
attempt a match.

Note in the preceding example, that the C<E<lt>commitE<gt>> was only placed
in production two. If production one had been:

    request: 'explain' <commit> expression

then production two would be (inappropriately) skipped if a leading
"explain..." was encountered.

Both C<E<lt>commitE<gt>> and C<E<lt>uncommitE<gt>> directives always succeed, and their value
is always 1.


=item Rejecting a production

The C<E<lt>rejectE<gt>> directive immediately causes the current production
to fail (it is exactly equivalent to, but more obvious than, the
action C<{undef}>). A C<E<lt>rejectE<gt>> is useful when it is desirable to get
the side effects of the actions in one production, without prejudicing a match
by some other production later in the rule. For example, to insert
tracing code into the parse:

    complex_rule: { print "In complex rule...\n"; } <reject>

    complex_rule: simple_rule '+' 'i' '*' simple_rule
        | 'i' '*' simple_rule
        | simple_rule


It is also possible to specify a conditional rejection, using the
form C<E<lt>reject:I<condition>E<gt>>, which only rejects if the
specified condition is true. This form of rejection is exactly
equivalent to the action C<{(I<condition>)?undef:1}E<gt>>.
For example:

    command: save_command
       | restore_command
       | <reject: defined $::tolerant> { exit }
       | <error: Unknown command. Ignored.>

A C<E<lt>rejectE<gt>> directive never succeeds (and hence has no
associated value). A conditional rejection may succeed (if its
condition is not satisfied), in which case its value is 1.

As an extra optimization, C<Parse::RecDescent> ignores any production
which I<begins> with an unconditional C<E<lt>rejectE<gt>> directive,
since any such production can never successfully match or have any
useful side-effects. A level 1 warning is issued in all such cases.

Note that productions beginning with conditional
C<E<lt>reject:...E<gt>> directives are I<never> "optimized away" in
this manner, even if they are always guaranteed to fail (for example:
C<E<lt>reject:1E<gt>>)

Due to the way grammars are parsed, there is a minor restriction on the
condition of a conditional C<E<lt>reject:...E<gt>>: it cannot
contain any raw '<' or '>' characters. For example:

    line: cmd <reject: $thiscolumn > max> data

results in an error when a parser is built from this grammar (since the
grammar parser has no way of knowing whether the first > is a "less than"
or the end of the C<E<lt>reject:...E<gt>>.

To overcome this problem, put the condition inside a do{} block:

    line: cmd <reject: do{$thiscolumn > max}> data

Note that the same problem may occur in other directives that take
arguments. The same solution will work in all cases.


=item Skipping between terminals

The C<E<lt>skipE<gt>> directive enables the terminal prefix used in
a production to be changed. For example:

    OneLiner: Command <skip:'[ \t]*'> Arg(s) /;/

causes only blanks and tabs to be skipped before terminals in the
C<Arg> subrule (and any of I<its> subrules>, and also before the final
C</;/> terminal.  Once the production is complete, the previous
terminal prefix is reinstated. Note that this implies that distinct
productions of a rule must reset their terminal prefixes individually.

The C<E<lt>skipE<gt>> directive evaluates to the I<previous> terminal
prefix, so it's easy to reinstate a prefix later in a production:

    Command: <skip:","> CSV(s) <skip:$item[1]> Modifier

The value specified after the colon is interpolated into a pattern, so
all of the following are equivalent (though their efficiency increases
down the list):

    <skip: "$colon|$comma">   # ASSUMING THE VARS HOLD THE OBVIOUS VALUES

    <skip: ':|,'>

    <skip: q{[:,]}>

    <skip: qr/[:,]/>

There is no way of directly setting the prefix for
an entire rule, except as follows:

    Rule: <skip: '[ \t]*'> Prod1
        | <skip: '[ \t]*'> Prod2a Prod2b
        | <skip: '[ \t]*'> Prod3

or, better:

    Rule: <skip: '[ \t]*'>
    (
        Prod1
      | Prod2a Prod2b
      | Prod3
    )

The skip pattern is passed down to subrules, so setting the skip for
the top-level rule as described above actually sets the prefix for the
entire grammar (provided that you only call the method corresponding
to the top-level rule itself). Alternatively, or if you have more than
one top-level rule in your grammar, you can provide a global
C<E<lt>skipE<gt>> directive prior to defining any rules in the
grammar. These are the preferred alternatives to setting
C<$Parse::RecDescent::skip>.

Additionally, using C<E<lt>skipE<gt>> actually allows you to have
a completely dynamic skipping behaviour. For example:

   Rule_with_dynamic_skip: <skip: $::skip_pattern> Rule

Then you can set C<$::skip_pattern> before invoking
C<Rule_with_dynamic_skip> and have it skip whatever you specified.

B<Note: Up to release 1.51 of Parse::RecDescent, an entirely different
mechanism was used for specifying terminal prefixes. The current
method is not backwards-compatible with that early approach. The
current approach is stable and will not change again.>

B<Note: the global C<E<lt>skipE<gt>> directive added in 1.967_004 did
not interpolate the pattern argument, instead the pattern was placed
inside of single quotes and then interpolated. This behavior was
changed in 1.967_010 so that all C<E<lt>skipE<gt>> directives behavior
similarly.>

=item Resynchronization

The C<E<lt>resyncE<gt>> directive provides a visually distinctive
means of consuming some of the text being parsed, usually to skip an
erroneous input. In its simplest form C<E<lt>resyncE<gt>> simply
consumes text up to and including the next newline (C<"\n">)
character, succeeding only if the newline is found, in which case it
causes its surrounding rule to return zero on success.

In other words, a C<E<lt>resyncE<gt>> is exactly equivalent to the token
C</[^\n]*\n/> followed by the action S<C<{ $return = 0 }>> (except that
productions beginning with a C<E<lt>resyncE<gt>> are ignored when generating
error messages). A typical use might be:

    script : command(s)

    command: save_command
       | restore_command
       | <resync> # TRY NEXT LINE, IF POSSIBLE

It is also possible to explicitly specify a resynchronization
pattern, using the C<E<lt>resync:I<pattern>E<gt>> variant. This version
succeeds only if the specified pattern matches (and consumes) the
parsed text. In other words, C<E<lt>resync:I<pattern>E<gt>> is exactly
equivalent to the token C</I<pattern>/> (followed by a S<C<{ $return = 0 }>>
action). For example, if commands were terminated by newlines or semi-colons:

    command: save_command
       | restore_command
       | <resync:[^;\n]*[;\n]>

The value of a successfully matched C<E<lt>resyncE<gt>> directive (of either
type) is the text that it consumed. Note, however, that since the
directive also sets C<$return>, a production consisting of a lone
C<E<lt>resyncE<gt>> succeeds but returns the value zero (which a calling rule
may find useful to distinguish between "true" matches and "tolerant" matches).
Remember that returning a zero value indicates that the rule I<succeeded> (since
only an C<undef> denotes failure within C<Parse::RecDescent> parsers.


=item Error handling

The C<E<lt>errorE<gt>> directive provides automatic or user-defined
generation of error messages during a parse. In its simplest form
C<E<lt>errorE<gt>> prepares an error message based on
the mismatch between the last item expected and the text which cause
it to fail. For example, given the rule:

    McCoy: curse ',' name ', I'm a doctor, not a' a_profession '!'
     | pronoun 'dead,' name '!'
     | <error>

the following strings would produce the following messages:

=over 4

=item "Amen, Jim!"

       ERROR (line 1): Invalid McCoy: Expected curse or pronoun
           not found

=item "Dammit, Jim, I'm a doctor!"

       ERROR (line 1): Invalid McCoy: Expected ", I'm a doctor, not a"
           but found ", I'm a doctor!" instead

=item "He's dead,\n"

       ERROR (line 2): Invalid McCoy: Expected name not found

=item "He's alive!"

       ERROR (line 1): Invalid McCoy: Expected 'dead,' but found
           "alive!" instead

=item "Dammit, Jim, I'm a doctor, not a pointy-eared Vulcan!"

       ERROR (line 1): Invalid McCoy: Expected a profession but found
           "pointy-eared Vulcan!" instead


=back

Note that, when autogenerating error messages, all underscores in any
rule name used in a message are replaced by single spaces (for example
"a_production" becomes "a production"). Judicious choice of rule
names can therefore considerably improve the readability of automatic
error messages (as well as the maintainability of the original
grammar).

If the automatically generated error is not sufficient, it is possible to
provide an explicit message as part of the error directive. For example:

    Spock: "Fascinating ',' (name | 'Captain') '.'
     | "Highly illogical, doctor."
     | <error: He never said that!>

which would result in I<all> failures to parse a "Spock" subrule printing the
following message:

       ERROR (line <N>): Invalid Spock:  He never said that!

The error message is treated as a "qq{...}" string and interpolated
when the error is generated (I<not> when the directive is specified!).
Hence:

    <error: Mystical error near "$text">

would correctly insert the ambient text string which caused the error.

There are two other forms of error directive: C<E<lt>error?E<gt>> and
S<C<E<lt>error?: msgE<gt>>>. These behave just like C<E<lt>errorE<gt>>
and S<C<E<lt>error: msgE<gt>>> respectively, except that they are
only triggered if the rule is "committed" at the time they are
encountered. For example:

    Scotty: "Ya kenna change the Laws of Phusics," <commit> name
      | name <commit> ',' 'she's goanta blaw!'
      | <error?>

will only generate an error for a string beginning with "Ya kenna
change the Laws o' Phusics," or a valid name, but which still fails to match the
corresponding production. That is, C<$parser-E<gt>Scotty("Aye, Cap'ain")> will
fail silently (since neither production will "commit" the rule on that
input), whereas S<C<$parser-E<gt>Scotty("Mr Spock, ah jest kenna do'ut!")>>
will fail with the error message:

       ERROR (line 1): Invalid Scotty: expected 'she's goanta blaw!'
           but found 'I jest kenna do'ut!' instead.

since in that case the second production would commit after matching
the leading name.

Note that to allow this behaviour, all C<E<lt>errorE<gt>> directives which are
the first item in a production automatically uncommit the rule just
long enough to allow their production to be attempted (that is, when
their production fails, the commitment is reinstated so that
subsequent productions are skipped).

In order to I<permanently> uncommit the rule before an error message,
it is necessary to put an explicit C<E<lt>uncommitE<gt>> before the
C<E<lt>errorE<gt>>. For example:

    line: 'Kirk:'  <commit> Kirk
    | 'Spock:' <commit> Spock
    | 'McCoy:' <commit> McCoy
    | <uncommit> <error?> <reject>
    | <resync>


Error messages generated by the various C<E<lt>error...E<gt>> directives
are not displayed immediately. Instead, they are "queued" in a buffer and
are only displayed once parsing ultimately fails. Moreover,
C<E<lt>error...E<gt>> directives that cause one production of a rule
to fail are automatically removed from the message queue
if another production subsequently causes the entire rule to succeed.
This means that you can put
C<E<lt>error...E<gt>> directives wherever useful diagnosis can be done,
and only those associated with actual parser failure will ever be
displayed. Also see L<"GOTCHAS">.

As a general rule, the most useful diagnostics are usually generated
either at the very lowest level within the grammar, or at the very
highest. A good rule of thumb is to identify those subrules which
consist mainly (or entirely) of terminals, and then put an
C<E<lt>error...E<gt>> directive at the end of any other rule which calls
one or more of those subrules.

There is one other situation in which the output of the various types of
error directive is suppressed; namely, when the rule containing them
is being parsed as part of a "look-ahead" (see L<"Look-ahead">). In this
case, the error directive will still cause the rule to fail, but will do
so silently.

An unconditional C<E<lt>errorE<gt>> directive always fails (and hence has no
associated value). This means that encountering such a directive
always causes the production containing it to fail. Hence an
C<E<lt>errorE<gt>> directive will inevitably be the last (useful) item of a
rule (a level 3 warning is issued if a production contains items after an unconditional
C<E<lt>errorE<gt>> directive).

An C<E<lt>error?E<gt>> directive will I<succeed> (that is: fail to fail :-), if
the current rule is uncommitted when the directive is encountered. In
that case the directive's associated value is zero. Hence, this type
of error directive I<can> be used before the end of a
production. For example:

    command: 'do' <commit> something
       | 'report' <commit> something
       | <error?: Syntax error> <error: Unknown command>


B<Warning:> The C<E<lt>error?E<gt>> directive does I<not> mean "always fail (but
do so silently unless committed)". It actually means "only fail (and report) if
committed, otherwise I<succeed>". To achieve the "fail silently if uncommitted"
semantics, it is necessary to use:

    rule: item <commit> item(s)
    | <error?> <reject>  # FAIL SILENTLY UNLESS COMMITTED

However, because people seem to expect a lone C<E<lt>error?E<gt>> directive
to work like this:

    rule: item <commit> item(s)
    | <error?: Error message if committed>
    | <error:  Error message if uncommitted>

Parse::RecDescent automatically appends a
C<E<lt>rejectE<gt>> directive if the C<E<lt>error?E<gt>> directive
is the only item in a production. A level 2 warning (see below)
is issued when this happens.

The level of error reporting during both parser construction and
parsing is controlled by the presence or absence of four global
variables: C<$::RD_ERRORS>, C<$::RD_WARN>, C<$::RD_HINT>, and
<$::RD_TRACE>. If C<$::RD_ERRORS> is defined (and, by default, it is)
then fatal errors are reported.

Whenever C<$::RD_WARN> is defined, certain non-fatal problems are also reported.

Warnings have an associated "level": 1, 2, or 3. The higher the level,
the more serious the warning. The value of the corresponding global
variable (C<$::RD_WARN>) determines the I<lowest> level of warning to
be displayed. Hence, to see I<all> warnings, set C<$::RD_WARN> to 1.
To see only the most serious warnings set C<$::RD_WARN> to 3.
By default C<$::RD_WARN> is initialized to 3, ensuring that serious but
non-fatal errors are automatically reported.

There is also a grammar directive to turn on warnings from within the
grammar: C<< <warn> >>. It takes an optional argument, which specifies
the warning level: C<< <warn: 2> >>.

See F<"DIAGNOSTICS"> for a list of the various error and warning messages
that Parse::RecDescent generates when these two variables are defined.

Defining any of the remaining variables (which are not defined by
default) further increases the amount of information reported.
Defining C<$::RD_HINT> causes the parser generator to offer
more detailed analyses and hints on both errors and warnings.
Note that setting C<$::RD_HINT> at any point automagically
sets C<$::RD_WARN> to 1. There is also a C<< <hint> >> directive, which can
be hard-coded into a grammar.

Defining C<$::RD_TRACE> causes the parser generator and the parser to
report their progress to STDERR in excruciating detail (although, without hints
unless $::RD_HINT is separately defined). This detail
can be moderated in only one respect: if C<$::RD_TRACE> has an
integer value (I<N>) greater than 1, only the I<N> characters of
the "current parsing context" (that is, where in the input string we
are at any point in the parse) is reported at any time.

C<$::RD_TRACE> is mainly useful for debugging a grammar that isn't
behaving as you expected it to. To this end, if C<$::RD_TRACE> is
defined when a parser is built, any actual parser code which is
generated is also written to a file named "RD_TRACE" in the local
directory.

There are two directives associated with the C<$::RD_TRACE> variable.
If a grammar contains a C<< <trace_build> >> directive anywhere in its
specification, C<$::RD_TRACE> is turned on during the parser construction
phase.  If a grammar contains a C<< <trace_parse> >> directive anywhere in its
specification, C<$::RD_TRACE> is turned on during any parse the parser
performs.

Note that the four variables belong to the "main" package, which
makes them easier to refer to in the code controlling the parser, and
also makes it easy to turn them into command line flags ("-RD_ERRORS",
"-RD_WARN", "-RD_HINT", "-RD_TRACE") under B<perl -s>.

The corresponding directives are useful to "hardwire" the various
debugging features into a particular grammar (rather than having to set
and reset external variables).

=item Redirecting diagnostics

The diagnostics provided by the tracing mechanism always go to STDERR.
If you need them to go elsewhere, localize and reopen STDERR prior to the
parse.

For example:

    {
        local *STDERR = IO::File->new(">$filename") or die $!;

        my $result = $parser->startrule($text);
    }


=item Consistency checks

Whenever a parser is build, Parse::RecDescent carries out a number of
(potentially expensive) consistency checks. These include: verifying that the
grammar is not left-recursive and that no rules have been left undefined.

These checks are important safeguards during development, but unnecessary
overheads when the grammar is stable and ready to be deployed. So
Parse::RecDescent provides a directive to disable them: C<< <nocheck> >>.

If a grammar contains a C<< <nocheck> >> directive anywhere in its
specification, the extra compile-time checks are by-passed.


=item Specifying local variables

It is occasionally convenient to specify variables which are local
to a single rule. This may be achieved by including a
C<E<lt>rulevar:...E<gt>> directive anywhere in the rule. For example:

    markup: <rulevar: $tag>

    markup: tag {($tag=$item[1]) =~ s/^<|>$//g} body[$tag]

The example C<E<lt>rulevar: $tagE<gt>> directive causes a "my" variable named
C<$tag> to be declared at the start of the subroutine implementing the
C<markup> rule (that is, I<before> the first production, regardless of
where in the rule it is specified).

Specifically, any directive of the form:
C<E<lt>rulevar:I<text>E<gt>> causes a line of the form C<my I<text>;>
to be added at the beginning of the rule subroutine, immediately after
the definitions of the following local variables:

    $thisparser $commit
    $thisrule   @item
    $thisline   @arg
    $text   %arg

This means that the following C<E<lt>rulevarE<gt>> directives work
as expected:

    <rulevar: $count = 0 >

    <rulevar: $firstarg = $arg[0] || '' >

    <rulevar: $myItems = \@item >

    <rulevar: @context = ( $thisline, $text, @arg ) >

    <rulevar: ($name,$age) = $arg{"name","age"} >

If a variable that is also visible to subrules is required, it needs
to be C<local>'d, not C<my>'d. C<rulevar> defaults to C<my>, but if C<local>
is explicitly specified:

    <rulevar: local $count = 0 >

then a C<local>-ized variable is declared instead, and will be available
within subrules.

Note however that, because all such variables are "my" variables, their
values I<do not persist> between match attempts on a given rule. To
preserve values between match attempts, values can be stored within the
"local" member of the C<$thisrule> object:

    countedrule: { $thisrule->{"local"}{"count"}++ }
         <reject>
       | subrule1
       | subrule2
       | <reject: $thisrule->{"local"}{"count"} == 1>
         subrule3


When matching a rule, each C<E<lt>rulevarE<gt>> directive is matched as
if it were an unconditional C<E<lt>rejectE<gt>> directive (that is, it
causes any production in which it appears to immediately fail to match).
For this reason (and to improve readability) it is usual to specify any
C<E<lt>rulevarE<gt>> directive in a separate production at the start of
the rule (this has the added advantage that it enables
C<Parse::RecDescent> to optimize away such productions, just as it does
for the C<E<lt>rejectE<gt>> directive).


=item Dynamically matched rules

Because regexes and double-quoted strings are interpolated, it is relatively
easy to specify productions with "context sensitive" tokens. For example:

    command:  keyword  body  "end $item[1]"

which ensures that a command block is bounded by a
"I<E<lt>keywordE<gt>>...end I<E<lt>same keywordE<gt>>" pair.

Building productions in which subrules are context sensitive is also possible,
via the C<E<lt>matchrule:...E<gt>> directive. This directive behaves
identically to a subrule item, except that the rule which is invoked to match
it is determined by the string specified after the colon. For example, we could
rewrite the C<command> rule like this:

    command:  keyword  <matchrule:body>  "end $item[1]"

Whatever appears after the colon in the directive is treated as an interpolated
string (that is, as if it appeared in C<qq{...}> operator) and the value of
that interpolated string is the name of the subrule to be matched.

Of course, just putting a constant string like C<body> in a
C<E<lt>matchrule:...E<gt>> directive is of little interest or benefit.
The power of directive is seen when we use a string that interpolates
to something interesting. For example:

    command:    keyword <matchrule:$item[1]_body> "end $item[1]"

    keyword:    'while' | 'if' | 'function'

    while_body: condition block

    if_body:    condition block ('else' block)(?)

    function_body:  arglist block

Now the C<command> rule selects how to proceed on the basis of the keyword
that is found. It is as if C<command> were declared:

    command:    'while'    while_body    "end while"
       |    'if'       if_body   "end if"
       |    'function' function_body "end function"


When a C<E<lt>matchrule:...E<gt>> directive is used as a repeated
subrule, the rule name expression is "late-bound". That is, the name of
the rule to be called is re-evaluated I<each time> a match attempt is
made. Hence, the following grammar:

    { $::species = 'dogs' }

    pair:   'two' <matchrule:$::species>(s)

    dogs:   /dogs/ { $::species = 'cats' }

    cats:   /cats/

will match the string "two dogs cats cats" completely, whereas it will
only match the string "two dogs dogs dogs" up to the eighth letter. If
the rule name were "early bound" (that is, evaluated only the first
time the directive is encountered in a production), the reverse
behaviour would be expected.

Note that the C<matchrule> directive takes a string that is to be treated
as a rule name, I<not> as a rule invocation. That is,
it's like a Perl symbolic reference, not an C<eval>. Just as you can say:

    $subname = 'foo';

    # and later...

    &{$foo}(@args);

but not:

    $subname = 'foo(@args)';

    # and later...

    &{$foo};

likewise you can say:

    $rulename = 'foo';

    # and in the grammar...

    <matchrule:$rulename>[@args]

but not:

    $rulename = 'foo[@args]';

    # and in the grammar...

    <matchrule:$rulename>


=item Deferred actions

The C<E<lt>defer:...E<gt>> directive is used to specify an action to be
performed when (and only if!) the current production ultimately succeeds.

Whenever a C<E<lt>defer:...E<gt>> directive appears, the code it specifies
is converted to a closure (an anonymous subroutine reference) which is
queued within the active parser object. Note that,
because the deferred code is converted to a closure, the values of any
"local" variable (such as C<$text>, <@item>, etc.) are preserved
until the deferred code is actually executed.

If the parse ultimately succeeds
I<and> the production in which the C<E<lt>defer:...E<gt>> directive was
evaluated formed part of the successful parse, then the deferred code is
executed immediately before the parse returns. If however the production
which queued a deferred action fails, or one of the higher-level
rules which called that production fails, then the deferred action is
removed from the queue, and hence is never executed.

For example, given the grammar:

    sentence: noun trans noun
    | noun intrans

    noun:     'the dog'
        { print "$item[1]\t(noun)\n" }
    |     'the meat'
        { print "$item[1]\t(noun)\n" }

    trans:    'ate'
        { print "$item[1]\t(transitive)\n" }

    intrans:  'ate'
        { print "$item[1]\t(intransitive)\n" }
       |  'barked'
        { print "$item[1]\t(intransitive)\n" }

then parsing the sentence C<"the dog ate"> would produce the output:

    the dog  (noun)
    ate  (transitive)
    the dog  (noun)
    ate  (intransitive)

This is because, even though the first production of C<sentence>
ultimately fails, its initial subrules C<noun> and C<trans> do match,
and hence they execute their associated actions.
Then the second production of C<sentence> succeeds, causing the
actions of the subrules C<noun> and C<intrans> to be executed as well.

On the other hand, if the actions were replaced by C<E<lt>defer:...E<gt>>
directives:

    sentence: noun trans noun
    | noun intrans

    noun:     'the dog'
        <defer: print "$item[1]\t(noun)\n" >
    |     'the meat'
        <defer: print "$item[1]\t(noun)\n" >

    trans:    'ate'
        <defer: print "$item[1]\t(transitive)\n" >

    intrans:  'ate'
        <defer: print "$item[1]\t(intransitive)\n" >
       |  'barked'
        <defer: print "$item[1]\t(intransitive)\n" >

the output would be:

    the dog  (noun)
    ate  (intransitive)

since deferred actions are only executed if they were evaluated in
a production which ultimately contributes to the successful parse.

In this case, even though the first production of C<sentence> caused
the subrules C<noun> and C<trans> to match, that production ultimately
failed and so the deferred actions queued by those subrules were subsequently
discarded. The second production then succeeded, causing the entire
parse to succeed, and so the deferred actions queued by the (second) match of
the C<noun> subrule and the subsequent match of C<intrans> I<are> preserved and
eventually executed.

Deferred actions provide a means of improving the performance of a parser,
by only executing those actions which are part of the final parse-tree
for the input data.

Alternatively, deferred actions can be viewed as a mechanism for building
(and executing) a
customized subroutine corresponding to the given input data, much in the
same way that autoactions (see L<"Autoactions">) can be used to build a
customized data structure for specific input.

Whether or not the action it specifies is ever executed,
a C<E<lt>defer:...E<gt>> directive always succeeds, returning the
number of deferred actions currently queued at that point.


=item Parsing Perl

Parse::RecDescent provides limited support for parsing subsets of Perl,
namely: quote-like operators, Perl variables, and complete code blocks.

The C<E<lt>perl_quotelikeE<gt>> directive can be used to parse any Perl
quote-like operator: C<'a string'>, C<m/a pattern/>, C<tr{ans}{lation}>,
etc.  It does this by calling Text::Balanced::quotelike().

If a quote-like operator is found, a reference to an array of eight elements
is returned. Those elements are identical to the last eight elements returned
by Text::Balanced::extract_quotelike() in an array context, namely:

=over 4

=item [0]

the name of the quotelike operator -- 'q', 'qq', 'm', 's', 'tr' -- if the
operator was named; otherwise C<undef>,

=item [1]

the left delimiter of the first block of the operation,

=item [2]

the text of the first block of the operation
(that is, the contents of
a quote, the regex of a match, or substitution or the target list of a
translation),

=item [3]

the right delimiter of the first block of the operation,

=item [4]

the left delimiter of the second block of the operation if there is one
(that is, if it is a C<s>, C<tr>, or C<y>); otherwise C<undef>,

=item [5]

the text of the second block of the operation if there is one
(that is, the replacement of a substitution or the translation list
of a translation); otherwise C<undef>,

=item [6]

the right delimiter of the second block of the operation (if any);
otherwise C<undef>,

=item [7]

the trailing modifiers on the operation (if any); otherwise C<undef>.

=back

If a quote-like expression is not found, the directive fails with the usual
C<undef> value.

The C<E<lt>perl_variableE<gt>> directive can be used to parse any Perl
variable: $scalar, @array, %hash, $ref->{field}[$index], etc.
It does this by calling Text::Balanced::extract_variable().

If the directive matches text representing a valid Perl variable
specification, it returns that text. Otherwise it fails with the usual
C<undef> value.

The C<E<lt>perl_codeblockE<gt>> directive can be used to parse curly-brace-delimited block of Perl code, such as: { $a = 1; f() =~ m/pat/; }.
It does this by calling Text::Balanced::extract_codeblock().

If the directive matches text representing a valid Perl code block,
it returns that text. Otherwise it fails with the usual C<undef> value.

You can also tell it what kind of brackets to use as the outermost
delimiters. For example:

    arglist: <perl_codeblock ()>

causes an arglist to match a perl code block whose outermost delimiters
are C<(...)> (rather than the default C<{...}>).


=item Constructing tokens

Eventually, Parse::RecDescent will be able to parse tokenized input, as
well as ordinary strings. In preparation for this joyous day, the
C<E<lt>token:...E<gt>> directive has been provided.
This directive creates a token which will be suitable for
input to a Parse::RecDescent parser (when it eventually supports
tokenized input).

The text of the token is the value of the
immediately preceding item in the production. A
C<E<lt>token:...E<gt>> directive always succeeds with a return
value which is the hash reference that is the new token. It also
sets the return value for the production to that hash ref.

The C<E<lt>token:...E<gt>> directive makes it easy to build
a Parse::RecDescent-compatible lexer in Parse::RecDescent:

    my $lexer = new Parse::RecDescent q
    {
    lex:    token(s)

    token:  /a\b/          <token:INDEF>
         |  /the\b/        <token:DEF>
         |  /fly\b/        <token:NOUN,VERB>
         |  /[a-z]+/i { lc $item[1] }  <token:ALPHA>
         |  <error: Unknown token>

    };

which will eventually be able to be used with a regular Parse::RecDescent
grammar:

    my $parser = new Parse::RecDescent q
    {
    startrule: subrule1 subrule 2

    # ETC...
    };

either with a pre-lexing phase:

    $parser->startrule( $lexer->lex($data) );

or with a lex-on-demand approach:

    $parser->startrule( sub{$lexer->token(\$data)} );

But at present, only the C<E<lt>token:...E<gt>> directive is
actually implemented. The rest is vapourware.

=item Specifying operations

One of the commonest requirements when building a parser is to specify
binary operators. Unfortunately, in a normal grammar, the rules for
such things are awkward:

    disjunction:    conjunction ('or' conjunction)(s?)
        { $return = [ $item[1], @{$item[2]} ] }

    conjunction:    atom ('and' atom)(s?)
        { $return = [ $item[1], @{$item[2]} ] }

or inefficient:

    disjunction:    conjunction 'or' disjunction
        { $return = [ $item[1], @{$item[2]} ] }
       |    conjunction
        { $return = [ $item[1] ] }

    conjunction:    atom 'and' conjunction
        { $return = [ $item[1], @{$item[2]} ] }
       |    atom
        { $return = [ $item[1] ] }

and either way is ugly and hard to get right.

The C<E<lt>leftop:...E<gt>> and C<E<lt>rightop:...E<gt>> directives provide an
easier way of specifying such operations. Using C<E<lt>leftop:...E<gt>> the
above examples become:

    disjunction:    <leftop: conjunction 'or' conjunction>
    conjunction:    <leftop: atom 'and' atom>

The C<E<lt>leftop:...E<gt>> directive specifies a left-associative binary operator.
It is specified around three other grammar elements
(typically subrules or terminals), which match the left operand,
the operator itself, and the right operand respectively.

A C<E<lt>leftop:...E<gt>> directive such as:

    disjunction:    <leftop: conjunction 'or' conjunction>

is converted to the following:

    disjunction:    ( conjunction ('or' conjunction)(s?)
        { $return = [ $item[1], @{$item[2]} ] } )

In other words, a C<E<lt>leftop:...E<gt>> directive matches the left operand followed by zero
or more repetitions of both the operator and the right operand. It then
flattens the matched items into an anonymous array which becomes the
(single) value of the entire C<E<lt>leftop:...E<gt>> directive.

For example, an C<E<lt>leftop:...E<gt>> directive such as:

    output:  <leftop: ident '<<' expr >

when given a string such as:

    cout << var << "str" << 3

would match, and C<$item[1]> would be set to:

    [ 'cout', 'var', '"str"', '3' ]

In other words:

    output:  <leftop: ident '<<' expr >

is equivalent to a left-associative operator:

    output:  ident          { $return = [$item[1]]   }
          |  ident '<<' expr        { $return = [@item[1,3]]     }
          |  ident '<<' expr '<<' expr      { $return = [@item[1,3,5]]   }
          |  ident '<<' expr '<<' expr '<<' expr    { $return = [@item[1,3,5,7]] }
          #  ...etc...


Similarly, the C<E<lt>rightop:...E<gt>> directive takes a left operand, an operator, and a right operand:

    assign:  <rightop: var '=' expr >

and converts them to:

    assign:  ( (var '=' {$return=$item[1]})(s?) expr
        { $return = [ @{$item[1]}, $item[2] ] } )

which is equivalent to a right-associative operator:

    assign:  expr       { $return = [$item[1]]       }
          |  var '=' expr       { $return = [@item[1,3]]     }
          |  var '=' var '=' expr   { $return = [@item[1,3,5]]   }
          |  var '=' var '=' var '=' expr   { $return = [@item[1,3,5,7]] }
          #  ...etc...


Note that for both the C<E<lt>leftop:...E<gt>> and C<E<lt>rightop:...E<gt>> directives, the directive does not normally
return the operator itself, just a list of the operands involved. This is
particularly handy for specifying lists:

    list: '(' <leftop: list_item ',' list_item> ')'
        { $return = $item[2] }

There is, however, a problem: sometimes the operator is itself significant.
For example, in a Perl list a comma and a C<=E<gt>> are both
valid separators, but the C<=E<gt>> has additional stringification semantics.
Hence it's important to know which was used in each case.

To solve this problem the
C<E<lt>leftop:...E<gt>> and C<E<lt>rightop:...E<gt>> directives
I<do> return the operator(s) as well, under two circumstances.
The first case is where the operator is specified as a subrule. In that instance,
whatever the operator matches is returned (on the assumption that if the operator
is important enough to have its own subrule, then it's important enough to return).

The second case is where the operator is specified as a regular
expression. In that case, if the first bracketed subpattern of the
regular expression matches, that matching value is returned (this is analogous to
the behaviour of the Perl C<split> function, except that only the first subpattern
is returned).

In other words, given the input:

    ( a=>1, b=>2 )

the specifications:

    list:      '('  <leftop: list_item separator list_item>  ')'

    separator: ',' | '=>'

or:

    list:      '('  <leftop: list_item /(,|=>)/ list_item>  ')'

cause the list separators to be interleaved with the operands in the
anonymous array in C<$item[2]>:

    [ 'a', '=>', '1', ',', 'b', '=>', '2' ]


But the following version:

    list:      '('  <leftop: list_item /,|=>/ list_item>  ')'

returns only the operators:

    [ 'a', '1', 'b', '2' ]

Of course, none of the above specifications handle the case of an empty
list, since the C<E<lt>leftop:...E<gt>> and C<E<lt>rightop:...E<gt>> directives
require at least a single right or left operand to match. To specify
that the operator can match "trivially",
it's necessary to add a C<(s?)> qualifier to the directive:

    list:      '('  <leftop: list_item /(,|=>)/ list_item>(s?)  ')'

Note that in almost all the above examples, the first and third arguments
of the C<<leftop:...E<gt>> directive were the same subrule. That is because
C<<leftop:...E<gt>>'s are frequently used to specify "separated" lists of the
same type of item. To make such lists easier to specify, the following
syntax:

    list:   element(s /,/)

is exactly equivalent to:

    list:   <leftop: element /,/ element>

Note that the separator must be specified as a raw pattern (i.e.
not a string or subrule).


=item Scored productions

By default, Parse::RecDescent grammar rules always accept the first
production that matches the input. But if two or more productions may
potentially match the same input, choosing the first that does so may
not be optimal.

For example, if you were parsing the sentence "time flies like an arrow",
you might use a rule like this:

    sentence: verb noun preposition article noun { [@item] }
    | adjective noun verb article noun   { [@item] }
    | noun verb preposition article noun { [@item] }

Each of these productions matches the sentence, but the third one
is the most likely interpretation. However, if the sentence had been
"fruit flies like a banana", then the second production is probably
the right match.

To cater for such situations, the C<E<lt>score:...E<gt>> can be used.
The directive is equivalent to an unconditional C<E<lt>rejectE<gt>>,
except that it allows you to specify a "score" for the current
production. If that score is numerically greater than the best
score of any preceding production, the current production is cached for later
consideration. If no later production matches, then the cached
production is treated as having matched, and the value of the
item immediately before its C<E<lt>score:...E<gt>> directive is returned as the
result.

In other words, by putting a C<E<lt>score:...E<gt>> directive at the end of
each production, you can select which production matches using
criteria other than specification order. For example:

    sentence: verb noun preposition article noun { [@item] } <score: sensible(@item)>
    | adjective noun verb article noun   { [@item] } <score: sensible(@item)>
    | noun verb preposition article noun { [@item] } <score: sensible(@item)>

Now, when each production reaches its respective C<E<lt>score:...E<gt>>
directive, the subroutine C<sensible> will be called to evaluate the
matched items (somehow). Once all productions have been tried, the
one which C<sensible> scored most highly will be the one that is
accepted as a match for the rule.

The variable $score always holds the current best score of any production,
and the variable $score_return holds the corresponding return value.

As another example, the following grammar matches lines that may be
separated by commas, colons, or semi-colons. This can be tricky if
a colon-separated line also contains commas, or vice versa. The grammar
resolves the ambiguity by selecting the rule that results in the
fewest fields:

    line: seplist[sep=>',']  <score: -@{$item[1]}>
    | seplist[sep=>':']  <score: -@{$item[1]}>
    | seplist[sep=>" "]  <score: -@{$item[1]}>

    seplist: <skip:""> <leftop: /[^$arg{sep}]*/ "$arg{sep}" /[^$arg{sep}]*/>

Note the use of negation within the C<E<lt>score:...E<gt>> directive
to ensure that the seplist with the most items gets the lowest score.

As the above examples indicate, it is often the case that all productions
in a rule use exactly the same C<E<lt>score:...E<gt>> directive. It is
tedious to have to repeat this identical directive in every production, so
Parse::RecDescent also provides the C<E<lt>autoscore:...E<gt>> directive.

If an C<E<lt>autoscore:...E<gt>> directive appears in any
production of a rule, the code it specifies is used as the scoring
code for every production of that rule, except productions that already
end with an explicit C<E<lt>score:...E<gt>> directive. Thus the rules above could
be rewritten:

    line: <autoscore: -@{$item[1]}>
    line: seplist[sep=>',']
    | seplist[sep=>':']
    | seplist[sep=>" "]


    sentence: <autoscore: sensible(@item)>
    | verb noun preposition article noun { [@item] }
    | adjective noun verb article noun   { [@item] }
    | noun verb preposition article noun { [@item] }

Note that the C<E<lt>autoscore:...E<gt>> directive itself acts as an
unconditional C<E<lt>rejectE<gt>>, and (like the C<E<lt>rulevar:...E<gt>>
directive) is pruned at compile-time wherever possible.


=item Dispensing with grammar checks

During the compilation phase of parser construction, Parse::RecDescent performs
a small number of checks on the grammar it's given. Specifically it checks that
the grammar is not left-recursive, that there are no "insatiable" constructs of
the form:

    rule: subrule(s) subrule

and that there are no rules missing (i.e. referred to, but never defined).

These checks are important during development, but can slow down parser
construction in stable code. So Parse::RecDescent provides the
E<lt>nocheckE<gt> directive to turn them off. The directive can only appear
before the first rule definition, and switches off checking throughout the rest
of the current grammar.

Typically, this directive would be added when a parser has been thoroughly
tested and is ready for release.

=back


=head2 Subrule argument lists

It is occasionally useful to pass data to a subrule which is being invoked. For
example, consider the following grammar fragment:

    classdecl: keyword decl

    keyword:   'struct' | 'class';

    decl:      # WHATEVER

The C<decl> rule might wish to know which of the two keywords was used
(since it may affect some aspect of the way the subsequent declaration
is interpreted). C<Parse::RecDescent> allows the grammar designer to
pass data into a rule, by placing that data in an I<argument list>
(that is, in square brackets) immediately after any subrule item in a
production. Hence, we could pass the keyword to C<decl> as follows:

    classdecl: keyword decl[ $item[1] ]

    keyword:   'struct' | 'class';

    decl:      # WHATEVER

The argument list can consist of any number (including zero!) of comma-separated
Perl expressions. In other words, it looks exactly like a Perl anonymous
array reference. For example, we could pass the keyword, the name of the
surrounding rule, and the literal 'keyword' to C<decl> like so:

    classdecl: keyword decl[$item[1],$item[0],'keyword']

    keyword:   'struct' | 'class';

    decl:      # WHATEVER

Within the rule to which the data is passed (C<decl> in the above examples)
that data is available as the elements of a local variable C<@arg>. Hence
C<decl> might report its intentions as follows:

    classdecl: keyword decl[$item[1],$item[0],'keyword']

    keyword:   'struct' | 'class';

    decl:      { print "Declaring $arg[0] (a $arg[2])\n";
         print "(this rule called by $arg[1])" }

Subrule argument lists can also be interpreted as hashes, simply by using
the local variable C<%arg> instead of C<@arg>. Hence we could rewrite the
previous example:

    classdecl: keyword decl[keyword => $item[1],
        caller  => $item[0],
        type    => 'keyword']

    keyword:   'struct' | 'class';

    decl:      { print "Declaring $arg{keyword} (a $arg{type})\n";
         print "(this rule called by $arg{caller})" }

Both C<@arg> and C<%arg> are always available, so the grammar designer may
choose whichever convention (or combination of conventions) suits best.

Subrule argument lists are also useful for creating "rule templates"
(especially when used in conjunction with the C<E<lt>matchrule:...E<gt>>
directive). For example, the subrule:

    list:     <matchrule:$arg{rule}> /$arg{sep}/ list[%arg]
        { $return = [ $item[1], @{$item[3]} ] }
    |     <matchrule:$arg{rule}>
        { $return = [ $item[1]] }

is a handy template for the common problem of matching a separated list.
For example:

    function: 'func' name '(' list[rule=>'param',sep=>';'] ')'

    param:    list[rule=>'name',sep=>','] ':' typename

    name:     /\w+/

    typename: name


When a subrule argument list is used with a repeated subrule, the argument list
goes I<before> the repetition specifier:

    list:   /some|many/ thing[ $item[1] ](s)

The argument list is "late bound". That is, it is re-evaluated for every
repetition of the repeated subrule.
This means that each repeated attempt to match the subrule may be
passed a completely different set of arguments if the value of the
expression in the argument list changes between attempts. So, for
example, the grammar:

    { $::species = 'dogs' }

    pair:   'two' animal[$::species](s)

    animal: /$arg[0]/ { $::species = 'cats' }

will match the string "two dogs cats cats" completely, whereas
it will only match the string "two dogs dogs dogs" up to the
eighth letter. If the value of the argument list were "early bound"
(that is, evaluated only the first time a repeated subrule match is
attempted), one would expect the matching behaviours to be reversed.

Of course, it is possible to effectively "early bind" such argument lists
by passing them a value which does not change on each repetition. For example:

    { $::species = 'dogs' }

    pair:   'two' { $::species } animal[$item[2]](s)

    animal: /$arg[0]/ { $::species = 'cats' }


Arguments can also be passed to the start rule, simply by appending them
to the argument list with which the start rule is called (I<after> the
"line number" parameter). For example, given:

    $parser = new Parse::RecDescent ( $grammar );

    $parser->data($text, 1, "str", 2, \@arr);

    #         ^^^^^  ^  ^^^^^^^^^^^^^^^
    #       |    |     |
    # TEXT TO BE PARSED  |     |
    # STARTING LINE NUMBER     |
    # ELEMENTS OF @arg WHICH IS PASSED TO RULE data

then within the productions of the rule C<data>, the array C<@arg> will contain
C<("str", 2, \@arr)>.


=head2 Alternations

Alternations are implicit (unnamed) rules defined as part of a production. An
alternation is defined as a series of '|'-separated productions inside a
pair of round brackets. For example:

    character: 'the' ( good | bad | ugly ) /dude/

Every alternation implicitly defines a new subrule, whose
automatically-generated name indicates its origin:
"_alternation_<I>_of_production_<P>_of_rule<R>" for the appropriate
values of <I>, <P>, and <R>. A call to this implicit subrule is then
inserted in place of the brackets. Hence the above example is merely a
convenient short-hand for:

    character: 'the'
       _alternation_1_of_production_1_of_rule_character
       /dude/

    _alternation_1_of_production_1_of_rule_character:
       good | bad | ugly

Since alternations are parsed by recursively calling the parser generator,
any type(s) of item can appear in an alternation. For example:

    character: 'the' ( 'high' "plains"  # Silent, with poncho
         | /no[- ]name/ # Silent, no poncho
         | vengeance_seeking    # Poncho-optional
         | <error>
         ) drifter

In this case, if an error occurred, the automatically generated
message would be:

    ERROR (line <N>): Invalid implicit subrule: Expected
          'high' or /no[- ]name/ or generic,
          but found "pacifist" instead

Since every alternation actually has a name, it's even possible
to extend or replace them:

    parser->Replace(
    "_alternation_1_of_production_1_of_rule_character:
        'generic Eastwood'"
        );

More importantly, since alternations are a form of subrule, they can be given
repetition specifiers:

    character: 'the' ( good | bad | ugly )(?) /dude/


=head2 Incremental Parsing

C<Parse::RecDescent> provides two methods - C<Extend> and C<Replace> - which
can be used to alter the grammar matched by a parser. Both methods
take the same argument as C<Parse::RecDescent::new>, namely a
grammar specification string

C<Parse::RecDescent::Extend> interprets the grammar specification and adds any
productions it finds to the end of the rules for which they are specified. For
example:

    $add = "name: 'Jimmy-Bob' | 'Bobby-Jim'\ndesc: colour /necks?/";
    parser->Extend($add);

adds two productions to the rule "name" (creating it if necessary) and one
production to the rule "desc".

C<Parse::RecDescent::Replace> is identical, except that it first resets are
rule specified in the additional grammar, removing any existing productions.
Hence after:

    $add = "name: 'Jimmy-Bob' | 'Bobby-Jim'\ndesc: colour /necks?/";
    parser->Replace($add);

there are I<only> valid "name"s and the one possible description.

A more interesting use of the C<Extend> and C<Replace> methods is to call them
inside the action of an executing parser. For example:

    typedef: 'typedef' type_name identifier ';'
           { $thisparser->Extend("type_name: '$item[3]'") }
       | <error>

    identifier: ...!type_name /[A-Za-z_]w*/

which automatically prevents type names from being typedef'd, or:

    command: 'map' key_name 'to' abort_key
           { $thisparser->Replace("abort_key: '$item[2]'") }
       | 'map' key_name 'to' key_name
           { map_key($item[2],$item[4]) }
       | abort_key
           { exit if confirm("abort?") }

    abort_key: 'q'

    key_name: ...!abort_key /[A-Za-z]/

which allows the user to change the abort key binding, but not to unbind it.

The careful use of such constructs makes it possible to reconfigure a
a running parser, eliminating the need for semantic feedback by
providing syntactic feedback instead. However, as currently implemented,
C<Replace()> and C<Extend()> have to regenerate and re-C<eval> the
entire parser whenever they are called. This makes them quite slow for
large grammars.

In such cases, the judicious use of an interpolated regex is likely to
be far more efficient:

    typedef: 'typedef' type_name/ identifier ';'
           { $thisparser->{local}{type_name} .= "|$item[3]" }
       | <error>

    identifier: ...!type_name /[A-Za-z_]w*/

    type_name: /$thisparser->{local}{type_name}/


=head2 Precompiling parsers

Normally Parse::RecDescent builds a parser from a grammar at run-time.
That approach simplifies the design and implementation of parsing code,
but has the disadvantage that it slows the parsing process down - you
have to wait for Parse::RecDescent to build the parser every time the
program runs. Long or complex grammars can be particularly slow to
build, leading to unacceptable delays at start-up.

To overcome this, the module provides a way of "pre-building" a parser
object and saving it in a separate module. That module can then be used
to create clones of the original parser.

A grammar may be precompiled using the C<Precompile> class method.
For example, to precompile a grammar stored in the scalar $grammar,
and produce a class named PreGrammar in a module file named PreGrammar.pm,
you could use:

    use Parse::RecDescent;

    Parse::RecDescent->Precompile([$options_hashref], $grammar, "PreGrammar", ["RuntimeClass"]);

The first required argument is the grammar string, the second is the
name of the class to be built. The name of the module file is
generated automatically by appending ".pm" to the last element of the
class name. Thus

    Parse::RecDescent->Precompile($grammar, "My::New::Parser");

would produce a module file named Parser.pm.

After the class name, you may specify the name of the runtime_class
called by the Precompiled parser.  See L</"Precompiled runtimes"> for
more details.

An optional hash reference may be supplied as the first argument to
C<Precompile>.  This argument is currently EXPERIMENTAL, and may change
in a future release of Parse::RecDescent.  The only supported option
is currently C<-standalone>, see L</"Standalone precompiled parsers">.

It is somewhat tedious to have to write a small Perl program just to
generate a precompiled grammar class, so Parse::RecDescent has some special
magic that allows you to do the job directly from the command-line.

If your grammar is specified in a file named F<grammar>, you can generate
a class named Yet::Another::Grammar like so:

    > perl -MParse::RecDescent - grammar Yet::Another::Grammar [Runtime::Class]

This would produce a file named F<Grammar.pm> containing the full
definition of a class called Yet::Another::Grammar. Of course, to use
that class, you would need to put the F<Grammar.pm> file in a
directory named F<Yet/Another>, somewhere in your Perl include path.

Having created the new class, it's very easy to use it to build
a parser. You simply C<use> the new module, and then call its
C<new> method to create a parser object. For example:

    use Yet::Another::Grammar;
    my $parser = Yet::Another::Grammar->new();

The effect of these two lines is exactly the same as:

    use Parse::RecDescent;

    open GRAMMAR_FILE, "grammar" or die;
    local $/;
    my $grammar = <GRAMMAR_FILE>;

    my $parser = Parse::RecDescent->new($grammar);

only considerably faster.

Note however that the parsers produced by either approach are exactly
the same, so whilst precompilation has an effect on I<set-up> speed,
it has no effect on I<parsing> speed. RecDescent 2.0 will address that
problem.

=head3 Standalone precompiled parsers

Until version 1.967003 of Parse::RecDescent, parser modules built with
C<Precompile> were dependent on Parse::RecDescent.  Future
Parse::RecDescent releases with different internal implementations
would break pre-existing precompiled parsers.

Version 1.967_005 added the ability for Parse::RecDescent to include
itself in the resulting .pm file if you pass the boolean option
C<-standalone> to C<Precompile>:

    Parse::RecDescent->Precompile({ -standalone => 1, },
        $grammar, "My::New::Parser");

Parse::RecDescent is included as C<$class::_Runtime> in order to avoid
conflicts between an installed version of Parse::RecDescent and other
precompiled, standalone parser made with Parse::RecDescent.  The name
of this class may be changed with the C<-runtime_class> option to
Precompile.  This renaming is experimental, and is subject to change
in future versions.

Precompiled parsers remain dependent on Parse::RecDescent by default,
as this feature is still considered experimental.  In the future,
standalone parsers will become the default.

=head3 Precompiled runtimes

Standalone precompiled parsers each include a copy of
Parse::RecDescent.  For users who have a family of related precompiled
parsers, this is very inefficient.  C<Precompile> now supports an
experimental C<-runtime_class> option.  To build a precompiled parser
with a different runtime name, call:

    Parse::RecDescent->Precompile({
            -standalone => 1,
            -runtime_class => "My::Runtime",
        },
        $grammar, "My::New::Parser");

The resulting standalone parser will contain a copy of
Parse::RecDescent, renamed to "My::Runtime".

To build a set of parsers that C<use> a custom-named runtime, without
including that runtime in the output, simply build those parsers with
C<-runtime_class> and without C<-standalone>:

    Parse::RecDescent->Precompile({
            -runtime_class => "My::Runtime",
        },
        $grammar, "My::New::Parser");

The runtime itself must be generated as well, so that it may be
C<use>d by My::New::Parser.  To generate the runtime file, use one of
the two folling calls:

    Parse::RecDescent->PrecompiledRuntime("My::Runtime");

    Parse::RecDescent->Precompile({
            -standalone => 1,
            -runtime_class => "My::Runtime",
        },
        '', # empty grammar
        "My::Runtime");

=head1 GOTCHAS

This section describes common mistakes that grammar writers seem to
make on a regular basis.

=head2 1. Expecting an error to always invalidate a parse

A common mistake when using error messages is to write the grammar like this:

    file: line(s)

    line: line_type_1
    | line_type_2
    | line_type_3
    | <error>

The expectation seems to be that any line that is not of type 1, 2 or 3 will
invoke the C<E<lt>errorE<gt>> directive and thereby cause the parse to fail.

Unfortunately, that only happens if the error occurs in the very first line.
The first rule states that a C<file> is matched by one or more lines, so if
even a single line succeeds, the first rule is completely satisfied and the
parse as a whole succeeds. That means that any error messages generated by
subsequent failures in the C<line> rule are quietly ignored.

Typically what's really needed is this:

    file: line(s) eofile    { $return = $item[1] }

    line: line_type_1
    | line_type_2
    | line_type_3
    | <error>

    eofile: /^\Z/

The addition of the C<eofile> subrule  to the first production means that
a file only matches a series of successful C<line> matches I<that consume the
complete input text>. If any input text remains after the lines are matched,
there must have been an error in the last C<line>. In that case the C<eofile>
rule will fail, causing the entire C<file> rule to fail too.

Note too that C<eofile> must match C</^\Z/> (end-of-text), I<not>
C</^\cZ/> or C</^\cD/> (end-of-file).

And don't forget the action at the end of the production. If you just
write:

    file: line(s) eofile

then the value returned by the C<file> rule will be the value of its
last item: C<eofile>. Since C<eofile> always returns an empty string
on success, that will cause the C<file> rule to return that empty
string. Apart from returning the wrong value, returning an empty string
will trip up code such as:

    $parser->file($filetext) || die;

(since "" is false).

Remember that Parse::RecDescent returns undef on failure,
so the only safe test for failure is:

    defined($parser->file($filetext)) || die;


=head2 2. Using a C<return> in an action

An action is like a C<do> block inside the subroutine implementing the
surrounding rule. So if you put a C<return> statement in an action:

    range: '(' start '..' end )'
        { return $item{end} }
       /\s+/

that subroutine will immediately return, without checking the rest of
the items in the current production (e.g. the C</\s+/>) and without
setting up the necessary data structures to tell the parser that the
rule has succeeded.

The correct way to set a return value in an action is to set the C<$return>
variable:

    range: '(' start '..' end )'
                { $return = $item{end} }
           /\s+/


=head2 2. Setting C<$Parse::RecDescent::skip> at parse time

If you want to change the default skipping behaviour (see
L<Terminal Separators> and the C<E<lt>skip:...E<gt>> directive) by setting
C<$Parse::RecDescent::skip> you have to remember to set this variable
I<before> creating the grammar object.

For example, you might want to skip all Perl-like comments with this
regular expression:

   my $skip_spaces_and_comments = qr/
         (?mxs:
            \s+         # either spaces
            | \# .*?$   # or a dash and whatever up to the end of line
         )*             # repeated at will (in whatever order)
      /;

And then:

   my $parser1 = Parse::RecDescent->new($grammar);

   $Parse::RecDescent::skip = $skip_spaces_and_comments;

   my $parser2 = Parse::RecDescent->new($grammar);

   $parser1->parse($text); # this does not cope with comments
   $parser2->parse($text); # this skips comments correctly

The two parsers behave differently, because any skipping behaviour
specified via C<$Parse::RecDescent::skip> is hard-coded when the
grammar object is built, not at parse time.


=head1 DIAGNOSTICS

Diagnostics are intended to be self-explanatory (particularly if you
use B<-RD_HINT> (under B<perl -s>) or define C<$::RD_HINT> inside the program).

C<Parse::RecDescent> currently diagnoses the following:

=over 4

=item *

Invalid regular expressions used as pattern terminals (fatal error).

=item *

Invalid Perl code in code blocks (fatal error).

=item *

Lookahead used in the wrong place or in a nonsensical way (fatal error).

=item *

"Obvious" cases of left-recursion (fatal error).

=item *

Missing or extra components in a C<E<lt>leftopE<gt>> or C<E<lt>rightopE<gt>>
directive.

=item *

Unrecognisable components in the grammar specification (fatal error).

=item *

"Orphaned" rule components specified before the first rule (fatal error)
or after an C<E<lt>errorE<gt>> directive (level 3 warning).

=item *

Missing rule definitions (this only generates a level 3 warning, since you
may be providing them later via C<Parse::RecDescent::Extend()>).

=item *

Instances where greedy repetition behaviour will almost certainly
cause the failure of a production (a level 3 warning - see
L<"ON-GOING ISSUES AND FUTURE DIRECTIONS"> below).

=item *

Attempts to define rules named 'Replace' or 'Extend', which cannot be
called directly through the parser object because of the predefined
meaning of C<Parse::RecDescent::Replace> and
C<Parse::RecDescent::Extend>. (Only a level 2 warning is generated, since
such rules I<can> still be used as subrules).

=item *

Productions which consist of a single C<E<lt>error?E<gt>>
directive, and which therefore may succeed unexpectedly
(a level 2 warning, since this might conceivably be the desired effect).

=item *

Multiple consecutive lookahead specifiers (a level 1 warning only, since their
effects simply accumulate).

=item *

Productions which start with a C<E<lt>rejectE<gt>> or C<E<lt>rulevar:...E<gt>>
directive. Such productions are optimized away (a level 1 warning).

=item *

Rules which are autogenerated under C<$::AUTOSTUB> (a level 1 warning).

=back

=head1 AUTHOR

Damian Conway (damian@conway.org)
Jeremy T. Braun (JTBRAUN@CPAN.org) [current maintainer]

=head1 BUGS AND IRRITATIONS

There are undoubtedly serious bugs lurking somewhere in this much code :-)
Bug reports, test cases and other feedback are most welcome.

Ongoing annoyances include:

=over 4

=item *

There's no support for parsing directly from an input stream.
If and when the Perl Gods give us regular expressions on streams,
this should be trivial (ahem!) to implement.

=item *

The parser generator can get confused if actions aren't properly
closed or if they contain particularly nasty Perl syntax errors
(especially unmatched curly brackets).

=item *

The generator only detects the most obvious form of left recursion
(potential recursion on the first subrule in a rule). More subtle
forms of left recursion (for example, through the second item in a
rule after a "zero" match of a preceding "zero-or-more" repetition,
or after a match of a subrule with an empty production) are not found.

=item *

Instead of complaining about left-recursion, the generator should
silently transform the grammar to remove it. Don't expect this
feature any time soon as it would require a more sophisticated
approach to parser generation than is currently used.

=item *

The generated parsers don't always run as fast as might be wished.

=item *

The meta-parser should be bootstrapped using C<Parse::RecDescent> :-)

=back

=head1 ON-GOING ISSUES AND FUTURE DIRECTIONS

=over 4

=item 1.

Repetitions are "incorrigibly greedy" in that they will eat everything they can
and won't backtrack if that behaviour causes a production to fail needlessly.
So, for example:

    rule: subrule(s) subrule

will I<never> succeed, because the repetition will eat all the
subrules it finds, leaving none to match the second item. Such
constructions are relatively rare (and C<Parse::RecDescent::new> generates a
warning whenever they occur) so this may not be a problem, especially
since the insatiable behaviour can be overcome "manually" by writing:

    rule: penultimate_subrule(s) subrule

    penultimate_subrule: subrule ...subrule

The issue is that this construction is exactly twice as expensive as the
original, whereas backtracking would add only 1/I<N> to the cost (for
matching I<N> repetitions of C<subrule>). I would welcome feedback on
the need for backtracking; particularly on cases where the lack of it
makes parsing performance problematical.

=item 2.

Having opened that can of worms, it's also necessary to consider whether there
is a need for non-greedy repetition specifiers. Again, it's possible (at some
cost) to manually provide the required functionality:

    rule: nongreedy_subrule(s) othersubrule

    nongreedy_subrule: subrule ...!othersubrule

Overall, the issue is whether the benefit of this extra functionality
outweighs the drawbacks of further complicating the (currently
minimalist) grammar specification syntax, and (worse) introducing more overhead
into the generated parsers.

=item 3.

An C<E<lt>autocommitE<gt>> directive would be nice. That is, it would be useful to be
able to say:

    command: <autocommit>
    command: 'find' name
       | 'find' address
       | 'do' command 'at' time 'if' condition
       | 'do' command 'at' time
       | 'do' command
       | unusual_command

and have the generator work out that this should be "pruned" thus:

    command: 'find' name
       | 'find' <commit> address
       | 'do' <commit> command <uncommit>
        'at' time
        'if' <commit> condition
       | 'do' <commit> command <uncommit>
        'at' <commit> time
       | 'do' <commit> command
       | unusual_command

There are several issues here. Firstly, should the
C<E<lt>autocommitE<gt>> automatically install an C<E<lt>uncommitE<gt>>
at the start of the last production (on the grounds that the "command"
rule doesn't know whether an "unusual_command" might start with "find"
or "do") or should the "unusual_command" subgraph be analysed (to see
if it I<might> be viable after a "find" or "do")?

The second issue is how regular expressions should be treated. The simplest
approach would be simply to uncommit before them (on the grounds that they
I<might> match). Better efficiency would be obtained by analyzing all preceding
literal tokens to determine whether the pattern would match them.

Overall, the issues are: can such automated "pruning" approach a hand-tuned
version sufficiently closely to warrant the extra set-up expense, and (more
importantly) is the problem important enough to even warrant the non-trivial
effort of building an automated solution?

=back

=head1 SUPPORT

=head2 Source Code Repository

L<http://github.com/jtbraun/Parse-RecDescent>

=head2 Mailing List

Visit L<http://www.perlfoundation.org/perl5/index.cgi?parse_recdescent> to sign up for the mailing list.

L<http://www.PerlMonks.org> is also a good place to ask
questions. Previous posts about Parse::RecDescent can typically be
found with this search:
L<http://perlmonks.org/index.pl?node=recdescent>.

=head2 FAQ

Visit L<Parse::RecDescent::FAQ> for answers to frequently (and not so
frequently) asked questions about Parse::RecDescent.

=head2 View/Report Bugs

To view the current bug list or report a new issue visit
L<https://rt.cpan.org/Public/Dist/Display.html?Name=Parse-RecDescent>.

=head1 SEE ALSO

L<Regexp::Grammars> provides Parse::RecDescent style parsing using native
Perl 5.10 regular expressions.


=head1 LICENCE AND COPYRIGHT

Copyright (c) 1997-2007, Damian Conway C<< <DCONWAY@CPAN.org> >>. All rights
reserved.

This module is free software; you can redistribute it and/or
modify it under the same terms as Perl itself. See L<perlartistic>.


=head1 DISCLAIMER OF WARRANTY

BECAUSE THIS SOFTWARE IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY
FOR THE SOFTWARE, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT WHEN
OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES
PROVIDE THE SOFTWARE "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER
EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE
ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE SOFTWARE IS WITH
YOU. SHOULD THE SOFTWARE PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL
NECESSARY SERVICING, REPAIR, OR CORRECTION.

IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR
REDISTRIBUTE THE SOFTWARE AS PERMITTED BY THE ABOVE LICENCE, BE
LIABLE TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL,
OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR INABILITY TO USE
THE SOFTWARE (INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR DATA BEING
RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD PARTIES OR A
FAILURE OF THE SOFTWARE TO OPERATE WITH ANY OTHER SOFTWARE), EVEN IF
SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE POSSIBILITY OF
SUCH DAMAGES.
