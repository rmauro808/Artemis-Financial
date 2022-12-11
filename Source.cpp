#include <iostream>         // cout, cerr
#include <cstdlib>          // EXIT_FAILURE
#include <GL/glew.h>        // GLEW library
#include <GLFW/glfw3.h>     // GLFW library
#define STB_IMAGE_IMPLEMENTATION


// GLM Math Header inclusions
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <camera.h>
#include "stb_image.h"
#include "meshes.h"


using namespace std; // Uses the standard namespace

/*Shader program Macro*/
#ifndef GLSL
#define GLSL(Version, Source) "#version " #Version " core \n" #Source
#endif

// Unnamed namespace
namespace
{
	const char* const WINDOW_TITLE = "BaseBall Scene"; // Macro for window title

	// Variables for window width and height
	const int WINDOW_WIDTH = 1200;
	const int WINDOW_HEIGHT = 800;

	// Main GLFW window
	GLFWwindow* gWindow = nullptr;
	// Shader program
	GLuint gSurfaceProgramId;
	GLuint gLightProgramId;
	Camera gCameraFront(glm::vec3(0.0f, 3.0f, 5.0f));
	Camera* g_pCurrentCamera = NULL;
	// Texture
	GLuint gFieldTextureId;
	GLuint gHomePlateTextureId;
	GLuint gBallTextureId;
	GLuint gBatTextureId;

	glm::vec2 gUVScale(2.0f, 2.0f);
	GLint gTexWrapMode = GL_REPEAT;

	float gLastX = WINDOW_WIDTH / 2.0f;
	float gLastY = WINDOW_HEIGHT / 2.0f;
	bool gFirstMouse = true;
	// timing
	float gDeltaTime = 0.0f; // time between current frame and last frame
	float gLastFrame = 0.0f;

