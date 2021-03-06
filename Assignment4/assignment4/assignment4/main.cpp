#include "glsupport.h"
#include "matrix4.h"
#include "geometrymaker.h"
#include "quat.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <iostream>
#include <direct.h>
#include <cstring>
#include <vector>
#include <math.h>
#include <algorithm>

// global variables
GLuint program;

// attributes
GLuint positionAttribute;
GLuint normalAttribute;
GLuint textureCoordinateAttribute;
GLuint binormalAttribute;
GLuint tangentAttribute;
// uniform
GLuint projectionMatrixUniformLocation;
GLuint modelViewMatrixUniformLocation;
GLuint normalMatrixUniformLocation;
//GLuint colorUniformLocation;
GLuint diffuseTextureUniformLocation;
GLuint specularTextureUniformLocation;
GLuint normalTextureUniformLocation;

int windowHeight = 1024;
int windowWidth = 1024;

struct Light {
	GLuint lightPositionUniformLocation;
	GLuint lightColorUniformLocation;
	GLuint specularLightColorUniformLocation;
};

Light lights[2];

Matrix4 eyeMatrix;

struct Vertex {
	Cvec3f position, normal, binormal, tangent;
	Cvec2f texture;
	Vertex() {};
	Vertex(float posX, float posY, float posZ, float normalX, float normalY, float normalZ) : position(posX, posY, posZ), normal(normalX, normalY, normalZ) {};
	Vertex& operator = (const GenericVertex& v) {
		position = v.pos;
		normal = v.normal;
		texture = v.tex;
		binormal = v.binormal;
		tangent = v.tangent;
		return *this;
	}
};
struct Transform {
	Quat rotation;
	//float rotationX;
	//float rotationY;
	//float rotationZ;
	Cvec3 scale;
	Cvec3 translation;
	Transform() : scale(1.0, 1.0, 1.0), translation(0.0, 0.0, 0.0), rotation() {};
	Matrix4 createMatrix() {
		Matrix4 origin;
		origin = Matrix4::makeScale(scale) * origin;
		//origin = Matrix4::makeXRotation(rotationX) * origin;
		//origin = Matrix4::makeYRotation(rotationY) * origin;
		//origin = Matrix4::makeZRotation(rotationZ) * origin;
		origin = quatToMatrix(rotation) * origin;
		origin = Matrix4::makeTranslation(translation) * origin;
		return origin;
	}
};
class Geometry{
private:
	std::vector<Vertex> vertices;
	std::vector<unsigned short> indices;
	GLuint vertexBufferObject;
	GLuint indexBufferObject;
	GLuint diffuseTexture;
	GLuint specularTexture;
	GLuint normalTexture;
	int numIndices;
protected:
	void loadObjFile(const std::string &fileName, std::vector<Vertex> &outVertices, std::vector<unsigned short>& outIndices) {
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string err;
		bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, fileName.c_str(), NULL, true);
		if (ret) {
			for (size_t i = 0; i < shapes.size(); i++) {
				for (size_t j = 0; j < shapes[i].mesh.indices.size(); j++) {
					unsigned int vertexOffset = shapes[i].mesh.indices[j].vertex_index * 3;
					unsigned int normalOffset = shapes[i].mesh.indices[j].normal_index * 3;
					unsigned int texOffset = shapes[i].mesh.indices[j].texcoord_index * 2;
					Vertex v;
					v.position[0] = attrib.vertices[vertexOffset];
					v.position[1] = attrib.vertices[vertexOffset + 1];
					v.position[2] = attrib.vertices[vertexOffset + 2];
					v.normal[0] = attrib.normals[normalOffset];
					v.normal[1] = attrib.normals[normalOffset + 1];
					v.normal[2] = attrib.normals[normalOffset + 2];
					v.texture[0] = attrib.texcoords[texOffset];
					v.texture[1] = 1.0 - attrib.texcoords[texOffset + 1];
					outVertices.push_back(v);
					outIndices.push_back(outVertices.size() - 1);
				}
			}
		}
		else {
			std::cout << err << std::endl;
			assert(false);
		}
	}
	void calculateFaceTangent(const Cvec3f &v1, const Cvec3f &v2, const Cvec3f &v3,
		const Cvec2f &texCoord1, const Cvec2f &texCoord2, const Cvec2f &texCoord3,
		Cvec3f &tangent, Cvec3f &binormal) {
		Cvec3f side0 = v1 - v2;
		Cvec3f side1 = v3 - v1;
		Cvec3f normal = cross(side1, side0);
		float deltaV0 = texCoord1[1] - texCoord2[1];
		float deltaV1 = texCoord3[1] - texCoord1[1];
		tangent = side0 * deltaV1 - side1 * deltaV0;
		normalize(tangent);
		float deltaU0 = texCoord1[0] - texCoord2[0];
		float deltaU1 = texCoord3[0] - texCoord1[0];
		binormal = side0 * deltaU1 - side1 * deltaU0;
		normalize(binormal);
		Cvec3f tangentCross = cross(tangent, binormal);
		if (dot(tangentCross, normal) < 0.0f) {
			tangent = tangent * (-1);
		}
	}
