package org.chromium.chrome.browser.cheetah_signin;

import android.content.Intent;

import com.google.android.gms.auth.api.signin.GoogleSignInAccount;

/**
 * Created by ivan2kh on 12/13/17.
 */

public interface ActivityIntentProvider {
    interface ActivityIntentListener {
        void onActivityResult(int requestCode, int resultCode, Intent data);
    }

    void addListener(ActivityIntentListener listener);
    void removeListener(ActivityIntentListener listener);

    void onSignedInAccount(GoogleSignInAccount account);
}
