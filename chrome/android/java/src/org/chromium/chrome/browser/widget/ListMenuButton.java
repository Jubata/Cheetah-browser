// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.content.Context;
import android.support.annotation.StringRes;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListPopupWindow;

import org.chromium.chrome.R;

/**
 * A menu button meant to be used with modern lists throughout Chrome.  Will automatically show and
 * anchor a popup on press and will rely on a delegate for both querying the items and returning the
 * results.
 */
public class ListMenuButton extends TintedImageButton {
    /** A class that represents a single item in the popup menu. */
    public static class Item {
        private final String mTextString;
        private final @StringRes int mTextId;
        private final boolean mEnabled;

        /**
         * Creates a new {@link Item}.
         * @param textId  The string resource id for the text to show for this item.
         * @param enabled Whether or not this item should be enabled.
         */
        public Item(Context context, @StringRes int textId, boolean enabled) {
            mTextString = context.getString(textId);
            mTextId = textId;
            mEnabled = enabled;
        }

        @Override
        public String toString() {
            return mTextString;
        }

        /** @return Whether or not this {@link Item} should be enabled. */
        public boolean getIsEnabled() {
            return mEnabled;
        }

        /** @return The string resource id this {@link Item} will show. */
        public @StringRes int getTextId() {
            return mTextId;
        }
    }

    /** A delegate used to populate the menu and to be notified of menu selection events. */
    public static interface Delegate {
        /**
         * Will be called every time the menu is about to be created to determine what content
         * should live in the menu.
         * @return A list of {@link Item}s to show in the menu.
         */
        Item[] getItems();

        /**
         * Will be called when an item was selected from the menu.
         * @param item The {@link Item} that was selected.
         */
        void onItemSelected(Item item);
    }

    private ListPopupWindow mPopupMenu;
    private Delegate mDelegate;

    /**
     * Creates a new {@link ListMenuButton}.
     * @param context The {@link Context} used to build the visuals from.
     * @param attrs   The specific {@link AttributeSet} used to build the button.
     */
    public ListMenuButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Sets the delegate this menu will rely on for populating the popup menu and handling selection
     * responses.  The menu will not show or work without it.
     *
     * @param delegate The {@link Delegate} to use for menu creation and selection handling.
     */
    public void setDelegate(Delegate delegate) {
        dismiss();
        mDelegate = delegate;
    }

    /** Called to dismiss any popup menu that might be showing for this button. */
    public void dismiss() {
        if (mPopupMenu == null) return;
        mPopupMenu.dismiss();
    }

    // View implementation.
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        setOnClickListener((view) -> showMenu());
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        dismiss();
    }

    private void showMenu() {
        if (mDelegate == null) throw new IllegalStateException("Delegate was not set.");

        // Referenced by PopupWindow helper classes (ArrayAdapter and OnItemClickListener).
        final Item[] items = mDelegate.getItems();

        if (items == null || items.length == 0) {
            throw new IllegalStateException("Delegate provided no items.");
        }

        // If we are about to show the menu, dismiss any previous incarnation in case the items have
        // changed.
        dismiss();

        mPopupMenu = new ListPopupWindow(getContext(), null, 0, R.style.ListMenuStyle);
        mPopupMenu.setAdapter(new ArrayAdapter<Item>(getContext(), R.layout.list_menu_item, items) {
            @Override
            public boolean areAllItemsEnabled() {
                return false;
            }

            @Override
            public boolean isEnabled(int position) {
                return items[position].getIsEnabled();
            }

            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                View view = super.getView(position, convertView, parent);
                view.setEnabled(isEnabled(position));
                return view;
            }
        });
        mPopupMenu.setAnchorView(this);
        // This may need to be an attribute and/or parameter in the future to allow different lists
        // to customize the width of the popup menu.  For now relying on a constant is fine.
        mPopupMenu.setWidth(getResources().getDimensionPixelSize(R.dimen.list_menu_width));
        mPopupMenu.setVerticalOffset(-getHeight());
        mPopupMenu.setModal(true);
        mPopupMenu.setOnItemClickListener((parent, view, position, id) -> {
            if (mDelegate != null) mDelegate.onItemSelected(items[position]);

            // TODO(crbug.com/600642): Somehow the on click event can be triggered way after we
            // dismiss the popup.
            if (mPopupMenu != null) mPopupMenu.dismiss();
        });
        mPopupMenu.setOnDismissListener(() -> { mPopupMenu = null; });

        mPopupMenu.show();
        mPopupMenu.getListView().setDivider(null);
    }
}