public:
	Geometry(const std::string &fileName) {
		loadObjFile(fileName, vertices, indices);
		numIndices = indices.size();

		for (size_t i = 0; i < vertices.size(); i = i + 3){
			Cvec3f tangent;
			Cvec3f binormal;
			calculateFaceTangent(vertices[i].position, vertices[i + 1].position, vertices[i + 2].position,
				vertices[i].texture, vertices[i + 1].texture, vertices[i + 2].texture, tangent, binormal);
			vertices[i].binormal = binormal;
			vertices[i + 1].binormal = binormal;
			vertices[i + 2].binormal = binormal;

			vertices[i].tangent = tangent;
			vertices[i + 1].tangent = tangent;
			vertices[i + 2].tangent = tangent;
		}

		// create vertex position VBOs
		glGenBuffers(1, &vertexBufferObject);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
		glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);
		// create vertex index VBOs
		glGenBuffers(1, &indexBufferObject);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferObject);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * indices.size(), indices.data(), GL_STATIC_DRAW);
	}
	void loadDiffuseTexture(const char *filepath) {
		diffuseTexture = loadGLTexture(filepath);
	}
	void loadSpecularTexture(const char *filepath) {
		specularTexture = loadGLTexture(filepath);
	}
	void loadNormalTexture(const char *filepath) {
		normalTexture = loadGLTexture(filepath);
	}
	void Draw() {
		glUniform1i(diffuseTextureUniformLocation, 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, diffuseTexture);
		glUniform1i(specularTextureUniformLocation, 1);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, specularTexture);
		glUniform1i(normalTextureUniformLocation, 2);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, normalTexture);

		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
		glEnableVertexAttribArray(positionAttribute);
		glVertexAttribPointer(positionAttribute, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

		glEnableVertexAttribArray(normalAttribute);
		glVertexAttribPointer(normalAttribute, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

		glEnableVertexAttribArray(textureCoordinateAttribute);
		glVertexAttribPointer(textureCoordinateAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texture));

		glEnableVertexAttribArray(binormalAttribute);
		glVertexAttribPointer(binormalAttribute, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, binormal));

		glEnableVertexAttribArray(tangentAttribute);
		glVertexAttribPointer(tangentAttribute, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferObject);
		glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, 0);

		glDisableVertexAttribArray(positionAttribute);
		glDisableVertexAttribArray(normalAttribute);
		glDisableVertexAttribArray(textureCoordinateAttribute);
		glDisableVertexAttribArray(binormalAttribute);
		glDisableVertexAttribArray(tangentAttribute);
	};
};
struct Object {
	Transform transform;
	Geometry *geometry;
	Object *parent;
	Object() : parent(nullptr), geometry(nullptr), transform() {};
	void Draw() {
		Matrix4 modelViewMatrix = inv(eyeMatrix) * getTransformMatrix();

		GLfloat glMatrix[16];
		modelViewMatrix.writeToColumnMajorMatrix(glMatrix);
		glUniformMatrix4fv(modelViewMatrixUniformLocation, 1, false, glMatrix);

		Matrix4 normalMatrix = transpose(inv(modelViewMatrix));
		GLfloat glNormalMatrix[16];
		normalMatrix.writeToColumnMajorMatrix(glNormalMatrix);
		glUniformMatrix4fv(normalMatrixUniformLocation, 1, false, glNormalMatrix);

		geometry->Draw();
	}
	Matrix4 getTransformMatrix() {
		if (parent == nullptr) return transform.createMatrix();
		else {
			return parent->getTransformMatrix() * transform.createMatrix();
		}
	}
};
Matrix4 makeLookAt(Cvec3 eye, Cvec3 center, Cvec3 up) {
	Matrix4 eyeMatrix;
	Cvec3 z = normalize(eye - center);
	Cvec3 y = normalize(up);
	Cvec3 x = cross(y, z);
	eyeMatrix[0] = x[0];
	eyeMatrix[1] = y[0];
	eyeMatrix[2] = z[0];
	eyeMatrix[3] = eye[0];
	eyeMatrix[4] = x[1];
	eyeMatrix[5] = y[1];
	eyeMatrix[6] = z[1];
	eyeMatrix[7] = eye[1];
	eyeMatrix[8] = x[2];
	eyeMatrix[9] = y[2];
	eyeMatrix[10] = z[2];
	eyeMatrix[11] = eye[2];
	eyeMatrix[12] = 0.0;
	eyeMatrix[13] = 0.0;
	eyeMatrix[14] = 0.0;
	eyeMatrix[15] = 1.0;
	return eyeMatrix;
}

