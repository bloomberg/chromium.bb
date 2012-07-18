package Devel::Declare;

use strict;
use warnings;
use 5.008001;

our $VERSION = '0.006011';

use constant DECLARE_NAME => 1;
use constant DECLARE_PROTO => 2;
use constant DECLARE_NONE => 4;
use constant DECLARE_PACKAGE => 8+1; # name implicit

use vars qw(%declarators %declarator_handlers @ISA);
use base qw(DynaLoader);
use Scalar::Util 'set_prototype';
use B::Hooks::OP::Check 0.19;

bootstrap Devel::Declare;

@ISA = ();

initialize();

sub import {
  my ($class, %args) = @_;
  my $target = caller;
  if (@_ == 1) { # "use Devel::Declare;"
    no strict 'refs';
    foreach my $name (qw(NAME PROTO NONE PACKAGE)) {
      *{"${target}::DECLARE_${name}"} = *{"DECLARE_${name}"};
    }
  } else {
    $class->setup_for($target => \%args);
  }
}

sub unimport {
  my ($class) = @_;
  my $target = caller;
  $class->teardown_for($target);
}

sub setup_for {
  my ($class, $target, $args) = @_;
  setup();
  foreach my $key (keys %$args) {
    my $info = $args->{$key};
    my ($flags, $sub);
    if (ref($info) eq 'ARRAY') {
      ($flags, $sub) = @$info;
    } elsif (ref($info) eq 'CODE') {
      $flags = DECLARE_NAME;
      $sub = $info;
    } elsif (ref($info) eq 'HASH') {
      $flags = 1;
      $sub = $info;
    } else {
      die "Info for sub ${key} must be [ \$flags, \$sub ] or \$sub or handler hashref";
    }
    $declarators{$target}{$key} = $flags;
    $declarator_handlers{$target}{$key} = $sub;
  }
}

sub teardown_for {
  my ($class, $target) = @_;
  delete $declarators{$target};
  delete $declarator_handlers{$target};
}

my $temp_name;
my $temp_save;

sub init_declare {
  my ($usepack, $use, $inpack, $name, $proto, $traits) = @_;
  my ($name_h, $XX_h, $extra_code)
       = $declarator_handlers{$usepack}{$use}->(
           $usepack, $use, $inpack, $name, $proto, defined(wantarray), $traits
         );
  ($temp_name, $temp_save) = ([], []);
  if ($name) {
    $name = "${inpack}::${name}" unless $name =~ /::/;
    shadow_sub($name, $name_h);
  }
  if ($XX_h) {
    shadow_sub("${inpack}::X", $XX_h);
  }
  if (defined wantarray) {
    return $extra_code || '0;';
  } else {
    return;
  }
}

sub shadow_sub {
  my ($name, $cr) = @_;
  push(@$temp_name, $name);
  no strict 'refs';
  my ($pack, $pname) = ($name =~ m/(.+)::([^:]+)/);
  push(@$temp_save, $pack->can($pname));
  no warnings 'redefine';
  no warnings 'prototype';
  *{$name} = $cr;
  set_in_declare(~~@{$temp_name||[]});
}

sub done_declare {
  no strict 'refs';
  my $name = shift(@{$temp_name||[]});
  die "done_declare called with no temp_name stack" unless defined($name);
  my $saved = shift(@$temp_save);
  $name =~ s/(.*):://;
  my $temp_pack = $1;
  delete ${"${temp_pack}::"}{$name};
  if ($saved) {
    no warnings 'prototype';
    *{"${temp_pack}::${name}"} = $saved;
  }
  set_in_declare(~~@{$temp_name||[]});
}

sub build_sub_installer {
  my ($class, $pack, $name, $proto) = @_;
  return eval "
    package ${pack};
    my \$body;
    sub ${name} (${proto}) :lvalue {\n"
    .'  if (wantarray) {
        goto &$body;
      }
      my $ret = $body->(@_);
      return $ret;
    };
    sub { ($body) = @_; };';
}

