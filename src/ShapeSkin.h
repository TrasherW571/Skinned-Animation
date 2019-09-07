#pragma once
#ifndef _SHAPESKIN_H_
#define _SHAPESKIN_H_

#include <iostream>
#include <memory>
#include <vector>

class MatrixStack;
class Program;

class ShapeSkin
{
public:
	ShapeSkin();
	virtual ~ShapeSkin();
	void loadMesh(const std::string &meshName);
	void loadAttachment(const std::string &filename);
	void loadSkeleton(const std::string &filename);
	void setProgram(std::shared_ptr<Program> p) { prog = p; }
	void init();
	void draw();
	void Skinning();
	void setTime(int t) { k = t; }
	std::vector<std::vector<glm::mat4> > getFrames() { return BoneFrames; }
	std::vector<std::vector<float> > getWeights() { return Weights; }
	
	
private:
	std::shared_ptr<Program> prog;
	std::vector<unsigned int> elemBuf;
	std::vector<float> posBuf;
	std::vector<float> norBuf;
	std::vector<float> texBuf;
	std::vector<glm::mat4> Bind;
	std::vector<std::vector<glm::mat4> > BoneFrames;
	std::vector<std::vector<float> > Weights;
	std::vector<std::vector<int> > Weight_Indices;
	int k;
	std::vector<float> Update_posBuf;
	std::vector<float> Update_norBuf;
	unsigned elemBufID;
	unsigned posBufID;
	unsigned norBufID;
	unsigned texBufID;
};

#endif
