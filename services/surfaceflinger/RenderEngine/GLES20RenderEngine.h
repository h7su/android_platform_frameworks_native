/*
 * Copyright 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef SF_GLES20RENDERENGINE_H_
#define SF_GLES20RENDERENGINE_H_

#include <stdint.h>
#include <sys/types.h>

#include <GLES2/gl2.h>

#include "RenderEngine.h"
#include "ProgramCache.h"
#include "Description.h"

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

class String8;
class Mesh;
class Texture;

class GLES20RenderEngine : public RenderEngine {
    GLuint mProtectedTexName;
    GLint mMaxViewportDims[2];
    GLint mMaxTextureSize;
    GLuint mVpWidth;
    GLuint mVpHeight;

    /*
     * Key is used to retrieve a Group in the cache.
     * A Key is generated from width and height
     */
    class Key {
        friend class GLES20RenderEngine;
        int mWidth;
        int mHeight;
    public:
        inline Key() : mWidth(0), mHeight(0) { }
        inline Key(int width, int height) :
                              mWidth(width), mHeight(height) { }
        inline Key(const Key& rhs) : mWidth(rhs.mWidth),
                                        mHeight(rhs.mHeight) { }

        friend inline int strictly_order_type(const Key& lhs, const Key& rhs) {
            if (lhs.mWidth != rhs.mWidth)
                return ((lhs.mWidth < rhs.mWidth) ? 1 : 0);

            if (lhs.mHeight != rhs.mHeight)
                return ((lhs.mHeight < rhs.mHeight) ? 1 : 0);

            return 0;
        }
    };

    struct Group {
        GLuint texture;
        GLuint fbo;
        GLuint width;
        GLuint height;
        mat4 colorTransform;
        Group() : width(0), height(0) { }
        bool isValid() { return ((width != 0) && (height != 0)); }
    };

    Description mState;
    Vector<Group> mGroupStack;
    DefaultKeyedVector<Key, Group> mGroupCache;

    virtual void bindImageAsFramebuffer(EGLImageKHR image,
            uint32_t* texName, uint32_t* fbName, uint32_t* status);
    virtual void unbindFramebuffer(uint32_t texName, uint32_t fbName);

public:
    GLES20RenderEngine();

protected:
    virtual ~GLES20RenderEngine();

    virtual void dump(String8& result);
    virtual void setViewportAndProjection(size_t vpw, size_t vph, size_t w, size_t h, bool yswap);
    virtual void setupLayerBlending(bool premultipliedAlpha, bool opaque, int alpha);
    virtual void setupDimLayerBlending(int alpha);
    virtual void setupLayerTexturing(const Texture& texture);
    virtual void setupLayerBlackedOut();
    virtual void setupFillWithColor(float r, float g, float b, float a);
    virtual void disableTexturing();
    virtual void disableBlending();

    virtual void drawMesh(const Mesh& mesh);

    virtual void beginGroup(const mat4& colorTransform);
    virtual void endGroup();
    virtual void getGroup(Group& group);
    virtual void putGroup(Group group);

    virtual size_t getMaxTextureSize() const;
    virtual size_t getMaxViewportDims() const;
};

// ---------------------------------------------------------------------------
}; // namespace android
// ---------------------------------------------------------------------------

#endif /* SF_GLES20RENDERENGINE_H_ */
