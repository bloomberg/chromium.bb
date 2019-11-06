package Data::Dumper::Concise;

use 5.006;

our $VERSION = '2.023';

require Exporter;
require Data::Dumper;

BEGIN { @ISA = qw(Exporter) }

@EXPORT = qw(Dumper DumperF DumperObject);

sub DumperObject {
  my $dd = Data::Dumper->new([]);
  $dd->Trailingcomma(1) if $dd->can('Trailingcomma');
  $dd->Terse(1)->Indent(1)->Useqq(1)->Deparse(1)->Quotekeys(0)->Sortkeys(1);
}

sub Dumper { DumperObject->Values([ @_ ])->Dump }

sub DumperF (&@) {
  my $code = shift;
  return $code->(map Dumper($_), @_);
}

=head1 NAME

Data::Dumper::Concise - Less indentation and newlines plus sub deparsing

=head1 SYNOPSIS

  use Data::Dumper::Concise;

  warn Dumper($var);

is equivalent to:

  use Data::Dumper;
  {
    local $Data::Dumper::Terse = 1;
    local $Data::Dumper::Indent = 1;
    local $Data::Dumper::Useqq = 1;
    local $Data::Dumper::Deparse = 1;
    local $Data::Dumper::Quotekeys = 0;
    local $Data::Dumper::Sortkeys = 1;
    local $Data::Dumper::Trailingcomma = 1;
    warn Dumper($var);
  }

So for the structure:

  { foo => "bar\nbaz", quux => sub { "fleem" } };

Data::Dumper::Concise will give you:

  {
    foo => "bar\nbaz",
    quux => sub {
        use warnings;
        use strict 'refs';
        'fleem';
    },
  }

instead of the default Data::Dumper output:

  $VAR1 = {
   'quux' => sub { "DUMMY" },
   'foo' => 'bar
  baz'
  };

(note the tab indentation, oh joy ...)

(The trailing comma on the last element of an array or hash is enabled by a new
feature in Data::Dumper version 2.159, which was first released in Perl 5.24.
Using Data::Dumper::Concise with an older version of Data::Dumper will still
work, but you won't get those commas.)

If you need to get the underlying L<Dumper> object just call C<DumperObject>.

Also try out C<DumperF> which takes a C<CodeRef> as the first argument to
format the output.  For example:

  use Data::Dumper::Concise;

  warn DumperF { "result: $_[0] result2: $_[1]" } $foo, $bar;

Which is the same as:

  warn 'result: ' . Dumper($foo) . ' result2: ' . Dumper($bar);

=head1 DESCRIPTION

This module always exports a single function, Dumper, which can be called
with an array of values to dump those values.

It exists, fundamentally, as a convenient way to reproduce a set of Dumper
options that we've found ourselves using across large numbers of applications,
primarily for debugging output.

The principle guiding theme is "all the concision you can get while still
having a useful dump and not doing anything cleverer than setting Data::Dumper
options" - it's been pointed out to us that Data::Dump::Streamer can produce
shorter output with less lines of code. We know. This is simpler and we've
never seen it segfault. But for complex/weird structures, it generally rocks.
You should use it as well, when Concise is underkill. We do.

Why is deparsing on when the aim is concision? Because you often want to know
what subroutine refs you have when debugging and because if you were planning
to eval this back in you probably wanted to remove subrefs first and add them
back in a custom way anyway. Note that this -does- force using the pure perl
Dumper rather than the XS one, but I've never in my life seen Data::Dumper
show up in a profile so "who cares?".

=head1 BUT BUT BUT ...

Yes, we know. Consider this module in the ::Tiny spirit and feel free to
write a Data::Dumper::Concise::ButWithExtraTwiddlyBits if it makes you
happy. Then tell us so we can add it to the see also section.

=head1 SUGARY SYNTAX

This package also provides:

L<Data::Dumper::Concise::Sugar> - provides Dwarn and DwarnS convenience functions

L<Devel::Dwarn> - shorter form for Data::Dumper::Concise::Sugar

=head1 SEE ALSO

We use for some purposes, and dearly love, the following alternatives:

L<Data::Dump> - prettiness oriented but not amazingly configurable

L<Data::Dump::Streamer> - brilliant. beautiful. insane. extensive. excessive. try it.

L<JSON::XS> - no, really. If it's just plain data, JSON is a great option.

=head1 AUTHOR

mst - Matt S. Trout <mst@shadowcat.co.uk>

=head1 CONTRIBUTORS

frew - Arthur Axel "fREW" Schmidt <frioux@gmail.com>

=head1 COPYRIGHT

Copyright (c) 2010 the Data::Dumper::Concise L</AUTHOR> and L</CONTRIBUTORS>
as listed above.

=head1 LICENSE

This library is free software and may be distributed under the same terms
as perl itself.

=cut

1;
