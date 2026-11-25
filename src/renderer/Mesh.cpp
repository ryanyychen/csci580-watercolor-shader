#include "Mesh.h"
#include <Eigen/Dense>
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#include <queue>
#include <random>

namespace
{

// Match shader locations 0 and 1 with a position and normal for each corner.
struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoord;

    // curvature values
    float k1;
    float k2;

};
} // namespace

Mesh::Mesh(const std::filesystem::path &path) : Mesh(std::vector<std::filesystem::path>{path})
{
}

Mesh::Mesh(const std::vector<std::filesystem::path> &paths)
    : Mesh(loadSceneFiles(paths))
{
}

Mesh::Mesh(const ImportedSceneData &imported, bool centerMesh) {
    if (imported.material)
    {
        material.color_ambient = imported.material->ambient;
        material.color_diffuse = imported.material->diffuse;
        material.color_specular = imported.material->specular;
        material.specular_power = imported.material->shininess;
    }

    std::vector<Vertex> vertices; // expanded vertex data for rendering
    std::vector<unsigned int> sourceVertexIndices;
    vertices.reserve(imported.triangles.size() * 3);
    sourceVertexIndices.reserve(imported.triangles.size() * 3);
    uvTriangles_.reserve(imported.triangles.size());
    hasTextureCoordinates_ = imported.hasTextureCoordinates;

    for (const glm::vec3 &position : imported.positions)
    {
        GeometryVertex vertex{};
        vertex.position = position;
        vertex.normal = glm::vec3(0.0f);
        vertices_.push_back(vertex);
    }

    for (const ImportedSceneTriangle &triangle : imported.triangles)
    {
        triangles_.push_back({triangle.vertexIndices[0], triangle.vertexIndices[1],
                              triangle.vertexIndices[2]});
        for (int i = 0; i < 3; ++i)
        {
            const unsigned int vertexIndex = triangle.vertexIndices[i];
            vertices.push_back({imported.positions[vertexIndex], triangle.normals[i],
                                triangle.texcoords[i], 0.0f, 0.0f});
            sourceVertexIndices.push_back(vertexIndex);
        }
        uvTriangles_.push_back({triangle.vertexIndices, triangle.texcoords});
    }

    if (vertices.size() > static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
        throw std::runtime_error("OBJ is too large for one draw call");

    if (centerMesh) {
        glm::vec3 lo = vertices.front().position, hi = lo;
        // Fit arbitrary input units into the default camera's view without distortion.
        // Uniform scaling preserves the directions of the imported normals.
        for (const auto &v : vertices)
        {
            lo = glm::min(lo, v.position);
            hi = glm::max(hi, v.position);
        }

        const glm::vec3 center = lo * 0.5f + hi * 0.5f;
        const glm::vec3 extent = hi - lo;
        const float span = std::max({extent.x, extent.y, extent.z});
        if (!std::isfinite(span) || span <= 0.0f)
            throw std::runtime_error("OBJ bounds cannot be normalized");

        for (auto &v : vertices)
            v.position = (v.position - center) * (2.0f / span);

        for (auto &v : vertices_)
            v.position = (v.position - center) * (2.0f / span);
    }
    if (hasTextureCoordinates_)
    {
        glm::vec2 uvMin(std::numeric_limits<float>::max());
        glm::vec2 uvMax(std::numeric_limits<float>::lowest());
        for (const auto &vertex : vertices)
        {
            uvMin = glm::min(uvMin, vertex.texcoord);
            uvMax = glm::max(uvMax, vertex.texcoord);
        }
        const glm::vec2 uvExtent = uvMax - uvMin;
        if (uvExtent.x <= 1e-8f || uvExtent.y <= 1e-8f)
        {
            hasTextureCoordinates_ = false;
        }
        else
        {
            constexpr float unitTileTolerance = 0.01f;
            usesUVNoiseMapping_ =
                uvMin.x >= -unitTileTolerance &&
                uvMin.y >= -unitTileTolerance &&
                uvMax.x <= 1.0f + unitTileTolerance &&
                uvMax.y <= 1.0f + unitTileTolerance;

            if (usesUVNoiseMapping_)
            {
                for (auto &vertex : vertices)
                    vertex.texcoord = (vertex.texcoord - uvMin) / uvExtent;
                for (auto &triangle : uvTriangles_)
                    for (auto &texcoord : triangle.texcoords)
                        texcoord = (texcoord - uvMin) / uvExtent;
            }
        }
    }

    populateVertexNeighbors();
    populateVertexNormals();
    populateVertexCurvatures();
    orientPrincipalDirections();

    for (size_t i = 0; i < vertices.size(); ++i)
    {
        const auto &curvature = vertices_[sourceVertexIndices[i]].curvature;
        vertices[i].k1 = curvature.k1;
        vertices[i].k2 = curvature.k2;
    }

    count_ = static_cast<GLsizei>(vertices.size());
    number_parts = count_ / 3;

    // Upload once: CPU import data can be discarded after glBufferData copies it.
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);

    // Stride skips a whole Vertex; offsetof selects each field within that vertex.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, normal)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                      reinterpret_cast<void *>(offsetof(Vertex, k1)));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                      reinterpret_cast<void *>(offsetof(Vertex, k2)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, texcoord)));
    glBindVertexArray(0);
    std::cout << "Assembly: " << count_ / 3 << " triangles\n";

    setupNormalBuffers();
    setupCurveBuffers();
}

