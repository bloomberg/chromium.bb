// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import org.chromium.base.Function;

/**
 * Interface for Observable state.
 *
 * Observables can have some data associated with them, which is provided to observers when the
 * Observable activates.
 *
 * @param <T> The type of the activation data.
 */
public abstract class Observable<T> {
    /**
     * Tracks this Observable with the given observer.
     *
     * When this Observable is activated, the observer will be opened with the activation data to
     * produce a scope. When this Observable is deactivated, that scope will be closed.
     *
     * When the returned Subscription is closed, the observer's scopes will be closed and the
     * observer will no longer be notified of updates.
     */
    public abstract Subscription subscribe(Observer<? super T> observer);

    /**
     * Creates an Observable that opens observers's scopes only if both `this` and `other` are
     * activated, and closes those scopes if either of `this` or `other` are deactivated.
     *
     * This is useful for creating an event handler that should only activate when two events
     * have occurred, but those events may occur in any order.
     */
    public final <U> Observable<Both<T, U>> and(Observable<U> other) {
        return flatMap(t -> other.flatMap(u -> just(Both.both(t, u))));
    }

    /**
     * Returns an Observable that is activated when `this` and `other` are activated in order.
     *
     * This is similar to `and()`, but does not activate if `other` is activated before `this`.
     */
    public final <U> Observable<Both<T, U>> andThen(Observable<U> other) {
        Controller<U> otherAfterThis = new Controller<>();
        other.subscribe((U value) -> {
            otherAfterThis.set(value);
            return otherAfterThis::reset;
        });
        subscribe(Observers.onEnter(x -> otherAfterThis.reset()));
        return and(otherAfterThis);
    }

    /**
     * Returns an Observable that applies the given Function to this Observable's activation values.
     */
    public final <R> Observable<R> map(Function<? super T, ? extends R> f) {
        return flatMap(t -> just(f.apply(t)));
    }

    /**
     * Returns an Observable that applies the given Function to this Observable's activation data,
     * and notifies observers with the activation data of the Observable returned by the Function.
     *
     * If you have a function that returns an Observable, you can use this to avoid using map() to
     * create an Observable of Observables.
     *
     * This is an extremely powerful operation! Observables are a monad where flatMap() is the
     * "bind" operation that combines an Observable with a function that returns an Observable to
     * create a new Observable.
     *
     * One use case could be using Observables as "promises" and using flatMap() with "async
     * functions" that return Observables to create asynchronous programs:
     *
     *   Observable<Foo> getFooAsync();
     *   Observable<Bar> getBarAsync(Foo foo);
     *   Scope useBar(Bar bar);
     *
     *   getFooAsync().flatMap(foo -> getBarAsync(foo)).subscribe(bar -> useBar(bar));
     */
    public final <R> Observable<R> flatMap(
            Function<? super T, ? extends Observable<? extends R>> f) {
        return make(observer -> subscribe(t -> f.apply(t).subscribe(r -> observer.open(r))));
    }

    /**
     * Returns an Observable that is only activated when `this` is activated with a value such that
     * the given `predicate` returns true.
     */
    public final Observable<T> filter(Predicate<? super T> predicate) {
        return flatMap(t -> predicate.test(t) ? just(t) : empty());
    }

    /**
     * Returns an Observable that is activated only when the given Observable is not activated.
     */
    public static Observable<?> not(Observable<?> observable) {
        Controller<Unit> opposite = new Controller<>();
        opposite.set(Unit.unit());
        observable.subscribe(x -> {
            opposite.reset();
            return () -> opposite.set(Unit.unit());
        });
        return opposite;
    }

    /**
     * Allows creating an Observable with a functional interface.
     */
    public static <T> Observable<T> make(
            Function<? super Observer<? super T>, ? extends Scope> impl) {
        return new Observable<T>() {
            @Override
            public Subscription subscribe(Observer<? super T> observer) {
                return impl.apply(observer)::close;
            }
        };
    }

    /**
     * A degenerate Observable that has no data.
     */
    public static <T> Observable<T> empty() {
        return make(observer -> Scopes.NO_OP);
    }

    /**
     * Creates an Observable that notifies subscribers with a single immutable value.
     *
     * This is the "return" operation in the Observable monad.
     */
    public static <T> Observable<T> just(T value) {
        if (value == null) return empty();
        return make(observer -> observer.open(value));
    }
}
