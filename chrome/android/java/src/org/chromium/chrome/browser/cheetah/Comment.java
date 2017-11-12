package org.chromium.chrome.browser.cheetah;

import java.util.Date;
import java.util.UUID;

/**
 * Created by ivan2kh on 11/2/17.
 */

public class Comment {
    private final UUID url;
    private final UUID commentId;
    private final UUID userId;
    public final  String user;
    private final String language;
    public final String text;
    private final Date timestamp;

    public Comment(UUID commentId) {
        this.commentId = commentId;
        url=null;
        userId = null;
        language="";
        text="";
        timestamp=null;
        user="";
    }

    public Comment(UUID url, UUID commentId, UUID userId, String user, String language,
                   String text, Date timestamp) {
        this.url = url;
        this.commentId = commentId;
        this.userId = userId;
        this.user = user;
        this.language = language;
        this.text = text;
        this.timestamp = timestamp;
    }
}
