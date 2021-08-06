// Windows includes (For Time, IO, etc.)
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <math.h>
#include <vector> // STL dynamic memory.

// OpenGL includes
#include <GL/glew.h>
#include <GL/freeglut.h>

// Assimp includes
#include <assimp/cimport.h> // scene importer
#include <assimp/scene.h> // collects data
#include <assimp/postprocess.h> // various extra operations

// Project includes
#include "maths_funcs.h"

// GLM includes
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>

/*----------------------------------------------------------------------------
MESH TO LOAD
----------------------------------------------------------------------------*/
#define ARM_MESH "./models/cone2.obj"
#define BALL_MESH "./models/ball.dae"
#define BODY_MESH "./models/body.dae"
/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

// Camera 
glm::vec3 cameraPos = glm::vec3(3.0f, 0.0f, 10.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec3 up = glm::vec3(0.0f, 0.1f, 0.0f);
#pragma region SimpleTypes
typedef struct
{
	size_t mPointCount = 0;
	std::vector<vec3> mVertices;
	std::vector<vec3> mNormals;
	std::vector<vec2> mTextureCoords;
} ModelData;
#pragma endregion SimpleTypes

using namespace std;
GLuint shaderProgramID;

ModelData mesh_data;
unsigned int mesh_vao = 0;
int width = 800;
int height = 600;

GLuint loc1, loc2, loc3;
GLfloat rotate_y = 0.0f;

const GLuint i = 8;
GLuint VAO[i], VBO[i * 2];
std::vector < ModelData > meshData;
std::vector < const char* > dataArray;

float delta;

float upperArmAngle;
float lowerArmAngle;

int effectors = 3;

int armLengths = 3.9;
int armLength = 4.0;
glm::vec3 links[3];
float angles[3];

glm::vec3 startingPos(7.6f, 0.0f, 0.0f);

bool keyC = false;
bool keyF = true;

void setUpLinks() {
	links[0] = glm::vec3(0.0f, 0.0f, 0.0f);
	links[1] = glm::vec3(3.9f, 0.0f, 0.0f);
	links[2] = glm::vec3(7.8f, 0.0f, 0.0f);
}
#pragma region MESH LOADING
/*----------------------------------------------------------------------------
MESH LOADING FUNCTION
----------------------------------------------------------------------------*/

ModelData load_mesh(const char* file_name) {
	ModelData modelData;

	/* Use assimp to read the model file, forcing it to be read as    */
	/* triangles. The second flag (aiProcess_PreTransformVertices) is */
	/* relevant if there are multiple meshes in the model file that   */
	/* are offset from the origin. This is pre-transform them so      */
	/* they're in the right position.                                 */
	const aiScene* scene = aiImportFile(
		file_name, 
		aiProcess_Triangulate | aiProcess_PreTransformVertices
	); 

	if (!scene) {
		fprintf(stderr, "ERROR: reading mesh %s\n", file_name);
		return modelData;
	}

	printf("  %i materials\n", scene->mNumMaterials);
	printf("  %i meshes\n", scene->mNumMeshes);
	printf("  %i textures\n", scene->mNumTextures);

	for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
		const aiMesh* mesh = scene->mMeshes[m_i];
		printf("    %i vertices in mesh\n", mesh->mNumVertices);
		modelData.mPointCount += mesh->mNumVertices;
		for (unsigned int v_i = 0; v_i < mesh->mNumVertices; v_i++) {
			if (mesh->HasPositions()) {
				const aiVector3D* vp = &(mesh->mVertices[v_i]);
				modelData.mVertices.push_back(vec3(vp->x, vp->y, vp->z));
			}
			if (mesh->HasNormals()) {
				const aiVector3D* vn = &(mesh->mNormals[v_i]);
				modelData.mNormals.push_back(vec3(vn->x, vn->y, vn->z));
			}
			if (mesh->HasTextureCoords(0)) {
				const aiVector3D* vt = &(mesh->mTextureCoords[0][v_i]);
				modelData.mTextureCoords.push_back(vec2(vt->x, vt->y));
			}
			if (mesh->HasTangentsAndBitangents()) {
				/* You can extract tangents and bitangents here              */
				/* Note that you might need to make Assimp generate this     */
				/* data for you. Take a look at the flags that aiImportFile  */
				/* can take.                                                 */
			}
		}
	}

	aiReleaseImport(scene);
	return modelData;
}

