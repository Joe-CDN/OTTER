#include <Logging.h>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <json.hpp>
#include <fstream>

#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>

#include "IndexBuffer.h"
#include "VertexBuffer.h"
#include "VertexArrayObject.h"
#include "Shader.h"
#include "Camera.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "InputHelpers.h"
#include "MeshBuilder.h"
#include "MeshFactory.h"
#include "ObjLoader.h"
#include "VertexTypes.h"

#define LOG_GL_NOTIFICATIONS

/*
	Handles debug messages from OpenGL
	https://www.khronos.org/opengl/wiki/Debug_Output#Message_Components
	@param source    Which part of OpenGL dispatched the message
	@param type      The type of message (ex: error, performance issues, deprecated behavior)
	@param id        The ID of the error or message (to distinguish between different types of errors, like nullref or index out of range)
	@param severity  The severity of the message (from High to Notification)
	@param length    The length of the message
	@param message   The human readable message from OpenGL
	@param userParam The pointer we set with glDebugMessageCallback (should be the game pointer)
*/
void GlDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
	std::string sourceTxt;
	switch (source) {
	case GL_DEBUG_SOURCE_API: sourceTxt = "DEBUG"; break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM: sourceTxt = "WINDOW"; break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceTxt = "SHADER"; break;
	case GL_DEBUG_SOURCE_THIRD_PARTY: sourceTxt = "THIRD PARTY"; break;
	case GL_DEBUG_SOURCE_APPLICATION: sourceTxt = "APP"; break;
	case GL_DEBUG_SOURCE_OTHER: default: sourceTxt = "OTHER"; break;
	}
	switch (severity) {
	case GL_DEBUG_SEVERITY_LOW:          LOG_INFO("[{}] {}", sourceTxt, message); break;
	case GL_DEBUG_SEVERITY_MEDIUM:       LOG_WARN("[{}] {}", sourceTxt, message); break;
	case GL_DEBUG_SEVERITY_HIGH:         LOG_ERROR("[{}] {}", sourceTxt, message); break;
		#ifdef LOG_GL_NOTIFICATIONS
	case GL_DEBUG_SEVERITY_NOTIFICATION: LOG_INFO("[{}] {}", sourceTxt, message); break;
		#endif
	default: break;
	}
}

GLFWwindow* window;
Camera::sptr camera = nullptr;

void GlfwWindowResizedCallback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
	camera->ResizeWindow(width, height);
}

bool initGLFW() {
	if (glfwInit() == GLFW_FALSE) {
		LOG_ERROR("Failed to initialize GLFW");
		return false;
	}

#ifdef _DEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif
	
	//Create a new GLFW window
	window = glfwCreateWindow(800, 800, "INFR1350U", nullptr, nullptr);
	glfwMakeContextCurrent(window);

	// Set our window resized callback
	glfwSetWindowSizeCallback(window, GlfwWindowResizedCallback);

	return true;
}

bool initGLAD() {
	if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0) {
		LOG_ERROR("Failed to initialize Glad");
		return false;
	}
	return true;
}

void InitImGui() {
	// Creates a new ImGUI context
	ImGui::CreateContext();
	// Gets our ImGUI input/output 
	ImGuiIO& io = ImGui::GetIO();
	// Enable keyboard navigation
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	// Allow docking to our window
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	// Allow multiple viewports (so we can drag ImGui off our window)
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	// Allow our viewports to use transparent backbuffers
	io.ConfigFlags |= ImGuiConfigFlags_TransparentBackbuffers;

	// Set up the ImGui implementation for OpenGL
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 410");

	// Dark mode FTW
	ImGui::StyleColorsDark();

	// Get our imgui style
	ImGuiStyle& style = ImGui::GetStyle();
	//style.Alpha = 1.0f;
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 0.8f;
	}
}

void ShutdownImGui()
{
	// Cleanup the ImGui implementation
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	// Destroy our ImGui context
	ImGui::DestroyContext();
}

std::vector<std::function<void()>> imGuiCallbacks;
void RenderImGui() {
	// Implementation new frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	// ImGui context new frame
	ImGui::NewFrame();

	if (ImGui::Begin("Debug")) {
		// Render our GUI stuff
		for (auto& func : imGuiCallbacks) {
			func();
		}
		ImGui::End();
	}
	
	// Make sure ImGui knows how big our window is
	ImGuiIO& io = ImGui::GetIO();
	int width{ 0 }, height{ 0 };
	glfwGetWindowSize(window, &width, &height);
	io.DisplaySize = ImVec2((float)width, (float)height);

	// Render all of our ImGui elements
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// If we have multiple viewports enabled (can drag into a new window)
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		// Update the windows that ImGui is using
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		// Restore our gl context
		glfwMakeContextCurrent(window);
	}
}

