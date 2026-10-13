#include "GLTexture.hpp"

namespace g2g {

GLTexture::GLTexture() = default;

GLTexture::~GLTexture() {
    destroy();
}

void GLTexture::init() {
    if (texture_id_ == 0) {
        glGenTextures(1, &texture_id_);
        glBindTexture(GL_TEXTURE_2D, texture_id_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void GLTexture::destroy() {
    if (texture_id_ != 0) {
        glDeleteTextures(1, &texture_id_);
        texture_id_ = 0;
        width_ = 0;
        height_ = 0;
    }
}

void GLTexture::upload(const uint8_t* rgba_data, int width, int height) {
    if (!rgba_data || width <= 0 || height <= 0) return;

    if (texture_id_ == 0) {
        init();
    }

    glBindTexture(GL_TEXTURE_2D, texture_id_);

    // Set 4-byte row alignment for RGBA
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    if (width_ != width || height_ != height) {
        width_ = width;
        height_ = height;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data);
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace g2g