#pragma endregion MESH LOADING

// Shader Functions- click on + to expand
#pragma region SHADER_FUNCTIONS
char* readShaderSource(const char* shaderFile) {
	FILE* fp;
	fopen_s(&fp, shaderFile, "rb");

	if (fp == NULL) { return NULL; }

	fseek(fp, 0L, SEEK_END);
	long size = ftell(fp);

	fseek(fp, 0L, SEEK_SET);
	char* buf = new char[size + 1];
	fread(buf, 1, size, fp);
	buf[size] = '\0';

	fclose(fp);

	return buf;
}


static void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType)
{
	// create a shader object
	GLuint ShaderObj = glCreateShader(ShaderType);

	if (ShaderObj == 0) {
		std::cerr << "Error creating shader..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	const char* pShaderSource = readShaderSource(pShaderText);

	// Bind the source code to the shader, this happens before compilation
	glShaderSource(ShaderObj, 1, (const GLchar**)&pShaderSource, NULL);
	// compile the shader and check for errors
	glCompileShader(ShaderObj);
	GLint success;
	// check for shader related errors using glGetShaderiv
	glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar InfoLog[1024] = { '\0' };
		glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
		std::cerr << "Error compiling "
			<< (ShaderType == GL_VERTEX_SHADER ? "vertex" : "fragment")
			<< " shader program: " << InfoLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	// Attach the compiled shader object to the program object
	glAttachShader(ShaderProgram, ShaderObj);
}

GLuint CompileShaders(const char* vertexShader, const char* fragmentShader)
{
	//Start the process of setting up our shaders by creating a program ID
	//Note: we will link all the shaders together into this ID
	shaderProgramID = glCreateProgram();
	if (shaderProgramID == 0) {
		std::cerr << "Error creating shader program..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// Create two shader objects, one for the vertex, and one for the fragment shader
	AddShader(shaderProgramID, vertexShader, GL_VERTEX_SHADER);
	AddShader(shaderProgramID, fragmentShader, GL_FRAGMENT_SHADER);

	GLint Success = 0;
	GLchar ErrorLog[1024] = { '\0' };
	// After compiling all shader objects and attaching them to the program, we can finally link it
	glLinkProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &Success);
	if (Success == 0) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Error linking shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// program has been successfully linked but needs to be validated to check whether the program can execute given the current pipeline state
	glValidateProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_VALIDATE_STATUS, &Success);
	if (!Success) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Invalid shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	// Finally, use the linked shader program
	// Note: this program will stay in effect for all draw calls until you replace it with another or explicitly disable its use
	glUseProgram(shaderProgramID);
	return shaderProgramID;
}
#pragma endregion SHADER_FUNCTIONS

#pragma region VBO_FUNCTIONS
void generateObjectBufferMesh(std::vector < const char* > dataArray) {

	loc1 = glGetAttribLocation(shaderProgramID, "vertex_position");
	loc2 = glGetAttribLocation(shaderProgramID, "vertex_normal");
	int counter = 0;

	for (int i = 0; i < dataArray.size(); i++) {
		mesh_data = load_mesh(dataArray[i]);
		meshData.push_back(mesh_data);

		glGenBuffers(1, &VBO[counter]);
		glBindBuffer(GL_ARRAY_BUFFER, VBO[counter]);
		glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mVertices[0], GL_STATIC_DRAW);

		glGenBuffers(1, &VBO[counter + 1]);
		glBindBuffer(GL_ARRAY_BUFFER, VBO[counter + 1]);
		glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mNormals[0], GL_STATIC_DRAW);

		glGenVertexArrays(1, &VAO[i]);
		glBindVertexArray(VAO[i]);

		glEnableVertexAttribArray(loc1);
		glBindBuffer(GL_ARRAY_BUFFER, VBO[counter]);
		glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		glEnableVertexAttribArray(loc2);
		glBindBuffer(GL_ARRAY_BUFFER, VBO[counter + 1]);
		glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		counter += 2;
	}
}
#pragma endregion VBO_FUNCTIONS