sub setup_declarators {
  my ($class, $pack, $to_setup) = @_;
  die "${class}->setup_declarators(\$pack, \\\%to_setup)"
    unless defined($pack) && ref($to_setup) eq 'HASH';
  my %setup_for_args;
  foreach my $name (keys %$to_setup) {
    my $info = $to_setup->{$name};
    my $flags = $info->{flags} || DECLARE_NAME;
    my $run = $info->{run};
    my $compile = $info->{compile};
    my $proto = $info->{proto} || '&';
    my $sub_proto = $proto;
    # make all args optional to enable lvalue for DECLARE_NONE
    $sub_proto =~ s/;//; $sub_proto = ';'.$sub_proto;
    #my $installer = $class->build_sub_installer($pack, $name, $proto);
    my $installer = $class->build_sub_installer($pack, $name, '@');
    $installer->(sub :lvalue {
#{ no warnings 'uninitialized'; warn 'INST: '.join(', ', @_)."\n"; }
      if (@_) {
        if (ref $_[0] eq 'HASH') {
          shift;
          if (wantarray) {
            my @ret = $run->(undef, undef, @_);
            return @ret;
          }
          my $r = $run->(undef, undef, @_);
          return $r;
        } else {
          return @_[1..$#_];
        }
      }
      return my $sv;
    });
    $setup_for_args{$name} = [
      $flags,
      sub {
        my ($usepack, $use, $inpack, $name, $proto, $shift_hashref, $traits) = @_;
        my $extra_code = $compile->($name, $proto, $traits);
        my $main_handler = sub { shift if $shift_hashref;
          ("DONE", $run->($name, $proto, @_));
        };
        my ($name_h, $XX);
        if (defined $proto) {
          $name_h = sub :lvalue { return my $sv; };
          $XX = $main_handler;
        } elsif (defined $name && length $name) {
          $name_h = $main_handler;
        }
        $extra_code ||= '';
        $extra_code = '}, sub {'.$extra_code;
        return ($name_h, $XX, $extra_code);
      }
    ];
  }
  $class->setup_for($pack, \%setup_for_args);
}

sub install_declarator {
  my ($class, $target_pack, $target_name, $flags, $filter, $handler) = @_;
  $class->setup_declarators($target_pack, {
    $target_name => {
      flags => $flags,
      compile => $filter,
      run => $handler,
   }
  });
}

sub linestr_callback_rv2cv {
  my ($name, $offset) = @_;
  $offset += toke_move_past_token($offset);
  my $pack = get_curstash_name();
  my $flags = $declarators{$pack}{$name};
  my ($found_name, $found_proto);
  if ($flags & DECLARE_NAME) {
    $offset += toke_skipspace($offset);
    my $linestr = get_linestr();
    if (substr($linestr, $offset, 2) eq '::') {
      substr($linestr, $offset, 2) = '';
      set_linestr($linestr);
    }
    if (my $len = toke_scan_word($offset, $flags & DECLARE_PACKAGE)) {
      $found_name = substr($linestr, $offset, $len);
      $offset += $len;
    }
  }
  if ($flags & DECLARE_PROTO) {
    $offset += toke_skipspace($offset);
    my $linestr = get_linestr();
    if (substr($linestr, $offset, 1) eq '(') {
      my $length = toke_scan_str($offset);
      $found_proto = get_lex_stuff();
      clear_lex_stuff();
      my $replace =
        ($found_name ? ' ' : '=')
        .'X'.(' ' x length($found_proto));
      $linestr = get_linestr();
      substr($linestr, $offset, $length) = $replace;
      set_linestr($linestr);
      $offset += $length;
    }
  }
  my @args = ($pack, $name, $pack, $found_name, $found_proto);
  $offset += toke_skipspace($offset);
  my $linestr = get_linestr();
  if (substr($linestr, $offset, 1) eq '{') {
    my $ret = init_declare(@args);
    $offset++;
    if (defined $ret && length $ret) {
      substr($linestr, $offset, 0) = $ret;
      set_linestr($linestr);
    }
  } else {
    init_declare(@args);
  }
  #warn "linestr now ${linestr}";
}

sub linestr_callback_const {
  my ($name, $offset) = @_;
  my $pack = get_curstash_name();
  my $flags = $declarators{$pack}{$name};
  if ($flags & DECLARE_NAME) {
    $offset += toke_move_past_token($offset);
    $offset += toke_skipspace($offset);
    if (toke_scan_word($offset, $flags & DECLARE_PACKAGE)) {
      my $linestr = get_linestr();
      substr($linestr, $offset, 0) = '::';
      set_linestr($linestr);
    }
  }
}

sub linestr_callback {
  my $type = shift;
  my $name = $_[0];
  my $pack = get_curstash_name();
  my $handlers = $declarator_handlers{$pack}{$name};
  if (ref $handlers eq 'CODE') {
    my $meth = "linestr_callback_${type}";
    __PACKAGE__->can($meth)->(@_);
  } elsif (ref $handlers eq 'HASH') {
    if ($handlers->{$type}) {
      $handlers->{$type}->(@_);
    }
  } else {
    die "PANIC: unknown thing in handlers for $pack $name: $handlers";
  }
}

=head1 NAME

Devel::Declare - Adding keywords to perl, in perl

=head1 SYNOPSIS

  use Method::Signatures;
  # or ...
  use MooseX::Declare;
  # etc.

  # Use some new and exciting syntax like:
  method hello (Str :$who, Int :$age where { $_ > 0 }) {
    $self->say("Hello ${who}, I am ${age} years old!");
  }

=head1 DESCRIPTION

L<Devel::Declare> can install subroutines called declarators which locally take
over Perl's parser, allowing the creation of new syntax.

This document describes how to create a simple declarator.

=head1 USAGE

We'll demonstrate the usage of C<Devel::Declare> with a motivating example: a new
C<method> keyword, which acts like the builtin C<sub>, but automatically unpacks
C<$self> and the other arguments.

  package My::Methods;
  use Devel::Declare;

=head2 Creating a declarator with C<setup_for>

You will typically create

  sub import {
    my $class = shift;
    my $caller = caller;

    Devel::Declare->setup_for(
        $caller,
        { method => { const => \&parser } }
    );
    no strict 'refs';
    *{$caller.'::method'} = sub (&) {};
  }

Starting from the end of this import routine, you'll see that we're creating a
subroutine called C<method> in the caller's namespace.  Yes, that's just a normal
subroutine, and it does nothing at all (yet!)  Note the prototype C<(&)> which means
that the caller would call it like so:

    method {
        my ($self, $arg1, $arg2) = @_;
        ...
    }

However we want to be able to call it like this

    method foo ($arg1, $arg2) {
        ...
    }

That's why we call C<setup_for> above, to register the declarator 'method' with a custom
parser, as per the next section.  It acts on an optype, usually C<'const'> as above.
(Other valid values are C<'check'> and C<'rv2cv'>).

For a simpler way to install new methods, see also L<Devel::Declare::MethodInstaller::Simple>

=head2 Writing a parser subroutine

This subroutine is called at I<compilation> time, and allows you to read the custom
syntaxes that we want (in a syntax that may or may not be valid core Perl 5) and
munge it so that the result will be parsed by the C<perl> compiler.

For this example, we're defining some globals for convenience:

    our ($Declarator, $Offset);

Then we define a parser subroutine to handle our declarator.  We'll look at this in
a few chunks.

    sub parser {
      local ($Declarator, $Offset) = @_;

C<Devel::Declare> provides some very low level utility methods to parse character
strings.  We'll define some useful higher level routines below for convenience,
and we can use these to parse the various elements in our new syntax.

Notice how our parser subroutine is invoked at compile time,
when the C<perl> parser is pointed just I<before> the declarator name.

      skip_declarator;          # step past 'method'
      my $name = strip_name;    # strip out the name 'foo', if present
      my $proto = strip_proto;  # strip out the prototype '($arg1, $arg2)', if present

Now we can prepare some code to 'inject' into the new subroutine.  For example we
might want the method as above to have C<my ($self, $arg1, $arg2) = @_> injected at
the beginning of it.  We also do some clever stuff with scopes that we'll look
at shortly.

      my $inject = make_proto_unwrap($proto);
      if (defined $name) {
        $inject = scope_injector_call().$inject;
      }
      inject_if_block($inject);

We've now managed to change C<method ($arg1, $arg2) { ... }> into C<method {
injected_code; ... }>.  This will compile...  but we've lost the name of the
method!

In a cute (or horrifying, depending on your perspective) trick, we temporarily
change the definition of the subroutine C<method> itself, to specialise it with
the C<$name> we stripped, so that it assigns the code block to that name.

Even though the I<next> time C<method> is compiled, it will be
redefined again, C<perl> caches these definitions in its parse
tree, so we'll always get the right one!

Note that we also handle the case where there was no name, allowing
an anonymous method analogous to an anonymous subroutine.

      if (defined $name) {
        $name = join('::', Devel::Declare::get_curstash_name(), $name)
          unless ($name =~ /::/);
        shadow(sub (&) { no strict 'refs'; *{$name} = shift; });
      } else {
        shadow(sub (&) { shift });
      }
    }


