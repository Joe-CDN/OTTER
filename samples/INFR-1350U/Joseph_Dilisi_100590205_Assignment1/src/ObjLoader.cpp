#include "ObjLoader.h"

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>

#pragma region String Trimming

// trim from start (in place)
static inline void ltrim(std::string& s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
		return !std::isspace(ch);
		}));
}

// trim from end (in place)
static inline void rtrim(std::string& s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
		return !std::isspace(ch);
		}).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string& s) {
	ltrim(s);
	rtrim(s);
}

#pragma endregion 

VertexArrayObject::sptr ObjLoader::LoadFromFile(const std::string& filename)
{
	// Open our file in binary mode
	std::ifstream file;
	file.open(filename, std::ios::binary);

	// If our file fails to open, we will throw an error
	if (!file) {
		throw std::runtime_error("Failed to open file");
	}

	MeshBuilder<VertexPosNormTexCol> mesh;
	std::string line;

	std::vector<glm::vec3> vertexData;
	std::vector<glm::vec2> textureData;
	std::vector<glm::vec3> normalData;
	std::vector <uint32_t> vertInd;
	std::vector <uint32_t> texInd;
	std::vector <uint32_t> normInd;

	glm::vec4 color = glm::vec4(1.0f);

	while (std::getline(file, line))
	{
		trim(line);
		if (line.substr(0, 1) == "#")
		{
			// Comment, no-op
			continue;
		}
		else if (line.substr(0, 2) == "v ")//read vertex data
		{
			std::istringstream ss = std::istringstream(line.substr(2));
			glm::vec3 pos;
			ss >> pos.x >> pos.y >> pos.z;
			vertexData.push_back(glm::vec3(pos.x, pos.y, pos.z));
		}
		else if (line.substr(0, 3) == "vt ")//read uv data
		{
			std::istringstream ss = std::istringstream(line.substr(3));
			glm::vec2 uv;
			ss >> uv.x >> uv.y;
			textureData.push_back(glm::vec2(uv.x, uv.y));
		}
		else if (line.substr(0, 3) == "vn ")//read normal data
		{
			std::istringstream ss = std::istringstream(line.substr(3));
			glm::vec3 norm;
			ss >> norm.x >> norm.y >> norm.z;
			normalData.push_back(glm::vec3(norm.x, norm.y, norm.z));
		}
		else if (line.substr(0, 2) == "f ")
		{
			std::istringstream ss = std::istringstream(line.substr(2));
			char temp;
			uint32_t p1, p2, p3;
			uint32_t n1, n2, n3;
			uint32_t uv1, uv2, uv3;

			ss >> p1 >> temp >> uv1 >> temp >> n1 >>
				  p2 >> temp >> uv2 >> temp >> n2 >>
				  p3 >> temp >> uv3 >> temp >> n3;

			vertInd.push_back(p1);
			vertInd.push_back(p2);
			vertInd.push_back(p3);
			texInd.push_back(uv1);
			texInd.push_back(uv2);
			texInd.push_back(uv3);
			normInd.push_back(n1);
			normInd.push_back(n2);
			normInd.push_back(n3);
		}
	}

	mesh.ReserveVertexSpace(vertInd.size());
	for (unsigned i = 0; i < vertInd.size(); i++)
	{			
		uint32_t vertexIndex = vertInd[i] - 1;
		uint32_t normalIndex = normInd[i] - 1;
		uint32_t textureIndex = texInd[i] - 1;
		mesh.AddVertex(vertexData[vertexIndex], normalData[normalIndex], textureData[textureIndex], color);
		mesh.AddIndex(i);
	}
	
	return mesh.Bake();
}
