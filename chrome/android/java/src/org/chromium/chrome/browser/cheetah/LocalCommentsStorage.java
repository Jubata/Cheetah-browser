package org.chromium.chrome.browser.cheetah;

import android.annotation.SuppressLint;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.os.AsyncTask;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.Log;

import org.chromium.base.ContextUtils;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;

/**
 * Created by ivan2kh on 11/17/17.
 */

public class LocalCommentsStorage {
    //Comment state
    public static final int DRAFT =  0b000001;
    public static final int UNSENT = 0b000010;
    public static final int ZOMBIE = 0b000110;

    private DatabaseHelper mDBHelper;

    private static LocalCommentsStorage sInstance;

    public static LocalCommentsStorage get() {
        if (sInstance == null) {
            sInstance = new LocalCommentsStorage();
        }
        return sInstance;
    }

    private LocalCommentsStorage() {
        onCreate(ContextUtils.getApplicationContext());
    }

    private static final class DatabaseHelper extends SQLiteOpenHelper {
        private static final String DATABASE_FILENAME = "localComments.db";
        private static final int DATABASE_VERSION = 1;

        public DatabaseHelper(Context context) {
            super(context, DATABASE_FILENAME, null, DATABASE_VERSION);
        }

        private void createTable(SQLiteDatabase db) {
            db.execSQL("CREATE TABLE comments ("
                    + "id TEXT NOT NULL PRIMARY KEY, "
                    + "text TEXT, "
                    + "user_id TEXT, "
                    + "user TEXT, "
                    + "timestamp INTEGER, "
                    + "url TEXT, "
                    + "state INTEGER);" +
                    "CREATE UNIQUE INDEX comments_id ON comments(id);"+
                    "CREATE INDEX comments_url ON comments(url);");
        }

        private void dropTable(SQLiteDatabase db) {
            db.execSQL("DROP TABLE IF EXISTS comments");
        }

        @Override
        public void onCreate(SQLiteDatabase sqLiteDatabase) {
            synchronized (this) {
                createTable(sqLiteDatabase);
            }
        }

        @Override
        public void onUpgrade(SQLiteDatabase sqLiteDatabase, int i, int i1) {
            dropTable(sqLiteDatabase);
            onCreate(sqLiteDatabase);
        }
    }

    public boolean onCreate(Context context) {
        mDBHelper = new DatabaseHelper(context);
        return true;
    }

    private String simplifyUri(URI uri) {
        String domain = uri.getHost();
        domain =  domain.startsWith("www.") ? domain.substring(4) : domain;
        return domain + "/" + uri.getPath();
    }

    public static Date createDate(long timestamp) {
        Calendar c = Calendar.getInstance();
        c.setTimeInMillis(timestamp);
        return c.getTime();
    }

    private static Comment fromCursor(Cursor cursor) {
        String user = cursor.getString(cursor.getColumnIndex("user"));
        String text = cursor.getString(cursor.getColumnIndex("text"));
        Date date = createDate( cursor.getLong(cursor.getColumnIndex("timestamp")) );
        UUID id = UUID.fromString(
                        cursor.getString(cursor.getColumnIndex("id")) );
        URI uri = null;
        try {
            uri = new URI(cursor.getString(cursor.getColumnIndex("url")));
        } catch (URISyntaxException e) {
            e.printStackTrace();//todo: handle properly
        }
        Comment comment = new Comment(null, null, null, user,
                "", text, date);
        comment.localCommentUUID = id;
        comment.uri = uri;
        return comment;
    }

    public void getUnsentAndZombiesAsync(URI uri, AsyncResult<HashMap<UUID, Comment>> callback) {
        runAsync(() -> {
            final SQLiteDatabase db = mDBHelper.getReadableDatabase();
            String dbUri = simplifyUri(uri);
            Cursor cursor = db.rawQuery(
                    "select * from comments where (state & ? AND url=?)",
                    new String[]{Integer.toString(UNSENT), dbUri});
            HashMap<UUID, Comment> comments = new HashMap<>();
            while (cursor.moveToNext()) {
                Comment comment = fromCursor(cursor);
                comment.localCommentUUID = UUID.randomUUID();
                comments.put(comment.localCommentUUID, comment);
            }
            cursor.close();
            return comments;
        }, comments -> callback.onPostExecute(comments));
    }

