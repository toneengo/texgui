#include "types.h"
#ifndef __gl_h_
#include "glad/gl.h"
#endif
#include "GLFW/glfw3.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include "texgui.h"
#include "texgui_opengl.hpp"
#include "util.h"

#include <filesystem>

#include <glm/gtc/type_ptr.hpp>
#include "shaders/texgui_shaders.hpp"
#include "msdf-atlas-gen/msdf-atlas-gen.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <iostream>

using namespace TexGui;
using namespace TexGui::Math;

constexpr int ATLAS_SIZE = 1024;
std::unordered_map<uint32_t, uint32_t> m_char_map;

GLContext::GLContext()
{
    m_ta.font.bind = 2;
    m_ta.texture.bind = 1;
    glEnable(GL_MULTISAMPLE);

    glCreateBuffers(1, &m_ssb.objects.buf);
    glNamedBufferStorage(m_ssb.objects.buf, (1 << 16) / 16 * sizeof(RenderData::Object), nullptr, GL_DYNAMIC_STORAGE_BIT);
    m_ssb.objects.bind = 0;

    glCreateBuffers(1, &m_ub.objIndex.buf);
    glNamedBufferStorage(m_ub.objIndex.buf, sizeof(int), nullptr, GL_DYNAMIC_STORAGE_BIT);
    m_ub.objIndex.bind = 1;

    glCreateBuffers(1, &m_ub.screen_size.buf);
    glNamedBufferStorage(m_ub.screen_size.buf, sizeof(int) * 2, &m_screen_size, GL_DYNAMIC_STORAGE_BIT);
    m_ub.screen_size.bind = 0;

    glCreateBuffers(1, &m_ub.bounds.buf);
    glNamedBufferStorage(m_ub.bounds.buf, sizeof(Math::fbox), nullptr, GL_DYNAMIC_STORAGE_BIT);
    m_ub.bounds.bind = 2;

    createShader(&m_shaders.text,
                 VERSION_HEADER + BUFFERS + TEXTVERT,
                 VERSION_HEADER + TEXTFRAG);

    createShader(&m_shaders.quad,
                 VERSION_HEADER + BUFFERS + QUADVERT,
                 VERSION_HEADER + QUADFRAG);

    createShader(&m_shaders.quad9slice,
                 VERSION_HEADER + BUFFERS + QUAD9SLICEVERT,
                 VERSION_HEADER + QUADFRAG);
    m_shaders.quad9slice.slices = m_shaders.quad9slice.getLocation("slices");

    createShader(&m_shaders.colquad,
                 VERSION_HEADER + BUFFERS + COLQUADVERT,
                 VERSION_HEADER + BASICFRAG);
}

extern Container Base;
void GLContext::initFromGlfwWindow(GLFWwindow* window)
{
    glfwGetWindowContentScale(window, &m_window_scale, nullptr);
    glfwGetWindowSize(window, &m_screen_size.x, &m_screen_size.y);
    glScissor(0,0,m_screen_size.x, m_screen_size.y);
    m_screen_size.x *= m_window_scale;
    m_screen_size.y *= m_window_scale; 
    Base.bounds.size = m_screen_size;
    glViewport(0, 0, m_screen_size.x, m_screen_size.y);
    glNamedBufferSubData(m_ub.screen_size.buf, 0, sizeof(int) * 2, &m_screen_size);
}

GLContext::~GLContext()
{

}

void GLContext::bindBuffers()
{
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, m_ssb.objects.bind, m_ssb.objects.buf);
    glBindBufferBase(GL_UNIFORM_BUFFER, m_ub.screen_size.bind, m_ub.screen_size.buf);
    glBindBufferBase(GL_UNIFORM_BUFFER, m_ub.objIndex.bind, m_ub.objIndex.buf);
    glBindBufferBase(GL_UNIFORM_BUFFER, m_ub.bounds.bind, m_ub.bounds.buf);
    glBindTextureUnit(m_ta.font.bind, m_ta.font.buf);
    glBindTextureUnit(m_ta.texture.bind, m_ta.texture.buf);
}

void GLContext::setScreenSize(int width, int height)
{
    m_screen_size.x = width;
    m_screen_size.y = height;
    glViewport(0, 0, width, height);
    glNamedBufferSubData(m_ub.screen_size.buf, 0, sizeof(int) * 2, &m_screen_size);
}