=head2 Parser utilities in detail

For simplicity, we're using global variables like C<$Offset> in these examples.
You may prefer to look at L<Devel::Declare::Context::Simple>, which
encapsulates the context much more cleanly.

=head3 C<skip_declarator>

This simple parser just moves across a 'token'.  The common case is
to skip the declarator, i.e.  to move to the end of the string
'method' and before the prototype and code block.

    sub skip_declarator {
      $Offset += Devel::Declare::toke_move_past_token($Offset);
    }

=head4 C<toke_move_past_token>

This builtin parser simply moves past a 'token' (matching C</[a-zA-Z_]\w*/>)
It takes an offset into the source document, and skips past the token.
It returns the number of characters skipped.

=head3 C<strip_name>

This parser skips any whitespace, then scans the next word (again matching a
'token').  We can then analyse the current line, and manipulate it (using pure
Perl).  In this case we take the name of the method out, and return it.

    sub strip_name {
      skipspace;
      if (my $len = Devel::Declare::toke_scan_word($Offset, 1)) {
        my $linestr = Devel::Declare::get_linestr();
        my $name = substr($linestr, $Offset, $len);
        substr($linestr, $Offset, $len) = '';
        Devel::Declare::set_linestr($linestr);
        return $name;
      }
      return;
    }

