// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.accounts.Account;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

/** SpinnerAdapter class used for the ActionBar accounts spinner. */
public class AccountsAdapter extends ArrayAdapter<Account> {
    private LayoutInflater mInflater;

    public AccountsAdapter(Context context, Account[] accounts) {
        // ArrayAdapter only uses the |resource| parameter to return a View from getView() and
        // getDropDownView(). But these methods are overridden here to return custom Views, so it's
        // OK to provide 0 as the resource for the base class.
        super(context, 0, accounts);
        mInflater = (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View view = mInflater.inflate(R.layout.account_selected, parent, false);
        Account account = getItem(position);
        TextView target = (TextView) view.findViewById(R.id.account_name);
        target.setText(account.name);
        return view;
    }

    @Override
    public View getDropDownView(int position, View convertView, ViewGroup parent) {
        TextView view = (TextView) mInflater.inflate(R.layout.account_dropdown, parent, false);
        Account account = getItem(position);
        view.setText(account.name);
        return view;
    }
}