extern std::vector<msdf_atlas::GlyphGeometry> glyphs;

float TexGui::computeTextWidth(const char* text, size_t numchars)
{
    float total = 0;
    for (size_t i = 0; i < numchars; i++)
    {
        total += glyphs[m_char_map[text[i]]].getAdvance();
    }
    return total;
}

int RenderData::drawText(const char* text, Math::fvec2 pos, const Math::fvec4& col, int size, uint32_t flags, int32_t len, int32_t zLayer)
{
    auto& objects = zLayer > 0 ? this->objects2 : this->objects;
    auto& commands = zLayer > 0 ? this->commands2 : this->commands;

    size_t numchars = len == -1 ? strlen(text) : len;
    size_t numcopy = numchars;

    const auto& a = glyphs[m_char_map['a']];
    if (flags & CENTER_Y)
    {
        double l, b, r, t;
        a.getQuadPlaneBounds(l, b, r, t);
        pos.y += (size - (t * size)) / 2;
    }

    if (flags & CENTER_X)
    {
        pos.x -= computeTextWidth(text, numchars) * size / 2.0;
    }

    float currx = pos.x;

    //const CharInfo& hyphen = m_char_map['-'];
    for (int idx = 0; idx < numchars; idx++)
    {
        const auto& info = glyphs[m_char_map[text[idx]]];
        /*
        if (flags & WRAPPED && currx + (info.advance + hyphen.advance) * scale > width)
        {
            if (text[idx] != ' ' && (idx > 0 && text[idx - 1] != ' '))
            {
                Character h = {
                    .rect = fbox(
                        currx + hyphen.bearing.x * scale + m_widget_pos.x,
                        pos.y - hyphen.bearing.y * scale + font_height * scale + m_widget_pos.y,
                        hyphen.size.x * scale,
                        hyphen.size.y * scale
                    ),
                    .col = col,
                    .layer = hyphen.layer,
                    .scale = scale,
                };
                objects.push_back(h);
                numcopy++;
            }
            currx = 0;
            pos.y += line_height;
        }
        */
        if (!info.isWhitespace())
        {
            int x, y, w, h;
            info.getBoxRect(x, y, w, h);
            double l, b, r, t;
            info.getQuadPlaneBounds(l, b, r, t);

            Character c = {
                .rect = fbox(
                    currx + l * size,
                    pos.y - b * size,
                    w * size,
                    h * size
                ),
                .texBounds = {x, y, w, h},
                .layer = 0,
                .size = size
            };

            objects.push_back(c);
        }
        else
            numcopy--;
        currx += font_px * info.getAdvance() * size / font_px;
    }

    commands.push_back({Command::CHARACTER, uint32_t(numcopy), flags});
    return currx;
}

#include <stack>
std::stack<fbox> scissorStack;
void RenderData::scissor(Math::fbox bounds)
{
    commands.push_back({Command::SCISSOR, 0, 0, nullptr, bounds});
}

void RenderData::descissor()
{
    commands.push_back({Command::DESCISSOR, 0, 0, nullptr});
}

void RenderData::drawTexture(const fbox& rect, TexEntry* e, int state, int pixel_size, uint32_t flags, const fbox& bounds, int32_t zLayer )
{
    if (bounds.width < 1 || bounds.height < 1) return;

    auto& objects = zLayer > 0 ? this->objects2 : this->objects;
    auto& commands = zLayer > 0 ? this->commands2 : this->commands;

    if (!e) return;
    int layer = 0;
    //#TODO: don't hard code numebrs
    if (getBit(e->has_state, state))
    {
        if (getBit(state, STATE_PRESS) && getBit(e->has_state, STATE_PRESS))
            layer = 2;
        else if (getBit(state, STATE_HOVER) && getBit(e->has_state, STATE_HOVER))
            layer = 1;
        else if (getBit(e->has_state, STATE_ACTIVE))
            layer = 3;
    }

    Quad q = {
        .rect = fbox(rect.pos, rect.size),
        .texBounds = e->bounds,
        .layer = layer,
        .pixelSize = pixel_size
    };

    objects.push_back(q);
    commands.push_back({Command::QUAD, 1, flags, e, bounds});
}

