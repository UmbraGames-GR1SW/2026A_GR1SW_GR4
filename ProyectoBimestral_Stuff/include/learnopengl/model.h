#ifndef MODEL_H
#define MODEL_H

#include <glad/glad.h> 

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <learnopengl/stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <learnopengl/mesh.h>
#include <learnopengl/shader.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
#include <algorithm>
#include <cctype>
using namespace std;

struct Triangle {
    glm::vec3 v0, v1, v2;
};

struct CollisionGrid {
    glm::vec3 minBounds = glm::vec3(0.0f);
    glm::vec3 maxBounds = glm::vec3(0.0f);
    glm::vec3 cellSize = glm::vec3(1.0f);
    int numCellsX = 0;
    int numCellsY = 0;
    int numCellsZ = 0;
    std::vector<std::vector<std::vector<std::vector<Triangle>>>> cells;
    bool isBuilt = false;
};

unsigned int TextureFromFile(const char *path, const string &directory, bool gamma = false);

class Model 
{
public:
    // model data 
    vector<Texture> textures_loaded;	// stores all the textures loaded so far, optimization to make sure textures aren't loaded more than once.
    vector<Mesh>    meshes;
    string directory;
    bool gammaCorrection;

    CollisionGrid collisionGrid;
    unsigned int boxVAO = 0;
    unsigned int boxVBO = 0;
    unsigned int boxEBO = 0;

    // constructor, expects a filepath to a 3D model.
    Model(string const &path, bool gamma = false) : gammaCorrection(gamma)
    {
        loadModel(path);
    }

    // draws the model, and thus all its meshes
    void Draw(Shader &shader)
    {
        for(unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }
    
private:
    // loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
    void loadModel(string const &path)
    {
        // read file via ASSIMP
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
        // check for errors
        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
        {
            cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
            return;
        }
        // retrieve the directory path of the filepath
        directory = path.substr(0, path.find_last_of('/'));

        // process ASSIMP's root node recursively
        processNode(scene->mRootNode, scene);
    }

    // processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
    void processNode(aiNode *node, const aiScene *scene)
    {
        // process each mesh located at the current node
        for(unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            // the node object only contains indices to index the actual objects in the scene. 
            // the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }
        // after we've processed all of the meshes (if any) we then recursively process each of the children nodes
        for(unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], scene);
        }

    }

    Mesh processMesh(aiMesh *mesh, const aiScene *scene)
    {
        // data to fill
        vector<Vertex> vertices;
        vector<unsigned int> indices;
        vector<Texture> textures;

        // walk through each of the mesh's vertices
        for(unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;
            glm::vec3 vector; // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.
            // positions
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;
            // normals
            if (mesh->HasNormals())
            {
                vector.x = mesh->mNormals[i].x;
                vector.y = mesh->mNormals[i].y;
                vector.z = mesh->mNormals[i].z;
                vertex.Normal = vector;
            }
            // texture coordinates
            if(mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
            {
                glm::vec2 vec;
                // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
                // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
                vec.x = mesh->mTextureCoords[0][i].x; 
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
                // tangent
                vector.x = mesh->mTangents[i].x;
                vector.y = mesh->mTangents[i].y;
                vector.z = mesh->mTangents[i].z;
                vertex.Tangent = vector;
                // bitangent
                vector.x = mesh->mBitangents[i].x;
                vector.y = mesh->mBitangents[i].y;
                vector.z = mesh->mBitangents[i].z;
                vertex.Bitangent = vector;
            }
            else
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);

            vertices.push_back(vertex);
        }
        // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
        for(unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            // retrieve all indices of the face and store them in the indices vector
            for(unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);        
        }
        // process materials
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];    
        
