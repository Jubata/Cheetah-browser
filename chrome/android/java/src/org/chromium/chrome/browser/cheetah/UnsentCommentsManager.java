package org.chromium.chrome.browser.cheetah;

import java.net.HttpURLConnection;
import java.util.Timer;
import java.util.TimerTask;

/**
 * Created by ivan2kh on 11/18/17.
 */

public class UnsentCommentsManager {
    private  Timer timer;
    private static UnsentCommentsManager unsentCommentsManager;
    FailureCallback failureCallback = new FailureCallback();
    private final int LONG_DELAY = 5000;
    private final int SHORT_DELAY = 1000;
    private static final Object sLock = new Object();


    public static void startIfNeeded() {
        synchronized (sLock) {
            LocalCommentsStorage localCommentsStorage = LocalCommentsStorage.get();
            Comment comment = localCommentsStorage.getOneUnsent();
            if(comment != null) {
                getUnsentCommentsManager().scheduleNew();
            }
        }
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
        synchronized (sLock) {
            if (timer != null) {
                return; //do nothing if timer already running
            }
            timer = new Timer();
            reschedule(LONG_DELAY);
        }
    }

    private void reschedule(int delay) {
        synchronized (sLock) {
            timer.schedule(new TimerTask() {
                @Override
                public void run() {
                    getUnsentCommentsManager().PostUnsent();
                }
            }, delay);
        }
    }

    private void cancel() {
        synchronized (sLock) {
            if(timer == null) {
                throw new RuntimeException("Timer was not created");
            } else {
                timer.cancel();
                timer = null;
            }
        }
    }

    private void PostUnsent() {
        synchronized (sLock) {
            LocalCommentsStorage localCommentsStorage = LocalCommentsStorage.get();
            Comment comment = localCommentsStorage.getOneUnsent();
            if(comment == null) {
                cancel();
            } else {
                CommentsReceiver.PostComment(true, comment, failureCallback);
            }
        }
    }

    public void onNewUnsent(Comment comment) {
        synchronized (sLock) {
            LocalCommentsStorage localCommentsStorage = LocalCommentsStorage.get();
            localCommentsStorage.insert(comment.text, comment.uri, LocalCommentsStorage.UNSENT);
            scheduleNew();
        }
    }


    private class FailureCallback implements CommentsReceiver.PostCallback {
        @Override
        public void onError(int responseCode, Exception e, Comment comment) {
            boolean markZombie = responseCode == HttpURLConnection.HTTP_NOT_ACCEPTABLE;

            if(markZombie) {
                synchronized (sLock) {
                    LocalCommentsStorage localCommentsStorage = LocalCommentsStorage.get();
                    localCommentsStorage.markAsZombie(comment.localCommentUUID);
                }
            }

            reschedule(LONG_DELAY); //sent next message
        }

        @Override
        public void onSuccess(Comment comment) {
            if(comment.localCommentUUID == null) {
                throw new RuntimeException("UnsentManager comment with localCommentUUID == null");
            }

            synchronized (sLock) {
                //we have successfully post comment after fail
                LocalCommentsStorage localCommentsStorage = LocalCommentsStorage.get();
                localCommentsStorage.delete(comment.localCommentUUID);
            }

            reschedule(SHORT_DELAY);
        }
    }
}