void RenderData::drawQuad(const Math::fbox& rect, const Math::fvec4& col, int32_t zLayer)
{
    auto& objects = zLayer > 0 ? this->objects2 : this->objects;
    auto& commands = zLayer > 0 ? this->commands2 : this->commands;

    ColQuad q = {
        .rect = fbox(rect.pos, rect.size),
        .col = col,
    };

    objects.push_back(q);
    commands.push_back({Command::COLQUAD, 1, 0, nullptr});
}

void GLContext::renderFromRD(RenderData& data) {
    bindBuffers();

    static auto renderBatch = [](GLContext& ctx, auto& objects, auto& commands) {
        glNamedBufferSubData(ctx.m_ssb.objects.buf, 0, sizeof(RenderData::Object) * objects.size(), objects.data());

        int count = 0;
        for (auto& c : commands)
        {
            glNamedBufferSubData(ctx.m_ub.objIndex.buf, 0, sizeof(int), &count);
            switch (c.type)
            {
                
                case RenderData::Command::QUAD:
                {

                    fbox sb = c.scissorBox;
                    sb.x -= ctx.m_screen_size.x / 2.f;
                    sb.y = ctx.m_screen_size.y / 2.f - c.scissorBox.y - c.scissorBox.height;
                    sb.pos /= vec2(ctx.m_screen_size.x/2.f, ctx.m_screen_size.y/2.f);
                    sb.size = c.scissorBox.size / vec2(ctx.m_screen_size.x/2.f, ctx.m_screen_size.y/2.f);

                    glNamedBufferSubData(ctx.m_ub.bounds.buf, 0, sizeof(Math::fbox), &sb);

                    glBindTextureUnit(ctx.m_ta.texture.bind, c.texentry->glID);
                    
                    if (c.flags & SLICE_9)
                    {
                        ctx.m_shaders.quad9slice.use();
                        glUniform4f(ctx.m_shaders.quad9slice.slices,
                                    c.texentry->top, c.texentry->right, c.texentry->bottom, c.texentry->left);
                        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, c.number * 9);
                    }
                    else {
                        ctx.m_shaders.quad.use();
                        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, c.number);
                    }
                    break;
                }
                case RenderData::Command::COLQUAD:
                {
                    ctx.m_shaders.colquad.use();
                    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, c.number);
                    break;
                }
                case RenderData::Command::CHARACTER:
                {
                    ctx.m_shaders.text.use();
                    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, c.number);
                    break;
                }
                default:
                    break;
            }
            count += c.number;
        }
    };

    // Later we can do better Z ordering or something. Idk what the best approach will be without having to sort everything :thinking:
    renderBatch(*this, data.objects, data.commands);
    renderBatch(*this, data.objects2, data.commands2);
}

