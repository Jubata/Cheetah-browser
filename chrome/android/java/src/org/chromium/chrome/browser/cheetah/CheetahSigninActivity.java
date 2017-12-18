package org.chromium.chrome.browser.cheetah;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.LayoutInflater;

import com.google.android.gms.auth.api.signin.GoogleSignInAccount;
import com.jubata.components.signin.ActivityIntentProvider;
import com.jubata.components.signin.GoogleSignin;

import org.chromium.base.Log;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.ManagedPreferencesUtils;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.signin.AccountManagementFragment;
import org.chromium.chrome.browser.signin.AccountSigninActivity;
import org.chromium.chrome.browser.signin.AccountSigninView;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.components.signin.AccountManagerResult;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

/**
 * Created by ivan2kh on 12/11/17.
 */

public class CheetahSigninActivity extends AppCompatActivity implements
        AccountSigninView.Delegate,
        AccountSigninView.Listener,
        ActivityIntentProvider {
    private static final String TAG = "CheetahSigninActivity";
    private Object mGoogleSignInClient;
    private HashSet<ActivityIntentListener> listeners = new HashSet<>();
    private GoogleSignin mGoogleSign;
    private AccountSigninView mAccountSigninView;
    private GoogleSignInAccount mAccount;


    /**
     * A convenience method to create a AccountSigninActivity passing the access point as an
     * intent. Checks if the sign in flow can be started before showing the activity.
     * @param accessPoint {@link AccountSigninActivity.AccessPoint} for starting signin flow. Used in metrics.
     * @return {@code true} if sign in has been allowed.
     */
    public static boolean startIfAllowed(Context context, @AccountSigninActivity.AccessPoint int accessPoint) {
        if (!SigninManager.get(context).isSignInAllowed()) {
            if (SigninManager.get(context).isSigninDisabledByPolicy()) {
                ManagedPreferencesUtils.showManagedByAdministratorToast(context);
            }
            return false;
        }

        context.startActivity(createIntentForDefaultSigninFlow(context, accessPoint, false));
        return true;
    }

    /**
     * Creates an {@link Intent} which can be used to start the default signin flow.
     * @param accessPoint {@link AccountSigninActivity.AccessPoint} for starting signin flow. Used in metrics.
     * @param isFromPersonalizedPromo Whether the signin activity is started from a personalized
     *         promo.
     */
    public static Intent createIntentForDefaultSigninFlow(
            Context context, @AccountSigninActivity.AccessPoint int accessPoint, boolean isFromPersonalizedPromo) {
        Intent intent = new Intent(context, AccountSigninActivity.class);
//        intent.putExtra(INTENT_SIGNIN_ACCESS_POINT, accessPoint);
//        intent.putExtra(INTENT_SIGNIN_FLOW_TYPE, SIGNIN_FLOW_DEFAULT);
//        intent.putExtra(INTENT_IS_FROM_PERSONALIZED_PROMO, isFromPersonalizedPromo);
        return intent;
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

        mAccountSigninView = (AccountSigninView) LayoutInflater.from(this).inflate(
                org.chromium.chrome.R.layout.account_signin_view, null);

        mAccountSigninView.initFromSelectionPage(false, this, this);

        setContentView(mAccountSigninView);


        mGoogleSign = new GoogleSignin(this, this);

    }

    @Override
    protected void onStart() {
        super.onStart();

        mGoogleSign.signIn();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        for(ActivityIntentListener listener: listeners) {
            listener.onActivityResult(requestCode, resultCode, data);
        }
    }

    @Override
    public Activity getActivity() {
        return this;
    }

    @Override
    public void onAccountSelectionCanceled() {
        finish();
    }

    @Override
    public void onNewAccount() {

    }

    @Override
    public void onAccountSelected(String accountName, boolean isDefaultAccount, boolean settingsClicked) {
        final Context context = this;
        SigninManager.get(this).signIn(mAccount.getAccount(), this, new SigninManager.SignInCallback() {
//        SigninManager.get(this).signIn(accountName, this, new SigninManager.SignInCallback() {
            @Override
            public void onSignInComplete() {
                if (settingsClicked) {
                    Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                            context, AccountManagementFragment.class.getName());
                    startActivity(intent);
                }
                finish();
            }

            @Override
            public void onSignInAborted() {}
        });
    }

    @Override
    public void onFailedToSetForcedAccount(String forcedAccountName) {

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
        AppHooks.get().getAccountManagerDelegate().setAccount(account);
        mAccount = account;
        AccountManagerResult<List<String>> result = new AccountManagerResult<List<String>>(
                Arrays.asList(account.getAccount().name)
        );
        mAccountSigninView.updateAccounts(result);
    }
}
