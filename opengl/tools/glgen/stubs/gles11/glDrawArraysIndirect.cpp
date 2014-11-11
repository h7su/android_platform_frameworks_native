/* void glDrawArraysIndirect ( GLenum mode, const void *indirect ) */
static void android_glDrawArraysIndirect(JNIEnv *_env, jobject, int mode, jlong indirect) {
    // In OpenGL ES, 'indirect' is a byte offset into a buffer, not a raw pointer.
    // GL checks for too-large values. Here we only need to check for successful signed 64-bit
    // to unsigned 32-bit conversion.
    if (sizeof(void*) != sizeof(jlong) && indirect > static_cast<jlong>(UINTPTR_MAX)) {
        jniThrowException(_env, "java/lang/IllegalArgumentException", "indirect offset too large");
        return;
    }
    glDrawArraysIndirect(mode, (const void*)indirect);
}