using namespace msdf_atlas;
std::vector<GlyphGeometry> glyphs;
void GLContext::loadFont(const char* fontFilename)
{
    int width = 0, height = 0;
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_ta.font.buf);
    bool success = false;
    font_px = 32;
    // Initialize instance of FreeType library
    if (msdfgen::FreetypeHandle *ft = msdfgen::initializeFreetype()) {
        // Load font file
        if (msdfgen::FontHandle *font = msdfgen::loadFont(ft, fontFilename)) {
            // Storage for glyph geometry and their coordinates in the atlas
            // FontGeometry is a helper class that loads a set of glyphs from a single font.
            // It can also be used to get additional font metrics, kerning information, etc.
            FontGeometry fontGeometry(&glyphs);
            // Load a set of character glyphs:
            // The second argument can be ignored unless you mix different font sizes in one atlas.
            // In the last argument, you can specify a charset other than ASCII.
            // To load specific glyph indices, use loadGlyphs instead.
            fontGeometry.loadCharset(font, 1.0, Charset::ASCII);

            // Apply MSDF edge coloring. See edge-coloring.h for other coloring strategies.
            const double maxCornerAngle = 3.0;
            for (GlyphGeometry &glyph : glyphs)
                glyph.edgeColoring(&msdfgen::edgeColoringSimple, maxCornerAngle, 0);
            // TightAtlasPacker class computes the layout of the atlas.
            TightAtlasPacker packer;
            // Set atlas parameters:
            // setDimensions or setDimensionsConstraint to find the best value
            packer.setDimensionsConstraint(DimensionsConstraint::SQUARE);
            // setScale for a fixed size or setMinimumScale to use the largest that fits
            packer.setScale(32.0);
            // setPixelRange or setUnitRange
            packer.setPixelRange(TexGui::Defaults::Font::MsdfPxRange);
            packer.setMiterLimit(1.0);
            // Compute atlas layout - pack glyphs
            packer.pack(glyphs.data(), glyphs.size());
            // Get final atlas dimensions
            packer.getDimensions(width, height);
            // The ImmediateAtlasGenerator class facilitates the generation of the atlas bitmap.
            ImmediateAtlasGenerator<
                float, // pixel type of buffer for individual glyphs depends on generator function
                3, // number of atlas color channels
                msdfGenerator, // function to generate bitmaps for individual glyphs
                BitmapAtlasStorage<byte, 3> // class that stores the atlas bitmap
                // For example, a custom atlas storage class that stores it in VRAM can be used.
            > generator(width, height);
            // GeneratorAttributes can be modified to change the generator's default settings.
            GeneratorAttributes attributes;
            generator.setAttributes(attributes);
            generator.setThreadCount(4);
            // Generate atlas bitmap
            generator.generate(glyphs.data(), glyphs.size());
            // The atlas bitmap can now be retrieved via atlasStorage as a BitmapConstRef.
            // The glyphs array (or fontGeometry) contains positioning data for typesetting text.
            msdfgen::BitmapConstRef<byte, 3> ref = generator.atlasStorage();

            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTextureStorage3D(m_ta.font.buf, 1, GL_RGB8, width, height, 1);
            glTextureSubImage3D(m_ta.font.buf, 0, 0, 0, 0, width, height, 1, GL_RGB, GL_UNSIGNED_BYTE, ref.pixels);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

            // Cleanup
            msdfgen::destroyFont(font);
        }
        msdfgen::deinitializeFreetype(ft);
    }

    int i = 0;
    for (auto& glyph : glyphs)
    {
        m_char_map[glyph.getCodepoint()] = i;
        i++;
    }

    glTextureParameteri(m_ta.font.buf, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);	
    glTextureParameteri(m_ta.font.buf, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_ta.font.buf, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_ta.font.buf, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    m_shaders.text.use();
    glUniform1i(m_shaders.text.getLocation("atlasWidth"), width);
    glUniform1i(m_shaders.text.getLocation("atlasHeight"), height);
    glUniform1i(m_shaders.text.getLocation("pxRange"), Defaults::Font::MsdfPxRange);
}

struct TexData
{
    unsigned char* hover;
    unsigned char* press;
};

    
int tc(int x, int y)
{
    return x + (y * ATLAS_SIZE);
}

