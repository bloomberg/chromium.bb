package Class::C3::Componentised::ApplyHooks;

use strict;
use warnings;

our %Before;
our %After;

sub BEFORE_APPLY (&) {
  push @{$Before{scalar caller}}, $_[0];
  $Class::C3::Componentised::APPLICATOR_FOR{scalar caller} = __PACKAGE__;
}
sub AFTER_APPLY  (&) {
  push @{$After {scalar caller}}, $_[0];
  $Class::C3::Componentised::APPLICATOR_FOR{scalar caller} = __PACKAGE__;
}

sub _apply_component_to_class {
  my ($me, $comp, $target, $apply) = @_;
  my @heritage = @{mro::get_linear_isa($comp)};

  my @before = map {
     my $to_run = $Before{$_};
     ($to_run?[$_,$to_run]:())
  } @heritage;

  for my $todo (@before) {
     my ($parent, $fn)  = @$todo;
     for my $f (reverse @$fn) {
        $target->$f($parent)
     }
  }

  $apply->();

  my @after = map {
     my $to_run = $After{$_};
     ($to_run?[$_,$to_run]:())
  } @heritage;

  for my $todo (reverse @after) {
     my ($parent, $fn)  = @$todo;
     for my $f (@$fn) {
        $target->$f($parent)
     }
  }
}

{
   no strict 'refs';
   sub import {
      my ($from, @args) = @_;
      my $to = caller;

      my $default = 1;
      my $i = 0;
      my $skip = 0;
      my @import;
      for my $arg (@args) {
         if ($skip) {
            $skip--;
            $i++;
            next
         }

         if ($arg eq '-before_apply') {
            $default = 0;
            $skip = 1;
            push @{$Before{$to}}, $args[$i + 1];
            $Class::C3::Componentised::APPLICATOR_FOR{$to} = $from;
         } elsif ($arg eq '-after_apply') {
            $default = 0;
            $skip = 1;
            push @{$After{$to}}, $args[$i + 1];
            $Class::C3::Componentised::APPLICATOR_FOR{$to} = $from;
         } elsif ($arg =~ /^BEFORE_APPLY|AFTER_APPLY$/) {
            $default = 0;
            push @import, $arg
         }
         $i++;
      }
      @import = qw(BEFORE_APPLY AFTER_APPLY)
         if $default;

      *{"$to\::$_"} = \&{"$from\::$_"} for @import
   }
}

1;

=head1 NAME

Class::C3::Componentised::ApplyHooks - Run methods before or after components are injected

=head1 SYNOPSIS

 package MyComponent;

 our %statistics;

 use Class::C3::Componentised::ApplyHooks
   -before_apply => sub {
     my ($class, $component) = @_;

     push @{$statistics{$class}}, '-before_apply';
   },
   -after_apply  => sub {
     my ($class, $component) = @_;

     push @{$statistics{$class}}, '-after_apply';
   }, qw(BEFORE_APPLY AFTER_APPLY);

 BEFORE_APPLY { push @{$statistics{$class}}, 'BEFORE_APPLY' };
 AFTER_APPLY { push @{$statistics{$class}}, 'AFTER_APPLY' };
 AFTER_APPLY { use Devel::Dwarn; Dwarn %statistics };

 1;

=head1 DESCRIPTION

This package allows a given component to run methods on the class that is being
injected into before or after the component is injected.  Note from the
L</SYNOPSIS> that all C<Load Actions> may be run more than once.

=head1 IMPORT ACTION

Both import actions simply run a list of coderefs that will be passed the class
that is being acted upon and the component that is being added to the class.

=head1 IMPORT OPTIONS

=head2 -before_apply

Adds a before apply action for the current component without importing
any subroutines into your namespace.

=head2 -after_apply

Adds an after apply action for the current component without importing
any subroutines into your namespace.

=head1 EXPORTED SUBROUTINES

=head2 BEFORE_APPLY

 BEFORE_APPLY { warn "about to apply $_[1] to class $_[0]"  };

Adds a before apply action for the current component.

=head2 AFTER_APPLY

 AFTER_APPLY { warn "just applied $_[1] to class $_[0]"  };

Adds an after apply action for the current component.

=cut
