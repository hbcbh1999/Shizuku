#include "ShaderManager.h"
#include "Shizuku.Core/Ogl/Shader.h"
#include "Shizuku.Core/Ogl/Ogl.h"
#include "CudaLbm.h"
#include "Domain.h"
#include "helper_cuda.h"
#include <SOIL/SOIL.h>
#include <glm/gtc/type_ptr.hpp>
#include <GLEW/glew.h>
#include <string>
#include <cstring>
#include <assert.h>

using namespace Shizuku::Core;

ShaderManager::ShaderManager()
{
    m_shaderProgram = new ShaderProgram;
    m_lightingProgram = new ShaderProgram;
    m_obstProgram = new ShaderProgram;
    m_floorProgram = new ShaderProgram;

    Ogl = std::shared_ptr < Shizuku::Core::Ogl >(new Shizuku::Core::Ogl());
}

void ShaderManager::CreateCudaLbm()
{
    m_cudaLbm = new CudaLbm;
}

CudaLbm* ShaderManager::GetCudaLbm()
{
    return m_cudaLbm;
}

cudaGraphicsResource* ShaderManager::GetCudaSolutionGraphicsResource()
{
    return m_cudaGraphicsResource;
}

cudaGraphicsResource* ShaderManager::GetCudaFloorLightTextureResource()
{
    return m_cudaFloorLightTextureResource;
}

cudaGraphicsResource* ShaderManager::GetCudaEnvTextureResource()
{
    return m_cudaEnvTextureResource;
}

void ShaderManager::CreateVboForCudaInterop(const unsigned int size)
{
    cudaGLSetGLDevice(gpuGetMaxGflopsDeviceId());
    CreateVbo(size, cudaGraphicsMapFlagsWriteDiscard);
    CreateElementArrayBuffer();
}

void ShaderManager::CleanUpGLInterOp()
{
    DeleteVbo();
    DeleteElementArrayBuffer();
}

void ShaderManager::CreateVbo(const unsigned int size, const unsigned int vboResFlags)
{
    std::shared_ptr<Ogl::Vao> main = Ogl->CreateVao("main");
    main->Bind();

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glBufferData(GL_ARRAY_BUFFER, size, 0, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    main->Unbind();

    cudaGraphicsGLRegisterBuffer(&m_cudaGraphicsResource, m_vbo, vboResFlags);
}

void ShaderManager::DeleteVbo()
{
    cudaGraphicsUnregisterResource(m_cudaGraphicsResource);
    glBindBuffer(1, m_vbo);
    glDeleteBuffers(1, &m_vbo);
    m_vbo = 0;
}

void ShaderManager::CreateElementArrayBuffer()
{
    const int numberOfElements = 2*(MAX_XDIM - 1)*(MAX_YDIM - 1);
    const int numberOfNodes = MAX_XDIM*MAX_YDIM;
    GLuint* elementIndices = new GLuint[numberOfElements * 3 * 2];
    for (int j = 0; j < MAX_YDIM-1; j++){
        for (int i = 0; i < MAX_XDIM-1; i++){
            //going clockwise, since y orientation will be flipped when rendered
            elementIndices[j*(MAX_XDIM-1)*6+i*6+0] = (i)+(j)*MAX_XDIM;
            elementIndices[j*(MAX_XDIM-1)*6+i*6+1] = (i+1)+(j)*MAX_XDIM;
            elementIndices[j*(MAX_XDIM-1)*6+i*6+2] = (i+1)+(j+1)*MAX_XDIM;

            elementIndices[j*(MAX_XDIM-1)*6+i*6+3] = (i)+(j)*MAX_XDIM;
            elementIndices[j*(MAX_XDIM-1)*6+i*6+4] = (i+1)+(j+1)*MAX_XDIM;
            elementIndices[j*(MAX_XDIM-1)*6+i*6+5] = (i)+(j+1)*MAX_XDIM;
        }
    }
    for (int j = 0; j < MAX_YDIM-1; j++){
        for (int i = 0; i < MAX_XDIM-1; i++){
            //going clockwise, since y orientation will be flipped when rendered
            elementIndices[numberOfElements*3+j*(MAX_XDIM-1)*6+i*6+0] = numberOfNodes+(i)+(j)*MAX_XDIM;
            elementIndices[numberOfElements*3+j*(MAX_XDIM-1)*6+i*6+1] = numberOfNodes+(i+1)+(j)*MAX_XDIM;
            elementIndices[numberOfElements*3+j*(MAX_XDIM-1)*6+i*6+2] = numberOfNodes+(i+1)+(j+1)*MAX_XDIM;

            elementIndices[numberOfElements*3+j*(MAX_XDIM-1)*6+i*6+3] = numberOfNodes+(i)+(j)*MAX_XDIM;
            elementIndices[numberOfElements*3+j*(MAX_XDIM-1)*6+i*6+4] = numberOfNodes+(i+1)+(j+1)*MAX_XDIM;
            elementIndices[numberOfElements*3+j*(MAX_XDIM-1)*6+i*6+5] = numberOfNodes+(i)+(j+1)*MAX_XDIM;
        }
    }
    Ogl->GetVao("main")->Bind();
    glGenBuffers(1, &m_elementArrayBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_elementArrayBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint)*numberOfElements*3*2, elementIndices, GL_DYNAMIC_DRAW);
    free(elementIndices);
    Ogl->GetVao("main")->Unbind();
}