void Mesh::populateVertexNeighbors() {
    for (const auto& tri : triangles_) {
        unsigned int indices[3] = {tri.v0, tri.v1, tri.v2};

        for (int i = 0; i < 3; i++) {
            unsigned int vertexInd = indices[i];
            GeometryVertex& vertex = vertices_[vertexInd];

            // get the other two vertices
            for (int j = 0; j < 3; j++) {
                // skip the current index
                if (i == j) {
                    continue;
                }
                unsigned int neighborInd = indices[j];
                auto& neighbors = vertex.neighbors;
                if (std::find(neighbors.begin(), neighbors.end(), neighborInd) == neighbors.end()) {
                    neighbors.push_back(neighborInd);
                }
            }
        }
    }
}

void Mesh::populateVertexNormals() {
    for (const auto& tri : triangles_) {
        unsigned int ind0 = tri.v0;
        unsigned int ind1 = tri.v1;
        unsigned int ind2 = tri.v2;

        GeometryVertex& v0 = vertices_[ind0];
        GeometryVertex& v1 = vertices_[ind1];
        GeometryVertex& v2 = vertices_[ind2];

        glm::vec3 p0 = v0.position;
        glm::vec3 p1 = v1.position;
        glm::vec3 p2 = v2.position;

        glm::vec3 faceNormal = cross(p1 - p0, p2 - p0);

        v0.normal += faceNormal;
        v1.normal += faceNormal;
        v2.normal += faceNormal;
    }
    for (auto& v : vertices_) {
        v.normal = normalize(v.normal);
    }
}