#pragma region INVERSE_KINETICS
// Analytical Solution, based on and closely following the code found at:
// http://graphics.cs.cmu.edu/nsp/course/15-464/Spring07/assignments/jlander_gamedev_sept98.pdf and 
// https://github.com/dwilliamson/GDMagArchive/blob/701948bbd74b7ae765be8cdaf4ae0f875e2bbf8e/sep98-with%20avi/Lander1.1/code/OGL/Kine/OGLView.cpp
void calculateAngles(float x, float y) {
	float l1 = 0, l2 = 0;
	float ex = 0, ey = 0;
	float sin2 = 0, cos2 = 0;
	float angle1 = 0, angle2 = 0, tan1 = 0;
	ex = x;
	ey = y;

	l1 = armLength;
	l2 = armLength;
	cos2 = ((ex * ex) + (ey * ey) - (l1 * l1) - (l2 * l2)) / (2 * l1 * l2);

	if (cos2 >= -1.0 && cos2 <= 1.0) {
		angle2 = (float)glm::acos(cos2);
		lowerArmAngle = glm::degrees(angle2);

		sin2 = (float)glm::sin(angle2);

		tan1 = (-(l2 * sin2 * ex) + ((l1 + (l2 * cos2)) * ey)) / ((l2 * sin2 * ey) + ((l1 + (l2 * cos2)) * ex));

		angle1 = glm::atan(tan1);
		upperArmAngle = glm::degrees(angle1);
	}
	
}

// CCD (Cyclic Coordinate Descent), based on and closely following (with some helper solutions taken from): 
// http://graphics.cs.cmu.edu/nsp/course/15-464/Spring07/assignments/jlander_gamedev_nov98.pdf and
// https://github.com/dwilliamson/GDMagArchive/blob/701948bbd74b7ae765be8cdaf4ae0f875e2bbf8e/nov98/lander/code/OGL/KineChain/MathDefs.cpp and 
// https://github.com/dwilliamson/GDMagArchive/blob/master/nov98/lander/code/OGL/KineChain/OGLView.cpp
double VectorSquaredDistance(glm::vec3 *v1, glm::vec3 *v2)
{
	return(((v1->x - v2->x) * (v1->x - v2->x)) +
		((v1->y - v2->y) * (v1->y - v2->y)) +
		((v1->z - v2->z) * (v1->z - v2->z)));
}

double VectorSquaredLength(glm::vec3 *v)
{
	return((v->x * v->x) + (v->y * v->y) + (v->z * v->z));
}

double VectorLength(glm::vec3 *v)
{
	return(sqrt(VectorSquaredLength(v)));
}

void NormalizeVector(glm::vec3 *v)
{
	float len = (float)VectorLength(v);
	if (len != 0.0)
	{
		v->x /= len;
		v->y /= len;
		v->z /= len;
	}
}

double DotProduct(glm::vec3 *v1, glm::vec3 *v2)
{
	return ((v1->x * v2->x) + (v1->y * v2->y) + (v1->z + v2->z));
}

void CrossProduct(glm::vec3 *v1, glm::vec3 *v2, glm::vec3 *result)
{
	result->x = (v1->y * v2->z) - (v1->z * v2->y);
	result->y = (v1->z * v2->x) - (v1->x * v2->z);
	result->z = (v1->x * v2->y) - (v1->y * v2->x);
}

void ComputeCCD(int x, int y) {
	glm::vec3 rootPos, curEnd, desiredEnd, targetVector, curVector, crossResult;
	double cosAngle, turnAngle, turnDeg;
	int link, tries;
	bool found = false;

	link = effectors - 1;
	tries = 0;

	while (tries < 100 && link >= 0) {
		rootPos.x = links[link].x;
		rootPos.y = links[link].y;
		rootPos.z = links[link].z;

		curEnd.x = links[effectors].x;
		curEnd.y = links[effectors].y;
		curEnd.z = 0.0f;

		desiredEnd.x = float(x);
		desiredEnd.y = float(y);
		desiredEnd.z = 0.0f;

		if (VectorSquaredDistance(&curEnd, &desiredEnd) > 1.0f) {
			curVector.x = curEnd.x - rootPos.x;
			curVector.y = curEnd.y - rootPos.y;
			curVector.z = curEnd.z - rootPos.z;

			targetVector.x = x - rootPos.x;
			targetVector.y = y - rootPos.y;
			targetVector.z = 0.0f;

			NormalizeVector(&curVector);
			NormalizeVector(&targetVector);

			cosAngle = DotProduct(&targetVector, &curVector);

			if (cosAngle < 0.99999) {
				CrossProduct(&targetVector, &curVector, &crossResult);
				if (crossResult.z > 0.0f) {
					turnAngle = glm::acos((float)cosAngle);
					turnDeg = glm::degrees(turnAngle);
					angles[link] -= (float)turnDeg;
				}
				else if (crossResult.z < 0.0f) {
					turnAngle = glm::acos((float)cosAngle);
					turnDeg = glm::degrees(turnAngle);
					angles[link] += (float)turnDeg;
				}
			} 
		}

		tries++;
		link--;
	}

}
#pragma endregion INVERSE_KINETICS

