// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cheetah;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.LinearLayout;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.ui.UiUtils;
import org.chromium.ui.widget.ButtonCompat;

import java.util.List;

/**
 * Container view for personalized signin promos.
 */
public class EditCommentView extends LinearLayout implements View.OnClickListener {
    protected TintedImageButton mDeleteButton;
    protected TintedImageButton mPostButton;
    protected EditText mEditText;
    private View mEditContainer;
    private float mUrlFocusChangePercent;
    protected ButtonCompat mAddCommentButton;
    protected PostCallback postCallback = new PostCallback();
    private Tab tab;

    public EditCommentView(Context context, AttributeSet attrs) {

        super(context, attrs);

    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mEditContainer = findViewById(R.id.comment_edit_container);
        mDeleteButton = findViewById(R.id.delete_comment_button);
        mPostButton = findViewById(R.id.post_comment_button);
        mEditText = findViewById(R.id.comment_edit_text);
        mAddCommentButton = findViewById(R.id.add_comment_button);

        mAddCommentButton.setOnClickListener(this);
        mDeleteButton.setOnClickListener(this);
        mPostButton.setOnClickListener(this);
    }

    /**
     * Updates percentage of current the URL focus change animation.
     * @param percent 1.0 is 100% focused, 0 is completely unfocused.
     */
    public void setUrlFocusChangePercent(float percent) {
        mUrlFocusChangePercent = percent;

        if (percent > 0f) {
            mEditContainer.setVisibility(VISIBLE);
        } else if (percent == 0f) {
            // If a URL focus change is in progress, then it will handle setting the visibility
            // correctly after it completes.  If done here, it would cause the URL to jump due
            // to a badly timed layout call.
            mEditContainer.setVisibility(GONE);
        }

    }

    @Override
    public void onClick(View view) {
        if(view == mAddCommentButton) {
            mAddCommentButton.setVisibility(GONE);
            mEditContainer.setVisibility(VISIBLE);
            mEditText.requestFocus();
            UiUtils.showKeyboard(mEditText);
        } else if(view == mDeleteButton) {
            mEditText.setText("");
        } else if(view == mPostButton) {
            if(tab != null) {
                CommentsReceiver.PostComment(true,
                        tab.getUrl(),
                        mEditText.getText().toString(),
                        postCallback
                );
            }
        }
    }

    public void setTab(Tab tab) {
        this.tab = tab;
    }

    class PostCallback implements CommentsReceiver.CommentsCallback {

        @Override
        public void onResponse(List<Comment> comments) {

        }

        @Override
        public void onError(int responseCode, Exception e) {

        }
    }

}