	Meshes meshes;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
/* Surface Vertex Shader Source Code*/
const GLchar* surfaceVertexShaderSource = GLSL(440,

	layout(location = 0) in vec3 vertexPosition; // VAP position 0 for vertex position data
layout(location = 1) in vec3 vertexNormal; // VAP position 1 for normals
layout(location = 2) in vec2 textureCoordinate;

out vec3 vertexFragmentNormal; // For outgoing normals to fragment shader
out vec3 vertexFragmentPos; // For outgoing color / pixels to fragment shader
out vec2 vertexTextureCoordinate;

//Uniform / Global variables for the  transform matrices
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	gl_Position = projection * view * model * vec4(vertexPosition, 1.0f); // Transforms vertices into clip coordinates

	vertexFragmentPos = vec3(model * vec4(vertexPosition, 1.0f)); // Gets fragment / pixel position in world space only (exclude view and projection)

	vertexFragmentNormal = mat3(transpose(inverse(model))) * vertexNormal; // get normal vectors in world space only and exclude normal translation properties
	vertexTextureCoordinate = textureCoordinate;
}
);
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
/* Surface Fragment Shader Source Code*/
const GLchar* surfaceFragmentShaderSource = GLSL(440,

	in vec3 vertexFragmentNormal; // For incoming normals
in vec3 vertexFragmentPos; // For incoming fragment position
in vec2 vertexTextureCoordinate;

out vec4 fragmentColor; // For outgoing cube color to the GPU

// Uniform / Global variables for object color, light color, light position, and camera/view position
uniform vec3 objectColor;
uniform vec3 ambientColor;
uniform vec3 light1Color;
uniform vec3 light1Position;
uniform vec3 light2Color;
uniform vec3 light2Position;
uniform vec3 viewPosition;
uniform sampler2D uTexture; // Useful when working with multiple textures
uniform vec2 uvScale;
uniform float ambientStrength = 0.1f; // Set ambient or global lighting strength
uniform float specularIntensity = 0.8f;
uniform float highlightSize = 16.0f;

void main()
{
	/*Phong lighting model calculations to generate ambient, diffuse, and specular components*/

	//Calculate Ambient lighting
	vec3 ambient = ambientStrength * ambientColor; // Generate ambient light color

	//**Calculate Diffuse lighting**
	vec3 norm = normalize(vertexFragmentNormal); // Normalize vectors to 1 unit
	vec3 light1Direction = normalize(light1Position - vertexFragmentPos); // Calculate distance (light direction) between light source and fragments/pixels on cube
	float impact1 = max(dot(norm, light1Direction), 0.0);// Calculate diffuse impact by generating dot product of normal and light
	vec3 diffuse1 = impact1 * light1Color; // Generate diffuse light color
	vec3 light2Direction = normalize(light2Position - vertexFragmentPos); // Calculate distance (light direction) between light source and fragments/pixels on cube
	float impact2 = max(dot(norm, light2Direction), 0.0);// Calculate diffuse impact by generating dot product of normal and light
	vec3 diffuse2 = impact2 * light2Color; // Generate diffuse light color

	//**Calculate Specular lighting**
	vec3 viewDir = normalize(viewPosition - vertexFragmentPos); // Calculate view direction
	vec3 reflectDir1 = reflect(-light1Direction, norm);// Calculate reflection vector
	//Calculate specular component
	float specularComponent1 = pow(max(dot(viewDir, reflectDir1), 0.0), highlightSize);
	vec3 specular1 = specularIntensity * specularComponent1 * light1Color;
	vec3 reflectDir2 = reflect(-light2Direction, norm);// Calculate reflection vector
	//Calculate specular component
	float specularComponent2 = pow(max(dot(viewDir, reflectDir2), 0.0), highlightSize);
	vec3 specular2 = specularIntensity * specularComponent2 * light2Color;

	//**Calculate phong result**
	//Texture holds the color to be used for all three components
	vec4 textureColor = texture(uTexture, vertexTextureCoordinate * uvScale);
	vec3 phong1 = (ambient + diffuse1 + specular1) * textureColor.xyz; //objectColor;
	vec3 phong2 = (ambient + diffuse2 + specular2) * textureColor.xyz; //objectColor;

	fragmentColor = vec4(phong1 + phong2, 1.0); // Send lighting results to GPU
}
);
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/* Light Object Shader Source Code*/
const GLchar* lightVertexShaderSource = GLSL(330,
	layout(location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	gl_Position = projection * view * model * vec4(aPos, 1.0);
}
);
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/* Light Object Shader Source Code*/
const GLchar* lightFragmentShaderSource = GLSL(330,
	out vec4 FragColor;

void main()
{
	FragColor = vec4(1.0); // set all 4 vector values to 1.0
}
);
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// blinn shading with texture =============================
const GLchar* vertexShaderSource = GLSL(440,
	layout(location = 0) in vec3 position;
layout(location = 2) in vec2 textureCoordinate;

out vec2 vertexTextureCoordinate;


//Global variables for the transform matrices
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	gl_Position = projection * view * model * vec4(position, 1.0f); // transforms vertices to clip coordinates
	vertexTextureCoordinate = textureCoordinate;
}
);

const char* fragmentShaderSource = GLSL(440,
	in vec2 vertexTextureCoordinate;

out vec4 fragmentColor;

uniform sampler2D uTexture;
uniform vec2 uvScale;

void main()
{
	fragmentColor = texture(uTexture, vertexTextureCoordinate * uvScale);
}
);

/* User-defined Function prototypes to:
 * initialize the program, set the window size,
 * redraw graphics on the window when resized,
 * and render graphics on the screen
 */
bool UInitialize(int, char* [], GLFWwindow** window);
void UResizeWindow(GLFWwindow* window, int width, int height);
void UProcessInput(GLFWwindow* window);
void UMousePositionCallback(GLFWwindow* window, double xpos, double ypos);
void UMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void UMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void URender();
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId);
void UDestroyShaderProgram(GLuint programId);
bool UCreateTexture(const char* filename, GLuint& textureId);
void UDestroyTexture(GLuint textureId);
bool keypress = false;
bool bOrthoMode = false;

