// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cheetah;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.ntp.cards.SuggestionsCategoryInfo;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.ContentSuggestionsCardLayout;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.ContentSuggestionsAdditionalAction;
import org.chromium.chrome.browser.tab.Tab;

import java.util.ArrayList;
import java.util.List;

/**
 * Provides access to the snippets to display on the NTP using the C++ ContentSuggestionsService.
 */
public class CommentsSource implements SuggestionsSource {
    private final ObserverList<Observer> mObserverList = new ObserverList<>();
    private final Tab activeTab;

    /**
     * Creates a SnippetsBridge for getting snippet data for the current user.
     *
     * @param profile Profile of the user that we will retrieve snippets for.
     */
    public CommentsSource(Profile profile, Tab activeTab) {
        this.activeTab = activeTab;
    }

    @Override
    public void destroy() {
        mObserverList.clear();
    }

    @Override
    public boolean areRemoteSuggestionsEnabled() {
        return true; //IK??? what for
    }

    @Override
    public void fetchRemoteSuggestions() {
        //IK!!! use it
    }

    @Override
    public int[] getCategories() {
        int[] categories2 = new int[1];
        categories2[0] = 10001;
        return categories2;
    }

    private boolean fetched = false;

    @Override
    @CategoryStatus
    public int getCategoryStatus(int category) {
        if(category == 10001) {
            if (fetched)
                return CategoryStatus.AVAILABLE;
            else
                return CategoryStatus.AVAILABLE_LOADING; //IK!!! use loading
        }
        return CategoryStatus.NOT_PROVIDED;
    }

    @Override
    public SuggestionsCategoryInfo getCategoryInfo(int category) {
        if(category == 10001) {
            SuggestionsCategoryInfo info = new SuggestionsCategoryInfo(10001,
                    "comments",
                    ContentSuggestionsCardLayout.MINIMAL_CARD,
                    ContentSuggestionsAdditionalAction.NONE,
                    false,
                    "no suggestions"
                    );
            return info;
        }
        throw new IllegalArgumentException("ouch");
    }

    private List<SnippetArticle> articles = new ArrayList<>();

    @Override
    public List<SnippetArticle> getSuggestionsForCategory(int category) {
        if(category == 10001) {
            if(!fetched && activeTab != null) {
                CommentsReceiver.GetComments(false, activeTab.getUrl(),
                        new CommentsReceiver.CommentsCallback() {
                            @Override
                            public void onResponse(List<Comment> comments) {
                                articles.clear();
                                for (Comment comment : comments) {
                                    SnippetArticle article = new SnippetArticle(10001,
                                            "0",
                                            comment.text,
                                            comment.user,
                                            "http://yandex.ru",
                                            1,
                                            0,
                                            1,
                                            false,
                                            0x80ff0000);
                                    articles.add(article);
                                }
                                fetched = true;
                                for (Observer observer : mObserverList) {
                                    observer.onCategoryStatusChanged(10001, CategoryStatus.AVAILABLE);
                                    observer.onNewSuggestions(10001);
                                    observer.onFullRefreshRequired();
                                }

                            }
                        });
                SnippetArticle article = new SnippetArticle(10001,
                    "0",
                    "Dummy article",
                    "",
                    "",
                    1,
                    0,
                    1,
                    false,
                    0x80ff0000);
                articles.add(article);
                return articles;
            }
//            articles.clear();
//            SnippetArticle article = new SnippetArticle(10001,
//                    "0",
//                    "Some text",
//                    "publisher",
//                    "http://yandex.ru",
//                    1,
//                    0,
//                    1,
//                    false,
//                    0x80ff0000);
//            articles.add(article);
//            SnippetArticle article2 = new SnippetArticle(10001,
//                    "0",
//                    "Some text2",
//                    "publisher",
//                    "http://yandex.ru",
//                    1,
//                    0,
//                    1,
//                    false,
//                    0x80ff0000);
//            articles.add(article2);
            return articles;
        }
        throw new IllegalArgumentException("ouch");
    }

    @Override
    public void fetchSuggestionImage(SnippetArticle suggestion, Callback<Bitmap> callback) {
        //IK interesting
        callback.onResult(Bitmap.createBitmap(1,1, Bitmap.Config.ARGB_8888));
    }

    @Override
    public void fetchSuggestionFavicon(SnippetArticle suggestion, int minimumSizePx,
            int desiredSizePx, Callback<Bitmap> callback) {
        //IK interesting
        callback.onResult(Bitmap.createBitmap(1,1, Bitmap.Config.ARGB_8888));
    }

    @Override
    public void fetchContextualSuggestions(String url, Callback<List<SnippetArticle>> callback) {
        callback.onResult(new ArrayList<SnippetArticle>()); //IK what for
    }

    @Override
    public void fetchContextualSuggestionImage(
            SnippetArticle suggestion, Callback<Bitmap> callback) {
        //IK interesting
        callback.onResult(Bitmap.createBitmap(1,1, Bitmap.Config.ARGB_8888));
    }

    @Override
    public void dismissSuggestion(SnippetArticle suggestion) {
        //IK interesting
    }

    @Override
    public void dismissCategory(@CategoryInt int category) {
    }

    @Override
    public void restoreDismissedCategories() {

    }

    @Override
    public void addObserver(Observer observer) {
        assert observer != null;
        mObserverList.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObserverList.removeObserver(observer);
    }

    @Override
    public void fetchSuggestions(@CategoryInt int category, String[] displayedSuggestionIds,
            Callback<List<SnippetArticle>> successCallback, Runnable failureRunnable) {
        successCallback.onResult(new ArrayList<SnippetArticle>());
    }
}
