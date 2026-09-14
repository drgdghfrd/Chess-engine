package com.kalab.chess.enginesupport;

import android.annotation.TargetApi;
import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;

public class ChessEngineProvider extends ContentProvider {
    private static final String MIME_TYPE = "application/x-chess-engine";
    private static final String TAG = "ChessEngineProvider";

    @Override public boolean onCreate() { return true; }

    @Override public AssetFileDescriptor openAssetFile(Uri uri, String mode) throws FileNotFoundException {
        String fileName = uri.getLastPathSegment();
        if (fileName == null) throw new FileNotFoundException();
        try {
            return getContext().getAssets().openFd(fileName);
        } catch (IOException ignored) {
            File f = new File(getNativeLibraryDir(), fileName);
            try {
                return new AssetFileDescriptor(ParcelFileDescriptor.open(f, ParcelFileDescriptor.MODE_READ_ONLY), 0,
                        AssetFileDescriptor.UNKNOWN_LENGTH);
            } catch (FileNotFoundException e) {
                Log.e(TAG, "Engine not found: " + f, e);
                throw e;
            }
        }
    }

    @TargetApi(Build.VERSION_CODES.GINGERBREAD)
    private String getNativeLibraryDir() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD)
            return getContext().getApplicationInfo().nativeLibraryDir;
        return getContext().getApplicationInfo().dataDir + File.separator + "lib";
    }

    @Override public String getType(Uri uri) { return MIME_TYPE; }
    @Override public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) { throw new UnsupportedOperationException(); }
    @Override public int delete(Uri uri, String selection, String[] selectionArgs) { throw new UnsupportedOperationException(); }
    @Override public Uri insert(Uri uri, ContentValues values) { throw new UnsupportedOperationException(); }
    @Override public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) { throw new UnsupportedOperationException(); }
}
