package Object::Tiny; # git description: 5abde2e

use strict 'vars', 'subs';

our $VERSION = '1.09';

sub import {
	return unless shift eq 'Object::Tiny';
	my $pkg   = caller;
	my $child = !! @{"${pkg}::ISA"};
	eval join "\n",
		"package $pkg;",
		($child ? () : "\@${pkg}::ISA = 'Object::Tiny';"),
		map {
			defined and ! ref and /^[^\W\d]\w*\z/s
			or die "Invalid accessor name '$_'";
			"sub $_ { return \$_[0]->{$_} }"
		} @_;
	die "Failed to generate $pkg" if $@;
	return 1;
}

sub new {
	my $class = shift;
	bless { @_ }, $class;
}

1;

__END__

=pod

=head1 NAME

Object::Tiny - Class building as simple as it gets

=head1 SYNOPSIS

  # Define a class
  package Foo;
  
  use Object::Tiny qw{ bar baz };
  
  1;
  
  
  # Use the class
  my $object = Foo->new( bar => 1 );
  
  print "bar is " . $object->bar . "\n";

=head1 DESCRIPTION

There's a whole bunch of class builders out there. In fact, creating
a class builder seems to be something of a rite of passage (this is
my fifth, at least).

Unfortunately, most of the time I want a class builder I'm in a
hurry and sketching out lots of fairly simple data classes with fairly
simple structure, mostly just read-only accessors, and that's about it.

Often this is for code that won't end up on CPAN, so adding a small
dependency doesn't matter much. I just want to be able to define these
classes FAST.

By which I mean LESS typing than writing them by hand, not more. And
I don't need all those weird complex features that bloat out the code
and take over the whole way I build modules.

And so, I present yet another member of the Tiny family of modules,
Object::Tiny.

The goal here is really just to save me some typing. There's others
that could do the job just fine, but I want something that does as little
as possible and creates code the same way I'd have written it by hand
anyway.

To use Object::Tiny, just call it with a list of accessors to be created.

  use Object::Tiny 'foo', 'bar';

For a large list, I lay it out like this...

  use Object::Tiny qw{
      item_font_face
      item_font_color
      item_font_size
      item_text_content
      item_display_time
      seperator_font_face
      seperator_font_color
      seperator_font_size
      seperator_text_content
  };

This will create a bunch of simple accessors, and set the inheritance to
be the child of Object::Tiny.

Object::Tiny is empty other than a basic C<new> constructor which
does the following

  sub new {
      my $class = shift;
      return bless { @_ }, $class;
  }

In fact, if doing the following in your class gets annoying...

  sub new {
      my $class = shift;
      my $self  = $class->SUPER::new( @_ );
  
      # Extra checking and such
      ...
  
      return $self;
  }

... then feel free to ditch the SUPER call and just create the hash
yourself! It's not going to make a lick of different and there's nothing
magic going on under the covers you might break.

And that's really all there is to it. Let a million simple data classes
bloom. Features? We don't need no stinking features.

=head2 Handling Subclasses

If the class you are using Object::Tiny for is already a subclass of
another Object::Tiny class (or a subclass of anything else) it doesn't
really work to make the class use multiple inheritance.

So in this case, Object::Tiny will create the accessors you specify, but
WON'T make it a subclass of Object::Tiny.

=head2 Why bother when Class::Accessor::* already does the same thing?

As a class builder, L<Object::Tiny> inevitably is compared to
L<Class::Accessor> and related modules. They seem so similar, so why would
I reimplement it?

The answer is that for experienced developers that don't need or want
hand-holding, Object::Tiny is just outright better, faster or cheaper
on every single metric than L<Class::Accessor::Fast>, which
is the most comparable member of the Class::Accessor::* family.

B<Object::Tiny is 93% smaller than Class::Accessor::Fast>

L<Class::Accessor::Fast> requires about 125k of memory to load.

Object::Tiny requires about 8k of memory to load.

B<Object::Tiny is 75% more terse to use than Class::Accessor::Fast>

Object::Tiny is used with the least possible number of keystrokes
(short of making the actual name Object::Tiny smaller).

And it requires no ugly constructor methods.

I mean really, what sort of a method name is 'mk_ro_accessors'. That sort
of thing went out of style in the early nineties.

Using Class::Accessor::Fast...

  package Foo::Bar;
  use base 'Class::Accessor::Fast';
  Foo::Bar->mk_ro_accessors(qw{ foo bar baz });