=head4 C<toke_scan_word>

This builtin parser, given an offset into the source document,
matches a 'token' as above but does not skip.  It returns the
length of the token matched, if any.

=head4 C<get_linestr>

This builtin returns the full text of the current line of the source document.

=head4 C<set_linestr>

This builtin sets the full text of the current line of the source document.
Beware that injecting a newline into the middle of the line is likely
to fail in surprising ways.  Generally, Perl's parser can rely on the
`current line' actually being only a single line.  Use other kinds of
whitespace instead, in the code that you inject.

=head3 C<skipspace>

This parser skips whitsepace.

    sub skipspace {
      $Offset += Devel::Declare::toke_skipspace($Offset);
    }

=head4 C<toke_skipspace>

This builtin parser, given an offset into the source document,
skips over any whitespace, and returns the number of characters
skipped.

=head3 C<strip_proto>

This is a more complex parser that checks if it's found something that
starts with C<'('> and returns everything till the matching C<')'>.

    sub strip_proto {
      skipspace;

      my $linestr = Devel::Declare::get_linestr();
      if (substr($linestr, $Offset, 1) eq '(') {
        my $length = Devel::Declare::toke_scan_str($Offset);
        my $proto = Devel::Declare::get_lex_stuff();
        Devel::Declare::clear_lex_stuff();
        $linestr = Devel::Declare::get_linestr();
        substr($linestr, $Offset, $length) = '';
        Devel::Declare::set_linestr($linestr);
        return $proto;
      }
      return;
    }

=head4 C<toke_scan_str>

This builtin parser uses Perl's own parsing routines to match a "stringlike"
expression.  Handily, this includes bracketed expressions (just think about
things like C<q(this is a quote)>).

Also it Does The Right Thing with nested delimiters (like C<q(this (is (a) quote))>).

It returns the effective length of the expression matched.  Really, what
it returns is the difference in position between where the string started,
within the buffer, and where it finished.  If the string extended across
multiple lines then the contents of the buffer may have been completely
replaced by the new lines, so this position difference is not the same
thing as the actual length of the expression matched.  However, because
moving backward in the buffer causes problems, the function arranges
for the effective length to always be positive, padding the start of
the buffer if necessary.

Use C<get_lex_stuff> to get the actual matched text, the content of
the string.  Because of the behaviour around multiline strings, you
can't reliably get this from the buffer.  In fact, after the function
returns, you can't rely on any content of the buffer preceding the end
of the string.

If the string being scanned is not well formed (has no closing delimiter),
C<toke_scan_str> returns C<undef>.  In this case you cannot rely on the
contents of the buffer.

=head4 C<get_lex_stuff>

This builtin returns what was matched by C<toke_scan_str>.  To avoid segfaults,
you should call C<clear_lex_stuff> immediately afterwards.

=head2 Munging the subroutine

Let's look at what we need to do in detail.

=head3 C<make_proto_unwrap>

We may have defined our method in different ways, which will result
in a different value for our prototype, as parsed above.  For example:

    method foo         {  # undefined
    method foo ()      {  # ''
    method foo ($arg1) {  # '$arg1'

We deal with them as follows, and return the appropriate C<my ($self, ...) = @_;>
string.

    sub make_proto_unwrap {
      my ($proto) = @_;
      my $inject = 'my ($self';
      if (defined $proto) {
        $inject .= ", $proto" if length($proto);
        $inject .= ') = @_; ';
      } else {
        $inject .= ') = shift;';
      }
      return $inject;
    }

=head3 C<inject_if_block>

Now we need to inject it after the opening C<'{'> of the method body.
We can do this with the building blocks we defined above like C<skipspace>
and C<get_linestr>.

    sub inject_if_block {
      my $inject = shift;
      skipspace;
      my $linestr = Devel::Declare::get_linestr;
      if (substr($linestr, $Offset, 1) eq '{') {
        substr($linestr, $Offset+1, 0) = $inject;
        Devel::Declare::set_linestr($linestr);
      }
    }

=head3 C<scope_injector_call>

We want to be able to handle both named and anonymous methods.  i.e.

    method foo () { ... }
    my $meth = method () { ... };

These will then get rewritten as

    method { ... }
    my $meth = method { ... };

where 'method' is a subroutine that takes a code block.  Spot the problem?
The first one doesn't have a semicolon at the end of it!  Unlike 'sub' which
is a builtin, this is just a normal statement, so we need to terminate it.
Luckily, using C<B::Hooks::EndOfScope>, we can do this!

  use B::Hooks::EndOfScope;

We'll add this to what gets 'injected' at the beginning of the method source.

  sub scope_injector_call {
    return ' BEGIN { MethodHandlers::inject_scope }; ';
  }

So at the beginning of every method, we are passing a callback that will get invoked
at the I<end> of the method's compilation... i.e. exactly then the closing C<'}'>
is compiled.

  sub inject_scope {
    on_scope_end {
      my $linestr = Devel::Declare::get_linestr;
      my $offset = Devel::Declare::get_linestr_offset;
      substr($linestr, $offset, 0) = ';';
      Devel::Declare::set_linestr($linestr);
    };
  }

=head2 Shadowing each method.

=head3 C<shadow>

We override the current definition of 'method' using C<shadow>.

    sub shadow {
      my $pack = Devel::Declare::get_curstash_name;
      Devel::Declare::shadow_sub("${pack}::${Declarator}", $_[0]);
    }

For a named method we invoked like this:

    shadow(sub (&) { no strict 'refs'; *{$name} = shift; });

So in the case of a C<method foo { ... }>, this call would redefine C<method>
to be a subroutine that exports 'sub foo' as the (munged) contents of C<{...}>.

The case of an anonymous method is also cute:

    shadow(sub (&) { shift });

This means that

    my $meth = method () { ... };

is rewritten with C<method> taking the codeblock, and returning it as is to become
the value of C<$meth>.

=head4 C<get_curstash_name>

This returns the package name I<currently being compiled>.

=head4 C<shadow_sub>

Handles the details of redefining the subroutine.

=head1 SEE ALSO

One of the best ways to learn C<Devel::Declare> is still to look at
modules that use it:

L<http://cpants.perl.org/dist/used_by/Devel-Declare>.

=head1 AUTHORS

Matt S Trout - E<lt>mst@shadowcat.co.ukE<gt> - original author

Company: http://www.shadowcat.co.uk/
Blog: http://chainsawblues.vox.com/

Florian Ragwitz E<lt>rafl@debian.orgE<gt> - maintainer

osfameron E<lt>osfameron@cpan.orgE<gt> - first draft of documentation

=head1 COPYRIGHT AND LICENSE

This library is free software under the same terms as perl itself

Copyright (c) 2007, 2008, 2009  Matt S Trout

Copyright (c) 2008, 2009  Florian Ragwitz

stolen_chunk_of_toke.c based on toke.c from the perl core, which is

Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
2000, 2001, 2002, 2003, 2004, 2005, 2006, by Larry Wall and others

=cut

1;
