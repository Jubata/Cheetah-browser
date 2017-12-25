package org.chromium.chrome.browser.cheetah;

import com.google.android.gms.auth.api.signin.GoogleSignInAccount;

import java.net.URI;
import java.util.Comparator;
import java.util.Date;
import java.util.UUID;

/**
 * Created by ivan2kh on 11/2/17.
 */

public class Comment {
    public UUID commentId = null;
    public UUID localCommentUUID = null;
    public UUID userId = null;
    public  String user = "";
    public String language = "";
    public String text = "";
    public Date timestamp = null;
    public URI uri = null;
    public String userName = "";
    public String userPic = "";

    public void setAccount(GoogleSignInAccount account) {

    }

    public static class DateComparator implements Comparator<Comment> {
        @Override
        public int compare(Comment o1, Comment o2) {
            return o2.timestamp.compareTo(o1.timestamp);
        }
    }
}
