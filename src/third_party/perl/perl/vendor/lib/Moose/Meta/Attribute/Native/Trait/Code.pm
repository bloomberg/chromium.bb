package Moose::Meta::Attribute::Native::Trait::Code;
our $VERSION = '2.2011';

use Moose::Role;
with 'Moose::Meta::Attribute::Native::Trait';

sub _helper_type { 'CodeRef' }

no Moose::Role;

1;

# ABSTRACT: Helper trait for CodeRef attributes

__END__

=pod

=encoding UTF-8

=head1 NAME

Moose::Meta::Attribute::Native::Trait::Code - Helper trait for CodeRef attributes

=head1 VERSION

version 2.2011

=head1 SYNOPSIS

  package Foo;
  use Moose;

  has 'callback' => (
      traits  => ['Code'],
      is      => 'ro',
      isa     => 'CodeRef',
      default => sub {
          sub { print "called" }
      },
      handles => {
          call => 'execute',
      },
  );

  my $foo = Foo->new;
  $foo->call;    # prints "called"

=head1 DESCRIPTION

This trait provides native delegation methods for code references.

=head1 DEFAULT TYPE

If you don't provide an C<isa> value for your attribute, it will default to
C<CodeRef>.

=head1 PROVIDED METHODS

=over 4

=item * B<execute(@args)>

Calls the coderef with the given args.

=item * B<execute_method(@args)>

Calls the coderef with the instance as invocant and given args.

=back

=head1 BUGS

See L<Moose/BUGS> for details on reporting bugs.

=head1 AUTHORS

=over 4

=item *

Stevan Little <stevan.little@iinteractive.com>

=item *

Dave Rolsky <autarch@urth.org>

=item *

Jesse Luehrs <doy@tozt.net>

=item *

Shawn M Moore <code@sartak.org>

=item *

יובל קוג'מן (Yuval Kogman) <nothingmuch@woobling.org>

=item *

Karen Etheridge <ether@cpan.org>

=item *

Florian Ragwitz <rafl@debian.org>

=item *

Hans Dieter Pearcey <hdp@weftsoar.net>

=item *

Chris Prather <chris@prather.org>

=item *

Matt S Trout <mst@shadowcat.co.uk>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2006 by Infinity Interactive, Inc.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
