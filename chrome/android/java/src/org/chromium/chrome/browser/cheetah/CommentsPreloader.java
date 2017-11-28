package org.chromium.chrome.browser.cheetah;

import android.util.Log;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.content_public.browser.LoadUrlParams;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.HashMap;
import java.util.UUID;

/**
 * Created by ivan2kh on 11/24/17.
 */

public class CommentsPreloader implements CommentsSync.Listener {
    private final ChromeActivity mActivity;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private Tab mTab;
    private TabObserver mTabObserver;
    private String currentUrl = "";
    private Comments comments = new Comments();
    private URI remoteUri = null;
    private static final Object sLock = new Object();

    @Override
    public void onRemoteChanged() {
        Invalidate();
    }

    @Override
    public void onLocalChanged() {
        Invalidate();
    }

    private void Invalidate() {
        synchronized (sLock) {
            remoteUri = null;
        }
        ReloadComments(currentUrl);
    }

    static class Comments {
        HashMap<UUID, Comment> remote;
        HashMap<UUID, Comment> unsent;
    }

    public CommentsPreloader(ChromeActivity activity) {
        mActivity = activity;

        mTabObserver = new TabObserver();

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onChange() {
                Tab tab = mActivity.getActivityTab();
                if (tab == mTab) return;
                swapToTab(tab);
            }
        };

        TabModelSelector tabModelSelector = mActivity.getTabModelSelector();
        if (tabModelSelector != null) {
            tabModelSelector.addObserver(mTabModelSelectorObserver);
        }

        CommentsSync.getSync().addListener(this);
    }


    Comments getComments(URI uri) {
        synchronized (sLock) {
            if (remoteUri!=null && remoteUri.equals(uri)) {
                return comments;
            } else {
                return null;
            }
        }
    }

    private void swapToTab(Tab tab) {
        assert tab != null;
        if (mTab != null) {
            mTab.removeObserver(mTabObserver);
        }

        mTab = tab;
        if(mTab!=null) {
            mTab.addObserver(mTabObserver);
            mTabObserver.onContentChanged(mTab);
        }
    }

    private class TabObserver extends EmptyTabObserver {
        @Override
        public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
            onChangeUrl(params.getUrl());
        }

        @Override
        public void onUrlUpdated(Tab tab) {
            onChangeUrl(tab.getUrl());
        }

        private void onChangeUrl(String url) {
            if (currentUrl.equals(url)) {
                return;
            }
            currentUrl = url;

            ReloadComments(url);
        }
    }

    private void ReloadComments(String url) {
        final URI uri;
        try {
            uri = new URI(url);
            CommentsReceiver.GetComments(true, uri,
                    new CommentsReceiver.CommentsCallback() {
                        @Override
                        public void onResponse(HashMap<UUID, Comment> remote) {
                            LocalCommentsStorage localCommentsStorage = LocalCommentsStorage.get();
                            localCommentsStorage.getUnsentAndZombiesAsync(uri,
                                    (HashMap<UUID, Comment> unsent) -> {
                                        synchronized (sLock) {
                                            comments.unsent = unsent;
                                            comments.remote = remote;
                                            remoteUri = uri;
                                        }
                                    });
                        }

                        @Override
                        public void onError(int responseCode, Exception e) {
                        }
                    });
        } catch (URISyntaxException e) {
            e.printStackTrace();
        }
    }
}