// main function. Entry point to the OpenGL program
int main(int argc, char* argv[])
{
	if (!UInitialize(argc, argv, &gWindow))
		return EXIT_FAILURE;

	// Create the meshes
	meshes.CreateMeshes();

	// Create the shader program
	if (!UCreateShaderProgram(surfaceVertexShaderSource, surfaceFragmentShaderSource, gSurfaceProgramId))
		return EXIT_FAILURE;

	if (!UCreateShaderProgram(lightVertexShaderSource, lightFragmentShaderSource, gLightProgramId))
		return EXIT_FAILURE;

	// Load texture
	const char* texFilename = "resources/textures/dirt.jpg";
	if (!UCreateTexture(texFilename, gFieldTextureId))
	{
		cout << "Failed to load texture " << texFilename << endl;
		return EXIT_FAILURE;
	}

	const char* texFilename2 = "resources/textures/homeplate.jpg";
	if (!UCreateTexture(texFilename2, gHomePlateTextureId))
	{
		cout << "Failed to load texture " << texFilename2 << endl;
		return EXIT_FAILURE;
	}

	const char* texFilename3 = "resources/textures/baseball.jpg";
	if (!UCreateTexture(texFilename3, gBallTextureId))
	{
		cout << "Failed to load texture " << texFilename3 << endl;
		return EXIT_FAILURE;
	}

	const char* texFilename4 = "resources/textures/bat.jpg";
	if (!UCreateTexture(texFilename4, gBatTextureId))
	{
		cout << "Failed to load texture " << texFilename4 << endl;
		return EXIT_FAILURE;
	}

	glEnable(GL_DEPTH_TEST);

	// tell opengl for each sampler to which texture unit it belongs to (only has to be done once)
	glUseProgram(gSurfaceProgramId);
	// We set the texture as texture unit 0
	glUniform1i(glGetUniformLocation(gSurfaceProgramId, "uTexture"), 0);
	glUniform2f(glGetUniformLocation(gSurfaceProgramId, "uvScale"), gUVScale.x, gUVScale.y);

	gCameraFront.Front = glm::vec3(0.0, -1.0, -2.0f);
	gCameraFront.Up = glm::vec3(0.0, 1.0, 0.0);
	g_pCurrentCamera = &gCameraFront;

	// render loop
	// -----------
	while (!glfwWindowShouldClose(gWindow))
	{
		// per-frame timing
		// --------------------
		float currentFrame = glfwGetTime();
		gDeltaTime = currentFrame - gLastFrame;
		gLastFrame = currentFrame;

		// input
		// -----
		UProcessInput(gWindow);

		URender();

		glfwPollEvents();
	}

	// Release mesh data
	meshes.DestroyMeshes();

	UDestroyShaderProgram(gSurfaceProgramId);
	UDestroyShaderProgram(gLightProgramId);

	exit(EXIT_SUCCESS); // Terminates the program successfully
}


