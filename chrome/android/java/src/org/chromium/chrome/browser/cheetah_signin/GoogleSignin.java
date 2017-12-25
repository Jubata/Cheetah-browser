package org.chromium.chrome.browser.cheetah_signin;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v7.app.AppCompatActivity;

import com.google.android.gms.auth.api.Auth;
import com.google.android.gms.auth.api.signin.GoogleSignInAccount;
import com.google.android.gms.auth.api.signin.GoogleSignInOptions;
import com.google.android.gms.auth.api.signin.GoogleSignInResult;
import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.common.api.OptionalPendingResult;
import com.google.android.gms.common.api.ResultCallback;
import com.google.android.gms.common.api.Status;

import org.chromium.chrome.GoogleAPIKeys;


public class GoogleSignin implements GoogleApiClient.OnConnectionFailedListener,
        ActivityIntentProvider.ActivityIntentListener {
    private final GoogleApiClient mGoogleApiClient;
    private final AppCompatActivity mActivity;
    private final ActivityIntentProvider mProvider;

    private static final int RC_SIGN_IN = 9001;

    public GoogleSignin(AppCompatActivity activity, ActivityIntentProvider provider) {
        mActivity = activity;
        // Configure Google Sign In
        GoogleSignInOptions gso = new GoogleSignInOptions.Builder(GoogleSignInOptions.DEFAULT_SIGN_IN)
                .requestIdToken(GoogleAPIKeys.GOOGLE_CLIENT_ID_CHEETAH)
                .requestEmail()
                .build();

        mGoogleApiClient = new GoogleApiClient.Builder(activity)
                .enableAutoManage(activity /* FragmentActivity */, this /* OnConnectionFailedListener */)
                .addApi(Auth.GOOGLE_SIGN_IN_API, gso)
                .build();

        mProvider = provider;
    }

    private static GoogleSignInOptions createSignInOptions() {
        GoogleSignInOptions gso = new GoogleSignInOptions.Builder(GoogleSignInOptions.DEFAULT_SIGN_IN)
                .requestIdToken(GoogleAPIKeys.GOOGLE_CLIENT_ID_CHEETAH)
                .requestEmail()
                .build();
        return gso;
    }

    public static GoogleSignInAccount silentSignin(Context context) {
        GoogleSignInOptions gso = createSignInOptions();

        GoogleApiClient googleApiClient = new GoogleApiClient.Builder(context)
                .addConnectionCallbacks(new GoogleApiClient.ConnectionCallbacks() {
                    @Override
                    public void onConnected(@android.support.annotation.Nullable Bundle bundle) {

                    }

                    @Override
                    public void onConnectionSuspended(int i) {

                    }
                })
                .addOnConnectionFailedListener(connectionResult -> {})
                .addApi(Auth.GOOGLE_SIGN_IN_API, gso)
                .build();
        googleApiClient.connect();
        GoogleSignInAccount account = null;
        OptionalPendingResult<GoogleSignInResult> opr =
                Auth.GoogleSignInApi.silentSignIn(googleApiClient);
        if (opr.isDone()) {
            // Users cached credentials are valid, GoogleSignInResult containing ID token
            // is available immediately. This likely means the current ID token is already
            // fresh and can be sent to your server.
            GoogleSignInResult result = opr.get();
            if (result.isSuccess()) {
                account = result.getSignInAccount();
            }
        }
        googleApiClient.disconnect();

        return account;
    }

    @Override
    public void onConnectionFailed(@NonNull ConnectionResult connectionResult) {

    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        mProvider.removeListener(this);
        // Result returned from launching the Intent from GoogleSignInApi.getSignInIntent(...);
        if (requestCode == RC_SIGN_IN) {
            GoogleSignInResult result = Auth.GoogleSignInApi.getSignInResultFromIntent(data);
            handleSignInResult(result);
        }
    }

    private void handleSignInResult(GoogleSignInResult result) {
//        Log.d(TAG, "handleSignInResult:" + result.isSuccess());
        if (result.isSuccess()) {
            // Signed in successfully, show authenticated UI.
            GoogleSignInAccount acct = result.getSignInAccount();
            mProvider.onSignedInAccount(acct);

            CheetahSignInManager.get().onSignedIn();
//            acct.getDisplayName();
//            mStatusTextView.setText(getString(R.string.signed_in_fmt, acct.getDisplayName()));
//            updateUI(true);
        } else {
            // Signed out, show unauthenticated UI.
//            updateUI(false);
        }
    }


    public void signIn() {
        Intent signInIntent = Auth.GoogleSignInApi.getSignInIntent(mGoogleApiClient);
        mProvider.addListener(this);
        mActivity.startActivityForResult(signInIntent, RC_SIGN_IN);
    }

    public void signOut() {
        Auth.GoogleSignInApi.signOut(mGoogleApiClient).setResultCallback(
                new ResultCallback<Status>() {
                    @Override
                    public void onResult(Status status) {
                        if(status.isSuccess()) {
                            CheetahSignInManager.get().onSignedOut();
                        }
                        // [START_EXCLUDE]
//                        updateUI(false);
                        // [END_EXCLUDE]
                    }
                });
    }
}