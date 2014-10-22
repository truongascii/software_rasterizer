#include "camera.h"

bool CPUHasAVX()
{
    bool avxSupported = false;

    int cpuInfo[4];
    __cpuid(cpuInfo, 1);

    bool osUsesXSAVE_XRSTORE = cpuInfo[2] & (1 << 27) || false;
    bool cpuAVXSuport = cpuInfo[2] & (1 << 28) || false;

    if (osUsesXSAVE_XRSTORE && cpuAVXSuport) {
        avxSupported = (_xgetbv(_XCR_XFEATURE_ENABLED_MASK) & 0x6) || false;
    }

    return avxSupported;
}

struct Vertex {
    Vector4 position;
    float u, v;
};

struct Texture {
    Texture() : width(0), height(0), pixels(nullptr) {}
    ~Texture() { _aligned_free(pixels); }

    bool Load(const char *fileName)
    {
        ILuint image;
        ilGenImages(1, &image);
        ilBindImage(image);

        if (!ilLoadImage(fileName)) {
            fprintf(stderr, "Failed to load image: %s", fileName);
            return false;
        }

        if (!ilConvertImage(IL_BGRA, IL_UNSIGNED_BYTE)) {
            fprintf(stderr, "Failed to convert image %s to RGBA GL_UNSIGNED_BYTE", fileName);
            return false;
        }

        width = ilGetInteger(IL_IMAGE_WIDTH);
        height = ilGetInteger(IL_IMAGE_HEIGHT);
        pixels = (uint32_t *)_aligned_malloc(width * height * sizeof(uint32_t), 16);

        int format = ilGetInteger(IL_IMAGE_FORMAT);
        int type = ilGetInteger(IL_IMAGE_TYPE);

        if (ilCopyPixels(0, 0, 0, width, height, 1, format, type, pixels) == IL_FALSE) {
            fprintf(stderr, "Failed to copy pixels from %s", fileName);
            ilDeleteImage(image);
            delete [] pixels;
        }

        ilBindImage(0);
        ilDeleteImage(image);

        return true;
    }

    int width;
    int height;

    uint32_t *pixels;
};

struct Geometry {
    Geometry() : vertices(nullptr), numVertices(0), indices(nullptr), numIndices(0) {}
    
    ~Geometry()
    {
        _aligned_free(vertices);
        _aligned_free(indices);
    }

    bool Load(aiMesh *mesh, aiMaterial *material)
    {
        numVertices = mesh->mNumVertices;
        vertices = (Vertex *)_aligned_malloc(numVertices * sizeof(Vertex), 16);
        numIndices = 3 * mesh->mNumFaces;
        indices = (uint16_t *)_aligned_malloc(numIndices * sizeof(uint16_t), 16);

        Vertex *vertex = vertices;
        aiVector3D *position = mesh->mVertices;
        aiVector3D *texcoords = mesh->mTextureCoords[0];

        for (int i = 0; i < numVertices; ++i) {
            aiVector3D p = *position++;
            vertex->position = Vector4(p.x, p.y, p.z, 1.0f);

            aiVector3D t = *texcoords++;
            vertex->u = t.x;
            vertex->v = t.y;

            ++vertex;
        }

        uint16_t *index = indices;
        aiFace *face = mesh->mFaces;

        for (size_t i = 0; i < mesh->mNumFaces; ++i) {
            *index++ = face->mIndices[0];
            *index++ = face->mIndices[1];
            *index++ = face->mIndices[2];

            ++face;
        }

        aiString path;

        if (material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
            if (!texture.Load(path.C_Str()))
                return false;
        }

        return true;
    }

    Vertex *vertices;
    int numVertices;
    uint16_t *indices;
    int numIndices;
    Texture texture;
};

struct World {
    World() : geometries(nullptr), numGeometries(0) {}
    ~World() { delete [] geometries; }

