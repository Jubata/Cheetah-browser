package org.chromium.chrome.browser.cheetah;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationState;
import org.chromium.components.sync.AndroidSyncSettings;

import java.net.HttpURLConnection;
import java.net.URI;
import java.util.HashSet;
import java.util.Timer;
import java.util.TimerTask;

/**
 * Created by ivan2kh on 11/18/17.
 */

public class UnsentCommentsManager implements ApplicationStatus.ApplicationStateListener {
    private  Timer timer;
    private static UnsentCommentsManager unsentCommentsManager;
    PostCommentCallback postCommentCallback = new PostCommentCallback();
    private final int LONG_DELAY = 5000;
    private final int SHORT_DELAY = 1000;
    private static final Object sLock = new Object();
    private HashSet<CommentsListener> mListeners = new HashSet<>();


    public static void startIfNeeded() {
        LocalCommentsStorage localCommentsStorage = LocalCommentsStorage.get();
        localCommentsStorage.getOneUnsentAsync((comment) -> {
                    if (comment != null) getUnsentCommentsManager().scheduleNew();
                }
        );
    }

    private UnsentCommentsManager() {
        ApplicationStatus.registerApplicationStateListener(this);
    }

    public static UnsentCommentsManager getUnsentCommentsManager() {
        synchronized (sLock) {
            if (unsentCommentsManager == null) {
                unsentCommentsManager = new UnsentCommentsManager();
            }
            return unsentCommentsManager;
        }
    }

    public void scheduleNew() {
        if (timer != null) {
            return; //do nothing if timer already running
        }
        timer = new Timer();
        reschedule(SHORT_DELAY);
    }

    private void reschedule(int delay) {
        timer.schedule(new TimerTask() {
            @Override
            public void run() {
                getUnsentCommentsManager().PostUnsent();
            }
        }, delay);
    }

    private void cancel() {
        if (timer == null) {
            throw new RuntimeException("Timer was not created");
        } else {
            timer.cancel();
            timer = null;
        }
    }

    private void PostUnsent() {
        LocalCommentsStorage localCommentsStorage = LocalCommentsStorage.get();
        localCommentsStorage.getOneUnsentAsync((Comment comment) -> {
            if (comment == null) {
                cancel();
            } else {
                CommentsReceiver.PostComment(true, comment, postCommentCallback);
            }
        });
    }

    public void onNewUnsent(Comment comment) {
        LocalCommentsStorage localCommentsStorage = LocalCommentsStorage.get();
        localCommentsStorage.insertAsync(comment.text, comment.uri, LocalCommentsStorage.UNSENT,
                (Void v) -> {
                    scheduleNew();
                    for(CommentsListener listener : mListeners) {
                        listener.onCommentsChanged(comment.uri);
                    }
                });

        CommentsSync.getSync().onLocalChanged();
    }

    @Override
    public void onApplicationStateChange(int newState) {
        if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            if(timer != null) {
                startIfNeeded();
            }
        } else if (newState == ApplicationState.HAS_PAUSED_ACTIVITIES) {
            if(timer != null) {
                cancel();
            }
        }
    }


    private class PostCommentCallback implements CommentsReceiver.PostCallback {
        @Override
        public void onError(int responseCode, Exception e, Comment comment) {
            boolean markZombie = responseCode == HttpURLConnection.HTTP_NOT_ACCEPTABLE;

            if(markZombie) {
                    LocalCommentsStorage localCommentsStorage = LocalCommentsStorage.get();
                    localCommentsStorage.markAsZombieAsync(comment.localCommentUUID,
                            (Void)->reschedule(LONG_DELAY) );
            }
            else {
                reschedule(LONG_DELAY); //sent next message
            }
        }

        @Override
        public void onSuccess(Comment comment) {
            if(comment.localCommentUUID == null) {
                throw new RuntimeException("UnsentManager comment with localCommentUUID == null");
            }

            //we have successfully post comment after fail
            LocalCommentsStorage localCommentsStorage = LocalCommentsStorage.get();
            localCommentsStorage.deleteAsync(comment.localCommentUUID, (Void) -> reschedule(SHORT_DELAY));
        }
    }

    public void AddListener(CommentsListener listener) {
        mListeners.add(listener);
    }

    public void RemoveListener(CommentsListener listener) {
        mListeners.remove(listener);
    }

    public interface CommentsListener {
        //local changes happen
        void onCommentsChanged(URI uri);
    }
}
