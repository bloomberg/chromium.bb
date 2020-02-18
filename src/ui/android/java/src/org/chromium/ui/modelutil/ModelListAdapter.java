// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import android.support.annotation.Nullable;
import android.util.Pair;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;

import org.chromium.base.VisibleForTesting;
import org.chromium.ui.R;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * Adapter for providing data and views to a ListView.
 *
 * To use, register a {@link PropertyModelChangeProcessor.ViewBinder} and {@link ViewBuilder}
 * for each view type in the list using
 * {@link #registerType(int, ViewBuilder, PropertyModelChangeProcessor.ViewBinder)}.
 * Then call {@link #updateModels(List)} to provide a list of items (represented by PropertyModels)
 * to display in the list. If the items in the list change (e.g. items are added, removed, or
 * change order), call #updateModels again with the new list of items. NOTE: There are plans to
 * change the API surface to work with a PropertyObservable instead.
 *
 * When creating a new view, ModelListAdapter will bind all set properties. When reusing/rebinding
 * a view, in addition to binding all properties set on the new model, properties that were
 * previously set on the old model but are not set on the new model will be bound to "reset" the
 * view. ViewBinders registered for this adapter may therefore need to handle bind calls for
 * properties that are not set on the model being bound.
 *
 * Additionally, ModelListAdapter will hook up a {@link PropertyModelChangeProcessor} when binding
 * views to ensure that changes to the PropertyModel for that list item are bound to the view.
 */
public class ModelListAdapter extends BaseAdapter {
    /**
     * An interface to provide a means to build specific view types.
     * @param <T> The type of view that the implementor will build.
     */
    public interface ViewBuilder<T extends View> {
        /**
         * @return A new view to show in the list.
         */
        T buildView();
    }

    private final List<Pair<Integer, PropertyModel>> mModelList = new ArrayList<>();
    private final SparseArray<Pair<ViewBuilder, PropertyModelChangeProcessor.ViewBinder>>
            mViewBuilderMap = new SparseArray<>();

    /**
     * Update the visible models (list items).
     * @param models A list of {@link PropertyModel}s to display. The Integer property in the pair
     *         indicates the view type, while the PropertyModel contains the properties for the item
     *         to display.
     */
    public void updateModels(List<Pair<Integer, PropertyModel>> models) {
        mModelList.clear();
        mModelList.addAll(models);
        notifyDataSetChanged();
    }

    @Override
    public int getCount() {
        return mModelList.size();
    }

    @Override
    public Object getItem(int position) {
        return mModelList.get(position);
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    /**
     * Register a new view type that this adapter knows how to show.
     * @param typeId The ID of the view type. This should not match any other view type registered
     *               in this adapter.
     * @param builder A mechanism for building new views of the specified type.
     * @param binder A means of binding a model to the provided view.
     */
    public <T extends View> void registerType(int typeId, ViewBuilder<T> builder,
            PropertyModelChangeProcessor.ViewBinder<PropertyModel, T, PropertyKey> binder) {
        assert mViewBuilderMap.get(typeId) == null;
        mViewBuilderMap.put(typeId, new Pair<>(builder, binder));
    }

    @Override
    public int getItemViewType(int position) {
        return mModelList.get(position).first;
    }

    @Override
    public int getViewTypeCount() {
        return Math.max(1, mViewBuilderMap.size());
    }

    @SuppressWarnings("unchecked")
    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        //  1. Destroy the old PropertyModelChangeProcessor if it exists.
        if (convertView != null && convertView.getTag(R.id.view_mcp) != null) {
            PropertyModelChangeProcessor propertyModelChangeProcessor =
                    (PropertyModelChangeProcessor) convertView.getTag(R.id.view_mcp);
            propertyModelChangeProcessor.destroy();
        }

        // 2. Build a new view if needed. Otherwise, fetch the old model from the convertView.
        PropertyModel oldModel = null;
        if (convertView == null || convertView.getTag(R.id.view_type) == null
                || (int) convertView.getTag(R.id.view_type) != getItemViewType(position)) {
            int modelTypeId = mModelList.get(position).first;
            convertView = mViewBuilderMap.get(modelTypeId).first.buildView();

            // Since the view type returned by getView is not guaranteed to return a view of that
            // type, we need a means of checking it. The "view_type" tag is attached to the views
            // and identify what type the view is. This should allow lists that aren't necessarily
            // recycler views to work correctly with heterogeneous lists.
            convertView.setTag(R.id.view_type, modelTypeId);
        } else {
            oldModel = (PropertyModel) convertView.getTag(R.id.view_model);
        }

        PropertyModel model = mModelList.get(position).second;
        PropertyModelChangeProcessor.ViewBinder binder =
                mViewBuilderMap.get(mModelList.get(position).first).second;

        // 3. Attach a PropertyModelChangeProcessor and PropertyModel to the view (for #1/2 above
        //    when re-using a view).
        convertView.setTag(R.id.view_mcp,
                PropertyModelChangeProcessor.create(
                        model, convertView, binder, /* performInitialBind = */ false));
        convertView.setTag(R.id.view_model, model);

        // 4. Bind properties to the convertView.
        bindNewModel(model, oldModel, convertView, binder);

        // TODO(tedchoc): Investigate whether this is still needed.
        convertView.jumpDrawablesToCurrentState();

        return convertView;
    }

    /**
     * Binds all set properties to the view. If oldModel is not null, binds properties that were
     * previously set in the oldModel but are not set in the new model.
     *
     * @param newModel The new model to bind to {@code view}.
     * @param oldModel The old model previously bound to {@code view}. May be null.
     * @param view The view to bind.
     * @param binder The ViewBinder that will bind model properties to {@code view}.
     */
    @VisibleForTesting
    static void bindNewModel(PropertyModel newModel, @Nullable PropertyModel oldModel, View view,
            PropertyModelChangeProcessor.ViewBinder binder) {
        Collection<PropertyKey> setProperties = newModel.getAllSetProperties();
        for (PropertyKey key : newModel.getAllProperties()) {
            if (oldModel != null) {
                // Skip binding properties that haven't changed.
                if (newModel.compareValue(oldModel, key)) {
                    continue;
                } else {
                    // When rebinding a view, if the value has changed it should be writable as
                    // views reasonably may not expect read-only keys not to change or be rebound.
                    assert key instanceof PropertyModel.WritableObjectPropertyKey
                            || key instanceof PropertyModel.WritableIntPropertyKey
                            || key instanceof PropertyModel.WritableBooleanPropertyKey
                            || key instanceof PropertyModel.WritableFloatPropertyKey;
                }
            } else if (!setProperties.contains(key)) {
                // If there is no previous model, skip binding properties that haven't been set.
                continue;
            }

            binder.bind(newModel, view, key);
        }
    }
}