// Initialize GLFW, GLEW, and create a window
bool UInitialize(int argc, char* argv[], GLFWwindow** window)
{
	// GLFW: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	// GLFW: window creation
	// ---------------------
	* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
	if (*window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent(*window);
	glfwSetFramebufferSizeCallback(*window, UResizeWindow);

	glfwSetCursorPosCallback(*window, UMousePositionCallback);
	glfwSetScrollCallback(*window, UMouseScrollCallback);
	glfwSetMouseButtonCallback(*window, UMouseButtonCallback);

	// tell GLFW to capture our mouse
	glfwSetInputMode(*window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// GLEW: initialize
	// ----------------
	// Note: if using GLEW version 1.13 or earlier
	glewExperimental = GL_TRUE;
	GLenum GlewInitResult = glewInit();

	if (GLEW_OK != GlewInitResult)
	{
		std::cerr << glewGetErrorString(GlewInitResult) << std::endl;
		return false;
	}

	// Displays GPU OpenGL version
	cout << "INFO: OpenGL Version: " << glGetString(GL_VERSION) << endl;

	return true;
}



// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
void UProcessInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);


	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);





	if (keypress)
	{
		double x, y;
		glfwGetCursorPos(window, &x, &y);
		cout << "Cursor at position (" << x << ", " << y << ")" << endl;
	}

	static const float cameraSpeed = 1.0f;

	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	float cameraOffset = cameraSpeed * gDeltaTime;

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		gCameraFront.ProcessKeyboard(FORWARD, gDeltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		gCameraFront.ProcessKeyboard(BACKWARD, gDeltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		gCameraFront.ProcessKeyboard(LEFT, gDeltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		gCameraFront.ProcessKeyboard(RIGHT, gDeltaTime);
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
		gCameraFront.ProcessKeyboard(UP, gDeltaTime);
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
		gCameraFront.ProcessKeyboard(DOWN, gDeltaTime);
	if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {

		bOrthoMode = !bOrthoMode;
	}
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void UMousePositionCallback(GLFWwindow* window, double xpos, double ypos)
{
	if (gFirstMouse)
	{
		gLastX = xpos;
		gLastY = ypos;
		gFirstMouse = false;
	}

	float xoffset = xpos - gLastX;
	float yoffset = gLastY - ypos; // reversed since y-coordinates go from bottom to top

	gLastX = xpos;
	gLastY = ypos;

	g_pCurrentCamera->ProcessMouseMovement(xoffset, yoffset);
}


// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void UMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	gDeltaTime -= (float)yoffset;
	if (gDeltaTime < 1.0f)
		gDeltaTime = 1.0f;
	if (gDeltaTime > 1.1f)
		gDeltaTime = 0.1f;
}

// glfw: handle mouse button events
// --------------------------------
void UMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	switch (button)
	{
	case GLFW_MOUSE_BUTTON_LEFT:
	{
		if (action == GLFW_PRESS)
			cout << "Left mouse button pressed" << endl;
		else
			cout << "Left mouse button released" << endl;
	}
	break;

	case GLFW_MOUSE_BUTTON_MIDDLE:
	{
		if (action == GLFW_PRESS)
			cout << "Middle mouse button pressed" << endl;
		else
			cout << "Middle mouse button released" << endl;
	}
	break;

	case GLFW_MOUSE_BUTTON_RIGHT:
	{
		if (action == GLFW_PRESS)
			cout << "Right mouse button pressed" << endl;
		else
			cout << "Right mouse button released" << endl;
	}
	break;

	default:
		cout << "Unhandled mouse button event" << endl;
		break;
	}
}


// glfw: whenever the window size changed (by OS or user resize) this callback function executes
void UResizeWindow(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

void URender()
{
	GLint modelLoc;
	GLint viewLoc;
	GLint projLoc;
	GLint viewPosLoc;
	GLint ambStrLoc;
	GLint ambColLoc;
	GLint light1ColLoc;
	GLint light1PosLoc;
	GLint light2ColLoc;
	GLint light2PosLoc;
	GLint objColLoc;
	GLint specIntLoc;
	GLint highlghtSzLoc;
	glm::mat4 scale;
	glm::mat4 rotation;
	glm::mat4 rotation1;
	glm::mat4 rotation2;
	glm::mat4 translation;
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 projection;

	// Sets the background color of the window to black (it will be implicitely used by glClear)
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	view = gCameraFront.GetViewMatrix();
	projection;

	if (bOrthoMode == true) {
		projection = glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, -10.0f, 10.0f);
	}
	if (bOrthoMode == false) {
		projection = glm::perspective(45.0f, (GLfloat)WINDOW_WIDTH / (GLfloat)WINDOW_HEIGHT, 0.1f, 100.0f);
	}

	// Set the shader to be used
	glUseProgram(gSurfaceProgramId);

	// Retrieves and passes transform matrices to the Shader program
	modelLoc = glGetUniformLocation(gSurfaceProgramId, "model");
	viewLoc = glGetUniformLocation(gSurfaceProgramId, "view");
	projLoc = glGetUniformLocation(gSurfaceProgramId, "projection");
	viewPosLoc = glGetUniformLocation(gSurfaceProgramId, "viewPosition");
	ambStrLoc = glGetUniformLocation(gSurfaceProgramId, "ambientStrength");
	ambColLoc = glGetUniformLocation(gSurfaceProgramId, "ambientColor");
	light1ColLoc = glGetUniformLocation(gSurfaceProgramId, "light1Color");
	light1PosLoc = glGetUniformLocation(gSurfaceProgramId, "light1Position");
	light2ColLoc = glGetUniformLocation(gSurfaceProgramId, "light2Color");
	light2PosLoc = glGetUniformLocation(gSurfaceProgramId, "light2Position");
	objColLoc = glGetUniformLocation(gSurfaceProgramId, "objectColor");
	specIntLoc = glGetUniformLocation(gSurfaceProgramId, "specularIntensity");
	highlghtSzLoc = glGetUniformLocation(gSurfaceProgramId, "highlightSize");

	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

	//*******************************
	// Configure the light properties
	//*******************************
	//set the camera view location
	glUniform3f(viewPosLoc, g_pCurrentCamera->Position.x, g_pCurrentCamera->Position.y, g_pCurrentCamera->Position.z);
	//set ambient lighting strength
	glUniform1f(ambStrLoc, 0.3f);
	//set ambient color
	glUniform3f(ambColLoc, 0.4f, 0.4f, 0.4f);
	glUniform3f(light1ColLoc, 1.0f, 0.0f, 0.0f);	//red
	glUniform3f(light1PosLoc, -1.0f, 3.0f, .7f);
	glUniform3f(light2ColLoc, 0.0f, 1.0f, 0.0f);	//green
	glUniform3f(light2PosLoc, 1.0f, 3.0f, .7f);
	//set specular intensity
	glUniform1f(specIntLoc, 1.0f);
	//set specular highlight size
	glUniform1f(highlghtSzLoc, 32.0f);

	// Activate the VBOs contained within the mesh's VAO
	glBindVertexArray(meshes.gPlaneMesh.vao);
	//*************************************
	// Render the field plane
	//*************************************
	// 1. Scales the object
	scale = glm::scale(glm::vec3(4.0f, 1.0f, 6.0f));
	// 2. Rotates shape
	rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
	// 3. translate shape
	translation = glm::translate(glm::vec3(0.0f, 0.0f, 0.0f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
	glUniform2f(glGetUniformLocation(gSurfaceProgramId, "uvScale"), 10.0f, 10.0f);

	// bind textures on corresponding texture units
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gFieldTextureId);

	glDrawElements(GL_TRIANGLES, meshes.gPlaneMesh.nIndices, GL_UNSIGNED_INT, (void*)0);
	glBindVertexArray(0);

	// Activate the VBOs contained within the mesh's VAO
	glBindVertexArray(meshes.gCubeMesh.vao);

	//*************************************
	// Render the home plate
	//*************************************
	// 1. Scales the object
	scale = glm::scale(glm::vec3(.8f, .1f, .4f));
	// 2. Rotates shape
	rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
	// 3. translate shape
	translation = glm::translate(glm::vec3(0.0f, 0.0f, 0.0f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
	glUniform2f(glGetUniformLocation(gSurfaceProgramId, "uvScale"), 1.0f, 1.0f);

	// bind textures on corresponding texture units
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gHomePlateTextureId);

	glDrawArrays(GL_TRIANGLES, 0, meshes.gCubeMesh.nVertices);

	glBindVertexArray(0);

	// Activate the VBOs contained within the mesh's VAO
	glBindVertexArray(meshes.gPrismMesh.vao);

	// 1. Scales the object
	scale = glm::scale(glm::vec3(.8f, .1f, .4f));
	// 2. Rotates shape
	rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
	// 3. translate shape
	translation = glm::translate(glm::vec3(0.0f, 0.0f, 0.40f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
	glUniform2f(glGetUniformLocation(gSurfaceProgramId, "uvScale"), 1.0f, 1.0f);

	// bind textures on corresponding texture units
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gHomePlateTextureId);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, meshes.gPrismMesh.nVertices);

	glBindVertexArray(0);

	// Activate the VBOs contained within the mesh's VAO
	glBindVertexArray(meshes.gSphereMesh.vao);

	//*************************************
	// Render the baseball
	//*************************************
	// 1. Scales the object
	scale = glm::scale(glm::vec3(.05f, .05f, .05f));
	// 2. Rotates shape
	rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
	// 3. translate shape
	translation = glm::translate(glm::vec3(-.5f, 0.05f, .1f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
	glUniform2f(glGetUniformLocation(gSurfaceProgramId, "uvScale"), 1.0f, 1.0f);

	// bind textures on corresponding texture units
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gBallTextureId);

	glDrawElements(GL_TRIANGLES, meshes.gSphereMesh.nIndices, GL_UNSIGNED_INT, NULL);

	//***************************
	//TIP OF BAT Sphere
	//***************************
	scale = glm::scale(glm::vec3(.0407f, .0407f, .0405f));
	// 2. Rotates shape
	rotation = glm::rotate(8.8f, glm::vec3(-5.0f, 5.0f, 5.0f));
	// 3. translate shape
	translation = glm::translate(glm::vec3(-.404f, 0.038f, -0.4089f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
	glUniform2f(glGetUniformLocation(gSurfaceProgramId, "uvScale"), 1.0f, 1.0f);

	// bind textures on corresponding texture units
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gBatTextureId);

	glDrawElements(GL_TRIANGLES, meshes.gSphereMesh.nIndices, GL_UNSIGNED_INT, NULL);

	glBindVertexArray(0);

	glBindVertexArray(meshes.gCylinderMesh.vao);
	//*************************************
	// Render the Cylinder
	//*************************************
	// 1. Scales the object
	scale = glm::scale(glm::vec3(0.04f, 0.3f, 0.04f));
	// 2. Rotates shape
	rotation = glm::rotate(-11.005f, glm::vec3(0.4, 0.0f, 1.0f));
	// 3. translate shape
	translation = glm::translate(glm::vec3(-0.4f, 0.038f, -0.41f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
	glUniform2f(glGetUniformLocation(gSurfaceProgramId, "uvScale"), 1.0f, 1.0f);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gBatTextureId);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 36);    // bottom
	glDrawArrays(GL_TRIANGLE_FAN, 36, 36); // top
	glDrawArrays(GL_TRIANGLE_STRIP, 72, 146); //sides
	glBindVertexArray(0);

	glBindVertexArray(meshes.gTaperedCylinderMesh.vao);
	//*************************************
	// Render the Tapered Cylinder
	//*************************************
	// 1. Scales the object
	scale = glm::scale(glm::vec3(0.04f, 0.6f, 0.04f));
	// 2. Rotates shape
	rotation = glm::rotate(-11.005f, glm::vec3(0.4, 0.0f, 1.0f));
	// 3. translate shape
	translation = glm::translate(glm::vec3(-0.678f, 0.041f, -0.299f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
	glUniform2f(glGetUniformLocation(gSurfaceProgramId, "uvScale"), 1.0f, 1.0f);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gBatTextureId);
	
	glDrawArrays(GL_TRIANGLE_FAN, 0, 36);    // bottom
	glDrawArrays(GL_TRIANGLE_FAN, 36, 36); // top
	glDrawArrays(GL_TRIANGLE_STRIP, 72, 146); //sides
	glBindVertexArray(0);

	glBindVertexArray(meshes.gCylinderMesh.vao);
	//*************************************
	// Render the bottom Cylinder
	//*************************************
	// 1. Scales the object
	scale = glm::scale(glm::vec3(0.0206f, 0.3f, 0.0206f));
	// 2. Rotates shape
	rotation = glm::rotate(-11.005f, glm::vec3(0.4, 0.0f, 1.0f));
	// 3. translate shape
	translation = glm::translate(glm::vec3(-1.23f, 0.0458f, -0.078));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
	glUniform2f(glGetUniformLocation(gSurfaceProgramId, "uvScale"), 1.0f, 1.0f);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gBatTextureId);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 36);    // bottom
	glDrawArrays(GL_TRIANGLE_FAN, 36, 36); // top
	glDrawArrays(GL_TRIANGLE_STRIP, 72, 146); //sides
	glBindVertexArray(0);

	glBindVertexArray(meshes.gTorusMesh.vao);
	
	//*************************************
	// Render the Torus
	//*************************************
	// Scale the object
	scale = glm::scale(glm::vec3(0.0275f, 0.0275f, 0.05f));
	// 2. Rotates shape
	rotation = glm::rotate(5.1f, glm::vec3(0.0, 1.0f, 0.0f));
	// 3. translate shape
	translation = glm::translate(glm::vec3(-1.51f, 0.0485f, 0.034f));
	
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
	glUniform2f(glGetUniformLocation(gSurfaceProgramId, "uvScale"), 1.0f, 1.0f);

	glDrawArrays(GL_TRIANGLES, 0, meshes.gTorusMesh.nVertices);
	glBindVertexArray(0);

	glUseProgram(0);

	//*************************************
	// Render the 2 light objects
	//*************************************
	// Set the shader to be used
	glUseProgram(gLightProgramId);

	// Retrieves and passes transform matrices to the Shader program
	modelLoc = glGetUniformLocation(gLightProgramId, "model");
	viewLoc = glGetUniformLocation(gLightProgramId, "view");
	projLoc = glGetUniformLocation(gLightProgramId, "projection");

	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

	// Activate the VBOs contained within the mesh's VAO
	glBindVertexArray(meshes.gPyramidMesh.vao);

	// 1. Scales the object by 2
	scale = glm::scale(glm::vec3(.3f, .3f, .3f));
	// 2. Rotates shape by 15 degrees in the x axis
	rotation = glm::rotate(-0.2f, glm::vec3(1.0, 0.0f, 0.0f));
	// 3. Place object at the origin
	translation = glm::translate(glm::vec3(-1.0f, 2.0f, .7f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;

	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	//glDrawArrays(GL_TRIANGLES, 0, gPyramidMesh.nVertices);
	glDrawElements(GL_TRIANGLES, meshes.gPyramidMesh.nIndices, GL_UNSIGNED_INT, NULL);

	// 1. Scales the object by 2
	scale = glm::scale(glm::vec3(.3f, .3f, .3f));
	// 2. Rotates shape by 15 degrees in the x axis
	rotation = glm::rotate(-0.2f, glm::vec3(1.0, 0.0f, 0.0f));
	// 3. Place object at the origin
	translation = glm::translate(glm::vec3(1.0f, 2.0f, .7f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;

	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	//glDrawArrays(GL_TRIANGLES, 0, gPyramidMesh.nVertices);
	glDrawElements(GL_TRIANGLES, meshes.gPyramidMesh.nIndices, GL_UNSIGNED_INT, NULL);

	glBindVertexArray(0);

	glUseProgram(0);

	// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
	glfwSwapBuffers(gWindow);    // Flips the the back buffer with the front buffer every frame.
}

// Implements the UCreateShaders function
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId)
{
	// Compilation and linkage error reporting
	int success = 0;
	char infoLog[512];

	// Create a Shader program object.
	programId = glCreateProgram();

	// Create the vertex and fragment shader objects
	GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);

	// Retrive the shader source
	glShaderSource(vertexShaderId, 1, &vtxShaderSource, NULL);
	glShaderSource(fragmentShaderId, 1, &fragShaderSource, NULL);

	// Compile the vertex shader, and print compilation errors (if any)
	glCompileShader(vertexShaderId); // compile the vertex shader
	// check for shader compile errors
	glGetShaderiv(vertexShaderId, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShaderId, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;

		return false;
	}

	glCompileShader(fragmentShaderId); // compile the fragment shader
	// check for shader compile errors
	glGetShaderiv(fragmentShaderId, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShaderId, sizeof(infoLog), NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;

		return false;
	}

	// Attached compiled shaders to the shader program
	glAttachShader(programId, vertexShaderId);
	glAttachShader(programId, fragmentShaderId);

	glLinkProgram(programId);   // links the shader program
	// check for linking errors
	glGetProgramiv(programId, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(programId, sizeof(infoLog), NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;

		return false;
	}

	glUseProgram(programId);    // Uses the shader program

	return true;
}


void UDestroyShaderProgram(GLuint programId)
{
	glDeleteProgram(programId);
}

// Images are loaded with Y axis going down, but OpenGL's Y axis goes up, so let's flip it
void flipImageVertically(unsigned char* image, int width, int height, int channels)
{
	for (int j = 0; j < height / 2; ++j)
	{
		int index1 = j * width * channels;
		int index2 = (height - 1 - j) * width * channels;

		for (int i = width * channels; i > 0; --i)
		{
			unsigned char tmp = image[index1];
			image[index1] = image[index2];
			image[index2] = tmp;
			++index1;
			++index2;
		}
	}
}

/*Generate and load the texture*/
bool UCreateTexture(const char* filename, GLuint& textureId)
{
	int width, height, channels;
	unsigned char* image = stbi_load(filename, &width, &height, &channels, 0);
	if (image)
	{
		flipImageVertically(image, width, height, channels);

		glGenTextures(1, &textureId);
		glBindTexture(GL_TEXTURE_2D, textureId);

		// set the texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		if (channels == 3)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
		else if (channels == 4)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
		else
		{
			cout << "Not implemented to handle image with " << channels << " channels" << endl;
			return false;
		}

		glGenerateMipmap(GL_TEXTURE_2D);

		stbi_image_free(image);
		glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture

		return true;
	}

	// Error loading the image
	return false;
}


void UDestroyTexture(GLuint textureId)
{
	glGenTextures(1, &textureId);
}