// based off Rusinkiewicz - Estimating Curvatures and Their Derivatives on Triangle Meshes
using CoordinateSystem = std::pair<glm::vec3, glm::vec3>;
void Mesh::populateVertexCurvatures() {
    // initialize a list of u_p, v_p where (u,v) are the directions of an orthonormal coordinate system in the tangent frame of the vertex
    std::vector<CoordinateSystem> uv_p;
    for (std::size_t i = 0; i < vertices_.size(); i++) {
        GeometryVertex v = vertices_[i];
        if (length(v.normal) < 1e-8f) {
            // Handle invalid normal.
            uv_p.emplace_back(glm::vec3(0.0f), glm::vec3(0.0f));
            continue;
        }

        glm::vec3 n_p = normalize(v.normal);
        // Pick a reference vector that isn't parallel to n_p
        glm::vec3 reference(0.0f, 1.0f, 0.0f);

        if (abs(dot(n_p, reference)) > 0.99f) {
            reference = glm::vec3(1.0f, 0.0f, 0.0f);
        }

        // Construct tangent frame
        glm::vec3 u_p = normalize(cross(reference, n_p));
        glm::vec3 v_p = cross(n_p, u_p);
        CoordinateSystem uv = std::make_pair(u_p, v_p);
        uv_p.emplace_back(uv);
    }
    std::vector<Eigen::Matrix2f> vertexTensors(vertices_.size(), Eigen::Matrix2f::Zero());
    std::vector<float> vertexWeights(vertices_.size(), 0.0f);
    for (const auto& tri : triangles_) {
        // Compute the curvature of each face
        unsigned int ind0 = tri.v0;
        unsigned int ind1 = tri.v1;
        unsigned int ind2 = tri.v2;

        GeometryVertex& v0 = vertices_[ind0];
        GeometryVertex& v1 = vertices_[ind1];
        GeometryVertex& v2 = vertices_[ind2];

        std::array<unsigned int, 3> indices{ ind0, ind1, ind2 };

        glm::vec3 e0 = v2.position - v1.position;
        glm::vec3 e1 = v0.position - v2.position;
        glm::vec3 e2 = v1.position - v0.position;

        glm::vec3 faceCross = cross(
            v1.position - v0.position,
            v2.position - v0.position
        );

        // check for degenerate triangles
        if (length(faceCross) < 1e-8f) 
            continue;

        glm::vec3 n_f = normalize(faceCross);

        // get u v directions in tangent frame of face
        glm::vec3 u_f = normalize(e0);
        glm::vec3 v_f = cross(n_f, u_f);

        glm::vec3 n0 = normalize(v0.normal);
        glm::vec3 n1 = normalize(v1.normal);
        glm::vec3 n2 = normalize(v2.normal);

        // construct system of equations to solve for tensor
        Eigen::Matrix<float, 6, 3> A;
        A <<
            dot(e0, u_f), dot(e0, v_f), 0.0f,
            0.0f, dot(e0, u_f), dot(e0, v_f),
            dot(e1, u_f), dot(e1, v_f), 0.0f,
            0.0f, dot(e1, u_f), dot(e1, v_f),
            dot(e2, u_f), dot(e2, v_f), 0.0f,
            0.0f, dot(e2, u_f), dot(e2, v_f);
        Eigen::Matrix<float, 6, 1> B;
        B <<
            dot(n2 - n1, u_f),
            dot(n2 - n1, v_f),
            dot(n0 - n2, u_f),
            dot(n0 - n2, v_f),
            dot(n1 - n0, u_f),
            dot(n1 - n0, v_f);

        Eigen::ColPivHouseholderQR<Eigen::Matrix<float, 6, 3>> qr(A);
        if (qr.rank() < 3)
            continue;

        const Eigen::Vector3f faceTensor = qr.solve(B);
        if (!faceTensor.allFinite())
            continue;

        Eigen::Matrix2f T_f;
        T_f << faceTensor(0), faceTensor(1),
            faceTensor(1), faceTensor(2);

        for (auto index : indices) {
            // re-express face tensor in terms of vertex coordinate system
            // add tensor, with weight, to vertex curvature
            glm::vec3 u_p = uv_p[index].first;
            glm::vec3 v_p = uv_p[index].second;

            Eigen::Matrix2f R;
            R << dot(u_f, u_p), dot(u_f, v_p),
                dot(v_f, u_p), dot(v_f, v_p);
            Eigen::Matrix2f T_p = R.transpose() * T_f * R;

            // give the face tensor a weight based on the area of the triangle
            float weight = 0.5f * length(cross(v1.position - v0.position, v2.position - v0.position));
            vertexTensors[index] += weight * T_p;
            vertexWeights[index] += weight;
        }
    }
    for (std::size_t i = 0; i < vertices_.size(); i++) {
        if (vertexWeights[i] <= 0.0f)
            continue;

        vertexTensors[i] /= vertexWeights[i];
        // extract eigenvalues/vectors for curvature and direction data
        // save in vertex
        Eigen::SelfAdjointEigenSolver<Eigen::Matrix2f> solver(vertexTensors[i]);

        if (solver.info() != Eigen::Success)
            continue;

        Eigen::Vector2f curvatures = solver.eigenvalues();
        Eigen::Matrix2f directions = solver.eigenvectors();
        // Eigen returns eigenvalues in ascending order.
        vertices_[i].curvature.k1 = curvatures(0);
        vertices_[i].curvature.k2 = curvatures(1);

        // Convert the 2D eigenvectors into 3D tangent-space directions.
        glm::vec3 u_p = uv_p[i].first;
        glm::vec3 v_p = uv_p[i].second;

        Eigen::Vector2f d1 = directions.col(0);
        Eigen::Vector2f d2 = directions.col(1);

        vertices_[i].curvature.dir1 =
            normalize(d1(0) * u_p + d1(1) * v_p);

        vertices_[i].curvature.dir2 =
            normalize(d2(0) * u_p + d2(1) * v_p);
        }
}