std::vector<Object> myObjects;

GLuint FXAAFrameBuffer;
GLuint FXAAFrameBufferTexture;
GLuint FXAADepthBufferTexture;
GLuint FXAAScreenPositionBuffer;
GLuint FXAAScreenUVBuffer;
GLuint FXAAScreenProgram;
GLuint FXAAScreenFrameBufferUnifrom;
GLuint FXAAScreenPositionAttribute;
GLuint FXAAScreenTexCoordAttribute;
GLuint FXAAResolutionUniform;

GLuint GuassianHorizontalFrameBuffer;
GLuint GuassianHorizontalFrameBufferTexture;
GLuint GuassianHorizontalDepthBufferTexture;
GLuint GuassianHorizontalScreenPositionBuffer;
GLuint GuassianHorizontalScreenUVBuffer;
GLuint GuassianHorizontalScreenProgram;
GLuint GuassianHorizontalScreenFrameBufferUnifrom;
GLuint GuassianHorizontalScreenPositionAttribute;
GLuint GuassianHorizontalScreenTexCoordAttribute;
GLuint GuassianHorizontalResolutionUniform;

GLuint GuassianVerticalFrameBuffer;
GLuint GuassianVerticalFrameBufferTexture;
GLuint GuassianVerticalDepthBufferTexture;
GLuint GuassianVerticalScreenPositionBuffer;
GLuint GuassianVerticalScreenUVBuffer;
GLuint GuassianVerticalScreenProgram;
GLuint GuassianVerticalScreenFrameBufferUnifrom;
GLuint GuassianVerticalScreenPositionAttribute;
GLuint GuassianVerticalScreenTexCoordAttribute;
GLuint GuassianVerticalResolutionUniform;