    bool Load(const char *fileName)
    {
        Assimp::Importer importer;

        aiPropertyStore* properties = aiCreatePropertyStore();
        aiSetImportPropertyInteger(properties, AI_CONFIG_IMPORT_TER_MAKE_UVS, 1);
        aiSetImportPropertyFloat(properties, AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.0f);
        aiSetImportPropertyInteger(properties, AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);

        const aiScene *loadedScene = aiImportFileExWithProperties(fileName, aiProcessPreset_TargetRealtime_MaxQuality, nullptr, properties);

        aiReleasePropertyStore(properties);

        if (loadedScene == nullptr) {
            fprintf(stderr, "Failed to load scene %s: %s", fileName, importer.GetErrorString());
            return false;
        }

        numGeometries = loadedScene->mNumMeshes;
        geometries = new Geometry[numGeometries];

        int totalTriangles = 0;

        for (int i = 0; i < numGeometries; ++i) {
            aiMesh *mesh = loadedScene->mMeshes[i];
            if (!geometries[i].Load(mesh, loadedScene->mMaterials[mesh->mMaterialIndex]))
                return false;
            totalTriangles += 3 * mesh->mNumFaces;
        }

        printf("Total triangles: %d\n", totalTriangles);

        return true;
    }

    Geometry *geometries;
    int numGeometries;
};

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *backBuffer;

unsigned int *colorBuffer;
float *depthBuffer;

int width = 1920;
int height = 1080;

