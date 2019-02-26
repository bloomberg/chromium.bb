package MooseX::Types::TypeDecorator;
{
  $MooseX::Types::TypeDecorator::VERSION = '0.31';
}

#ABSTRACT: Wraps Moose::Meta::TypeConstraint objects with added features

use strict;
use warnings;

use Carp::Clan qw( ^MooseX::Types );
use Moose::Util::TypeConstraints ();
use Moose::Meta::TypeConstraint::Union;
use Scalar::Util qw(blessed);

use overload(
    '0+' => sub {
            my $self = shift @_;
            my $tc = $self->{__type_constraint};
            return 0+$tc;
     },
    '""' => sub {
    		my $self = shift @_;
    		if(blessed $self) {
        		return $self->__type_constraint->name;     		
    		} else {
    			return "$self";
    		}
    },
    bool => sub { 1 },
    '|' => sub {
        
        ## It's kind of ugly that we need to know about Union Types, but this
        ## is needed for syntax compatibility.  Maybe someday we'll all just do
        ## Or[Str,Str,Int]

        my @args = @_[0,1]; ## arg 3 is special,  see the overload docs.
        my @tc = grep {blessed $_} map {
            blessed $_ ? $_ :
            Moose::Util::TypeConstraints::find_or_parse_type_constraint($_)
              || __PACKAGE__->_throw_error( "$_ is not a type constraint")
        } @args;

        ( scalar @tc == scalar @args)
            || __PACKAGE__->_throw_error(
			  "one of your type constraints is bad.  Passed: ". join(', ', @args) ." Got: ". join(', ', @tc));

        ( scalar @tc >= 2 )
            || __PACKAGE__->_throw_error("You must pass in at least 2 type names to make a union");

        my $union = Moose::Meta::TypeConstraint::Union->new(type_constraints=>\@tc);
        return Moose::Util::TypeConstraints::register_type_constraint($union);
    },
    fallback => 1,
    
);


sub new {
    my $class = shift @_;
    if(my $arg = shift @_) {
        if(blessed $arg && $arg->isa('Moose::Meta::TypeConstraint')) {
            return bless {'__type_constraint'=>$arg}, $class;
        } elsif(
            blessed $arg &&
            $arg->isa('MooseX::Types::UndefinedType') 
          ) {
            ## stub in case we'll need to handle these types differently
            return bless {'__type_constraint'=>$arg}, $class;
        } elsif(blessed $arg) {
            __PACKAGE__->_throw_error("Argument must be ->isa('Moose::Meta::TypeConstraint') or ->isa('MooseX::Types::UndefinedType'), not ". blessed $arg);
        } else {
            __PACKAGE__->_throw_error("Argument cannot be '$arg'");
        }
    } else {
        __PACKAGE__->_throw_error("This method [new] requires a single argument.");        
    }
}


sub __type_constraint {
    my $self = shift @_;    
    if(blessed $self) {
        if(defined(my $tc = shift @_)) {
            $self->{__type_constraint} = $tc;
        }
        return $self->{__type_constraint};        
    } else {
        __PACKAGE__->_throw_error('cannot call __type_constraint as a class method');
    }
}


sub isa {
    my ($self, $target) = @_;  
    if(defined $target) {
    	if(blessed $self) {
    		return $self->__type_constraint->isa($target);
    	} else {
    		return;
    	}
    } else {
        return;
    }
}



sub can {
    my ($self, $target) = @_;
    if(defined $target) {
    	if(blessed $self) {
    		return $self->__type_constraint->can($target);
    	} else {
    		return;
    	}
    } else {
        return;
    }
}


sub meta {
	my $self = shift @_;
	if(blessed $self) {
		return $self->__type_constraint->meta;
	} 
}


sub _throw_error {
    shift;
    require Moose;
    unshift @_, 'Moose';
    goto &Moose::throw_error;
}


sub DESTROY {
    return;
}


sub AUTOLOAD {
    
    my ($self, @args) = @_;
    my ($method) = (our $AUTOLOAD =~ /([^:]+)$/);
    
    ## We delegate with this method in an attempt to support a value of
    ## __type_constraint which is also AUTOLOADing, in particular the class
    ## MooseX::Types::UndefinedType which AUTOLOADs during autovivication.
    
    my $return;
    eval {
        $return = $self->__type_constraint->$method(@args);
    }; if($@) {
        __PACKAGE__->_throw_error($@);
    } else {
        return $return;
    }
}


1;

__END__
=pod

=head1 NAME

MooseX::Types::TypeDecorator - Wraps Moose::Meta::TypeConstraint objects with added features

=head1 VERSION

version 0.31

=head1 DESCRIPTION

This is a decorator object that contains an underlying type constraint.  We use
this to control access to the type constraint and to add some features.

=head1 METHODS

This class defines the following methods.

=head2 new

Old school instantiation

=head2 __type_constraint ($type_constraint)

Set/Get the type_constraint.

=head2 isa

handle $self->isa since AUTOLOAD can't.

=head2 can

handle $self->can since AUTOLOAD can't.

=head2 meta

have meta examine the underlying type constraints

=head2 _throw_error

properly delegate error messages

=head2 DESTROY

We might need it later

=head2 AUTOLOAD

Delegate to the decorator target.

=head1 LICENSE

This program is free software; you can redistribute it and/or modify
it under the same terms as perl itself.

=head1 AUTHOR

Robert "phaylon" Sedlacek <rs@474.at>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011 by Robert "phaylon" Sedlacek.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