std::vector<glm::vec2> Mesh::createPrincipalDirectionField(
    int size, PrincipalDirection selectedDirection) const
{
    return createDirectionField(size, selectedDirection, -1, 0);
}

std::array<std::vector<glm::vec2>, 6> Mesh::createTriplanarPrincipalDirectionFields(
    int size, PrincipalDirection selectedDirection) const
{
    if (size <= 0)
        throw std::invalid_argument("Direction field size must be positive");

    std::array<std::vector<glm::vec2>, 6> fields;
    for (int axis = 0; axis < 3; ++axis)
    {
        fields[axis * 2] =
            createDirectionField(size, selectedDirection, axis, 1);
        fields[axis * 2 + 1] =
            createDirectionField(size, selectedDirection, axis, -1);
    }
    return fields;
}

std::vector<glm::vec2> Mesh::createDirectionField(
    int size, PrincipalDirection selectedDirection,
    int projectionAxis, int projectionSign) const
{
    if (projectionAxis == -1 && !usesUVNoiseMapping_)
        throw std::logic_error(
            "Cannot bake a UV principal direction field without single-tile OBJ UVs");
    if (size <= 0)
        throw std::invalid_argument("Direction field size must be positive");
    if (projectionAxis < -1 || projectionAxis > 2 ||
        (projectionAxis >= 0 && projectionSign != 1 && projectionSign != -1))
        throw std::invalid_argument("Invalid principal direction projection");

    const size_t texelCount = static_cast<size_t>(size) * size;
    std::vector<glm::vec2> accumulated(texelCount, glm::vec2(0.0f));

    auto principalDirection = [selectedDirection](const PrincipalCurvature &curvature) {
        return selectedDirection == PrincipalDirection::Dir1
            ? curvature.dir1
            : curvature.dir2;
    };
    auto isValid = [](const PrincipalCurvature &curvature, const glm::vec3 &direction) {
        const float gap = std::abs(curvature.k2 - curvature.k1);
        const float scale = std::max(
            {std::abs(curvature.k1), std::abs(curvature.k2), 1e-6f});
        return glm::length(direction) > 1e-6f && gap > 1e-4f * scale;
    };
    auto cross2 = [](const glm::vec2 &a, const glm::vec2 &b) {
        return a.x * b.y - a.y * b.x;
    };
    auto projectedUV = [projectionAxis](const glm::vec3 &position) {
        if (projectionAxis == 0)
            return glm::vec2(0.5f) + 0.5f * glm::vec2(position.y, position.z);
        if (projectionAxis == 1)
            return glm::vec2(0.5f) + 0.5f * glm::vec2(position.x, position.z);
        return glm::vec2(0.5f) + 0.5f * glm::vec2(position.x, position.y);
    };

    for (const UVTriangle &triangle : uvTriangles_)
    {
        const GeometryVertex &v0 = vertices_[triangle.vertexIndices[0]];
        const GeometryVertex &v1 = vertices_[triangle.vertexIndices[1]];
        const GeometryVertex &v2 = vertices_[triangle.vertexIndices[2]];
        glm::vec2 uv0;
        glm::vec2 uv1;
        glm::vec2 uv2;
        if (projectionAxis >= 0)
        {
            const glm::vec3 faceNormal =
                glm::cross(v1.position - v0.position, v2.position - v0.position);
            const float normalLength = glm::length(faceNormal);
            if (normalLength < 1e-8f)
                continue;
            const glm::vec3 normalizedNormal = faceNormal / normalLength;
            const glm::vec3 absoluteNormal = glm::abs(normalizedNormal);
            int dominantAxis = 0;
            if (absoluteNormal.y > absoluteNormal.x)
                dominantAxis = 1;
            if (absoluteNormal.z > absoluteNormal[dominantAxis])
                dominantAxis = 2;
            if (dominantAxis != projectionAxis ||
                (normalizedNormal[projectionAxis] > 0.0f ? 1 : -1) != projectionSign)
                continue;
            uv0 = projectedUV(v0.position);
            uv1 = projectedUV(v1.position);
            uv2 = projectedUV(v2.position);
        }
        else
        {
            uv0 = triangle.texcoords[0];
            uv1 = triangle.texcoords[1];
            uv2 = triangle.texcoords[2];
        }
        const float uvDeterminant = cross2(uv1 - uv0, uv2 - uv0);
        if (std::abs(uvDeterminant) < 1e-10f)
            continue;

        const glm::vec3 edge1 = v1.position - v0.position;
        const glm::vec3 edge2 = v2.position - v0.position;
        const float g00 = glm::dot(edge1, edge1);
        const float g01 = glm::dot(edge1, edge2);
        const float g11 = glm::dot(edge2, edge2);
        const float gramDeterminant = g00 * g11 - g01 * g01;
        if (gramDeterminant < 1e-12f)
            continue;

        const float minU = std::min({uv0.x, uv1.x, uv2.x});
        const float maxU = std::max({uv0.x, uv1.x, uv2.x});
        const float minV = std::min({uv0.y, uv1.y, uv2.y});
        const float maxV = std::max({uv0.y, uv1.y, uv2.y});
        const int minX = std::max(0, static_cast<int>(std::floor(minU * size - 0.5f)));
        const int maxX = std::min(size - 1, static_cast<int>(std::ceil(maxU * size - 0.5f)));
        const int minY = std::max(0, static_cast<int>(std::floor(minV * size - 0.5f)));
        const int maxY = std::min(size - 1, static_cast<int>(std::ceil(maxV * size - 0.5f)));

        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                const glm::vec2 uvPoint(
                    (static_cast<float>(x) + 0.5f) / size,
                    (static_cast<float>(y) + 0.5f) / size);
                const float b1 = cross2(uvPoint - uv0, uv2 - uv0) / uvDeterminant;
                const float b2 = cross2(uv1 - uv0, uvPoint - uv0) / uvDeterminant;
                const float b0 = 1.0f - b1 - b2;
                if (b0 < -1e-5f || b1 < -1e-5f || b2 < -1e-5f)
                    continue;

                glm::vec3 directionSum(0.0f);
                glm::vec3 reference(0.0f);
                float validWeight = 0.0f;
                const GeometryVertex *corners[3] = {&v0, &v1, &v2};
                const float barycentric[3] = {
                    std::max(0.0f, b0), std::max(0.0f, b1), std::max(0.0f, b2)};
                for (int corner = 0; corner < 3; ++corner)
                {
                    const glm::vec3 cornerDirection =
                        principalDirection(corners[corner]->curvature);
                    if (!isValid(corners[corner]->curvature, cornerDirection))
                        continue;
                    if (glm::length(reference) < 1e-6f)
                        reference = glm::normalize(cornerDirection);
                    glm::vec3 aligned = glm::normalize(cornerDirection);
                    if (glm::dot(aligned, reference) < 0.0f)
                        aligned = -aligned;
                    directionSum += barycentric[corner] * aligned;
                    validWeight += barycentric[corner];
                }
                if (validWeight <= 1e-6f || glm::length(directionSum) < 1e-6f)
                    continue;

                const glm::vec3 surfaceDirection =
                    glm::normalize(directionSum / validWeight);
                const float rhs0 = glm::dot(surfaceDirection, edge1);
                const float rhs1 = glm::dot(surfaceDirection, edge2);
                const float alpha = (rhs0 * g11 - rhs1 * g01) / gramDeterminant;
                const float beta = (rhs1 * g00 - rhs0 * g01) / gramDeterminant;
                glm::vec2 uvDirection =
                    alpha * (uv1 - uv0) + beta * (uv2 - uv0);
                uvDirection *= static_cast<float>(size);
                const float directionLength = glm::length(uvDirection);
                if (directionLength < 1e-6f || !std::isfinite(directionLength))
                    continue;
                uvDirection /= directionLength;

                const size_t index = static_cast<size_t>(y) * size + x;
                if (glm::dot(accumulated[index], uvDirection) < 0.0f)
                    uvDirection = -uvDirection;
                accumulated[index] += uvDirection;
            }
        }
    }

    for (glm::vec2 &direction : accumulated)
    {
        const float length = glm::length(direction);
        direction = length > 1e-6f ? direction / length : glm::vec2(0.0f);
    }
    return accumulated;
}