void display() {

	// tell GL to only draw onto a pixel if the shape is closer to the viewer
	glEnable(GL_DEPTH_TEST); // enable depth-testing
	glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// View and Proj
	glm::mat4 view = glm::mat4(1.0f);
	view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);

	// ------------------- Models -------------------
	glUseProgram(shaderProgramID);

	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "proj"), 1, GL_FALSE, glm::value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "view"), 1, GL_FALSE, glm::value_ptr(view));

	glm::vec3 armColor = glm::vec3(0.0f, 0.8f, 0.0f);

	// Analytical Solution
	if (keyF) {
		// Upper Arm
		glm::mat4 upperArmModel = glm::mat4(1.0f);
		upperArmModel = glm::rotate(upperArmModel, glm::radians(upperArmAngle), glm::vec3(0.0f, 0.0f, 1.0f));
		glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "model"), 1, GL_FALSE, glm::value_ptr(upperArmModel));
		glUniform3fv(glGetUniformLocation(shaderProgramID, "Kd"), 1, &armColor[0]);

		glBindVertexArray(VAO[0]);
		glDrawArrays(GL_TRIANGLES, 0, meshData[0].mPointCount);

		// Lower Arm
		glm::mat4 lowerArmModel = glm::mat4(1.0f);
		lowerArmModel = glm::translate(lowerArmModel, glm::vec3(3.9f, 0.0f, 0.0f));
		lowerArmModel = glm::rotate(lowerArmModel, glm::radians(lowerArmAngle), glm::vec3(0.0f, 0.0f, 1.0f));
		lowerArmModel = upperArmModel * lowerArmModel;

		glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "model"), 1, GL_FALSE, glm::value_ptr(lowerArmModel));
		glUniform3fv(glGetUniformLocation(shaderProgramID, "Kd"), 1, &armColor[0]);
		glDrawArrays(GL_TRIANGLES, 0, meshData[0].mPointCount);
	}

	// CCD
	if (keyC) {
		// Upper Arm
		glm::mat4 upperArmModel = glm::mat4(1.0f);
		upperArmModel = glm::rotate(upperArmModel, glm::radians(angles[0]), glm::vec3(0.0f, 0.0f, 1.0f));
		glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "model"), 1, GL_FALSE, glm::value_ptr(upperArmModel));
		glUniform3fv(glGetUniformLocation(shaderProgramID, "Kd"), 1, &armColor[0]);

		glBindVertexArray(VAO[0]);
		glDrawArrays(GL_TRIANGLES, 0, meshData[0].mPointCount);

		// Lower Arm
		glm::mat4 lowerArmModel = glm::mat4(1.0f);
		lowerArmModel = glm::translate(lowerArmModel, glm::vec3(3.9f, 0.0f, 0.0f));
		lowerArmModel = glm::rotate(lowerArmModel, glm::radians(angles[1]), glm::vec3(0.0f, 0.0f, 1.0f));
		lowerArmModel = upperArmModel * lowerArmModel;

		glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "model"), 1, GL_FALSE, glm::value_ptr(lowerArmModel));
		glUniform3fv(glGetUniformLocation(shaderProgramID, "Kd"), 1, &armColor[0]);
		glDrawArrays(GL_TRIANGLES, 0, meshData[0].mPointCount);


		// Link 3
		glm::mat4 link3Model = glm::mat4(1.0f);
		link3Model = glm::translate(link3Model, glm::vec3(3.9f, 0.0f, 0.0f));
		link3Model = glm::rotate(link3Model, glm::radians(angles[2]), glm::vec3(0.0f, 0.0f, 1.0f));
		link3Model = lowerArmModel * link3Model;

		glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "model"), 1, GL_FALSE, glm::value_ptr(link3Model));
		glUniform3fv(glGetUniformLocation(shaderProgramID, "Kd"), 1, &armColor[0]);
		glDrawArrays(GL_TRIANGLES, 0, meshData[0].mPointCount);

	}

	// Ball 
	glm::vec3 ballColor = glm::vec3(1.0f, 0.0f, 0.0f);
	glm::mat4 ballModel = glm::mat4(1.0f);
	ballModel = glm::translate(ballModel, startingPos);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "model"), 1, GL_FALSE, glm::value_ptr(ballModel));
	glUniform3fv(glGetUniformLocation(shaderProgramID, "Kd"), 1, &ballColor[0]);
	glBindVertexArray(VAO[1]);
	glDrawArrays(GL_TRIANGLES, 0, meshData[1].mPointCount);

	// Body
	glm::mat4 bodyModel = glm::mat4(1.0f);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "model"), 1, GL_FALSE, glm::value_ptr(bodyModel));
	glUniform3fv(glGetUniformLocation(shaderProgramID, "Kd"), 1, &armColor[0]);
	glBindVertexArray(VAO[2]);
	glDrawArrays(GL_TRIANGLES, 0, meshData[2].mPointCount);
	// ------------------------------------------------------------------------------------------------------------------
	glutSwapBuffers();
}