void ShaderManager::DeleteElementArrayBuffer(){
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &m_elementArrayBuffer);
}

template <typename T>
void ShaderManager::CreateShaderStorageBuffer(T defaultValue, const unsigned int numberOfElements, const std::string name)
{
    GLuint temp;
    glGenBuffers(1, &temp);
    T* data = new T[numberOfElements];
    for (int i = 0; i < numberOfElements; i++)
    {
        data[i] = defaultValue;
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, temp);
    glBufferData(GL_SHADER_STORAGE_BUFFER, numberOfElements*sizeof(T), data, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, temp);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    m_ssbos.push_back(Ssbo{ temp, name });
}

GLuint ShaderManager::GetShaderStorageBuffer(const std::string name)
{
    for (std::vector<Ssbo>::iterator it = m_ssbos.begin(); it != m_ssbos.end(); ++it)
    {
        if (it->m_name == name)
        {
            return it->m_id;
        }
    }
    return NULL;
}

GLuint ShaderManager::GetElementArrayBuffer()
{
    return m_elementArrayBuffer;
}

GLuint ShaderManager::GetVbo()
{
    return m_vbo;
}

ShaderProgram* ShaderManager::GetShaderProgram()
{
    return m_shaderProgram;
}

ShaderProgram* ShaderManager::GetLightingProgram()
{
    return m_lightingProgram;
}

ShaderProgram* ShaderManager::GetObstProgram()
{
    return m_obstProgram;
}

ShaderProgram* ShaderManager::GetFloorProgram()
{
    return m_floorProgram;
}

void ShaderManager::CompileShaders()
{
    GetShaderProgram()->Initialize("Surface");
    GetShaderProgram()->CreateShader("SurfaceShader.vert.glsl", GL_VERTEX_SHADER);
    GetShaderProgram()->CreateShader("SurfaceShader.frag.glsl", GL_FRAGMENT_SHADER);
    GetLightingProgram()->Initialize("Lighting");
    GetLightingProgram()->CreateShader("SurfaceShader.comp.glsl", GL_COMPUTE_SHADER);
    GetObstProgram()->Initialize("Obstructions");
    GetObstProgram()->CreateShader("Obstructions.comp.glsl", GL_COMPUTE_SHADER);
    GetFloorProgram()->Initialize("Floor");
    GetFloorProgram()->CreateShader("FloorShader.vert.glsl", GL_VERTEX_SHADER);
    GetFloorProgram()->CreateShader("FloorShader.frag.glsl", GL_FRAGMENT_SHADER);
}

void ShaderManager::AllocateStorageBuffers()
{
    CreateShaderStorageBuffer(GLfloat(0), MAX_XDIM*MAX_YDIM*9, "LbmA");
    CreateShaderStorageBuffer(GLfloat(0), MAX_XDIM*MAX_YDIM*9, "LbmB");
    CreateShaderStorageBuffer(GLint(0), MAX_XDIM*MAX_YDIM, "Floor");
    CreateShaderStorageBuffer(Obstruction{}, MAXOBSTS, "Obstructions");
    CreateShaderStorageBuffer(float4{0,0,0,1e6}, 1, "RayIntersection");
}