void display(void) {
	glBindFramebuffer(GL_FRAMEBUFFER, FXAAFrameBuffer);
	glViewport(0, 0, windowWidth, windowHeight);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_CULL_FACE);
	glUseProgram(program);

	Matrix4 projectionMatrix;
	projectionMatrix = projectionMatrix.makeProjection(45.0, 1.0, -0.1, -100.0);

	GLfloat glMatrixProjection[16];
	projectionMatrix.writeToColumnMajorMatrix(glMatrixProjection);
	glUniformMatrix4fv(projectionMatrixUniformLocation, 1, false, glMatrixProjection);


	glUniform3f(lights[0].lightPositionUniformLocation, 20.0f, 20.0f, 0.0f);
	glUniform3f(lights[0].lightColorUniformLocation, 0.87890625f, 0.0f, 0.0f);
	glUniform3f(lights[0].specularLightColorUniformLocation, 0.87890625f, 0.0f, 0.0f);
	glUniform3f(lights[1].lightPositionUniformLocation, -20.0f, 20.0f, 0.0f);
	glUniform3f(lights[1].lightColorUniformLocation, 0.4f, 0.8f, 1.0f);
	glUniform3f(lights[1].specularLightColorUniformLocation, 0.4f, 0.8f, 1.0f);

	for (size_t i = 0; i < myObjects.size(); i++) {
		myObjects[i].Draw();
	}

	// FXAA
	glBindFramebuffer(GL_FRAMEBUFFER, GuassianHorizontalFrameBuffer);

	glDisable(GL_CULL_FACE);
	glViewport(0, 0, windowWidth, windowHeight);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(FXAAScreenProgram);

	glUniform2f(FXAAResolutionUniform, windowWidth, windowHeight);

	glUniform1i(FXAAScreenFrameBufferUnifrom, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, FXAAFrameBufferTexture);

	glBindBuffer(GL_ARRAY_BUFFER, FXAAScreenPositionBuffer);
	glVertexAttribPointer(FXAAScreenPositionAttribute, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(FXAAScreenPositionAttribute);
	glBindBuffer(GL_ARRAY_BUFFER, FXAAScreenUVBuffer);
	glVertexAttribPointer(FXAAScreenTexCoordAttribute, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(FXAAScreenTexCoordAttribute);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(FXAAScreenPositionAttribute);
	glDisableVertexAttribArray(FXAAScreenTexCoordAttribute);

	// Guassian Blur - H
	glBindFramebuffer(GL_FRAMEBUFFER, GuassianVerticalFrameBuffer);

	glDisable(GL_CULL_FACE);
	glViewport(0, 0, windowWidth, windowHeight);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(GuassianHorizontalScreenProgram);

	glUniform1i(GuassianHorizontalScreenFrameBufferUnifrom, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, GuassianHorizontalFrameBufferTexture);

	glBindBuffer(GL_ARRAY_BUFFER, GuassianHorizontalScreenPositionBuffer);
	glVertexAttribPointer(GuassianHorizontalScreenPositionAttribute, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(GuassianHorizontalScreenPositionAttribute);
	glBindBuffer(GL_ARRAY_BUFFER, GuassianHorizontalScreenUVBuffer);
	glVertexAttribPointer(GuassianHorizontalScreenTexCoordAttribute, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(GuassianHorizontalScreenTexCoordAttribute);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(GuassianHorizontalScreenPositionAttribute);
	glDisableVertexAttribArray(GuassianHorizontalScreenTexCoordAttribute);

	// Guassian Blur - V
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glDisable(GL_CULL_FACE);
	glViewport(0, 0, windowWidth, windowHeight);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(GuassianVerticalScreenProgram);

	glUniform1i(GuassianVerticalScreenFrameBufferUnifrom, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, GuassianVerticalFrameBufferTexture);

	glBindBuffer(GL_ARRAY_BUFFER, GuassianVerticalScreenPositionBuffer);
	glVertexAttribPointer(GuassianVerticalScreenPositionAttribute, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(GuassianVerticalScreenPositionAttribute);
	glBindBuffer(GL_ARRAY_BUFFER, GuassianVerticalScreenUVBuffer);
	glVertexAttribPointer(GuassianVerticalScreenTexCoordAttribute, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(GuassianVerticalScreenTexCoordAttribute);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(GuassianVerticalScreenPositionAttribute);
	glDisableVertexAttribArray(GuassianVerticalScreenTexCoordAttribute);

	glutSwapBuffers();
}
void reshape(int w, int h) {
	windowWidth = w;
	windowHeight = h;
	glViewport(0, 0, w, h);
}
int previousX = 0, previousY = 0, currentX = 0, currentY = 0;
bool controllerOn = false;
Cvec3 getArcBallVector(int x, int y) {
	Cvec3 point = Cvec3((double)x / (double)windowWidth - 0.5,
						(double)y / (double)windowHeight - 0.5, 0);
	point[1] = -point[1];
	double square = point[0] * point[0] + point[1] * point[1];
	if (square <= 0.25) {
		point[2] = sqrt(0.25 - square);
	}
	else {
		normalize(point);
	}
	//std::cout << point[0] << "," << point[1] << "," << point[2] << std::endl;
	return point;
}

void mouse(int button, int state, int x, int y) {
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
		controllerOn = true;
		previousX = x;
		previousY = y;
		currentX = x;
		currentY = y;
	}
	else {
		controllerOn = false;
	}
}
void mouseMove(int x, int y) {
	if (controllerOn) {
		currentX = x;
		currentY = y;
		if (currentX != previousX || currentY != previousY) {
			Cvec3 vA = getArcBallVector(previousX, previousY);
			Cvec3 vB = getArcBallVector(currentX, currentY);
			Quat qA(0, vB);
			Quat qB(0, -vA);
			Quat rotation(qB * qA);
			/*for (size_t i = 0; i < myObjects.size(); i++) {
				myObjects[i].transform.rotation += rotation;
			}*/
			eyeMatrix = quatToMatrix(rotation) * eyeMatrix;
			previousX = currentX;
			previousY = currentY;
		}
	}
}
void idle(void) {
	glutPostRedisplay();
}

void init() {
	// need to initialize glew before create program
	GLenum err = glewInit();
	if (err != GLEW_OK)
		throw std::runtime_error("glewInit failed");
	glClearDepth(0.0f);

	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_GREATER);
	glReadBuffer(GL_BACK);

	program = glCreateProgram();
	readAndCompileShader(program, "vertex.glsl", "fragment.glsl");

	FXAAScreenProgram = glCreateProgram();
	readAndCompileShader(FXAAScreenProgram, "fxaa.vertex.glsl", "fxaa.fragment.glsl");

	GuassianHorizontalScreenProgram = glCreateProgram();
	readAndCompileShader(GuassianHorizontalScreenProgram, "guassianBlur.vertex.glsl", "guassianHorizontalBlur.fragment.glsl");

	GuassianVerticalScreenProgram = glCreateProgram();
	readAndCompileShader(GuassianVerticalScreenProgram, "guassianBlur.vertex.glsl", "guassianVerticalBlur.fragment.glsl");

	// get attributes
	positionAttribute = glGetAttribLocation(program, "position");
	normalAttribute = glGetAttribLocation(program, "normal");
	textureCoordinateAttribute = glGetAttribLocation(program, "texCoord");
	binormalAttribute = glGetAttribLocation(program, "binormal");
	tangentAttribute = glGetAttribLocation(program, "tangent");
	//get uniform
	modelViewMatrixUniformLocation = glGetUniformLocation(program, "modelViewMatrix");
	projectionMatrixUniformLocation = glGetUniformLocation(program, "projectionMatrix");
	normalMatrixUniformLocation = glGetUniformLocation(program, "normalMatrix");
	//colorUniformLocation = glGetUniformLocation(program, "uniformColor");
	diffuseTextureUniformLocation = glGetUniformLocation(program, "diffuseTexture");
	specularTextureUniformLocation = glGetUniformLocation(program, "specularTexture");
	normalTextureUniformLocation = glGetUniformLocation(program, "normalTexture");

	lights[0].lightPositionUniformLocation = glGetUniformLocation(program, "lights[0].lightPosition");
	lights[0].lightColorUniformLocation = glGetUniformLocation(program, "lights[0].lightColor");
	lights[0].specularLightColorUniformLocation = glGetUniformLocation(program, "lights[0].specularLightColor");
	lights[1].lightPositionUniformLocation = glGetUniformLocation(program, "lights[1].lightPosition");
	lights[1].lightColorUniformLocation = glGetUniformLocation(program, "lights[1].lightColor");
	lights[1].specularLightColorUniformLocation = glGetUniformLocation(program, "lights[1].specularLightColor");


	FXAAScreenPositionAttribute = glGetAttribLocation(FXAAScreenProgram, "position");
	FXAAScreenTexCoordAttribute = glGetAttribLocation(FXAAScreenProgram, "texCoord");
	FXAAScreenFrameBufferUnifrom = glGetUniformLocation(FXAAScreenProgram, "screenFramebuffer");
	FXAAResolutionUniform = glGetUniformLocation(FXAAScreenProgram, "resolution");

	GuassianHorizontalScreenPositionAttribute = glGetAttribLocation(GuassianHorizontalScreenProgram, "position");
	GuassianHorizontalScreenTexCoordAttribute = glGetAttribLocation(GuassianHorizontalScreenProgram, "texCoord");
	GuassianHorizontalScreenFrameBufferUnifrom = glGetUniformLocation(GuassianHorizontalScreenProgram, "screenFramebuffer");

	GuassianVerticalScreenPositionAttribute = glGetAttribLocation(GuassianVerticalScreenProgram, "position");
	GuassianVerticalScreenTexCoordAttribute = glGetAttribLocation(GuassianVerticalScreenProgram, "texCoord");
	GuassianVerticalScreenFrameBufferUnifrom = glGetUniformLocation(GuassianVerticalScreenProgram, "screenFramebuffer");

	eyeMatrix = makeLookAt(Cvec3(0.0, 5.0, 20.0), Cvec3(0.0, 5.0, 0.0), Cvec3(0.0, 1.0, 0.0));
	
	
	// add Objects
	Object* testObject = new Object();
	testObject->geometry = new Geometry("./Monk_Giveaway_Fixed.obj");
	testObject->geometry->loadDiffuseTexture("./Monk_D.tga");
	testObject->geometry->loadSpecularTexture("./Monk_S.tga");
	testObject->geometry->loadNormalTexture("./Monk_N.tga");
	testObject->transform.translation = Cvec3(-5.0, -5.0, 0.0);
	testObject->transform.rotation = Quat::makeYRotation(90.0);
	myObjects.push_back(*testObject);

	Object* testObject2 = new Object();
	testObject2->geometry = new Geometry("./Monk_Giveaway_Fixed.obj");
	testObject2->geometry->loadDiffuseTexture("./Monk_D.tga");
	testObject2->geometry->loadSpecularTexture("./Monk_S.tga");
	testObject2->geometry->loadNormalTexture("./Monk_N.tga");
	testObject2->transform.translation = Cvec3(5.0, -5.0, 0.0);
	testObject2->transform.rotation = Quat::makeYRotation(-90.0);
	myObjects.push_back(*testObject2);
	
	glGenFramebuffers(1, &FXAAFrameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, FXAAFrameBuffer);

	glGenTextures(1, &FXAAFrameBufferTexture);
	glBindTexture(GL_TEXTURE_2D, FXAAFrameBufferTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, windowWidth, windowHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FXAAFrameBufferTexture, 0);

	glGenTextures(1, &FXAADepthBufferTexture);
	glBindTexture(GL_TEXTURE_2D, FXAADepthBufferTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, windowWidth, windowHeight, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, windowWidth, windowHeight);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, FXAADepthBufferTexture, 0);

	//// Set the list of draw buffers.
	//GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	//glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glGenFramebuffers(1, &GuassianHorizontalFrameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, GuassianHorizontalFrameBuffer);

	glGenTextures(1, &GuassianHorizontalFrameBufferTexture);
	glBindTexture(GL_TEXTURE_2D, GuassianHorizontalFrameBufferTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, windowWidth, windowHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, GuassianHorizontalFrameBufferTexture, 0);

	glGenTextures(1, &GuassianHorizontalDepthBufferTexture);
	glBindTexture(GL_TEXTURE_2D, GuassianHorizontalDepthBufferTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, windowWidth, windowHeight, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, windowWidth, windowHeight);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, GuassianHorizontalDepthBufferTexture, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glGenFramebuffers(1, &GuassianVerticalFrameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, GuassianVerticalFrameBuffer);

	glGenTextures(1, &GuassianVerticalFrameBufferTexture);
	glBindTexture(GL_TEXTURE_2D, GuassianVerticalFrameBufferTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, windowWidth, windowHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, GuassianVerticalFrameBufferTexture, 0);

	glGenTextures(1, &GuassianVerticalDepthBufferTexture);
	glBindTexture(GL_TEXTURE_2D, GuassianVerticalDepthBufferTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, windowWidth, windowHeight, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, windowWidth, windowHeight);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, GuassianVerticalDepthBufferTexture, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	GLfloat screenTriangleUVs[] = {
		1.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f
	};
	GLfloat screenTrianglePosition[] = {
		1.0f, 1.0f,
		1.0f, -1.0f,
		-1.0f, -1.0f,
		-1.0f, -1.0f,
		-1.0f, 1.0f,
		1.0f, 1.0f
	};

	glGenBuffers(1, &FXAAScreenPositionBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, FXAAScreenPositionBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 12, screenTrianglePosition, GL_STATIC_DRAW);

	glGenBuffers(1, &FXAAScreenUVBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, FXAAScreenUVBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 12, screenTriangleUVs, GL_STATIC_DRAW);

	glGenBuffers(1, &GuassianHorizontalScreenPositionBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, GuassianHorizontalScreenPositionBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 12, screenTrianglePosition, GL_STATIC_DRAW);

	glGenBuffers(1, &GuassianHorizontalScreenUVBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, GuassianHorizontalScreenUVBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 12, screenTriangleUVs, GL_STATIC_DRAW);

	glGenBuffers(1, &GuassianVerticalScreenPositionBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, GuassianVerticalScreenPositionBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 12, screenTrianglePosition, GL_STATIC_DRAW);

	glGenBuffers(1, &GuassianVerticalScreenUVBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, GuassianVerticalScreenUVBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 12, screenTriangleUVs, GL_STATIC_DRAW);
}

int main(int argc, char **argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(windowWidth, windowHeight);
	glutCreateWindow("Assignment4");

	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutIdleFunc(idle);

	// enable blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glClearColor(1, 1, 1, 1.0);

	// enable culling
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glutMouseFunc(mouse);
	glutMotionFunc(mouseMove);

	init();
	glutMainLoop();
	system("pause");
	return 0;
}