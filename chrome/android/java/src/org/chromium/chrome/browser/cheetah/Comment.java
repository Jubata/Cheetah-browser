package org.chromium.chrome.browser.cheetah;

import java.net.URI;
import java.util.Comparator;
import java.util.Date;
import java.util.UUID;

/**
 * Created by ivan2kh on 11/2/17.
 */

public class Comment {
    private final UUID urlUUID;
    private final UUID commentId;
    public UUID localCommentUUID;
    private final UUID userId;
    public final  String user;
    private final String language;
    public String text;
    public final Date timestamp;
    public URI uri;

    public Comment() {
        commentId = null;
        urlUUID=null;
        userId = null;
        language="";
        text="";
        timestamp=null;
        user="";
    }

    public Comment(UUID commentId) {
        this.commentId = commentId;
        urlUUID=null;
        userId = null;
        language="";
        text="";
        timestamp=null;
        user="";
    }

    public Comment(UUID url, UUID commentId, UUID userId, String user, String language,
                   String text, Date timestamp) {
        this.urlUUID = url;
        this.commentId = commentId;
        this.userId = userId;
        this.user = user;
        this.language = language;
        this.text = text;
        this.timestamp = timestamp;
    }

    public static class DateComparator implements Comparator<Comment> {
        public int compare(Comment o1, Comment o2) {
            return o2.timestamp.compareTo(o1.timestamp);
        }
    }
}