void ShaderManager::SetUpTextures()
{
    int width, height;
    unsigned char* image = SOIL_load_image("BlueSky.png", &width, &height, 0, SOIL_LOAD_RGB);
    assert(image != NULL);
//    float* tex = new float[width*height];
//    for (int i = 0; i < width*height; ++i)
//    {
//        unsigned char color[4];
//        std::memcpy(&color, &image[3 * i], 3*sizeof(unsigned char));
//        color[3] = unsigned char(255);
//        std::memcpy(&tex[i], &color, sizeof(float));
////        for (int j = 0; j < 4; ++j)
////        {
////            printf("%u,", color[j]);
////        }
////        std::cout << std::endl;
//    }

    float* tex = new float[4*width*height];
    for (int i = 0; i < width*height; ++i)
    {
        unsigned char color[4];
        std::memcpy(&color, &image[3 * i], 3*sizeof(unsigned char));
        color[3] = unsigned char(255);
        tex[4*i] = color[0];
        tex[4*i+1] = color[1];
        tex[4*i+2] = color[2];
        tex[4*i+3] = color[3];
//        if (i > (width) * 1024 + 1024 && i < (width) * 1024 + 2048)
//        {
//            for (int j = 0; j < 4; ++j)
//            {
//                printf("%f,", tex[4*i+j]);
//            }
//            std::cout << std::endl;
//        }
    }



    glGenTextures(1, &m_envTexture);
    glBindTexture(GL_TEXTURE_2D, m_envTexture);

    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    cudaGraphicsGLRegisterImage(&m_cudaEnvTextureResource, m_envTexture, GL_TEXTURE_2D, cudaGraphicsMapFlagsReadOnly);

    SOIL_free_image_data(image);

    glBindTexture(GL_TEXTURE_2D, 0);


    // set up FBO and texture to render to 
    glGenFramebuffers(1, &m_floorFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_floorFbo);

    glGenTextures(1, &m_floorLightTexture);
    glBindTexture(GL_TEXTURE_2D, m_floorLightTexture);

    const GLuint textureWidth = 1024;
    const GLuint textureHeight = 1024;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, textureWidth, textureHeight, 0, GL_RGBA, GL_FLOAT, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_floorLightTexture, 0);

    GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBuffers);

    //if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        //return false;

    cudaGraphicsGLRegisterImage(&m_cudaFloorLightTextureResource, m_floorLightTexture, GL_TEXTURE_2D, cudaGraphicsMapFlagsReadOnly);

    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

void ShaderManager::InitializeObstSsbo()
{
    const GLuint obstSsbo = GetShaderStorageBuffer("Obstructions");
    CudaLbm* cudaLbm = GetCudaLbm();
    Obstruction* obst_h = cudaLbm->GetHostObst();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, obstSsbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAXOBSTS*sizeof(Obstruction), obst_h, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, obstSsbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

int ShaderManager::RayCastMouseClick(glm::vec3 &rayCastIntersection, const glm::vec3 rayOrigin,
    const glm::vec3 rayDir)
{
    Domain domain = *m_cudaLbm->GetDomain();
    int xDim = domain.GetXDim();
    int yDim = domain.GetYDim();
    glm::vec4 intersectionCoord{ 0, 0, 0, 0 };
    const GLuint ssbo_rayIntersection = GetShaderStorageBuffer("RayIntersection");
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, ssbo_rayIntersection);

    ShaderProgram* const shader = GetLightingProgram();
    shader->Use();

    shader->SetUniform("maxXDim", MAX_XDIM);
    shader->SetUniform("maxyDim", MAX_YDIM);
    shader->SetUniform("maxObsts", MAXOBSTS);
    shader->SetUniform("xDim", xDim);
    shader->SetUniform("yDim", yDim);
    shader->SetUniform("xDimVisible", domain.GetXDimVisible());
    shader->SetUniform("yDimVisible", domain.GetYDimVisible());
    shader->SetUniform("rayOrigin", rayOrigin);
    shader->SetUniform("rayDir", rayDir);

    shader->RunSubroutine("ResetRayCastData", glm::ivec3{ 1, 1, 1 });
    shader->RunSubroutine("RayCast", glm::ivec3{ xDim, yDim, 1 });

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_rayIntersection);
    GLfloat* intersect = (GLfloat*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
        sizeof(glm::vec4), GL_MAP_READ_BIT);
    std::memcpy(&intersectionCoord, intersect, sizeof(glm::vec4));
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

    int returnVal;
    if (intersectionCoord.w > 1e5) //ray did not intersect with any objects
    {
        returnVal = 1;
    }
    else
    {
        shader->RunSubroutine("ResetRayCastData", glm::ivec3{ 1, 1, 1 });
        rayCastIntersection.x = intersectionCoord.x;
        rayCastIntersection.y = intersectionCoord.y;
        rayCastIntersection.z = intersectionCoord.z;
        returnVal = 0;
    }
    shader->Unset();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return returnVal;
}

