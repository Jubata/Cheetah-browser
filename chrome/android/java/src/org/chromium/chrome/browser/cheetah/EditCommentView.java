// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cheetah;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.AsyncTask;
import android.util.AttributeSet;
import android.view.View;
import android.widget.EditText;
import android.widget.LinearLayout;

import com.google.android.gms.auth.api.signin.GoogleSignInAccount;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.cheetah_signin.CheetahSignInManager;
import org.chromium.chrome.browser.cheetah_signin.CheetahSigninActivity;
import org.chromium.chrome.browser.cheetah_signin.GoogleSignin;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.ui.UiUtils;
import org.chromium.ui.widget.ButtonCompat;

import java.net.URI;
import java.net.URISyntaxException;

/**
 * Container view for personalized signin promos.
 */
public class EditCommentView extends LinearLayout implements View.OnClickListener, CheetahSignInManager.SignInListener {
    protected TintedImageButton mDeleteButton;
    protected TintedImageButton mPostButton;
    protected EditText mEditText;
    private View mEditContainer;
    protected ButtonCompat mAddCommentButton;
    private Tab tab;

    public EditCommentView(Context context, AttributeSet attrs) {
        super(context, attrs);

    }

    private void updateSinginState() {
        mAddCommentButton.setText(CheetahSignInManager.get().isSignedIn() ?
                R.string.add_comment :
                R.string.sign_in_to_chrome);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        // View is now attached

        CheetahSignInManager.get().addListener(this);
        updateSinginState();
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

        updateEditField();
        updateSinginState();
    }

    private void updateEditField() {
        //switchToEdit(false);
        if (tab != null) {
            try {
                LocalCommentsStorage.get().getDraftAsync(new URI(tab.getUrl()),
                        (Comment comment) -> {
                            if (comment != null && mEditText != null) {
                                mEditText.setText(comment.text);
                                switchToEdit(true);
                            }
                        }
                );
            } catch (URISyntaxException e) {
                e.printStackTrace();//todo: add metrics here
                //todo: add alert dialog
            }
        }
    }

    public void switchToEdit(boolean editMode) {
        if (editMode) {
            mAddCommentButton.setVisibility(GONE);
            mEditContainer.setVisibility(VISIBLE);
            mEditText.requestFocus();
            UiUtils.showKeyboard(mEditText);
            mEditContainer.setVisibility(VISIBLE);
        } else {
            mEditContainer.setVisibility(GONE);
            mAddCommentButton.setVisibility(VISIBLE);
            UiUtils.hideKeyboard(mEditText);
        }
    }

    @Override
    public void onClick(View view) {
        if(view == mAddCommentButton) {
            if(! CheetahSignInManager.get().isSignedIn() ) {
                CheetahSigninActivity.startActivity(getContext());
            } else {
                switchToEdit(true);
            }
        } else if(view == mDeleteButton) {
            finishEdit();
        } else if(view == mPostButton) {
            postComment();
        }
    }

    @SuppressLint("StaticFieldLeak")
    private void postComment() {
        String editText = mEditText.getText().toString().trim();
        if(tab == null || editText.isEmpty()) {
            return;
        }

        Comment comment = new Comment();
        try {

            comment.text = mEditText.getText().toString();
            comment.uri = new URI(tab.getUrl());


        } catch (URISyntaxException e) {
            e.printStackTrace();
        }

        finishEdit();

        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                GoogleSignInAccount account = GoogleSignin.silentSignin(ContextUtils.getApplicationContext());
                if(account != null) {
                    comment.userPic = account.getPhotoUrl().toString();
                    comment.userName = account.getDisplayName();
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void aVoid) {
                super.onPostExecute(aVoid);

                UnsentCommentsManager manager = UnsentCommentsManager.getUnsentCommentsManager();
                manager.onNewUnsent(comment);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, (Void)null);
    }

    //Switch to button mode + delete draft
    private void finishEdit() {
        mEditText.setText("");
        switchToEdit(false);
        if(tab != null) {
            try {
                LocalCommentsStorage.get().deleteDraftAsync(  new URI(tab.getUrl()) );
            } catch (URISyntaxException e) {
                e.printStackTrace();//todo: add metrics here
                //todo: add alert dialog
            }
        }
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        CheetahSignInManager.get().removeListener(this);

        String editText = mEditText.getText().toString().trim();
        if(editText.isEmpty()) {
            try {
                LocalCommentsStorage.get().deleteDraftAsync(  new URI(tab.getUrl()) );
            } catch (URISyntaxException e) {
                e.printStackTrace();
            }
        } else if(tab != null) {
            try {
                LocalCommentsStorage.get().updateDraftAsync(
                        mEditText.getText().toString(),
                        new URI(tab.getUrl()) );
            } catch (URISyntaxException e) {
                e.printStackTrace();
            }
        }
    }

    public void setTab(Tab tab) {
        this.tab = tab;
        updateEditField();
    }

    @Override
    public void onSignedIn() {
        updateSinginState();
    }

    @Override
    public void onSignedOut() {
        updateSinginState();
    }
}