    public void getOneUnsentAsync(AsyncResult<Comment> callback) {
        runAsync(() -> {
            final SQLiteDatabase db = mDBHelper.getReadableDatabase();
            Comment comment;
            Cursor cursor = db.rawQuery(
                    "select * from comments where state=? limit 1", new String[]{Integer.toString(UNSENT)});
            if (cursor.getCount() > 0) {
                cursor.moveToNext();
                comment = fromCursor(cursor);
            } else {
                comment = null;
            }
            cursor.close();
            return comment;
        }, comment -> callback.onPostExecute(comment));
    }

    public void insertAsync(String text, URI uri, int state, AsyncResult<Void> callback) {
        runAsync(() -> {
            ContentValues insertValues = new ContentValues();
            insertValues.put("id", UUID.randomUUID().toString());
            insertValues.put("url", simplifyUri(uri));
            insertValues.put("text", text);
            insertValues.put("state", state);
            insertValues.put("timestamp", System.currentTimeMillis());
            final SQLiteDatabase db = mDBHelper.getWritableDatabase();
            db.insert("comments", null, insertValues);
            return null;
        }, (Void v) -> callback.onPostExecute(v));
        //todo: add metrics if failed
    }

    public void getDraftAsync(@NonNull URI uri, AsyncResult<Comment> callback) {
        runAsync(()-> {
            String dbUri = simplifyUri(uri);
            final SQLiteDatabase db = mDBHelper.getReadableDatabase();
            Comment comment = null;
            Cursor cursor = db.rawQuery(
                    "select * from comments where (state=? AND url=?) limit 1",
                    new String[]{Integer.toString(DRAFT), dbUri});
            if (cursor.getCount() > 0) {
                cursor.moveToNext();
                comment = fromCursor(cursor);
            }
            cursor.close();
            return comment;
        }, comment -> callback.onPostExecute(comment));
    }

    public void updateDraftAsync(String text, URI uri) {
        runAsync(() -> {
            try {
                ContentValues insertValues = new ContentValues();
                String dbUri = simplifyUri(uri);
                insertValues.put("id", UUID.nameUUIDFromBytes(dbUri.getBytes("UTF-8")).toString());
                insertValues.put("url", dbUri);
                insertValues.put("text", text);
                insertValues.put("state", DRAFT);
                insertValues.put("timestamp", System.currentTimeMillis());
                final SQLiteDatabase db = mDBHelper.getWritableDatabase();
                db.replace("comments", null, insertValues);
            } catch (UnsupportedEncodingException e) {
                e.printStackTrace();
            }
        });
    }

    public void deleteDraftAsync(URI uri) {
        runAsync(() -> {
            String dbUri = simplifyUri(uri);
            final SQLiteDatabase db = mDBHelper.getReadableDatabase();
            db.delete("comments", "(state = ? AND url = ?)", new String[]
                    {Integer.toString(DRAFT), dbUri});
        });
    }

    public void deleteAsync(UUID id, AsyncResult<Void> callback) {
        runAsync(() -> {
            final SQLiteDatabase db = mDBHelper.getReadableDatabase();
            db.delete("comments", "id = ?", new String[] {id.toString()});
            return null;
        }, (Void v) -> callback.onPostExecute(v));
    }

    public void markAsZombieAsync(UUID id, AsyncResult<Void> callback) {
        runAsync(() -> {
            final SQLiteDatabase db = mDBHelper.getWritableDatabase();
            ContentValues updateValues = new ContentValues();
            updateValues.put("state", ZOMBIE);
            db.update("comments", updateValues, "id = ?", new String[]{id.toString()});
            return null;
        }, (Void v) -> callback.onPostExecute(v));
    }

    @SuppressLint("StaticFieldLeak")
    private void runAsync(AsyncLambda asyncLambda) {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                asyncLambda.doInBackground();
                return null;
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR, (Void)null);
    }

    @SuppressLint("StaticFieldLeak")
    private <E> void runAsync(final GenericAsyncLambda<E> asyncLambda, final AsyncResult<E> callback) {
        new AsyncTask<Void, Void, E>() {
            @Override
            protected E doInBackground(Void... params) {
                return asyncLambda.doInBackground();
            }

            @Override
            protected void onPostExecute(E e) {
                callback.onPostExecute(e);
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR, (Void)null);
    }

    private interface AsyncLambda {
        void doInBackground();
    }

    public interface AsyncResult<E> {
        void onPostExecute(E e);
    }

    private interface GenericAsyncLambda<E> {
        E doInBackground();
    }
}