void ShaderManager::RenderFloorToTexture(Domain &domain)
{
    Ogl->GetVao("main")->Bind();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, m_floorFbo);
    glBindTexture(GL_TEXTURE_2D, m_floorLightTexture);
    //glMatrixMode(GL_PROJECTION);
    //glLoadIdentity();

    //glOrtho(-1,1,-1,1,-100,20);
    //gluPerspective(45.0, 1.0, 0.1, 10.0);


    ShaderProgram* floorShader = GetFloorProgram();
    floorShader->Use();

    floorShader->SetUniform("xDimVisible", domain.GetXDimVisible());
    floorShader->SetUniform("yDimVisible", domain.GetYDimVisible());
    glViewport(0, 0, 1024, 1024);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_elementArrayBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 16, 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 16, (GLvoid*)(3 * sizeof(GLfloat)));

    int yDimVisible = domain.GetYDimVisible();
    //Draw floor
    glDrawElements(GL_TRIANGLES, (MAX_XDIM - 1)*(yDimVisible - 1)*3*2, GL_UNSIGNED_INT, 
        BUFFER_OFFSET(sizeof(GLuint)*3*2*(MAX_XDIM - 1)*(MAX_YDIM - 1)));

    floorShader->Unset();




    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    Ogl->GetVao("main")->Unbind();
}

void ShaderManager::RenderVbo(const bool renderFloor, Domain &domain, const glm::mat4 &modelMatrix,
    const glm::mat4 &projectionMatrix)
{
    //glEnable(GL_BLEND);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //glMatrixMode(GL_MODELVIEW);
    //glLoadIdentity();

    //Draw solution field
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_elementArrayBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 16, 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 16, (GLvoid*)(3 * sizeof(GLfloat)));

    int yDimVisible = domain.GetYDimVisible();
    if (renderFloor)
    {
        //Draw floor
        glDrawElements(GL_TRIANGLES, (MAX_XDIM - 1)*(yDimVisible - 1)*3*2, GL_UNSIGNED_INT, 
            BUFFER_OFFSET(sizeof(GLuint)*3*2*(MAX_XDIM - 1)*(MAX_YDIM - 1)));
    }
    //Draw water surface
    glDrawElements(GL_TRIANGLES, (MAX_XDIM - 1)*(yDimVisible - 1)*3*2 , GL_UNSIGNED_INT, (GLvoid*)0);
}

void ShaderManager::RunComputeShader(const glm::vec3 cameraPosition, const ContourVariable contVar,
        const float contMin, const float contMax)
{
    const GLuint ssbo_lbmA = GetShaderStorageBuffer("LbmA");
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_lbmA);
    const GLuint ssbo_lbmB = GetShaderStorageBuffer("LbmB");
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo_lbmB);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_vbo);
    const GLuint ssbo_floor = GetShaderStorageBuffer("Floor");
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbo_floor);
    const GLuint ssbo_obsts = GetShaderStorageBuffer("Obstructions");
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, ssbo_obsts);
    ShaderProgram* const shader = GetLightingProgram();

    shader->Use();

    Domain domain = *m_cudaLbm->GetDomain();
    const int xDim = domain.GetXDim();
    const int yDim = domain.GetYDim();
    shader->SetUniform("maxXDim", MAX_XDIM);
    shader->SetUniform("maxyDim", MAX_YDIM);
    shader->SetUniform("maxObsts", MAXOBSTS);
    shader->SetUniform("xDim", xDim);
    shader->SetUniform("yDim", yDim);
    shader->SetUniform("xDimVisible", domain.GetXDimVisible());
    shader->SetUniform("yDimVisible", domain.GetYDimVisible());
    shader->SetUniform("cameraPosition", cameraPosition);
    shader->SetUniform("uMax", m_inletVelocity);
    shader->SetUniform("omega", m_omega);
    shader->SetUniform("contourVar", contVar);
    shader->SetUniform("contourMin", contMin);
    shader->SetUniform("contourMax", contMax);

    for (int i = 0; i < 5; i++)
    {
        shader->RunSubroutine("MarchLbm", glm::ivec3{ xDim, yDim, 1 });
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo_lbmA);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_lbmB);

        shader->RunSubroutine("MarchLbm", glm::ivec3{ xDim, yDim, 1 });
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_lbmA);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo_lbmB);
    }
    
    shader->RunSubroutine("UpdateFluidVbo", glm::ivec3{ xDim, yDim, 1 });
    shader->RunSubroutine("DeformFloorMeshUsingCausticRay", glm::ivec3{ xDim, yDim, 1 });
    shader->RunSubroutine("ComputeFloorLightIntensitiesFromMeshDeformation", glm::ivec3{ xDim, yDim, 1 });
    shader->RunSubroutine("ApplyCausticLightingToFloor", glm::ivec3{ xDim, yDim, 1 });
    shader->RunSubroutine("PhongLighting", glm::ivec3{ xDim, yDim, 2 });
    shader->RunSubroutine("UpdateObstructionTransientStates", glm::ivec3{ xDim, yDim, 1 });
    shader->RunSubroutine("CleanUpVbo", glm::ivec3{ MAX_XDIM, MAX_YDIM, 2 });
    
    shader->Unset();

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}