std::vector<glm::vec3> Mesh::createNormalVisualization() const {
    std::vector<glm::vec3> normalData;
    const float lineLength = 0.05f;

    for (const auto& v : vertices_) {
        glm::vec3 start = v.position;
        glm::vec3 end = v.position + lineLength * v.normal;
        normalData.push_back(start);
        normalData.push_back(end);
    }

    return normalData;
}

void Mesh::orientPrincipalDirections()
{
    const std::size_t numVertices = vertices_.size();

    // Use a fixed seed so the result is reproducible.
    std::mt19937 rng(42);

    auto orientField = [&](bool useDir1)
    {
        // Access either principal direction field.
        auto getDirection = [&](std::size_t i) -> glm::vec3&
        {
            return useDir1
                ? vertices_[i].curvature.dir1
                : vertices_[i].curvature.dir2;
        };

        // Identify vertices where the principal directions are meaningful.
        std::vector<bool> valid(numVertices, false);

        for (std::size_t i = 0; i < numVertices; i++)
        {
            auto& curvature = vertices_[i].curvature;
            glm::vec3& direction = getDirection(i);

            float directionLength = glm::length(direction);
            float curvatureGap = std::abs(curvature.k2 - curvature.k1);

            // Avoid nearly isotropic regions where principal directions
            // are poorly defined.
            float curvatureScale = std::max(
                { std::abs(curvature.k1), std::abs(curvature.k2), 1e-6f }
            );

            valid[i] =
                directionLength > 1e-6f &&
                curvatureGap > 1e-4f * curvatureScale;

            if (valid[i])
                direction /= directionLength;
        }

        // Find connected components of valid vertices.
        std::vector<bool> componentVisited(numVertices, false);

        for (std::size_t start = 0; start < numVertices; start++)
        {
            if (!valid[start] || componentVisited[start])
                continue;

            std::vector<unsigned int> component;
            std::queue<unsigned int> queue;

            queue.push(static_cast<unsigned int>(start));
            componentVisited[start] = true;

            while (!queue.empty())
            {
                unsigned int current = queue.front();
                queue.pop();

                component.push_back(current);

                for (unsigned int neighbor : vertices_[current].neighbors)
                {
                    if (neighbor >= numVertices ||
                        !valid[neighbor] ||
                        componentVisited[neighbor])
                    {
                        continue;
                    }

                    componentVisited[neighbor] = true;
                    queue.push(neighbor);
                }
            }

            // Save the original directions for this component.
            std::vector<glm::vec3> original(numVertices);

            for (unsigned int index : component)
                original[index] = getDirection(index);

            std::vector<glm::vec3> bestDirections = original;
            float bestScore = -std::numeric_limits<float>::infinity();

            // Try several different BFS starting vertices.
            std::vector<unsigned int> seeds = component;
            std::shuffle(seeds.begin(), seeds.end(), rng);

            const std::size_t numTrials =
                std::min<std::size_t>(16, seeds.size());

            for (std::size_t trial = 0; trial < numTrials; trial++)
            {
                std::vector<glm::vec3> candidate = original;
                std::vector<bool> visited(numVertices, false);

                std::queue<unsigned int> bfsQueue;

                unsigned int seed = seeds[trial];
                bfsQueue.push(seed);
                visited[seed] = true;

                // Greedy BFS sign correction.
                while (!bfsQueue.empty())
                {
                    unsigned int current = bfsQueue.front();
                    bfsQueue.pop();

                    for (unsigned int neighbor : vertices_[current].neighbors)
                    {
                        if (neighbor >= numVertices ||
                            !valid[neighbor] ||
                            visited[neighbor])
                        {
                            continue;
                        }

                        // Flip the neighbor if it points opposite
                        // to the current direction.
                        if (glm::dot(
                                candidate[current],
                                candidate[neighbor]) < 0.0f)
                        {
                            candidate[neighbor] *= -1.0f;
                        }

                        visited[neighbor] = true;
                        bfsQueue.push(neighbor);
                    }
                }

                // Evaluate smoothness over all undirected edges.
                float score = 0.0f;

                for (unsigned int i : component)
                {
                    for (unsigned int j : vertices_[i].neighbors)
                    {
                        if (j > i && valid[j])
                        {
                            score += glm::dot(candidate[i], candidate[j]);
                        }
                    }
                }

                // Keep the orientation with the highest smoothness.
                if (score > bestScore)
                {
                    bestScore = score;
                    bestDirections = candidate;
                }
            }

            // Apply the best orientation to this component.
            for (unsigned int index : component)
                getDirection(index) = bestDirections[index];
        }
    };

    // Orient the two principal direction fields independently.
    orientField(true);
    orientField(false);
}