Using Object::Tiny...

  package Foo::Bar;
  use Object::Tiny qw{ foo bar baz };

Further, Object::Tiny lets you pass your params in directly, without
having to wrap them in an additional HASH reference that will just be
copied ANYWAY inside the constructor.

Using Class::Accessor::Fast...

  my $object = Foo::Bar->new( {
      foo => 1,
      bar => 2,
      baz => 3,
  } );

Using Object::Tiny...

  my $object = Foo::Bar->new(
      foo => 1,
      bar => 2,
      baz => 3,
  );

B<Object::Tiny constructors are 110% faster than Class::Accessor::Fast>

Object::Tiny accessors are identical in speed to Class::Accessor::Fast
accessors, but Object::Tiny constructors are TWICE as fast as
Class::Accessor::Fast constructors, DESPITE C:A:Fast forcing you to pass
by reference (which is typically done for speed reasons).

  Benchmarking constructor plus accessors...
               Rate accessor     tiny
  accessor 100949/s       --     -45%
  tiny     182382/s      81%       --
  
  Benchmarking constructor alone...
               Rate accessor     tiny
  accessor 156470/s       --     -54%
  tiny     342231/s     119%       --
  
  Benchmarking accessors alone...
             Rate     tiny accessor
  tiny     81.0/s       --      -0%
  accessor 81.0/s       0%       --

B<Object::Tiny pollutes your API 95% less than Class::Accessor::Fast>

Object::Tiny adds two methods to your class, C<new> and C<import>. The
C<new> constructor is so trivial you can just ignore it and use your own
if you wish, and the C<import> will shortcut and do nothing (it is used to
implement the C<"use Object::Tiny qw{ foo bar baz };"> syntax itself).

So if you make your own import, you can ignore the Object::Tiny one.

Class::Accessor::Fast isn't quite as light, adding all sorts of useless
extra public methods (why on earth would you want to add method accessors
at run-time?).

Here's what the classes used in the benchmark end up like.

    DB<1> use Class::Inspector
  
    DB<2> x Class::Inspector->methods('Foo_Bar_Tiny');
  0  ARRAY(0xfda780)
     0  'bar'
     1  'baz'
     2  'foo'
     3  'import'
     4  'new'
  
    DB<3> x Class::Inspector->methods('Foo_Bar_Accessor');
  0  ARRAY(0xfdb3c8)
     0  '_bar_accessor'
     1  '_baz_accessor'
     2  '_carp'
     3  '_croak'
     4  '_foo_accessor'
     5  '_mk_accessors'
     6  'accessor_name_for'
     7  'bar'
     8  'baz'
     9  'best_practice_accessor_name_for'
     10  'best_practice_mutator_name_for'
     11  'follow_best_practice'
     12  'foo'
     13  'get'
     14  'make_accessor'
     15  'make_ro_accessor'
     16  'make_wo_accessor'
     17  'mk_accessors'
     18  'mk_ro_accessors'
     19  'mk_wo_accessors'
     20  'mutator_name_for'
     21  'new'
     22  'set'

As you can see, Object::Tiny adds 2 methods to your class, Class::Accessor
adds 16 methods, plus one extra one for every accessor.

B<Object::Tiny doesn't have any of the caveats of Class::Accessor::Fast>

When you call B<use Object::Tiny qw{ foo bar baz }> it isn't treated as some
sort of specification for the class, it's just a list of accessors you want
made for you.

So if you want to customize C<foo> you don't need to get into contortions with
"pure" base classes or calling alternate internal methods. Just make your own
C<foo> method and remove C<foo> from the list passed to the C<use> call.

B<Object::Tiny is more back-compatible than Class::Accessor::Fast>

Class::Accessor::Fast has a minimum Perl dependency of 5.005002.

Object::Tiny has a minimum Perl dependency of 5.004.

B<Object::Tiny has no module dependencies whatsoever>

Object::Tiny does not load ANYTHING at all outside of its own single .pm file.

So Object::Tiny will never get confused in odd situations due to old or weird
versions of other modules (Class::Accessor::Fast has a dependency on base.pm,
which has some caveats of its own).

=head1 SUPPORT

Bugs should be reported via the CPAN bug tracker at

L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=Object-Tiny>

For other issues, contact the author.

=head1 AUTHOR

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

=head1 SEE ALSO

L<Config::Tiny>

=head1 COPYRIGHT

Copyright 2007 - 2011 Adam Kennedy.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
