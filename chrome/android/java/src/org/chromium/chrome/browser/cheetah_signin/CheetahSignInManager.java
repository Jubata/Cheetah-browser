package org.chromium.chrome.browser.cheetah_signin;


import android.annotation.SuppressLint;
import android.support.annotation.UiThread;
import android.os.AsyncTask;

import com.google.android.gms.auth.api.signin.GoogleSignInAccount;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;

import java.util.HashSet;

import static org.chromium.base.ThreadUtils.runOnUiThread;

/**
 * TODO: Add a class header comment!
 */

public class CheetahSignInManager {
    private static CheetahSignInManager sSignInManager;
    private boolean mIsSignedIn = false;
    private boolean mIsValidSignInState = false;
    private HashSet<SignInListener> listeners = new HashSet<>();
    private static Object sLock = new Object();

    public interface SignInListener {
        void onSignedIn();
        void onSignedOut();
    }

    public static CheetahSignInManager get() {
        if(sSignInManager == null) {
            sSignInManager = new CheetahSignInManager();
        }
        return sSignInManager;
    }

    @UiThread
    public void addListener(SignInListener listener) {
        assert ThreadUtils.runningOnUiThread();
        listeners.add(listener);
    }

    @UiThread
    public void removeListener(SignInListener listener) {
        assert ThreadUtils.runningOnUiThread();
        listeners.remove(listener);
    }

    public void onSignedIn() {
        mIsValidSignInState = true;
        mIsSignedIn = true;

        for(SignInListener listener: listeners) {
            listener.onSignedIn();
        }
    }

    public void onSignedOut() {
        mIsValidSignInState = true;
        mIsSignedIn = false;

        for(SignInListener listener: listeners) {
            listener.onSignedOut();
        }
    }

    public boolean isSignedIn() {
        if(!mIsValidSignInState) {
            synchronized (sLock) {
                if(!mIsValidSignInState) {
                    GoogleSignInAccount account =
                            GoogleSignin.silentSignin(ContextUtils.getApplicationContext());
                    mIsValidSignInState = true;
                    mIsSignedIn = account != null;

                    runOnUiThread((Runnable) () -> {
                        if (mIsSignedIn) {
                            onSignedIn();
                        } else {
                            onSignedOut();
                        }
                    });
                }
            }
        }
        return mIsSignedIn;
    }

    @SuppressLint("StaticFieldLeak")
    public void initializeAsync() {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                isSignedIn();
                return null;
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, (Void)null);
    }
}