std::vector<glm::vec3> Mesh::createCurveDirectionVisuals() const {
    std::vector<glm::vec3> curveData;
    const float maxLineLength = 0.01f; // length of vector along maximal curvature direction
    const float minLineLength = 0.005f; // length of vector along minimal curvature direction

    for (const auto& v : vertices_) {
        glm::vec3 start = v.position;
        glm::vec3 end1 = v.position + minLineLength * v.curvature.dir1;
        glm::vec3 end2 = v.position + maxLineLength * v.curvature.dir2;
        curveData.push_back(start);
        curveData.push_back(end1);
        curveData.push_back(start);
        curveData.push_back(end2);
    }

    return curveData;
}

void Mesh::setupNormalBuffers()
{
    std::vector<glm::vec3> normalData = createNormalVisualization();

    if (normalData.size() > static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
    {
        throw std::runtime_error("Too many normal visualization vertices");
    }
    normalCount_ = static_cast<GLsizei>(normalData.size());

    glGenVertexArrays(1, &normalVAO_);
    glGenBuffers(1, &normalVBO_);

    glBindVertexArray(normalVAO_);

    glBindBuffer(GL_ARRAY_BUFFER, normalVBO_);
    glBufferData(
        GL_ARRAY_BUFFER,
        normalData.size() * sizeof(glm::vec3),
        normalData.data(),
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(glm::vec3),
        nullptr
    );

    glBindVertexArray(0);
}

void Mesh::setupCurveBuffers()
{
    std::vector<glm::vec3> curveData = createCurveDirectionVisuals();

    if (curveData.size() > static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
    {
        throw std::runtime_error("Too many normal visualization vertices");
    }
    curveCount_ = static_cast<GLsizei>(curveData.size());

    glGenVertexArrays(1, &curveVAO_);
    glGenBuffers(1, &curveVBO_);

    glBindVertexArray(curveVAO_);

    glBindBuffer(GL_ARRAY_BUFFER, curveVBO_);
    glBufferData(
        GL_ARRAY_BUFFER,
        curveData.size() * sizeof(glm::vec3),
        curveData.data(),
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(glm::vec3),
        nullptr
    );

    glBindVertexArray(0);
}

Mesh::~Mesh()
{
    // The owning scene must be destroyed while its OpenGL context is still current.
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
    glDeleteBuffers(1, &normalVBO_);
    glDeleteVertexArrays(1, &normalVAO_);
}

void Mesh::DrawVertexNormals() const
{
    glBindVertexArray(normalVAO_);
    glDrawArrays(GL_LINES, 0, normalCount_);
    glBindVertexArray(0);
}

void Mesh::DrawCurvatureDirections() const
{
    glBindVertexArray(curveVAO_);
    glDrawArrays(GL_LINES, 0, curveCount_);
    glBindVertexArray(0);
}


void Mesh::Draw() const
{
    // Vertices are already expanded, so no element/index buffer is needed here.
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, count_);
}
