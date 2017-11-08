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
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Shows a card prompting the user to sign in. This item is also an {@link OptionalLeaf}, and sign
 * in state changes control its visibility.
 */
public class NewComment extends OptionalLeaf implements ImpressionTracker.Listener {




    public NewComment() {
        updateVisibility();
    }

    @Override
    @ItemViewType
    protected int getItemViewType() {
        return ItemViewType.COMMENT_FORM;
    }



    /**
     * @return a {@link NewTabPageViewHolder} which will contain the view for the signin promo.
     */
    public NewTabPageViewHolder createViewHolder(SuggestionsRecyclerView parent,
            ContextMenuManager contextMenuManager, UiConfig config) {
            return new PersonalizedPromoViewHolder(
                    parent, config, contextMenuManager);

    }

    @Override
    protected void onBindViewHolder(NewTabPageViewHolder holder) {
            ((PersonalizedPromoViewHolder) holder).onBindViewHolder();
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
    public static class PersonalizedPromoViewHolder extends CardViewHolder {

        public PersonalizedPromoViewHolder(SuggestionsRecyclerView parent, UiConfig config,
                ContextMenuManager contextMenuManager) {
            super(R.layout.edit_comment_view,
                    parent, config, contextMenuManager);
            if (!FeatureUtilities.isChromeHomeEnabled()) {
                getParams().topMargin = parent.getResources().getDimensionPixelSize(
                        R.dimen.ntp_sign_in_promo_margin_top);
            }

        }

        @Override
        public void onBindViewHolder() {
            super.onBindViewHolder();
            updatePersonalizedSigninPromo();
        }

        @DrawableRes
        @Override
        protected int selectBackground(boolean hasCardAbove, boolean hasCardBelow) {
            // Modern does not update the card background.
            assert !FeatureUtilities.isChromeHomeEnabled();
            return R.drawable.ntp_signin_promo_card_single;
        }

        /**
         * Triggers an update of the personalized signin promo. Intended to be used as
         * {@link PartialBindCallback}.
         */
        public static void update(NewTabPageViewHolder viewHolder) {
            ((PersonalizedPromoViewHolder) viewHolder).updatePersonalizedSigninPromo();
        }

        private void updatePersonalizedSigninPromo() {

            EditCommentView view = (EditCommentView) itemView;
            TextView description = view.findViewById(R.id.signin_promo_description);
            description.setText(R.string.signin_promo_description_ntp_content_suggestions);
            ButtonCompat button = view.findViewById(R.id.signin_promo_signin_button);
            button.setText(R.string.sign_in_to_chrome);
        }

    }

}
