#pragma once

using GLenum = unsigned int;
using GLbitfield = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLchar = char;
using GLfloat = float;
using GLboolean = unsigned char;
using EGLBoolean = unsigned int;

namespace rh::gl {

// Targets, queries and parameters (values from the OpenGL ES 3.2 and EGL specifications).
inline constexpr GLenum kTexture2D = 0x0DE1;
inline constexpr GLenum kTexture0 = 0x84C0;
inline constexpr GLenum kDrawFramebuffer = 0x8CA9;
inline constexpr GLenum kReadFramebuffer = 0x8CA8;
inline constexpr GLenum kTriangles = 0x0004;
inline constexpr GLenum kColorBufferBit = 0x4000;
inline constexpr GLenum kVertexShader = 0x8B31;
inline constexpr GLenum kFragmentShader = 0x8B30;
inline constexpr GLenum kRgba8 = 0x8058;

inline constexpr GLenum kCompileStatus = 0x8B81;
inline constexpr GLenum kLinkStatus = 0x8B82;

inline constexpr GLenum kTextureMinFilter = 0x2801;
inline constexpr GLenum kTextureMagFilter = 0x2800;
inline constexpr GLenum kTextureWrapS = 0x2802;
inline constexpr GLenum kTextureWrapT = 0x2803;
inline constexpr GLint kNearest = 0x2600;
inline constexpr GLint kLinear = 0x2601;
inline constexpr GLint kClampToEdge = 0x812F;

// glGet* state queries
inline constexpr GLenum kCurrentProgram = 0x8B8D;
inline constexpr GLenum kActiveTexture = 0x84E0;
inline constexpr GLenum kTextureBinding2D = 0x8069;
inline constexpr GLenum kVertexArrayBinding = 0x85B5;
inline constexpr GLenum kDrawFramebufferBinding = 0x8CA6;
inline constexpr GLenum kReadFramebufferBinding = 0x8CAA;
inline constexpr GLenum kViewport = 0x0BA2;
inline constexpr GLenum kColorWritemask = 0x0C23;

// Capabilities (glEnable / glDisable / glIsEnabled)
inline constexpr GLenum kScissorTest = 0x0C11;
inline constexpr GLenum kBlend = 0x0BE2;
inline constexpr GLenum kDepthTest = 0x0B71;
inline constexpr GLenum kCullFace = 0x0B44;
inline constexpr GLenum kStencilTest = 0x0B90;

// EGL surface attributes
inline constexpr int kEglHeight = 0x3056;
inline constexpr int kEglWidth = 0x3057;

}  // namespace rh::gl

extern "C" {

// ---- hooked by this project ------------------------------------------------------------------
void glShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
void glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                       GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                       GLbitfield mask, GLenum filter);
EGLBoolean eglSwapBuffers(void* dpy, void* surface);

// ---- called by this project ------------------------------------------------------------------
EGLBoolean eglQuerySurface(void* dpy, void* surface, int attribute, int* value);

// shaders and programs
GLuint glCreateShader(GLenum type);
void glCompileShader(GLuint shader);
void glDeleteShader(GLuint shader);
void glGetShaderiv(GLuint shader, GLenum pname, GLint* params);
void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
GLuint glCreateProgram(void);
void glAttachShader(GLuint program, GLuint shader);
void glLinkProgram(GLuint program);
void glGetProgramiv(GLuint program, GLenum pname, GLint* params);
void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
void glUseProgram(GLuint program);
GLint glGetUniformLocation(GLuint program, const GLchar* name);
void glUniform1f(GLint location, GLfloat v0);
void glUniform2f(GLint location, GLfloat v0, GLfloat v1);

// textures, vertex arrays, framebuffers, drawing
void glGenTextures(GLsizei n, GLuint* textures);
void glDeleteTextures(GLsizei n, const GLuint* textures);
void glBindTexture(GLenum target, GLuint texture);
void glActiveTexture(GLenum texture);
void glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
void glTexParameteri(GLenum target, GLenum pname, GLint param);
void glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                         GLint x, GLint y, GLsizei width, GLsizei height);
void glGenVertexArrays(GLsizei n, GLuint* arrays);
void glBindVertexArray(GLuint array);
void glBindFramebuffer(GLenum target, GLuint framebuffer);
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void glDrawArrays(GLenum mode, GLint first, GLsizei count);

// state queries and switches (used to save and restore the game's state)
void glGetIntegerv(GLenum pname, GLint* data);
void glGetBooleanv(GLenum pname, GLboolean* data);
GLboolean glIsEnabled(GLenum cap);
void glEnable(GLenum cap);
void glDisable(GLenum cap);
void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
GLenum glGetError(void);

}  // extern "C"
