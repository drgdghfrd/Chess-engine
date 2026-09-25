package com.chesszero.app

import android.content.ContentProvider
import android.content.ContentValues
import android.content.res.AssetFileDescriptor
import android.content.res.AssetManager
import android.database.Cursor
import android.net.Uri
import android.os.ParcelFileDescriptor
import java.io.File
import java.io.FileNotFoundException
import java.io.FileOutputStream
import java.io.IOException

/**
 * Minimal Open Exchange (OEX) content provider.
 *
 * Chess GUIs discover this APK via the intent.chess.provider.ENGINE activity
 * marker and read libchesszero.so through content://.../libchesszero.so.
 */
class ChessEngineProvider : ContentProvider() {
    override fun onCreate(): Boolean = true

    override fun getType(uri: Uri): String = "application/x-chess-engine"

    override fun openAssetFile(uri: Uri, mode: String): AssetFileDescriptor {
        if (!mode.startsWith("r")) throw FileNotFoundException("engine is read-only")
        val name = uri.lastPathSegment ?: throw FileNotFoundException("missing engine filename")
        if (name != OEX_FILE) throw FileNotFoundException("unknown engine filename: $name")

        val ctx = context ?: throw FileNotFoundException("provider context unavailable")
        return try {
            // Fast path: the APK build marks .so assets as uncompressed, so
            // Android can return a zero-copy descriptor into the APK.
            ctx.assets.openFd(OEX_FILE)
        } catch (_: IOException) {
            // Compatibility fallback for a GUI/device that receives an APK
            // where the asset is compressed despite the noCompress contract.
            val cached = File(ctx.cacheDir, OEX_FILE)
            if (!cached.exists() || cached.length() == 0L) {
                val temp = File(ctx.cacheDir, "$OEX_FILE.tmp")
                ctx.assets.open(OEX_FILE, AssetManager.ACCESS_STREAMING).use { input ->
                    FileOutputStream(temp).use { output ->
                        input.copyTo(output)
                    }
                }
                if (!temp.renameTo(cached)) {
                    temp.delete()
                    throw FileNotFoundException("failed to cache $OEX_FILE")
                }
            }
            AssetFileDescriptor(
                ParcelFileDescriptor.open(cached, ParcelFileDescriptor.MODE_READ_ONLY),
                0,
                cached.length()
            )
        }
    }

    override fun openFile(uri: Uri, mode: String): ParcelFileDescriptor {
        // Some OEX consumers call openFile/openFileDescriptor instead of
        // openInputStream. Return an independent fd so the temporary
        // AssetFileDescriptor can be closed safely here.
        val afd = openAssetFile(uri, mode)
        return try {
            ParcelFileDescriptor.dup(afd.fileDescriptor)
        } catch (e: IOException) {
            throw FileNotFoundException("failed to duplicate $OEX_FILE: ${e.message}")
        } finally {
            afd.close()
        }
    }

    override fun query(uri: Uri, projection: Array<out String>?, selection: String?,
                       selectionArgs: Array<out String>?, sortOrder: String?): Cursor? =
        throw UnsupportedOperationException("query is not supported")

    override fun insert(uri: Uri, values: ContentValues?): Uri? =
        throw UnsupportedOperationException("insert is not supported")

    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<out String>?): Int =
        throw UnsupportedOperationException("delete is not supported")

    override fun update(uri: Uri, values: ContentValues?, selection: String?,
                        selectionArgs: Array<out String>?): Int =
        throw UnsupportedOperationException("update is not supported")

    companion object {
        const val AUTHORITY = "com.chesszero.app.engine"
        const val OEX_FILE = "libchesszero.so"
    }
}
