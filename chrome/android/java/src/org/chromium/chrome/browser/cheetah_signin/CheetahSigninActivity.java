package org.chromium.chrome.browser.cheetah_signin;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.widget.Button;
import android.widget.TextView;

import com.google.android.gms.auth.api.signin.GoogleSignInAccount;

import org.chromium.chrome.R;

import org.chromium.base.Log;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.signin.AccountSigninChooseView;
import org.chromium.ui.widget.ButtonCompat;

import java.util.HashSet;

public class CheetahSigninActivity extends AppCompatActivity implements
        ActivityIntentProvider {
    private static final String TAG = "CheetahSigninActivity";
    private HashSet<ActivityIntentListener> listeners = new HashSet<>();
    private GoogleSignin mGoogleSign;
    private AccountSigninChooseView mSigninChooseView;

    public interface SignInReceiver {
        void onResult(GoogleSignInAccount account);
    }

    public void forceSignIn(Context context, SignInReceiver receiver) {
        GoogleSignInAccount account = GoogleSignin.silentSignin(context);
        if(account!=null) {
            receiver.onResult(account);
        }

    }

    /**
     * A convenience method to create a CheetahSigninActivity
     */
    public static void startActivity(Context context) {
        context.startActivity(new Intent(context, CheetahSigninActivity.class));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // The browser process must be started here because this activity may be started from the
        // recent apps list and it relies on other activities and the native library to be loaded.
        try {
            ChromeBrowserInitializer.getInstance(this).handleSynchronousStartup();
        } catch (ProcessInitException e) {
            Log.e(TAG, "Failed to start browser process.", e);
            // Since the library failed to initialize nothing in the application
            // can work, so kill the whole application not just the activity
            System.exit(-1);
        }

        // We don't trust android to restore the saved state correctly, so pass null.
        super.onCreate(null);
        setContentView(R.layout.account_signin_view_cheetah);

        mGoogleSign = new GoogleSignin(this, this);

        ButtonCompat positiveButton = (ButtonCompat) findViewById(R.id.positive_button);
        positiveButton.setOnClickListener(view -> mGoogleSign.signIn());

        Button negativeButton = (Button) findViewById(R.id.negative_button);
        negativeButton.setOnClickListener(view -> finish());

        String google = "Google";
        ((TextView) findViewById(R.id.account_name)).setText(google);
    }


    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        for(ActivityIntentListener listener: listeners) {
            listener.onActivityResult(requestCode, resultCode, data);
        }
    }

    @Override
    public void addListener(ActivityIntentListener listener) {
        listeners.add(listener);
    }

    @Override
    public void removeListener(ActivityIntentListener listener) {
        listeners.remove(listener);
    }

    @Override
    public void onSignedInAccount(GoogleSignInAccount account) {
        finish();
    }
}
