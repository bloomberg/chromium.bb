// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

namespace {

void PrintClipboardContents(GtkClipboard* clip) {
  GdkAtom* targets;
  int num_targets = 0;

  // This call is bugged, the cache it checks is often stale; see
  // <http://bugzilla.gnome.org/show_bug.cgi?id=557315>.
  // gtk_clipboard_wait_for_targets(clip, &targets, &num_targets);

  GtkSelectionData* target_data =
      gtk_clipboard_wait_for_contents(clip,
                                      gdk_atom_intern("TARGETS", false));
  if (!target_data) {
    printf("failed to get the contents!\n");
    return;
  }

  gtk_selection_data_get_targets(target_data, &targets, &num_targets);

  printf("%d available targets:\n---------------\n", num_targets);

  for (int i = 0; i < num_targets; i++) {
    printf("  [format: %s", gdk_atom_name(targets[i]));
    GtkSelectionData* data = gtk_clipboard_wait_for_contents(clip, targets[i]);
    if (!data) {
      printf("]: NULL\n\n");
      continue;
    }

    printf(" / length: %d / bits %d]: ", data->length, data->format);

    if (strstr(gdk_atom_name(targets[i]), "image")) {
      printf("(image omitted)\n\n");
      continue;
    } else if (strstr(gdk_atom_name(targets[i]), "TIMESTAMP")) {
      // TODO(estade): Print the time stamp in human readable format.
      printf("(time omitted)\n\n");
      continue;
    }

    for (int j = 0; j < data->length; j++) {
      // Output data one byte at a time. Currently wide strings look
      // pretty weird.
      printf("%c", (data->data[j] == 0 ? '_' : data->data[j]));
    }
    printf("\n\n");
  }

  if (num_targets <= 0) {
    printf("No targets advertised. Text is: ");
    gchar* text = gtk_clipboard_wait_for_text(clip);
    printf("%s\n", text ? text : "NULL");
    g_free(text);
  }

  g_free(targets);
}

}

/* Small program to dump the contents of GTK's clipboards to the terminal.
 * Feel free to add to it or improve formatting or whatnot.
 */
int main(int argc, char* argv[]) {
  gtk_init(&argc, &argv);

  printf("Desktop clipboard\n");
  PrintClipboardContents(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));

  printf("X clipboard\n");
  PrintClipboardContents(gtk_clipboard_get(GDK_SELECTION_PRIMARY));
}