void ShaderManager::UpdateObstructionsUsingComputeShader(const int obstId, Obstruction &newObst, const float scaleFactor)
{
    const GLuint ssbo_obsts = GetShaderStorageBuffer("Obstructions");
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_obsts);
    ShaderProgram* const shader = GetObstProgram();
    shader->Use();

    shader->SetUniform("targetObst.shape", newObst.shape);
    shader->SetUniform("targetObst.x", newObst.x);
    shader->SetUniform("targetObst.y", newObst.y);
    shader->SetUniform("targetObst.r1", newObst.r1);
    shader->SetUniform("targetObst.u", newObst.u);
    shader->SetUniform("targetObst.v", newObst.v);
    shader->SetUniform("targetObst.state", newObst.state);
    shader->SetUniform("targetObstId", obstId);

    shader->RunSubroutine("UpdateObstruction", glm::ivec3{ 1, 1, 1 });

    shader->Unset();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void ShaderManager::InitializeComputeShaderData()
{
    const GLuint ssbo_lbmA = GetShaderStorageBuffer("LbmA");
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_lbmA);
    const GLuint ssbo_lbmB = GetShaderStorageBuffer("LbmB");
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo_lbmB);
    const GLuint ssbo_obsts = GetShaderStorageBuffer("Obstructions");
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, ssbo_obsts);
    ShaderProgram* const shader = GetLightingProgram();

    shader->Use();

    Domain domain = *m_cudaLbm->GetDomain();
    shader->SetUniform("maxXDim", MAX_XDIM);
    shader->SetUniform("maxYDim", MAX_YDIM);
    shader->SetUniform("maxObsts", MAXOBSTS);//
    shader->SetUniform("xDim", domain.GetXDim());
    shader->SetUniform("yDim", domain.GetYDim());
    shader->SetUniform("xDimVisible", domain.GetXDim());
    shader->SetUniform("yDimVisible", domain.GetYDim());
    shader->SetUniform("uMax", m_inletVelocity);

    shader->RunSubroutine("InitializeDomain", glm::ivec3{ MAX_XDIM, MAX_YDIM, 1 });

    shader->Unset();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void ShaderManager::BindFloorLightTexture()
{
    glBindTexture(GL_TEXTURE_2D, m_floorLightTexture);
}

void ShaderManager::BindEnvTexture()
{
    glBindTexture(GL_TEXTURE_2D, m_envTexture);
}

void ShaderManager::UnbindFloorTexture()
{
    glBindTexture(GL_TEXTURE_2D, 0);
}

void ShaderManager::SetOmega(const float omega)
{
    m_omega = omega;
}

float ShaderManager::GetOmega()
{
    return m_omega;
}

void ShaderManager::SetInletVelocity(const float u)
{
    m_inletVelocity = u;
}

float ShaderManager::GetInletVelocity()
{
    return m_inletVelocity;
}

void ShaderManager::UpdateLbmInputs(const float u, const float omega)
{
    SetInletVelocity(u);
    SetOmega(omega);
}

void ShaderManager::RenderVboUsingShaders(const bool renderFloor, Domain &domain,
    const glm::mat4 &modelMatrix, const glm::mat4 &projectionMatrix)
{
    ShaderProgram* shader = GetShaderProgram();

    Ogl->GetVao("main")->Bind();
    shader->Use();
    glActiveTexture(GL_TEXTURE0);
    shader->SetUniform("modelMatrix", glm::transpose(modelMatrix));
    shader->SetUniform("projectionMatrix", glm::transpose(projectionMatrix));

    RenderVbo(renderFloor, domain, modelMatrix, projectionMatrix);

    shader->Unset();
    Ogl->GetVao("main")->Unbind();
}