bool Init(bool fullscreen)
{
    ilInit();

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
        return false;

    uint32_t windowFlags = SDL_WINDOW_SHOWN;

    if (fullscreen)
        windowFlags |= SDL_WINDOW_FULLSCREEN;

    window = SDL_CreateWindow("Software Renderer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, windowFlags);
    if (window == nullptr)
        return false;
    SDL_Surface *surface = SDL_GetWindowSurface(window);
    if (surface == nullptr)
        return false;
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (renderer == nullptr)
        return false;
    backBuffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (backBuffer == nullptr)
        return false;

    depthBuffer = (float *)_aligned_malloc(width * height * sizeof(float), 16);

    SDL_ShowCursor(false);
    SDL_SetWindowGrab(window, SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_TRUE);

    return true;
}

void Close()
{
    _aligned_free(depthBuffer);

    SDL_DestroyTexture(backBuffer);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
}

void Rasterize(Geometry *geometry, Matrix4 &worldMatrix, Camera &camera)
{
    Matrix4 worldViewProjection = camera.ViewProjetionMatrix() * worldMatrix;

    Texture *texture = &geometry->texture;
    Vertex *triangles = geometry->vertices;
    uint16_t *indices = geometry->indices;

    int numTriangles = geometry->numIndices / 3;

    for (int i = 0; i < numTriangles; ++i) {
        int i0 = *indices++;
        int i1 = *indices++;
        int i2 = *indices++;

        Vertex v0 = triangles[i0];
        Vertex v1 = triangles[i1];
        Vertex v2 = triangles[i2];

        Vector4 p0 = worldViewProjection * v0.position;
        Vector4 p1 = worldViewProjection * v1.position;
        Vector4 p2 = worldViewProjection * v2.position;
       
        if ((p1.getX() - p0.getX()) * (p2.getY() - p0.getY()) - (p2.getX() - p0.getX()) * (p1.getY() - p0.getY()) > 0.15)
            continue;

        float v0z = 1.0f / p0.getZ();
        float v1z = 1.0f / p1.getZ();
        float v2z = 1.0f / p2.getZ();

        float v0w = 1.0f / p0.getW();
        float v1w = 1.0f / p1.getW();
        float v2w = 1.0f / p2.getW();

        p0 = divPerElem(p0, Vector4(p0.getW() / width, p0.getZ() / height, 1.0f, 1.0f));
        p1 = divPerElem(p1, Vector4(p1.getW() / width, p1.getZ() / height, 1.0f, 1.0f));
        p2 = divPerElem(p2, Vector4(p2.getW() / width, p2.getZ() / height, 1.0f, 1.0f));

        Vector4 min = minPerElem(p0, minPerElem(p1, p2));
        Vector4 max = maxPerElem(p0, maxPerElem(p1, p2));

        int minx = std::max(0, (int) floor(min.getX()));
        int maxx = std::min(width, (int) ceil(max.getX()));
        int miny = std::max(0, (int) floor(min.getY()));
        int maxy = std::min(height, (int) ceil(max.getY()));

        float uz0 = v0.u * v0w;
        float uz1 = v1.u * v1w;
        float uz2 = v2.u * v2w;

        float vz0 = v0.v * v0w;
        float vz1 = v1.v * v1w;
        float vz2 = v2.v * v2w;

        float ax = p2.getX() - p1.getX();
        float ay = p1.getY() - p2.getY();

        float bx = p0.getX() - p2.getX();
        float by = p2.getY() - p0.getY();

        float b = 1.0f / (ay * bx + ax * (p0.getY() - p2.getY()));

        unsigned int *pixels = colorBuffer + miny * width;

        float *depth = depthBuffer + miny * width;
        
        int umax = texture->width;
        int vmax = texture->height;
        
        unsigned int *texturePixels = texture->pixels;

        for (int y = miny; y < maxy; ++y) {
            float cy = y - p2.getY();

            for (int x = minx; x < maxx; ++x) {
                float cx = x - p2.getX();

                float l0 = b * (ay * cx + ax * cy);
                float l1 = b * (by * cx + bx * cy);
                float l2 = 1.0f - l0 - l1;

                if (l0 > 0.0f && l1 > 0.0f && l2 > 0.0f) {
                    float z = l0 * v0z + l1 * v1z + l2 * v2z;

                    if (z > depth[x]) {
                        depth[x] = z;

                        float w = 1.0f / (l0 * v0w + l1 * v1w + l2 * v2w);

                        int u = int(umax * (l0 * uz0 + l1 * uz1 + l2 * uz2) * w);
                        int v = int(vmax * (l0 * vz0 + l1 * vz1 + l2 * vz2) * w);

                        pixels[x] = texturePixels[v * umax + u];
                    }
                }
            }

            pixels += width;
            depth += width;
        }
    }
}

void Rasterize(Matrix4 &worldMatrix, Geometry *geometries, int numGeometries, Camera &camera)
{
    memset(depthBuffer, 0x0, width * height * sizeof(float));

    int pitch = 0;
    SDL_LockTexture(backBuffer, nullptr, (void **)&colorBuffer, &pitch);

    memset(colorBuffer, 0x0, width * height * sizeof(unsigned int));

    for (int i = 0; i < numGeometries; ++i)
        Rasterize(&geometries[i], worldMatrix, camera);

    SDL_UnlockTexture(backBuffer);
    SDL_RenderCopy(renderer, backBuffer, nullptr, nullptr);
    SDL_RenderPresent(renderer);
    SDL_UpdateWindowSurface(window);
}

int main(int argc, char **argv)
{
    if (!CPUHasAVX()) {
        fprintf(stderr, "CPU without AVX instructions, quitting");
        return 1;
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <mesh_file>", argv[0]);
        return 1;
    }

    if (!Init(false))
        return 1;

    Camera camera;
    camera.SetClippingPlanes(0.01f, 10.0f);
    camera.SetViewport(0, 0, width, height);

    float prevTime = (float)SDL_GetTicks();
    float dt = 0.0f; 

    World world;
    if (!world.Load(argv[1]))
        return 1;

    float rotation = 0.0f;

    for (;;) {
        SDL_PumpEvents();

        if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_ESCAPE])
            break;

        camera.Update(dt);

        rotation += 0.001f * dt;
        Matrix4 worldMatrix = Matrix4::scale(Vector3(0.1f, -0.1f, 0.1f)) * Matrix4::rotationY(rotation);

        Rasterize(worldMatrix, world.geometries, world.numGeometries, camera);

        float time = (float)SDL_GetTicks();
        dt = time - prevTime;
        prevTime = time;

        char buffer[128];
        sprintf_s(buffer, "FPS: %f Frame Time: %f ms", 1000.0f / dt, dt);
        SDL_SetWindowTitle(window, buffer);
    }

    return 0;
}
