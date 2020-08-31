package StackTrace::Auto;
# ABSTRACT: a role for generating stack traces during instantiation
$StackTrace::Auto::VERSION = '0.200013';
use Moo::Role;
use Sub::Quote ();
use Module::Runtime 0.002 ();
use Scalar::Util ();

#pod =head1 SYNOPSIS
#pod
#pod First, include StackTrace::Auto in a Moose or Mooclass...
#pod
#pod   package Some::Class;
#pod   # NOTE: Moo can also be used here instead of Moose
#pod   use Moose;
#pod   with 'StackTrace::Auto';
#pod
#pod ...then create an object of that class...
#pod
#pod   my $obj = Some::Class->new;
#pod
#pod ...and now you have a stack trace for the object's creation.
#pod
#pod   print $obj->stack_trace->as_string;
#pod
#pod =attr stack_trace
#pod
#pod This attribute will contain an object representing the stack at the point when
#pod the error was generated and thrown.  It must be an object performing the
#pod C<as_string> method.
#pod
#pod =attr stack_trace_class
#pod
#pod This attribute may be provided to use an alternate class for stack traces.  The
#pod default is L<Devel::StackTrace|Devel::StackTrace>.
#pod
#pod In general, you will not need to think about this attribute.
#pod
#pod =cut

has stack_trace => (
  is       => 'ro',
  isa      => Sub::Quote::quote_sub(q{
    require Scalar::Util;
    die "stack_trace must be have an 'as_string' method!" unless
       Scalar::Util::blessed($_[0]) && $_[0]->can('as_string')
  }),
  default  => Sub::Quote::quote_sub(q{
    $_[0]->stack_trace_class->new(
      @{ $_[0]->stack_trace_args },
    );
  }),
  lazy => 1,
  init_arg => undef,
);

sub BUILD {};
before BUILD => sub { $_[0]->stack_trace };

has stack_trace_class => (
  is      => 'ro',
  isa     => Sub::Quote::quote_sub(q{
    die "stack_trace_class must be a class that responds to ->new"
      unless defined($_[0]) && !ref($_[0]) && $_[0]->can("new");
  }),
  coerce  => Sub::Quote::quote_sub(q{
    Module::Runtime::use_package_optimistically($_[0]);
  }),
  lazy    => 1,
  builder => '_build_stack_trace_class',
);

#pod =attr stack_trace_args
#pod
#pod This attribute is an arrayref of arguments to pass when building the stack
#pod trace.  In general, you will not need to think about it.
#pod
#pod =cut

has stack_trace_args => (
  is      => 'ro',
  isa     => Sub::Quote::quote_sub(q{
      die "stack_trace_args must be an arrayref"
          unless ref($_[0]) && ref($_[0]) eq "ARRAY";
  }),
  lazy    => 1,
  builder => '_build_stack_trace_args',
);

sub _build_stack_trace_class {
  return 'Devel::StackTrace';
}

sub _build_stack_trace_args {
  my ($self) = @_;

  Scalar::Util::weaken($self);  # Prevent memory leak

  my $found_mark = 0;
  return [
    filter_frames_early => 1,
    frame_filter => sub {
      my ($raw) = @_;
      my $sub = $raw->{caller}->[3];
      (my $package = $sub) =~ s/::\w+\z//;
      if ($found_mark == 2) {
          return 1;
      }
      elsif ($found_mark == 1) {
        return 0 if $sub =~ /::new$/ && $self->isa($package);
        $found_mark++;
        return 1;
      } else {
        $found_mark++ if $sub =~ /::new$/ && $self->isa($package);
        return 0;
      }
    },
  ];
}

no Moo::Role;
1;

__END__

=pod

=encoding UTF-8

=head1 NAME

StackTrace::Auto - a role for generating stack traces during instantiation

=head1 VERSION

version 0.200013

=head1 SYNOPSIS

First, include StackTrace::Auto in a Moose or Mooclass...

  package Some::Class;
  # NOTE: Moo can also be used here instead of Moose
  use Moose;
  with 'StackTrace::Auto';

...then create an object of that class...

  my $obj = Some::Class->new;

...and now you have a stack trace for the object's creation.

  print $obj->stack_trace->as_string;

=head1 ATTRIBUTES

=head2 stack_trace

This attribute will contain an object representing the stack at the point when
the error was generated and thrown.  It must be an object performing the
C<as_string> method.

=head2 stack_trace_class

This attribute may be provided to use an alternate class for stack traces.  The
default is L<Devel::StackTrace|Devel::StackTrace>.

In general, you will not need to think about this attribute.

=head2 stack_trace_args

This attribute is an arrayref of arguments to pass when building the stack
trace.  In general, you will not need to think about it.

=head1 AUTHORS

=over 4

=item *

Ricardo SIGNES <rjbs@cpan.org>

=item *

Florian Ragwitz <rafl@debian.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2015 by Ricardo SIGNES.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
