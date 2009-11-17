/* 
 * Copyright © 2008 Jérôme Glisse
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS, AUTHORS
 * AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 */
/*
 * Authors:
 *      Jérôme Glisse <glisse@freedesktop.org>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "radeon_track.h"

void radeon_track_add_event(struct radeon_track *track,
                            const char *file,
                            const char *func,
                            const char *op,
                            unsigned line)
{
    struct radeon_track_event *event;

    if (track == NULL) {
        return;
    }
    event = (void*)calloc(1,sizeof(struct radeon_track_event));
    if (event == NULL) {
        return;
    }
    event->line = line;
    event->file = strdup(file);
    event->func = strdup(func);
    event->op = strdup(op);
    if (event->file == NULL || event->func == NULL || event->op == NULL) {
        free(event->file);
        free(event->func);
        free(event->op);
        free(event);
        return;
    }
    event->next = track->events;
    track->events = event;
}

struct radeon_track *radeon_tracker_add_track(struct radeon_tracker *tracker,
                                              unsigned key)
{
    struct radeon_track *track;

    track = (struct radeon_track*)calloc(1, sizeof(struct radeon_track));
    if (track) {
        track->next = tracker->tracks.next;
        track->prev = &tracker->tracks;
        tracker->tracks.next = track;
        if (track->next) {
            track->next->prev = track;
        }
        track->key = key;
        track->events = NULL;
    }
    return track;
}

void radeon_tracker_remove_track(struct radeon_tracker *tracker,
                                 struct radeon_track *track)
{
    struct radeon_track_event *event;
    void *tmp;

    if (track == NULL) {
        return;
    }
    track->prev->next = track->next;
    if (track->next) {
        track->next->prev = track->prev;
    }
    track->next = track->prev = NULL;
    event = track->events;
    while (event) {
        tmp = event;
        free(event->file);
        free(event->func);
        free(event->op);
        event = event->next;
        free(tmp);
    }
    track->events = NULL;
    free(track);
}

void radeon_tracker_print(struct radeon_tracker *tracker, FILE *file)
{
    struct radeon_track *track;
    struct radeon_track_event *event;
    void *tmp;

    track = tracker->tracks.next;
    while (track) {
        event = track->events;
        fprintf(file, "[0x%08X] :\n", track->key);
        while (event) {
            tmp = event;
            fprintf(file, "  [0x%08X:%s](%s:%s:%d)\n",
                    track->key, event->op,  event->file,
                    event->func, event->line);
            free(event->file);
            free(event->func);
            free(event->op);
            event->file = NULL;
            event->func = NULL;
            event->op = NULL;
            event = event->next;
            free(tmp);
        }
        track->events = NULL;
        tmp = track;
        track = track->next;
        free(tmp);
    }
	tracker->tracks.next = NULL;
}
