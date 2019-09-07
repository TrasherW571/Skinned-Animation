#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <memory>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "GLSL.h"
#include "Program.h"
#include "Camera.h"
#include "MatrixStack.h"
#include "ShapeSkin.h"

using namespace std;

GLFWwindow *window; // Main application window
string RESOURCE_DIR = ""; // Where the resources are loaded from
string MESH_FILE = "";
string ATTACHMENT_FILE = "";
string SKELETON_FILE = "";
bool keyToggles[256] = {false};

shared_ptr<Camera> camera = NULL;
shared_ptr<ShapeSkin> shape = NULL;
shared_ptr<Program> progSimple = NULL;
shared_ptr<Program> progSkin = NULL;
vector<vector<glm::mat4> > Frames;
int k;


static void error_callback(int error, const char *description)
{
	cerr << description << endl;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, GL_TRUE);
	}
}

static void char_callback(GLFWwindow *window, unsigned int key)
{
	keyToggles[key] = !keyToggles[key];
	switch(key) {
		case 'g':
			break;
	}
}

static void cursor_position_callback(GLFWwindow* window, double xmouse, double ymouse)
{
	int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
	if(state == GLFW_PRESS) {
		camera->mouseMoved(xmouse, ymouse);
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	// Get the current mouse position.
	double xmouse, ymouse;
	glfwGetCursorPos(window, &xmouse, &ymouse);
	// Get current window size.
	int width, height;
	glfwGetWindowSize(window, &width, &height);
	if(action == GLFW_PRESS) {
		bool shift = mods & GLFW_MOD_SHIFT;
		bool ctrl  = mods & GLFW_MOD_CONTROL;
		bool alt   = mods & GLFW_MOD_ALT;
		camera->mouseClicked(xmouse, ymouse, shift, ctrl, alt);
	}
}

void loadScene(const string &meshFile, const string &attachmentFile, const string &skeletonFile)
{
	keyToggles[(unsigned)'c'] = true;
	
	camera = make_shared<Camera>();
	
	// Single shape for all the animations.
	shape = make_shared<ShapeSkin>();
	shape->loadMesh(meshFile);
	shape->loadAttachment(attachmentFile);
	shape->loadSkeleton(skeletonFile);
	Frames = shape->getFrames();
	// For drawing the grid, etc.
	progSimple = make_shared<Program>();
	progSimple->setShaderNames(RESOURCE_DIR + "simple_vert.glsl", RESOURCE_DIR + "simple_frag.glsl");
	progSimple->setVerbose(true);
	
	// For skinned shape, CPU/GPU
	progSkin = make_shared<Program>();
	progSkin->setShaderNames(RESOURCE_DIR + "skin_vert.glsl", RESOURCE_DIR + "skin_frag.glsl");
	progSkin->setVerbose(true);
}

void init()
{
	// Non-OpenGL things
	loadScene(MESH_FILE, ATTACHMENT_FILE, SKELETON_FILE);
	
	// Set background color
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	// Enable z-buffer test
	glEnable(GL_DEPTH_TEST);
	// Enable alpha blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	shape->init();
	progSimple->init();
	progSimple->addUniform("P");
	progSimple->addUniform("MV");
	
	progSkin->init();
	progSkin->addAttribute("aPos");
	progSkin->addAttribute("aNor");
	progSkin->addUniform("P");
	progSkin->addUniform("MV");
	progSkin->addUniform("lightPos");
	progSkin->addUniform("ka");
	progSkin->addUniform("kd");
	progSkin->addUniform("ks");
	progSkin->addUniform("s");
	progSkin->addUniform("i");
	
	// Initialize time.
	glfwSetTime(0.0);
	
	GLSL::checkError(GET_FILE_LINE);
}

void render()
{
	// Update time.
	double t = glfwGetTime();
	k = floor(t*50);
	k = k % Frames.size();
	shape->setTime(k);
	// Get current frame buffer size.
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	glViewport(0, 0, width, height);
	
	// Use the window size for camera.
	glfwGetWindowSize(window, &width, &height);
	camera->setAspect((float)width/(float)height);
	
	// Clear buffers
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if(keyToggles[(unsigned)'c']) {
		glEnable(GL_CULL_FACE);
	} else {
		glDisable(GL_CULL_FACE);
	}
	if(keyToggles[(unsigned)'z']) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	} else {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	
	auto P = make_shared<MatrixStack>();
	auto MV = make_shared<MatrixStack>();
	
	// Apply camera transforms
	P->pushMatrix();
	camera->applyProjectionMatrix(P);
	MV->pushMatrix();
	camera->applyViewMatrix(MV);
	
	// Draw grid
	progSimple->bind();
	glUniformMatrix4fv(progSimple->getUniform("P"), 1, GL_FALSE, glm::value_ptr(P->topMatrix()));
	glUniformMatrix4fv(progSimple->getUniform("MV"), 1, GL_FALSE, glm::value_ptr(MV->topMatrix()));
	float gridSizeHalf = 5.0f;
	int gridNx = 11;
	int gridNz = 11;
	glLineWidth(1);
	glColor3f(0.8f, 0.8f, 0.8f);
	glBegin(GL_LINES);
	for(int i = 0; i < gridNx; ++i) {
		float alpha = i / (gridNx - 1.0f);
		float x = (1.0f - alpha) * (-gridSizeHalf) + alpha * gridSizeHalf;
		glVertex3f(x, 0, -gridSizeHalf);
		glVertex3f(x, 0,  gridSizeHalf);
	}
	for(int i = 0; i < gridNz; ++i) {
		float alpha = i / (gridNz - 1.0f);
		float z = (1.0f - alpha) * (-gridSizeHalf) + alpha * gridSizeHalf;
		glVertex3f(-gridSizeHalf, 0, z);
		glVertex3f( gridSizeHalf, 0, z);
	}
	glEnd();
	progSimple->unbind();

	// Draw character
	MV->pushMatrix();
	progSkin->bind();
	glUniformMatrix4fv(progSkin->getUniform("P"), 1, GL_FALSE, glm::value_ptr(P->topMatrix()));
	glUniformMatrix4fv(progSkin->getUniform("MV"), 1, GL_FALSE, glm::value_ptr(MV->topMatrix()));
	glUniform3f(progSkin->getUniform("lightPos"), 0.0f, 0.5f, 0.0f);
	glUniform3f(progSkin->getUniform("ka"), 0.3f, 0.3f, 0.3f);
	glUniform3f(progSkin->getUniform("kd"), 0.0f, 0.0f, 1.0f);
	glUniform3f(progSkin->getUniform("ks"), 0.0f, 0.0f, 0.0f);
	glUniform1f(progSkin->getUniform("s"), 200.0f);
	glUniform1f(progSkin->getUniform("i"), 0.8f);
	shape->setProgram(progSkin);
	shape->draw();
	progSkin->unbind();
	MV->popMatrix();
	// Draw Bones
	/*
	progSimple->bind();
		for (int i = 0; i < 18; i++)
		{
			glm::mat4 E; // interpolation matrix
			for (int j = 0; j < 4; j++)
			{
				E[j] = glm::vec4(Frames[k][i][j].x, Frames[k][i][j].y, Frames[k][i][j].z, Frames[k][i][j].w);
			}
			MV->pushMatrix();
			MV->multMatrix(E);
			MV->scale(0.05f);
			glUniformMatrix4fv(progSimple->getUniform("P"), 1, GL_FALSE, glm::value_ptr(P->topMatrix()));
			glUniformMatrix4fv(progSimple->getUniform("MV"), 1, GL_FALSE, glm::value_ptr(MV->topMatrix()));
			glLineWidth(2);
			glBegin(GL_LINES);
			glColor3f(1, 0, 0);
			glVertex3f(0, 0, 0);
			glVertex3f(1, 0, 0);
			glColor3f(0, 1, 0);
			glVertex3f(0, 0, 0);
			glVertex3f(0, 1, 0);
			glColor3f(0, 0, 1);
			glVertex3f(0, 0, 0);
			glVertex3f(0, 0, 1);
			glEnd();
			glLineWidth(1);
			MV->popMatrix();
		}
	progSimple->unbind();
	GLSL::checkError(GET_FILE_LINE);
	*/

	// Pop matrix stacks.
	MV->popMatrix();
	P->popMatrix();

	GLSL::checkError(GET_FILE_LINE);
}

int main(int argc, char **argv)
{
	if(argc < 5) {
		cout << "Usage: Assignment2 <SHADER DIR> <MESH FILE> <ATTACHMENT FILE> <SKELETON FILE>" << endl;
		return 0;
	}
	RESOURCE_DIR = argv[1] + string("/");
	MESH_FILE = argv[2];
	ATTACHMENT_FILE = argv[3];
	SKELETON_FILE = argv[4];
	
	// Set error callback.
	glfwSetErrorCallback(error_callback);
	// Initialize the library.
	if(!glfwInit()) {
		return -1;
	}
	// Create a windowed mode window and its OpenGL context.
	window = glfwCreateWindow(640, 480, "YOUR NAME", NULL, NULL);
	if(!window) {
		glfwTerminate();
		return -1;
	}
	// Make the window's context current.
	glfwMakeContextCurrent(window);
	// Initialize GLEW.
	glewExperimental = true;
	if(glewInit() != GLEW_OK) {
		cerr << "Failed to initialize GLEW" << endl;
		return -1;
	}
	glGetError(); // A bug in glewInit() causes an error that we can safely ignore.
	cout << "OpenGL version: " << glGetString(GL_VERSION) << endl;
	cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
	// Set vsync.
	glfwSwapInterval(1);
	// Set keyboard callback.
	glfwSetKeyCallback(window, key_callback);
	// Set char callback.
	glfwSetCharCallback(window, char_callback);
	// Set cursor position callback.
	glfwSetCursorPosCallback(window, cursor_position_callback);
	// Set mouse button callback.
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	// Initialize scene.
	init();
	// Loop until the user closes the window.
	while(!glfwWindowShouldClose(window)) {
		// Render scene.
		render();
		// Swap front and back buffers.
		glfwSwapBuffers(window);
		// Poll for and process events.
		glfwPollEvents();
	}
	// Quit program.
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
