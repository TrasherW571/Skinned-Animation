#include <iostream>
#include <fstream>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "ShapeSkin.h"
#include "GLSL.h"
#include "Program.h"


using namespace std;

ShapeSkin::ShapeSkin() :
	prog(NULL),
	elemBufID(0),
	posBufID(0),
	norBufID(0),
	texBufID(0)
{
	k = 0;
}

ShapeSkin::~ShapeSkin()
{
}

void ShapeSkin::loadMesh(const string &meshName)
{
	// Load geometry
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	string errStr;
	bool rc = tinyobj::LoadObj(&attrib, &shapes, &materials, &errStr, meshName.c_str());
	if(!rc) {
		cerr << errStr << endl;
	} else {
		posBuf = attrib.vertices;
		Update_posBuf = posBuf;
		norBuf = attrib.normals;
		Update_norBuf = norBuf;
		texBuf = attrib.texcoords;
		assert(posBuf.size() == norBuf.size());
		// Loop over shapes
		for(size_t s = 0; s < shapes.size(); s++) {
			// Loop over faces (polygons)
			const tinyobj::mesh_t &mesh = shapes[s].mesh;
			size_t index_offset = 0;
			for(size_t f = 0; f < mesh.num_face_vertices.size(); f++) {
				size_t fv = mesh.num_face_vertices[f];
				// Loop over vertices in the face.
				for(size_t v = 0; v < fv; v++) {
					// access to vertex
					tinyobj::index_t idx = mesh.indices[index_offset + v];
					elemBuf.push_back(idx.vertex_index);
				}
				index_offset += fv;
				// per-face material (IGNORE)
				shapes[s].mesh.material_ids[f];
			}
		}
	}
}

void ShapeSkin::loadAttachment(const std::string &filename)
{
	int nverts, nbones;
	float B;
	ifstream in;
	in.open(filename);
	if(!in.good()) {
		cout << "Cannot read " << filename << endl;
		return;
	}
	string line;
	getline(in, line); // comment
	getline(in, line); // comment
	getline(in, line);
	stringstream ss0(line);
	ss0 >> nverts;
	ss0 >> nbones;
	assert(nverts == posBuf.size()/3);
	while(1) {
		getline(in, line);
		if(in.eof()) {
			break;
		}
		// Parse line
		stringstream ss(line);
		
		int count = 0;
		vector<float> Bone;
		vector<int> Indices;
		while (count < nbones)
		{
			ss >> B;
			// if B != 0, add it to vector
			// also put in another vector which bone it is
			if (B != 0.0)
			{
				Bone.push_back(B);
				Indices.push_back(count);
			}
			//Bone.push_back(B);
			count++;
		}
		Weights.push_back(Bone);
		Weight_Indices.push_back(Indices);
	}
	in.close();
}

