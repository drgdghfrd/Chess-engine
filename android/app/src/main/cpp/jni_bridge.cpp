#include <jni.h>
#include <cstddef>
#include <cstdint>
#include <string>

#include "EngineAPI.h"

namespace {

CZEngine* getEngine(jlong handle) {
    return reinterpret_cast<CZEngine*>(static_cast<std::intptr_t>(handle));
}

jstring toJString(JNIEnv* env, const char* value) {
    return env->NewStringUTF(value ? value : "");
}

}

extern "C" JNIEXPORT jlong JNICALL
Java_com_chesszero_app_NativeChessZero_nativeCreate(JNIEnv*, jclass) {
    return static_cast<jlong>(reinterpret_cast<std::intptr_t>(cz_engine_create()));
}

extern "C" JNIEXPORT void JNICALL
Java_com_chesszero_app_NativeChessZero_nativeDestroy(JNIEnv*, jclass, jlong handle) {
    cz_engine_destroy(getEngine(handle));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_chesszero_app_NativeChessZero_nativeSetPosition(JNIEnv* env, jclass,
                                                          jlong handle, jstring jFen) {
    if (!jFen) return JNI_FALSE;
    const char* fen = env->GetStringUTFChars(jFen, nullptr);
    if (!fen) return JNI_FALSE;
    const int ok = cz_engine_set_position_fen(getEngine(handle), fen);
    env->ReleaseStringUTFChars(jFen, fen);
    return ok ? JNI_TRUE : JNI_FALSE;
}


extern "C" JNIEXPORT jboolean JNICALL
Java_com_chesszero_app_NativeChessZero_nativeNewGame(JNIEnv*, jclass, jlong handle) {
    return cz_engine_new_game(getEngine(handle)) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_chesszero_app_NativeChessZero_nativeMakeMove(JNIEnv* env, jclass,
                                                       jlong handle, jstring jMove) {
    if (!jMove) return JNI_FALSE;
    const char* move = env->GetStringUTFChars(jMove, nullptr);
    if (!move) return JNI_FALSE;
    const int ok = cz_engine_make_move_uci(getEngine(handle), move);
    env->ReleaseStringUTFChars(jMove, move);
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_chesszero_app_NativeChessZero_nativeUndoMove(JNIEnv*, jclass, jlong handle) {
    return cz_engine_undo_move(getEngine(handle)) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_chesszero_app_NativeChessZero_nativeGetFen(JNIEnv* env, jclass, jlong handle) {
    char fen[256]{};
    cz_engine_get_fen(getEngine(handle), fen, sizeof(fen));
    return toJString(env, fen);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_chesszero_app_NativeChessZero_nativeLoadNetwork(JNIEnv* env, jclass,
                                                          jlong handle, jstring jPath) {
    if (!jPath) return JNI_FALSE;
    const char* path = env->GetStringUTFChars(jPath, nullptr);
    if (!path) return JNI_FALSE;
    const int ok = cz_engine_load_network(getEngine(handle), path);
    env->ReleaseStringUTFChars(jPath, path);
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_chesszero_app_NativeChessZero_nativeNnueLoaded(JNIEnv*, jclass, jlong handle) {
    return cz_engine_nnue_loaded(getEngine(handle)) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_chesszero_app_NativeChessZero_nativeSetThreads(JNIEnv*, jclass,
                                                         jlong handle, jint threads) {
    return cz_engine_set_threads(getEngine(handle), threads) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_chesszero_app_NativeChessZero_nativeSetHashMb(JNIEnv*, jclass,
                                                        jlong handle, jint mb) {
    return cz_engine_set_hash_mb(getEngine(handle), static_cast<std::size_t>(mb)) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_chesszero_app_NativeChessZero_nativeClearHash(JNIEnv*, jclass, jlong handle) {
    return cz_engine_clear_hash(getEngine(handle)) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_chesszero_app_NativeChessZero_nativeSearch(JNIEnv* env, jclass,
                                                     jlong handle, jint depth, jint movetimeMs) {
    char result[4096]{};
    cz_engine_search(getEngine(handle), depth, movetimeMs, result, sizeof(result));
    return toJString(env, result);
}

extern "C" JNIEXPORT void JNICALL
Java_com_chesszero_app_NativeChessZero_nativeWaitIdle(JNIEnv*, jclass, jlong handle) {
    cz_engine_wait_idle(getEngine(handle));
}

extern "C" JNIEXPORT void JNICALL
Java_com_chesszero_app_NativeChessZero_nativeStop(JNIEnv*, jclass, jlong handle) {
    cz_engine_stop(getEngine(handle));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_chesszero_app_NativeChessZero_nativeIsSearching(JNIEnv*, jclass, jlong handle) {
    return cz_engine_is_searching(getEngine(handle)) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_chesszero_app_NativeChessZero_nativeLastError(JNIEnv* env, jclass, jlong handle) {
    return toJString(env, cz_engine_last_error(getEngine(handle)));
}
