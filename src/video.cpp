#include <cstddef>
#include <limits.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include <GL/glew.h>
#include <SDL_image.h>
#include "types.h"
#include "utils.h"
#include "config.h"
#include "device.h"
#include "memory.h"
#include "ula.h"
#include "video.h"

namespace Video {
    int viewport_width = -1;
    int viewport_height = -1;
    GLuint screen_texture = 0;
    GLuint pbo = 0;

    void setup(){
        glEnable(GL_TEXTURE_2D);
        glGenTextures(1, &screen_texture);
        glBindTexture(GL_TEXTURE_2D, screen_texture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, NULL);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        glGenBuffers(1, &pbo);
    }

    u16* update(){
        glBindTexture(GL_TEXTURE_2D, screen_texture);
        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, viewport_height);
            glTexCoord2f(1.0f, 1.0f); glVertex2f(viewport_width, viewport_height);
            glTexCoord2f(1.0f, 0.0f); glVertex2f(viewport_width, 0.0f);
            glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
        glEnd();
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(GLushort), NULL, GL_STATIC_DRAW);
        return (u16*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    }

    void frame(){
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, NULL);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    void set_filter(Filter filter){
        glBindTexture(GL_TEXTURE_2D, screen_texture);
        switch (filter){
            case Nearest:
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                break;
            case Linear:
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                break;
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void viewport_setup(int width, int height){
        glViewport(0, 0, (GLsizei)width, (GLsizei)height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, width, height, 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        viewport_width = width;
        viewport_height = height;
    }

    void free(){
        if (screen_texture)
            glDeleteTextures(1, &screen_texture);
        if (pbo)
            glDeleteBuffers(1, &pbo);
        screen_texture = pbo = 0;
    }
}
