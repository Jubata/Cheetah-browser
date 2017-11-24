// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cheetah;

import android.support.annotation.DrawableRes;
import android.widget.TextView;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.metrics.ImpressionTracker;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.cards.CardViewHolder;
import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.ntp.cards.NodeVisitor;
import org.chromium.chrome.browser.ntp.cards.OptionalLeaf;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Shows a card prompting the user to sign in. This item is also an {@link OptionalLeaf}, and sign
 * in state changes control its visibility.
 */
public class NewComment extends OptionalLeaf implements ImpressionTracker.Listener {
    protected final Tab activeTab;

    public NewComment(Tab activeTab) {
        this.activeTab = activeTab;
        updateVisibility();
    }

    @Override
    @ItemViewType
    protected int getItemViewType() {
        return ItemViewType.COMMENT_FORM;
    }

    /**
     * @return a {@link NewTabPageViewHolder} which will contain the view for the comment.
     */
    public NewTabPageViewHolder createViewHolder(SuggestionsRecyclerView parent,
            ContextMenuManager contextMenuManager, UiConfig config) {
            return new NewCommentViewHolder(
                    parent, config, contextMenuManager, activeTab);

    }

    @Override
    protected void onBindViewHolder(NewTabPageViewHolder holder) {
            ((NewCommentViewHolder) holder).onBindViewHolder();
            return;

    }

    @Override
    protected void visitOptionalItem(NodeVisitor visitor) {
        visitor.visitSignInPromo();
    }

    @Override
    public void onImpression() {

    }

    private void updateVisibility() {
        setVisibilityInternal( true);
    }


    /**
     * View Holder for {@link NewComment} if the personalized promo is to be shown.
     */
    @VisibleForTesting
    public static class NewCommentViewHolder extends CardViewHolder {
        private Tab tab;

        public NewCommentViewHolder(SuggestionsRecyclerView parent, UiConfig config,
                                    ContextMenuManager contextMenuManager,
                                    Tab tab) {
            super(R.layout.edit_comment_view,
                    parent, config, contextMenuManager);
            this.tab = tab;
            if (!FeatureUtilities.isChromeHomeEnabled()) {
                getParams().topMargin = parent.getResources().getDimensionPixelSize(
                        R.dimen.ntp_sign_in_promo_margin_top);
            }

        }

        @Override
        public void onBindViewHolder() {
            super.onBindViewHolder();
            updateNewCommentView();
        }

        @DrawableRes
        @Override
        protected int selectBackground(boolean hasCardAbove, boolean hasCardBelow) {
            // Modern does not update the card background.
            assert !FeatureUtilities.isChromeHomeEnabled();
            return R.drawable.ntp_signin_promo_card_single;
        }

        private void updateNewCommentView() {
            EditCommentView view = (EditCommentView) itemView;
            view.setTab(tab);
        }

    }

}
