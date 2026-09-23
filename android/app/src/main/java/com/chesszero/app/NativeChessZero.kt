package com.chesszero.app

import java.io.Closeable

class NativeChessZero : Closeable {
    private var handle: Long = 0L

    init {
        handle = nativeCreate()
        check(handle != 0L) { "ChessZero native engine creation failed" }
    }

    fun setPosition(fen: String): Boolean = nativeSetPosition(handle, fen)
    fun newGame(): Boolean = nativeNewGame(handle)
    fun makeMove(uci: String): Boolean = nativeMakeMove(handle, uci)
    fun undoMove(): Boolean = nativeUndoMove(handle)
    fun currentFen(): String = nativeGetFen(handle)
    fun loadNetwork(path: String): Boolean = nativeLoadNetwork(handle, path)
    fun nnueLoaded(): Boolean = nativeNnueLoaded(handle)
    fun setThreads(threads: Int): Boolean = nativeSetThreads(handle, threads)
    fun setHashMb(mb: Int): Boolean = nativeSetHashMb(handle, mb)
    fun clearHash(): Boolean = nativeClearHash(handle)
    fun search(depth: Int, movetimeMs: Int = 0): String = nativeSearch(handle, depth, movetimeMs)
    fun stop() = nativeStop(handle)
    fun waitIdle() = nativeWaitIdle(handle)
    fun isSearching(): Boolean = nativeIsSearching(handle)
    fun lastError(): String = nativeLastError(handle)

    override fun close() {
        if (handle != 0L) {
            nativeDestroy(handle)
            handle = 0L
        }
    }

    companion object {
        init {
            System.loadLibrary("chesszero_android")
        }

        @JvmStatic private external fun nativeCreate(): Long
        @JvmStatic private external fun nativeDestroy(handle: Long)
        @JvmStatic private external fun nativeSetPosition(handle: Long, fen: String): Boolean
        @JvmStatic private external fun nativeNewGame(handle: Long): Boolean
        @JvmStatic private external fun nativeMakeMove(handle: Long, uci: String): Boolean
        @JvmStatic private external fun nativeUndoMove(handle: Long): Boolean
        @JvmStatic private external fun nativeGetFen(handle: Long): String
        @JvmStatic private external fun nativeLoadNetwork(handle: Long, path: String): Boolean
        @JvmStatic private external fun nativeNnueLoaded(handle: Long): Boolean
        @JvmStatic private external fun nativeSetThreads(handle: Long, threads: Int): Boolean
        @JvmStatic private external fun nativeSetHashMb(handle: Long, mb: Int): Boolean
        @JvmStatic private external fun nativeClearHash(handle: Long): Boolean
        @JvmStatic private external fun nativeSearch(handle: Long, depth: Int, movetimeMs: Int): String
        @JvmStatic private external fun nativeStop(handle: Long)
        @JvmStatic private external fun nativeWaitIdle(handle: Long)
        @JvmStatic private external fun nativeIsSearching(handle: Long): Boolean
        @JvmStatic private external fun nativeLastError(handle: Long): String
    }
}

data class SearchResult(
    val bestMove: String,
    val depth: Int,
    val scoreCp: Int,
    val nodes: Long,
    val pv: String,
)

fun parseSearchResult(raw: String): SearchResult? {
    val p = raw.split("|")
    if (p.size < 10 || p[0] != "bestmove") return null
    fun value(key: String): String? = p.zipWithNext().firstOrNull { it.first == key }?.second
    val best = p.getOrNull(1) ?: return null
    return SearchResult(
        best,
        value("depth")?.toIntOrNull() ?: 0,
        value("score")?.toIntOrNull() ?: 0,
        value("nodes")?.toLongOrNull() ?: 0L,
        value("pv") ?: "",
    )
}
