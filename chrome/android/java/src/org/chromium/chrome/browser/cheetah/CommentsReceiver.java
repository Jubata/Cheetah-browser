package org.chromium.chrome.browser.cheetah;

import android.net.Uri;
import android.os.AsyncTask;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.GoogleAPIKeys;
import org.chromium.chrome.browser.content.ContentUtils;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.net.MalformedURLException;
import java.net.URI;
import java.text.DateFormat;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.TimeZone;
import java.util.UUID;


public class CommentsReceiver {
    private static final String TAG = "feedback";
    private static String userAgent = "";
    private static final String httpServer = "http://client.cheetah-browser.com:8542/comments/";
    //private static final String httpsServer = "http://192.168.43.35:8542/comments/";
    private static final String httpsServer = "http://192.168.0.12:8542/comments/";
    //private static final String httpsServer = "https://client.cheetah-browser.com/comments/";
    private static final int connectionTimeout=2000;

    public interface CommentsCallback {
        void onResponse(HashMap<UUID, Comment> comments);
        void onError(int responseCode, Exception e);
    }

    public static void GetComments(
            boolean useHttps, final URI comment_uri, final CommentsCallback callback) {
        String url = useHttps ? httpsServer : httpServer;
        url = url +"get?url=" + Uri.encode(comment_uri.toString()) +
            "&api_key=" + Uri.encode(GoogleAPIKeys.GOOGLE_API_KEY_CHEETAH) +
            "&version=" + "1";

        JsonObjectHttpRequest.RequestCallback requestCallback =
            new JsonObjectHttpRequest.RequestCallback() {
                @Override
                public void onResponse(JSONObject result) {
                    ThreadUtils.assertOnUiThread();
                    //ArrayList<Comment> comments = new ArrayList<>();
                    HashMap<UUID, Comment> comments = new HashMap<>();
                    JSONArray array = null;
                    try {
                        array = result.optJSONArray("comments");
                        if(array!=null) {
                            for (int i = 0; i < array.length(); i++) {
                                Comment comment = new Comment();

                                JSONObject jsonObject = array.getJSONObject(i);

                                comment.commentId = UUID.fromString(jsonObject.getString("comment_id"));
                                comment.language = jsonObject.getString("language");
                                comment.text = jsonObject.getString("text");
                                comment.localCommentUUID = UUID.fromString(jsonObject.getString("local_comment_id"));

                                JSONObject user = jsonObject.optJSONObject("user");

                                if(user != null) {
                                    comment.userName = user.optString("name");
                                    comment.userPic = user.optString("pic_url");
                                }

                                DateFormat format = new SimpleDateFormat(
                                        "yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", Locale.ENGLISH);
                                format.setTimeZone(TimeZone.getTimeZone("UTC"));
                                comment.timestamp = format.parse(
                                        jsonObject.getString("timestamp"));

                                if (comments.containsKey(comment.localCommentUUID)) {
                                    comment.localCommentUUID = UUID.randomUUID();
                                }

                                comments.put(comment.localCommentUUID, comment);
                            }
                        }
                    } catch (JSONException e) {
                        e.printStackTrace();
                    } catch (ParseException e) {
                        e.printStackTrace();
                    } catch (IllegalArgumentException e) {
                        e.printStackTrace();
                    }

                    callback.onResponse(comments);
                }

                @Override
                public void onError(int responseCode, Exception e) {
                    callback.onError(responseCode, e);
                }
            };

        // Create the request.
        HttpRequest request = null;
        try {
            JSONObject payload = new JSONObject();
            request = new JsonObjectHttpRequest(
                    url, getUserAgent(), "en", payload, requestCallback);
            request.setConnectionTimeout(connectionTimeout);
        } catch (MalformedURLException e) {
            Log.e(TAG, "Error creating HTTP request", e);
            return;
        }
        // The callback will be called on the main thread.
        AsyncTask.THREAD_POOL_EXECUTOR.execute(request);
    }

    public interface PostCallback {
        void onError(int responseCode, Exception e, Comment comment);
        void onSuccess(Comment comment);
    }

    public static void PostComment(
            boolean useHttps, final Comment comment, final String idToken, final PostCallback callback) {
        String url = useHttps ? httpsServer : httpServer;
        url = url +"new?api_key=" + Uri.encode(GoogleAPIKeys.GOOGLE_API_KEY_CHEETAH);

        JsonObjectHttpRequest.RequestCallback requestCallback =
                new JsonObjectHttpRequest.RequestCallback() {
                    @Override
                    public void onResponse(JSONObject result) {
                        if(callback != null) {
                            callback.onSuccess(comment);
                        }
                    }

                    @Override
                    public void onError(int responseCode, Exception e) {
                        if(callback != null) {
                            callback.onError(responseCode, e, comment);
                        }
                    }
                };

        // Create the request.
        HttpRequest request = null;
        try {
            JSONObject payload = new JSONObject();
            try {
                payload.put("version",1);
                payload.put("text", comment.text);
                payload.put("url", comment.uri);
                payload.put("local_comment_id", comment.localCommentUUID);
                payload.put("id_token", idToken);
                payload.put("auth_type", "google_id_token");
            } catch (JSONException e) {
                e.printStackTrace();
            }
            request = new JsonObjectHttpRequest(
                    url, getUserAgent(), "en", payload, requestCallback);
            request.setConnectionTimeout(connectionTimeout);
        } catch (MalformedURLException e) {
            Log.e(TAG, "Error creating HTTP request", e);
            return;
        }
        // The callback will be called on the main thread.
        AsyncTask.THREAD_POOL_EXECUTOR.execute(request);
    }

    private static String getUserAgent() {
        if (userAgent.isEmpty()) {
            userAgent = ContentUtils.getBrowserUserAgent();
        }
        return userAgent;
    }

}