//#TODO: remove ATLAS_SIZE, determine width and height
std::unordered_map<std::string, TexGui::TexEntry> m_tex_map;
void GLContext::loadTextures(const char* dir)
{
    std::vector<std::filesystem::directory_entry> files;
    for (const auto& f : std::filesystem::recursive_directory_iterator(dir))
    {
        files.push_back(f);
    }

    GLuint texID = -1;
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &texID);
    m_ta.texture.buf = texID;


    for (auto& f : files)
    {
        if (!f.is_regular_file())
        {
            continue;
        }

        std::string pstr = f.path().string();
        std::string fstr = f.path().filename().string();
        fstr.erase(fstr.begin() + fstr.find('.'), fstr.end());

        if (!pstr.ends_with(".png"))
        {
            printf("Unexpected file in sprites folder: %s", pstr.c_str());
            continue;
        }

        int width, height, channels;
        unsigned char* im = stbi_load(pstr.c_str(), &width, &height, &channels, 4);
        if (im == nullptr)
        {
            std::cout << "Failed to load file: " << pstr << "\n";
            continue;
        }
        

        if (m_tex_map.contains(fstr))
        {
            if (m_tex_map[fstr].bounds.w != width || m_tex_map[fstr].bounds.h != height)
            {
                printf("Texture variant dimension mismatch: %s", pstr.c_str());
                continue;
            }
        }
        else
        {
            TexEntry& t = m_tex_map[fstr];
            t.bounds.w = width; 
            t.bounds.h = height; 

            t.top = height/3.f;
            t.right = width/3.f;
            t.bottom = height/3.f;
            t.left = width/3.f;
            t.glID = texID;
        }

        if (pstr.ends_with(".hover.png"))
        {
            m_tex_map[fstr]._hover = im;
            m_tex_map[fstr].has_state |= STATE_HOVER;
        }
        else if (pstr.ends_with(".press.png"))
        {
            m_tex_map[fstr]._press = im;
            m_tex_map[fstr].has_state |= STATE_PRESS;
        }
        else if (pstr.ends_with(".active.png"))
        {
            m_tex_map[fstr]._active = im;
            m_tex_map[fstr].has_state |= STATE_ACTIVE;
        }
        else
        {
            m_tex_map[fstr].data = im;
        }

    }

    // Do the packing
    stbrp_node* nodes = new stbrp_node[4096];
    stbrp_context ctx;
    stbrp_init_target(&ctx, ATLAS_SIZE, ATLAS_SIZE, nodes, 4096);
    stbrp_rect* boxes = new stbrp_rect[m_tex_map.size()];

    int i = 0;
    for (auto& e : m_tex_map)
    {
        boxes[i] = {
            i, e.second.bounds.w, e.second.bounds.h
        };
        i++;
    }
    if (!stbrp_pack_rects(&ctx, boxes, m_tex_map.size()))
    {
        std::cout << "Failed to pack all textures.\n";
        exit(1);
    }

    // Write all the textures into a big ol atlas
    unsigned char* atlas = new unsigned char[ATLAS_SIZE * ATLAS_SIZE * 4];
    unsigned char* hover = new unsigned char[ATLAS_SIZE * ATLAS_SIZE * 4];
    unsigned char* press = new unsigned char[ATLAS_SIZE * ATLAS_SIZE * 4];
    unsigned char* active = new unsigned char[ATLAS_SIZE * ATLAS_SIZE * 4];
    memset(atlas, 0, ATLAS_SIZE * ATLAS_SIZE * 4);
    memset(hover, 0, ATLAS_SIZE * ATLAS_SIZE * 4);
    memset(press, 0, ATLAS_SIZE * ATLAS_SIZE * 4);
    memset(active, 0, ATLAS_SIZE * ATLAS_SIZE * 4);

    i = 0;
    for (auto& p : m_tex_map)
    {
        TexEntry& e = p.second;
        stbrp_rect& rect = boxes[i++];
        // Copy the sprite data into the big chunga
        // Scanline copy
        if (e.data == nullptr)
        {
            continue;
        }

        e.bounds.x = rect.x;
        e.bounds.y = rect.y + rect.h;

        for (int row = 0; row < e.bounds.h; row++)
            memcpy(atlas     + 4 * tc(rect.x, rect.y + row), e.data     + (4 * e.bounds.w * row), e.bounds.w * 4);

        if (e._hover != nullptr)
            for (int row = 0; row < e.bounds.h; row++)
                memcpy(hover + 4 * tc(rect.x, rect.y + row), e._hover + (4 * e.bounds.w * row), e.bounds.w * 4);

        if (e._press != nullptr)
            for (int row = 0; row < e.bounds.h; row++)
                memcpy(press + 4 * tc(rect.x, rect.y + row), e._press + (4 * e.bounds.w * row), e.bounds.w * 4);

        if (e._active != nullptr)
            for (int row = 0; row < e.bounds.h; row++)
                memcpy(active + 4 * tc(rect.x, rect.y + row), e._active + (4 * e.bounds.w * row), e.bounds.w * 4);
    }

    glTextureStorage3D(m_ta.texture.buf, 1, GL_RGBA8, ATLAS_SIZE, ATLAS_SIZE, 4);

    glTextureSubImage3D(m_ta.texture.buf, 0, 0, 0, 0, ATLAS_SIZE, ATLAS_SIZE, 1, GL_RGBA, GL_UNSIGNED_BYTE, atlas);
    glTextureSubImage3D(m_ta.texture.buf, 0, 0, 0, 1, ATLAS_SIZE, ATLAS_SIZE, 1, GL_RGBA, GL_UNSIGNED_BYTE, hover);
    glTextureSubImage3D(m_ta.texture.buf, 0, 0, 0, 2, ATLAS_SIZE, ATLAS_SIZE, 1, GL_RGBA, GL_UNSIGNED_BYTE, press);
    glTextureSubImage3D(m_ta.texture.buf, 0, 0, 0, 3, ATLAS_SIZE, ATLAS_SIZE, 1, GL_RGBA, GL_UNSIGNED_BYTE, active);

    delete [] atlas;
    delete [] hover;
    delete [] press;
    delete [] active;

    glTextureParameteri(m_ta.texture.buf, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);	
    glTextureParameteri(m_ta.texture.buf, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_ta.texture.buf, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(m_ta.texture.buf, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


    for (auto& e : m_tex_map)
    {
        stbi_image_free(e.second.data);
        stbi_image_free(e.second._hover);
        stbi_image_free(e.second._press);
        stbi_image_free(e.second._active);
    }
}

IconSheet GLContext::loadIcons(const char* path, int32_t iconWidth, int32_t iconHeight)
{
    int width, height, channels;
    unsigned char* im = stbi_load(path, &width, &height, &channels, 4);
    
    GLuint texID = -1;
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &texID);
    glTextureStorage3D(texID, 1, GL_RGBA8, ATLAS_SIZE, ATLAS_SIZE, 1);
    glTextureSubImage3D(texID, 0, 0, 0, 0, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, im);

    glTextureParameteri(texID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);	
    glTextureParameteri(texID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(texID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    stbi_image_free(im);

    return { texID, iconWidth, iconHeight, width, height };
}

void Shader::use()
{
    glUseProgram(id);
}

uint32_t Shader::getLocation(const char * uniform_name)
{
    return shgets(uniforms, uniform_name).value.location;
}

void TexGui::createShader(Shader* shader, const std::string& vertexShader, const std::string& fragmentShader)
{
    sh_new_arena(shader->uniforms);

    GLuint vsh = glCreateShader(GL_VERTEX_SHADER);
    GLuint fsh = glCreateShader(GL_FRAGMENT_SHADER);

    auto v_shaderSource = vertexShader.c_str();
    auto f_shaderSource = fragmentShader.c_str();

    glShaderSource(vsh, 1, (const GLchar**)&v_shaderSource, NULL);
    glShaderSource(fsh, 1, (const GLchar**)&f_shaderSource, NULL);

    int logLength;
    glCompileShader(vsh);
    glGetShaderiv(vsh, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        char* vsherr = new char[logLength + 1];
        glGetShaderInfoLog(vsh, logLength, NULL, vsherr);
        printf("\n\nIn %s:\n%s\n", v_shaderSource, vsherr);
        delete[] vsherr;
        exit(1);
    }

    logLength = 0;
    glCompileShader(fsh);
    glGetShaderiv(fsh, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        char* fsherr = new char[logLength + 1];
        glGetShaderInfoLog(fsh, logLength, NULL, fsherr);
        printf("\n\nIn %s:\n%s\n", f_shaderSource, fsherr);
        delete[] fsherr;
        exit(1);
    }

    glCompileShader(vsh);
    glCompileShader(fsh);
    shader->id = glCreateProgram();

    glAttachShader(shader->id, vsh);
    glAttachShader(shader->id, fsh);
    glLinkProgram(shader->id);

    int success = GL_FALSE;
    glGetProgramiv(shader->id, GL_LINK_STATUS, &success);
    if (!success)
    {
        int logLength = 0;
        glGetProgramiv(shader->id, GL_INFO_LOG_LENGTH, &logLength);
        char* err = new char[logLength + 1];
        glGetProgramInfoLog(shader->id, 512, NULL, err);
        printf("Error compiling program:\n%s\n", err);
        delete[] err;
        exit(1);
    }

    // Shaders have been linked into the program, we don't need them anymore
    glDeleteShader(vsh);
    glDeleteShader(fsh);

    GLint uniform_count;
    GLchar * uniform_name;
    glGetProgramiv(shader->id, GL_ACTIVE_UNIFORMS, &uniform_count);

    if (uniform_count > 0)
    {
        GLint   max_name_len;
        GLsizei length = 0;
        GLsizei count = 0;
        GLenum  type = GL_NONE;

        glGetProgramiv(shader->id, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_name_len);

        uniform_name = new char[max_name_len];

        for (GLint i = 0; i < uniform_count; i++)
        {
            glGetActiveUniform(shader->id, i, max_name_len, &length, &count, &type, uniform_name);
            uniformInfo::Value info{
                glGetUniformLocation(shader->id, uniform_name),
                uint32_t(count)
            };
            shput(shader->uniforms, uniform_name, info);
        }
        delete[] uniform_name;
    }
}