int main() {
	Logger::Init(); // We'll borrow the logger from the toolkit, but we need to initialize it

	//Initialize GLFW
	if (!initGLFW())
		return 1;

	//Initialize GLAD
	if (!initGLAD())
		return 1;

	// Let OpenGL know that we want debug output, and route it to our handler function
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(GlDebugMessage, nullptr);
	
	// We'll be implementing a loader that works a bit like an OBJ loader to learn how to read files, we'll
	// load an exact copy of the mesh created above
	VertexArrayObject::sptr vaoMonkey = ObjLoader::LoadFromFile("monkey.obj");
	
	// Load our shaders
	Shader::sptr shader = Shader::Create();
	shader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
	shader->LoadShaderPartFromFile("shaders/frag_shader.glsl", GL_FRAGMENT_SHADER);  
	shader->Link(); 
	
	// GL states
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glm::mat4 transform3 = glm::mat4(1.0f);
	glm::mat4 transform4 = glm::mat4(1.0f);

	camera = Camera::Create();
	camera->SetPosition(glm::vec3(0, 3, 3)); // Set initial position
	camera->SetUp(glm::vec3(0, 0, 1)); // Use a z-up coordinate system
	camera->LookAt(glm::vec3(0.0f)); // Look at center of the screen
	camera->SetFovDegrees(90.0f); // Set an initial FOV
	
	// This is an example of a key press handling helper. Look at InputHelpers.h an .cpp to see
	// how this is implemented. Note that the ampersand here is capturing the variables within
	// the scope. If you wanted to do some method on the class, your best bet would be to give it a method and
	// use std::bind
	bool is_wireframe = false;
	KeyPressWatcher tKeyWatcher = KeyPressWatcher(GLFW_KEY_T, [&]() {
		is_wireframe = !is_wireframe;
		glPolygonMode(GL_FRONT, is_wireframe ? GL_LINE : GL_FILL);
	});

	InitImGui();
		
	// Our high-precision timer
	double lastFrame = glfwGetTime();

	bool spacePressed = false;
	
	///// Game loop /////
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		// Calculate the time since our last frame (dt)
		double thisFrame = glfwGetTime();
		float dt = static_cast<float>(thisFrame - lastFrame);

		// We need to poll our key watchers so they can do their logic with the GLFW state
		tKeyWatcher.Poll(window);

		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
			transform3 = glm::translate(transform3, glm::vec3( 1.0f * dt, 0.0f, 0.0f));
		}
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
			transform3 = glm::translate(transform3, glm::vec3(-1.0f * dt, 0.0f, 0.0f));
		}
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
			transform3 = glm::translate(transform3, glm::vec3(0.0f, -1.0f * dt, 0.0f));
		}
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
			transform3 = glm::translate(transform3, glm::vec3(0.0f,  1.0f * dt, 0.0f));
		}
		if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
			if (!spacePressed)
			{
				camera->toggleProj();
			}
			spacePressed = true;
		}
		else {
			spacePressed = false;
		}			
		
		transform3 = glm::translate(transform3, glm::vec3(-1.0f, 0.0f, 0.0f));
		transform3 = glm::rotate(glm::mat4(1.0f), static_cast<float>(thisFrame), glm::vec3(1, 0, 0));
		transform3 = glm::translate(transform3, glm::vec3(1.0f, 0.0f, 0.0f));

		transform4 = glm::translate(transform4, glm::vec3(1.0f, 0.0f, 0.0f));
		transform4 = glm::rotate(glm::mat4(1.0f), -static_cast<float>(thisFrame) * 0.5f, glm::vec3(1, 0, 0));
		transform4 = glm::translate(transform4, glm::vec3(-1.0f, 0.0f, 0.0f));

		glClearColor(0.08f, 0.17f, 0.31f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		shader->Bind();
		// These are the uniforms that update only once per frame

		shader->SetUniformMatrix("u_ModelViewProjection", camera->GetViewProjection() * transform3);
		shader->SetUniformMatrix("u_Model", transform3);
		shader->SetUniformMatrix("u_ModelRotation", glm::mat3(transform3));
		vaoMonkey->Render();
		shader->SetUniformMatrix("u_ModelViewProjection", camera->GetViewProjection()* transform4);
		shader->SetUniformMatrix("u_Model", transform4);
		shader->SetUniformMatrix("u_ModelRotation", glm::mat3(transform4));
		vaoMonkey->Render();

		glfwSwapBuffers(window);
		lastFrame = thisFrame;
	}

	ShutdownImGui();

	// Clean up the toolkit logger so we don't leak memory
	Logger::Uninitialize();
	return 0;
}