void updateScene() {

	static DWORD last_time = 0;
	DWORD curr_time = timeGetTime();
	if (last_time == 0)
		last_time = curr_time;
	delta = (curr_time - last_time) * 0.001f;
	last_time = curr_time;

	// Rotate the model slowly around the y axis at 20 degrees per second
	rotate_y += 20.0f * delta;
	rotate_y = fmodf(rotate_y, 360.0f);

	// Draw the next frame
	glutPostRedisplay();
}


void init()
{
	// Set up links
	setUpLinks();
	// Set up the shaders
	GLuint shaderProgramID = CompileShaders("./shaders/simpleVertexShader.txt", "./shaders/simpleFragmentShader.txt");
	// load mesh into a vertex buffer array
	dataArray.push_back(ARM_MESH);
	dataArray.push_back(BALL_MESH);
	dataArray.push_back(BODY_MESH);
	generateObjectBufferMesh(dataArray);

}

// Placeholder code for the keypress
void keypress(unsigned char key, int x, int y) {
	float cameraSpeed = 300.0f * delta;
	if (key == 'a') {
		cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
	}
	else if (key == 'd') {
		cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
	}
	else if (key == 'c') {
		startingPos.x = 11.4;
		startingPos.y = 0.0;
		cameraPos.x = 6.0f;
		cameraPos.z = 12.0f;
		keyC = true;
		keyF = false;
	}
	else if (key == 'f') {
		startingPos.x = 7.6;
		startingPos.y = 0.0;
		cameraPos.x = 3.0f;
		cameraPos.z = 10.0f;
		keyF = true;
		keyC = false;
	}
	else if (key == 'j') {
		cameraPos -= cameraSpeed * up;
	}
	else if (key == 's') {
		cameraPos -= cameraSpeed * cameraFront;
	}
	else if (key == 'u') {
		cameraPos += cameraSpeed * up;
	}
	else if (key == 'w') {
		cameraPos += cameraSpeed * cameraFront;
	}
}

void specialKeys(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_UP:
		startingPos.y += 0.1f;
		break;
	case GLUT_KEY_DOWN:
		startingPos.y -= 0.1f;
		break;
	case GLUT_KEY_RIGHT:
		startingPos.x += 0.1f;
		break;
	case GLUT_KEY_LEFT:
		startingPos.x -= 0.1f;
		break;
	}

	if (keyF) {
		calculateAngles(startingPos.x, startingPos.y);
	}
	else if (keyC) {
		ComputeCCD(startingPos.x, startingPos.y);
	}

}

int main(int argc, char** argv) {

	// Set up the window
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(width, height);
	glutCreateWindow("CS7GV5 - Real-Time Animation Assignment 2");

	// Tell glut where the display function is
	glutDisplayFunc(display);
	glutIdleFunc(updateScene);
	glutKeyboardFunc(keypress);
	glutSpecialFunc(specialKeys);//glutMouseFunc(mouseClick);
	// A call to glewInit() must be done after glut is initialized!
	GLenum res = glewInit();
	// Check for any errors
	if (res != GLEW_OK) {
		fprintf(stderr, "Error: '%s'\n", glewGetErrorString(res));
		return 1;
	}
	// Set up your objects and shaders
	init();
	// Begin infinite event loop
	glutMainLoop();
	return 0;
}
