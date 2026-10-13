#pragma once

#include <cstdint>
#include <GL/gl.h>

namespace g2g {

class GLTexture {
public:
    GLTexture();
    ~GLTexture();

    // Initialize or release texture
    void init();
    void destroy();

    // Upload RGBA frame data to texture
    void upload(const uint8_t* rgba_data, int width, int height);

    unsigned int getTextureId() const { return texture_id_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    bool isValid() const { return texture_id_ != 0 && width_ > 0 && height_ > 0; }

private:
    unsigned int texture_id_{0};
    int width_{0};
    int height_{0};
};

} // namespace g2g
