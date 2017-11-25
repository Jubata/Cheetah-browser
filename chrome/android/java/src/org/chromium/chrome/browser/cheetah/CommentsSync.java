package org.chromium.chrome.browser.cheetah;

import java.util.ArrayList;
import java.util.HashSet;

/**
 * Created by ivan2kh on 11/25/17.
 */

public class CommentsSync{
    private static CommentsSync sync;
    private final static Object sLock = new Object();
    private HashSet<Listener> listeners=new HashSet<>();

    public static CommentsSync getSync() {
        synchronized (sLock) {
            if(sync==null) {
                sync = new CommentsSync();
            }
            return sync;
        }
    }

    public void addListener(Listener l) {
        listeners.add(l);
    }

    public void removeListener(Listener l) {
        listeners.remove(l);
    }

    public void onRemoteChanged() {
        for(Listener l:listeners) {
            l.onRemoteChanged();
        }
    }

    public void onLocalChanged() {
        for(Listener l:listeners) {
            l.onLocalChanged();
        }
    }

    public interface Listener {
        void onRemoteChanged();
        void onLocalChanged();
    }
}