void ShapeSkin::loadSkeleton(const std::string &filename)
{
	int nframes, nbones;
	float qw, qx, qy, qz, px, py, pz;
	ifstream in;
	in.open(filename);
	if (!in.good()) {
		cout << "Cannot read " << filename << endl;
		return;
	}
	string line;
	getline(in, line); // comment
	getline(in, line); // comment
	getline(in, line); // comment
	getline(in, line);
	stringstream ss0(line);
	ss0 >> nframes;
	ss0 >> nbones;

	getline(in, line);
	stringstream ss1(line);
	for (int i = 0; i < 18; i++)
	{
		ss1 >> qx;
		ss1 >> qy;
		ss1 >> qz;
		ss1 >> qw;
		ss1 >> px;
		ss1 >> py;
		ss1 >> pz;
		glm::quat q(qw, qx, qy, qz);
		glm::vec3 p(px, py, pz);
		glm::mat4 B = mat4_cast(q);
		B[3] = glm::vec4(p, 1.0f);
		Bind.push_back(B);
	}
	for (int i = 0; i < 18; i++)
	{
		Bind[i] = glm::inverse(Bind[i]);
	}

	int lineCount = 1;
	while (1) {
		getline(in, line);
		if (in.eof()) {
			break;
		}
		// Parse line
		stringstream ss(line);
		int count = 0;
		vector<glm::mat4> Bones;
		while (count < nbones)
		{
			ss >> qx;
			ss >> qy;
			ss >> qz;
			ss >> qw;
			ss >> px;
			ss >> py;
			ss >> pz;

			glm::quat q(qw, qx, qy, qz);
			glm::vec3 p(px, py, pz);
			glm::mat4 E = mat4_cast(q);
			E[3] = glm::vec4(p, 1.0f);
			Bones.push_back(E);
			count++;
		}
		BoneFrames.push_back(Bones);
		
		lineCount++;
	}

	in.close();
}
void ShapeSkin::init()
{
	// Send the position array to the GPU
	glGenBuffers(1, &posBufID);
	
	// Send the normal array to the GPU
	glGenBuffers(1, &norBufID);

	// No texture info
	texBufID = 0;
	
	// Send the element array to the GPU
	glGenBuffers(1, &elemBufID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elemBufID);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, elemBuf.size()*sizeof(unsigned int), &elemBuf[0], GL_STATIC_DRAW);
	
	// Unbind the arrays
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	assert(glGetError() == GL_NO_ERROR);
}

void ShapeSkin::draw()
{
	assert(prog);
	Skinning();

	glBindBuffer(GL_ARRAY_BUFFER, posBufID);
	glBufferData(GL_ARRAY_BUFFER, posBuf.size() * sizeof(float), &Update_posBuf[0], GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, norBufID);
	glBufferData(GL_ARRAY_BUFFER, norBuf.size() * sizeof(float), &Update_norBuf[0], GL_DYNAMIC_DRAW);

	int h_pos = prog->getAttribute("aPos");
	glEnableVertexAttribArray(h_pos);
	glBindBuffer(GL_ARRAY_BUFFER, posBufID);
	glVertexAttribPointer(h_pos, 3, GL_FLOAT, GL_FALSE, 0, (const void *)0);
	
	int h_nor = prog->getAttribute("aNor");
	glEnableVertexAttribArray(h_nor);
	glBindBuffer(GL_ARRAY_BUFFER, norBufID);
	glVertexAttribPointer(h_nor, 3, GL_FLOAT, GL_FALSE, 0, (const void *)0);
	
	// Draw
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elemBufID);
	glDrawElements(GL_TRIANGLES, (int)elemBuf.size(), GL_UNSIGNED_INT, (const void *)0);
	
	glDisableVertexAttribArray(h_nor);
	glDisableVertexAttribArray(h_pos);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void ShapeSkin::Skinning()
{
	int index = 0;
	for (int i = 0; i < posBuf.size(); i += 3)
	{
		glm::vec4 initVertA(posBuf[i], posBuf[i + 1], posBuf[i + 2], 1.0f);
		glm::vec4 initVertB(norBuf[i], norBuf[i + 1], norBuf[i + 2], 0.0f);
		glm::vec4 A;
		glm::vec4 B;
		glm::vec4 SumA(0.0f, 0.0f, 0.0f, 0.0f);
		glm::vec4 SumB(0.0f, 0.0f, 0.0f, 0.0f);

		for (int j = 0; j < Weight_Indices[index].size(); j++) //18
		{
			int Bone = Weight_Indices[index][j];
			A = (Weights[index][j]) * (BoneFrames[k][Bone]) * Bind[Bone] * initVertA;
			B = (Weights[index][j]) * (BoneFrames[k][Bone]) * Bind[Bone] * initVertB;
			SumA += A;
			SumB += B;
		}
		Update_posBuf[i] = SumA.x;
		Update_posBuf[i+1] = SumA.y;
		Update_posBuf[i+2] = SumA.z;

		Update_norBuf[i] = SumB.x;
		Update_norBuf[i + 1] = SumB.y;
		Update_norBuf[i + 2] = SumB.z;
		index++;
	}
}