        aiColor4D diffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
        aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &diffuseColor);
        glm::vec3 diff(diffuseColor.r, diffuseColor.g, diffuseColor.b);

        // 1. diffuse maps
        vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
        // 2. specular maps
        vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
        textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
        // 3. normal maps
        std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
        textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
        // 4. height maps
        std::vector<Texture> heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
        textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
        
        // return a mesh object created from the extracted mesh data
        return Mesh(vertices, indices, textures, diff, mesh->mName.C_Str());
    }

    // checks all material textures of a given type and loads the textures if they're not loaded yet.
    // the required info is returned as a Texture struct.
    vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, string typeName)
    {
        vector<Texture> textures;
        for(unsigned int i = 0; i < mat->GetTextureCount(type); i++)
        {
            aiString str;
            mat->GetTexture(type, i, &str);
            // check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
            bool skip = false;
            for(unsigned int j = 0; j < textures_loaded.size(); j++)
            {
                if(std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
                {
                    textures.push_back(textures_loaded[j]);
                    skip = true; // a texture with the same filepath has already been loaded, continue to next one. (optimization)
                    break;
                }
            }
            if(!skip)
            {   // if texture hasn't been loaded already, load it
                Texture texture;
                texture.id = TextureFromFile(str.C_Str(), this->directory);
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                textures_loaded.push_back(texture);  // store it as texture loaded for entire model, to ensure we won't unnecesery load duplicate textures.
            }
        }
        return textures;
    }

public:
    void buildCollisionGrid(float targetCellSize = 0.25f)
    {
        glm::vec3 minB(1e10f);
        glm::vec3 maxB(-1e10f);
        bool hasValidVertices = false;

        for (const auto& mesh : meshes) {
            std::string lowerName = mesh.name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            if (lowerName.find("sky") != std::string::npos || lowerName.find("cielo") != std::string::npos) {
                std::cout << "Collision Grid: Ignored background mesh: " << mesh.name << std::endl;
                continue;
            }

            for (const auto& vertex : mesh.vertices) {
                minB = glm::min(minB, vertex.Position);
                maxB = glm::max(maxB, vertex.Position);
                hasValidVertices = true;
            }
        }

        if (!hasValidVertices) {
            std::cout << "Collision Grid: No valid vertices found to build collision grid." << std::endl;
            collisionGrid.isBuilt = false;
            return;
        }

        minB -= glm::vec3(0.01f);
        maxB += glm::vec3(0.01f);

        collisionGrid.minBounds = minB;
        collisionGrid.maxBounds = maxB;

        glm::vec3 dims = maxB - minB;
        collisionGrid.numCellsX = std::max(1, (int)std::ceil(dims.x / targetCellSize));
        collisionGrid.numCellsY = std::max(1, (int)std::ceil(dims.y / targetCellSize));
        collisionGrid.numCellsZ = std::max(1, (int)std::ceil(dims.z / targetCellSize));

        collisionGrid.cellSize.x = dims.x / collisionGrid.numCellsX;
        collisionGrid.cellSize.y = dims.y / collisionGrid.numCellsY;
        collisionGrid.cellSize.z = dims.z / collisionGrid.numCellsZ;

        collisionGrid.cells.resize(collisionGrid.numCellsX);
        for (int x = 0; x < collisionGrid.numCellsX; ++x) {
            collisionGrid.cells[x].resize(collisionGrid.numCellsY);
            for (int y = 0; y < collisionGrid.numCellsY; ++y) {
                collisionGrid.cells[x][y].resize(collisionGrid.numCellsZ);
            }
        }

        int totalTriangles = 0;

        for (const auto& mesh : meshes) {
            std::string lowerName = mesh.name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            if (lowerName.find("sky") != std::string::npos || lowerName.find("cielo") != std::string::npos) {
                continue;
            }

            for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
                Triangle tri;
                tri.v0 = mesh.vertices[mesh.indices[i]].Position;
                tri.v1 = mesh.vertices[mesh.indices[i+1]].Position;
                tri.v2 = mesh.vertices[mesh.indices[i+2]].Position;

                glm::vec3 triMin = glm::min(tri.v0, glm::min(tri.v1, tri.v2));
                glm::vec3 triMax = glm::max(tri.v0, glm::max(tri.v1, tri.v2));

                int minX = glm::clamp((int)std::floor((triMin.x - minB.x) / collisionGrid.cellSize.x), 0, collisionGrid.numCellsX - 1);
                int maxX = glm::clamp((int)std::floor((triMax.x - minB.x) / collisionGrid.cellSize.x), 0, collisionGrid.numCellsX - 1);

                int minY = glm::clamp((int)std::floor((triMin.y - minB.y) / collisionGrid.cellSize.y), 0, collisionGrid.numCellsY - 1);
                int maxY = glm::clamp((int)std::floor((triMax.y - minB.y) / collisionGrid.cellSize.y), 0, collisionGrid.numCellsY - 1);

                int minZ = glm::clamp((int)std::floor((triMin.z - minB.z) / collisionGrid.cellSize.z), 0, collisionGrid.numCellsZ - 1);
                int maxZ = glm::clamp((int)std::floor((triMax.z - minB.z) / collisionGrid.cellSize.z), 0, collisionGrid.numCellsZ - 1);

                for (int x = minX; x <= maxX; ++x) {
                    for (int y = minY; y <= maxY; ++y) {
                        for (int z = minZ; z <= maxZ; ++z) {
                            collisionGrid.cells[x][y][z].push_back(tri);
                        }
                    }
                }
                totalTriangles++;
            }
        }

        std::cout << "Collision Grid Built successfully! bounds: (" 
                  << dims.x << ", " << dims.y << ", " << dims.z << "), grid size: "
                  << collisionGrid.numCellsX << "x" << collisionGrid.numCellsY << "x" << collisionGrid.numCellsZ
                  << ", total triangles: " << totalTriangles << std::endl;

        if (boxVAO == 0) {
            glGenVertexArrays(1, &boxVAO);
            glGenBuffers(1, &boxVBO);
            glGenBuffers(1, &boxEBO);
        }

        glm::vec3 min = collisionGrid.minBounds;
        glm::vec3 max = collisionGrid.maxBounds;

        float boxVertices[] = {
            min.x, min.y, min.z,
            max.x, min.y, min.z,
            max.x, max.y, min.z,
            min.x, max.y, min.z,
            min.x, min.y, max.z,
            max.x, min.y, max.z,
            max.x, max.y, max.z,
            min.x, max.y, max.z
        };

        unsigned int boxIndices[] = {
            0, 1, 1, 2, 2, 3, 3, 0,
            4, 5, 5, 6, 6, 7, 7, 4,
            0, 4, 1, 5, 2, 6, 3, 7
        };

        glBindVertexArray(boxVAO);
        glBindBuffer(GL_ARRAY_BUFFER, boxVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(boxVertices), boxVertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, boxEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(boxIndices), boxIndices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindVertexArray(0);

        collisionGrid.isBuilt = true;
    }

    bool checkCollision(glm::vec3 &playerPosition, float sphereRadius, const glm::mat4 &modelMatrix)
    {
        if (!collisionGrid.isBuilt) return false;

        glm::mat4 invModel = glm::inverse(modelMatrix);
        glm::vec4 localPos4 = invModel * glm::vec4(playerPosition, 1.0f);
        glm::vec3 localPos = glm::vec3(localPos4) / localPos4.w;

        float scale = glm::length(glm::vec3(modelMatrix[0]));
        float localRadius = sphereRadius / scale;

        glm::vec3 localMin = localPos - glm::vec3(localRadius);
        glm::vec3 localMax = localPos + glm::vec3(localRadius);

        int minCellX = glm::clamp((int)std::floor((localMin.x - collisionGrid.minBounds.x) / collisionGrid.cellSize.x), 0, collisionGrid.numCellsX - 1);
        int maxCellX = glm::clamp((int)std::floor((localMax.x - collisionGrid.minBounds.x) / collisionGrid.cellSize.x), 0, collisionGrid.numCellsX - 1);

        int minCellY = glm::clamp((int)std::floor((localMin.y - collisionGrid.minBounds.y) / collisionGrid.cellSize.y), 0, collisionGrid.numCellsY - 1);
        int maxCellY = glm::clamp((int)std::floor((localMax.y - collisionGrid.minBounds.y) / collisionGrid.cellSize.y), 0, collisionGrid.numCellsY - 1);

        int minCellZ = glm::clamp((int)std::floor((localMin.z - collisionGrid.minBounds.z) / collisionGrid.cellSize.z), 0, collisionGrid.numCellsZ - 1);
        int maxCellZ = glm::clamp((int)std::floor((localMax.z - collisionGrid.minBounds.z) / collisionGrid.cellSize.z), 0, collisionGrid.numCellsZ - 1);

        bool collided = false;
        std::vector<Triangle> candidateTriangles;

        for (int x = minCellX; x <= maxCellX; ++x) {
            for (int y = minCellY; y <= maxCellY; ++y) {
                for (int z = minCellZ; z <= maxCellZ; ++z) {
                    for (const auto& tri : collisionGrid.cells[x][y][z]) {
                        candidateTriangles.push_back(tri);
                    }
                }
            }
        }

        for (int iter = 0; iter < 3; ++iter) {
            bool localCollision = false;

            for (const auto& tri : candidateTriangles) {
                glm::vec3 closestPoint = ClosestPointOnTriangle(localPos, tri.v0, tri.v1, tri.v2);
                glm::vec3 toPlayer = localPos - closestPoint;
                float dist = glm::length(toPlayer);

                if (dist < localRadius) {
                    glm::vec3 normal;
                    if (dist > 0.0001f) {
                        normal = toPlayer / dist;
                    } else {
                        normal = glm::normalize(glm::cross(tri.v1 - tri.v0, tri.v2 - tri.v0));
                    }

                    float penetration = localRadius - dist;
                    localPos += normal * penetration;
                    localCollision = true;
                    collided = true;
                }
            }

            if (!localCollision) break;
        }

        if (collided) {
            glm::vec4 worldPos4 = modelMatrix * glm::vec4(localPos, 1.0f);
            playerPosition = glm::vec3(worldPos4) / worldPos4.w;
        }

        return collided;
    }

    void drawOBB(Shader &shader, const glm::mat4 &modelMatrix)
    {
        if (!collisionGrid.isBuilt || boxVAO == 0) return;

        shader.use();
        shader.setMat4("model", modelMatrix);
        shader.setVec3("materialColor", glm::vec3(0.0f, 1.0f, 0.0f));
        shader.setBool("useMaterialColor", true);

        glBindVertexArray(boxVAO);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(2.0f);
        glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glBindVertexArray(0);
    }

    void drawHitbox(Shader &shader, const glm::mat4 &modelMatrix)
    {
        drawOBB(shader, modelMatrix);
    }

private:
    glm::vec3 ClosestPointOnTriangle(glm::vec3 p, glm::vec3 a, glm::vec3 b, glm::vec3 c)
    {
        glm::vec3 ab = b - a;
        glm::vec3 ac = c - a;
        glm::vec3 ap = p - a;
        float d1 = glm::dot(ab, ap);
        float d2 = glm::dot(ac, ap);
        if (d1 <= 0.0f && d2 <= 0.0f) return a;

        glm::vec3 bp = p - b;
        float d3 = glm::dot(ab, bp);
        float d4 = glm::dot(ac, bp);
        if (d3 >= 0.0f && d4 <= d3) return b;

        float vc = d1*d4 - d3*d2;
        if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
            float v = d1 / (d1 - d3);
            return a + v * ab;
        }

        glm::vec3 cp = p - c;
        float d5 = glm::dot(ab, cp);
        float d6 = glm::dot(ac, cp);
        if (d6 >= 0.0f && d5 <= d6) return c;

        float vb = d5*d2 - d1*d6;
        if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
            float w = d2 / (d2 - d6);
            return a + w * ac;
        }

        float va = d3*d6 - d5*d4;
        if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
            float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
            return b + w * (c - b);
        }

        float denom = 1.0f / (va + vb + vc);
        float v = vb * denom;
        float w = vc * denom;
        return a + ab * v + ac * w;
    }
};


unsigned int TextureFromFile(const char *path, const string &directory, bool gamma)
{
    string filename = string(path);
    filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format = GL_RGB; // valor por defecto seguro
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 2)
            format = GL_RG;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;
        else {
            std::cerr << "Unexpected nrComponents: " << nrComponents << " for file: " << filename << std::endl;
            stbi_image_free(data);
            return 0;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
#endif