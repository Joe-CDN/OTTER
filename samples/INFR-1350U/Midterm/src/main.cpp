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

#include "Graphics/IndexBuffer.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/VertexArrayObject.h"
#include "Graphics/Shader.h"
#include "Gameplay/Camera.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "Gameplay/Transform.h"
#include "Graphics/Texture2D.h"
#include "Graphics/Texture2DData.h"
#include "Utilities/InputHelpers.h"
#include "Utilities/MeshBuilder.h"
#include "Utilities/MeshFactory.h"
#include "Utilities/NotObjLoader.h"
#include "Utilities/ObjLoader.h"
#include "Utilities/VertexTypes.h"

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

void RenderVAO(
	const Shader::sptr& shader,
	const VertexArrayObject::sptr& vao,
	const Camera::sptr& camera,
	const Transform::sptr& transform)
{
	shader->SetUniformMatrix("u_ModelViewProjection", camera->GetViewProjection() * transform->LocalTransform());
	shader->SetUniformMatrix("u_Model", transform->LocalTransform());
	shader->SetUniformMatrix("u_NormalMatrix", transform->NormalMatrix());
	vao->Render();
}

void controlP1(const Transform::sptr& transform, float dt) {
	bool canMove;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		transform->RotateLocal(0.0f, -90.0f * dt, 0.0f);
		canMove = false;
	}
	else {
		canMove = true;
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		transform->RotateLocal(0.0f, 90.0f * dt,0.0f);
		canMove = false;
	}
	else {
		canMove = true;
	}

	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && canMove == true) {
		transform->MoveLocal(-4.0f * dt, 0.0f, 0.0f);
	}
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS && canMove == true) {
		transform->MoveLocal(4.0f * dt, 0.0f, 0.0f);
	}	
}
void controlP2(const Transform::sptr& transform, float dt) {
	bool canMove;
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
		transform->RotateLocal(0.0f, -90.0f * dt, 0.0f);
		canMove = false;
	}
	else {
		canMove = true;
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
		transform->RotateLocal(0.0f, 90.0f * dt, 0.0f);
		canMove = false;
	}
	else {
		canMove = true;
	}

	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS && canMove == true) {
		transform->MoveLocal(-4.0f * dt, 0.0f, 0.0f);
	}
	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS && canMove == true) {
		transform->MoveLocal(4.0f * dt, 0.0f, 0.0f);
	}
}
bool CheckCollision(const Transform::sptr& object1, const Transform::sptr& object2)
{
	bool collisionX = object1->GetLocalPosition().x + 3.0f >= object2->GetLocalPosition().x &&
		object2->GetLocalPosition().x + 3.0f >= object1->GetLocalPosition().x;
	// collision y-axis?
	bool collisionY = object1->GetLocalPosition().z + 3.0f >= object2->GetLocalPosition().z &&
		object2->GetLocalPosition().z + 3.0f >= object1->GetLocalPosition().z;
	// collision only if on both axes
	return collisionX && collisionY;
}
bool CheckCollisionBulletToTank(const Transform::sptr& bullet, const Transform::sptr& tank)
{
	bool collisionX = bullet->GetLocalPosition().x + 1.0f >= tank->GetLocalPosition().x &&
		tank->GetLocalPosition().x + 1.0f >= bullet->GetLocalPosition().x;
	// collision y-axis?
	bool collisionY = bullet->GetLocalPosition().z + 1.0f >= tank->GetLocalPosition().z &&
		tank->GetLocalPosition().z + 1.0f >= bullet->GetLocalPosition().z;
	// collision only if on both axes
	return collisionX && collisionY;
}
void WallCollision(const Transform::sptr& object1, float dt, bool frwrds)
{
	if (object1->GetLocalPosition().x >= 18.0f && frwrds)
		object1->MoveLocal(-4.0f * dt, 0.0f, 0.0f);
	if (object1->GetLocalPosition().x >= 18.0f && !frwrds)
		object1->MoveLocal(4.0f * dt, 0.0f, 0.0f);

	if (object1->GetLocalPosition().x <= -18.0f && frwrds)
		object1->MoveLocal(-4.0f * dt, 0.0f, 0.0f);
	if (object1->GetLocalPosition().x <= -18.0f && !frwrds)
		object1->MoveLocal(4.0f * dt, 0.0f, 0.0f);

	if (object1->GetLocalPosition().z >= 18.0f && frwrds)
		object1->MoveLocal(-4.0f * dt, 0.0f, 0.0f);
	if (object1->GetLocalPosition().z >= 18.0f && !frwrds)
		object1->MoveLocal(4.0f * dt, 0.0f, 0.0f);

	if (object1->GetLocalPosition().z <= -18.0f && frwrds)
		object1->MoveLocal(-4.0f * dt, 0.0f, 0.0f);
	if (object1->GetLocalPosition().z <= -18.0f && !frwrds)
		object1->MoveLocal(4.0f * dt, 0.0f, 0.0f);
}

struct Material
{
	Texture2D::sptr Albedo;
	Texture2D::sptr Specular;
	float           Shininess;
	float			mixRatio;
};

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

	// Enable texturing
	glEnable(GL_TEXTURE_2D);
	
	VertexArrayObject::sptr tankVao = ObjLoader::LoadFromFile("models/tank.obj");
	VertexArrayObject::sptr bulletVao = ObjLoader::LoadFromFile("models/bullet.obj");
	VertexArrayObject::sptr obstacleVao = ObjLoader::LoadFromFile("models/obstacle.obj");
	VertexArrayObject::sptr sceneVao = ObjLoader::LoadFromFile("models/arena.obj");
	VertexArrayObject::sptr scoreVao = ObjLoader::LoadFromFile("models/scoreCard.obj");
		
	// Load our shaders
	Shader::sptr shader = Shader::Create();
	shader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
	shader->LoadShaderPartFromFile("shaders/frag_blinn_phong_textured.glsl", GL_FRAGMENT_SHADER);  
	shader->Link();  

	glm::vec3 lightPos = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 lightCol = glm::vec3(1.0f);
	float     lightAmbientPow = 2.0f;
	float     lightSpecularPow = 0.5f;
	glm::vec3 ambientCol = glm::vec3(1.0f);
	float     ambientPow = 0.5f;
	float     shininess = 4.0f;
	float     lightLinearFalloff = 0.09f;
	float     lightQuadraticFalloff = 0.032f;
	
	// These are our application / scene level uniforms that don't necessarily update
	// every frame
	shader->SetUniform("u_LightPos", lightPos);
	shader->SetUniform("u_LightCol", lightCol);
	shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
	shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
	shader->SetUniform("u_AmbientCol", ambientCol);
	shader->SetUniform("u_AmbientStrength", ambientPow);
	shader->SetUniform("u_Shininess", shininess);
	shader->SetUniform("u_LightAttenuationConstant", 1.0f);
	shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
	shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);

	// We'll add some ImGui controls to control our shader
	imGuiCallbacks.push_back([&]() {
		if (ImGui::CollapsingHeader("Scene Level Lighting Settings"))
		{
			if (ImGui::ColorPicker3("Ambient Color", glm::value_ptr(ambientCol))) {
				shader->SetUniform("u_AmbientCol", ambientCol);
			}
			if (ImGui::SliderFloat("Fixed Ambient Power", &ambientPow, 0.01f, 1.0f)) {
				shader->SetUniform("u_AmbientStrength", ambientPow); 
			}
		}
		if (ImGui::CollapsingHeader("Light Level Lighting Settings")) 
		{
			if (ImGui::DragFloat3("Light Pos", glm::value_ptr(lightPos), 0.01f, -10.0f, 10.0f)) {
				shader->SetUniform("u_LightPos", lightPos);
			}
			if (ImGui::ColorPicker3("Light Col", glm::value_ptr(lightCol))) {
				shader->SetUniform("u_LightCol", lightCol);
			}
			if (ImGui::SliderFloat("Light Ambient Power", &lightAmbientPow, 0.0f, 1.0f)) {
				shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
			}
			if (ImGui::SliderFloat("Light Specular Power", &lightSpecularPow, 0.0f, 1.0f)) {
				shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
			}
			if (ImGui::DragFloat("Light Linear Falloff", &lightLinearFalloff, 0.01f, 0.0f, 1.0f)) {
				shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
			}
			if (ImGui::DragFloat("Light Quadratic Falloff", &lightQuadraticFalloff, 0.01f, 0.0f, 1.0f)) {
				shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
			}
		}
		if (ImGui::CollapsingHeader("Material Level Lighting Settings"))
		{
			if (ImGui::SliderFloat("Shininess", &shininess, 0.1f, 128.0f)) {
				shader->SetUniform("u_Shininess", shininess);
			}
		}
	});

	// GL states
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	// NEW STUFF

	// Create some transforms and initialize them
	Transform::sptr transforms[13];
	transforms[0] = Transform::Create();
	transforms[1] = Transform::Create();
	transforms[2] = Transform::Create();
	transforms[3] = Transform::Create();
	transforms[4] = Transform::Create();
	transforms[5] = Transform::Create();
	transforms[6] = Transform::Create();
	transforms[7] = Transform::Create();
	transforms[8] = Transform::Create();
	transforms[9] = Transform::Create();
	transforms[10] = Transform::Create();
	transforms[11] = Transform::Create();
	transforms[12] = Transform::Create();

	Transform::sptr scoreTrans[2];
	scoreTrans[0] = Transform::Create();
	scoreTrans[1] = Transform::Create();

	// We can use operator chaining, since our Set* methods return a pointer to the instance, neat!
	transforms[1]->SetLocalPosition(2.0f, 0.0f, 0.5f)->SetLocalRotation(00.0f, 0.0f, 0.0f);//player 1 tank transform
	transforms[2]->SetLocalPosition(-2.0f, 0.0f, 0.5f)->SetLocalRotation(00.0f, 180.0f, 0.0f);//player 2 tank transform
	transforms[3]->SetLocalPosition(2.0f, 0.0f, 0.5f)->SetLocalRotation(00.0f, 0.0f, 0.0f);//player 1 bullet transform
	transforms[4]->SetLocalPosition(-2.0f, 0.0f, 0.5f)->SetLocalRotation(00.0f, 180.0f, 0.0f);//player 2 bullet transform
	transforms[5]->SetLocalPosition(-16.0f, 0.0f, 16.0f)->SetLocalRotation(00.0f, 180.0f, 0.0f);//
	transforms[6]->SetLocalPosition(-16.0f, 0.0f, -16.0f)->SetLocalRotation(00.0f, 180.0f, 0.0f);//
	transforms[7]->SetLocalPosition(16.0f, 0.0f, -16.0f)->SetLocalRotation(00.0f, 180.0f, 0.0f);//
	transforms[8]->SetLocalPosition(16.0f, 0.0f, 16.0f)->SetLocalRotation(00.0f, 180.0f, 0.0f);//
	transforms[9]->SetLocalPosition(-8.0f, 0.0f, 0.0f)->SetLocalRotation(00.0f, 180.0f, 0.0f);//
	transforms[10]->SetLocalPosition(8.0f, 0.0f, 0.0f)->SetLocalRotation(00.0f, 180.0f, 0.0f);//
	transforms[11]->SetLocalPosition(0.0f, 0.0f, 8.0f)->SetLocalRotation(00.0f, 180.0f, 0.0f);//
	transforms[12]->SetLocalPosition(0.0f, 0.0f, -8.0f)->SetLocalRotation(00.0f, 180.0f, 0.0f);//

	scoreTrans[0]->SetLocalPosition(8.0f, 4.0f, 18.0f)->SetLocalRotation(00.0f, 180.0f, 0.0f);
	scoreTrans[1]->SetLocalPosition(-8.0f, 4.0f, 18.0f)->SetLocalRotation(00.0f, 180.0f, 0.0f);

	// Load our texture data from a file
	Texture2DData::sptr arenaT = Texture2DData::LoadFromFile("images/arenaTex.jpg");
	Texture2DData::sptr obstacleT = Texture2DData::LoadFromFile("images/obstacleTex.jpg");
	Texture2DData::sptr bulletT = Texture2DData::LoadFromFile("images/sample.png");
	Texture2DData::sptr p1TankT = Texture2DData::LoadFromFile("images/p1Tex.jpg");
	Texture2DData::sptr p2TankT = Texture2DData::LoadFromFile("images/p2Tex.jpg");
	Texture2DData::sptr score1T = Texture2DData::LoadFromFile("images/1Tex.jpg");
	Texture2DData::sptr score2T = Texture2DData::LoadFromFile("images/2Tex.jpg");
	Texture2DData::sptr score3T = Texture2DData::LoadFromFile("images/3Tex.jpg");
	Texture2DData::sptr score4T = Texture2DData::LoadFromFile("images/4Tex.jpg");
	Texture2DData::sptr score5T = Texture2DData::LoadFromFile("images/5Tex.jpg");

	Texture2D::sptr arenaDiffuse = Texture2D::Create();
	arenaDiffuse->LoadData(arenaT);
	Texture2D::sptr obstacleDiffuse = Texture2D::Create();
	obstacleDiffuse->LoadData(obstacleT);
	Texture2D::sptr bulletDiffuse = Texture2D::Create();
	bulletDiffuse->LoadData(bulletT);
	Texture2D::sptr p1Diffuse = Texture2D::Create();
	p1Diffuse->LoadData(p1TankT);
	Texture2D::sptr p2Diffuse = Texture2D::Create();
	p2Diffuse->LoadData(p2TankT);

	Texture2D::sptr oneDiffuse = Texture2D::Create();
	oneDiffuse->LoadData(score1T);
	Texture2D::sptr twoDiffuse = Texture2D::Create();
	twoDiffuse->LoadData(score2T);
	Texture2D::sptr thrDiffuse = Texture2D::Create();
	thrDiffuse->LoadData(score3T);
	Texture2D::sptr fouDiffuse = Texture2D::Create();
	fouDiffuse->LoadData(score4T);
	Texture2D::sptr fivDiffuse = Texture2D::Create();
	fivDiffuse->LoadData(score5T);

	Material arenaMat;
	Material obstacleMat;
	Material bulletMat;
	Material p1Mat;
	Material p2Mat;
	Material scoreMat[5];

	arenaMat.Albedo = arenaDiffuse;
	obstacleMat.Albedo = obstacleDiffuse;
	bulletMat.Albedo = bulletDiffuse;
	p1Mat.Albedo = p1Diffuse;
	p2Mat.Albedo = p2Diffuse;
	scoreMat[0].Albedo = oneDiffuse;
	scoreMat[1].Albedo = twoDiffuse;
	scoreMat[2].Albedo = thrDiffuse;
	scoreMat[3].Albedo = fouDiffuse;
	scoreMat[4].Albedo = fivDiffuse;
	
	camera = Camera::Create();
	camera->SetPosition(glm::vec3(0, 25, 0)); // Set initial position
	camera->SetUp(glm::vec3(0, 0, 1)); // Use a z-up coordinate system
	camera->LookAt(glm::vec3(0.0f)); // Look at center of the screen
	camera->SetFovDegrees(90.0f); // Set an initial FOV
	camera->SetOrthoHeight(3.0f);

	// We'll use a vector to store all our key press events for now
	std::vector<KeyPressWatcher> keyToggles;
	// This is an example of a key press handling helper. Look at InputHelpers.h an .cpp to see
	// how this is implemented. Note that the ampersand here is capturing the variables within
	// the scope. If you wanted to do some method on the class, your best bet would be to give it a method and
	// use std::bind
	keyToggles.emplace_back(GLFW_KEY_T, [&](){ camera->ToggleOrtho(); });

	InitImGui();
		
	// Our high-precision timer
	double lastFrame = glfwGetTime();
	bool p1Fired = false;
	bool p2Fired = false;
	bool p1Frwrds = false;
	bool p2Frwrds = false;
	bool p1Stopped = true;
	bool p2Stopped = true;
	int p1Health = 5;
	int p2Health = 5;
	int p1BulletHit = 3;
	int p2BulletHit = 3;
	float p1Theta;
	float p2Theta;
	
	///// Game loop /////
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		// Calculate the time since our last frame (dt)
		double thisFrame = glfwGetTime();
		float dt = static_cast<float>(thisFrame - lastFrame);		

		// We'll make sure our UI isn't focused before we start handling input for our game
		if (!ImGui::IsAnyWindowFocused()) {
			// We need to poll our key watchers so they can do their logic with the GLFW state
			// Note that since we want to make sure we don't copy our key handlers, we need a const
			// reference!
			for (const KeyPressWatcher& watcher : keyToggles) {
				watcher.Poll(window);
			}
			// We'll run some basic input to move our transform around			

			controlP1(transforms[1], dt);
			if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
				p1Fired = true;
			if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
				p1Frwrds = true;
				p1Stopped = false;
			}
			else {
				p1Stopped = true;
			}
			if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
				p1Frwrds = false;
				p1Stopped = false;
			}
			else {
				p1Stopped = true;
			}
			if (p1Fired) {
				transforms[3]->MoveLocal(16.0f * dt, 0.0f, 0.0f);
				if (glm::distance(transforms[1]->GetLocalPosition(), transforms[3]->GetLocalPosition()) > 20.0f || p1BulletHit <= 0) {
					p1Fired = false;
				}
			}
			else {
				transforms[3]->SetLocalPosition(transforms[1]->GetLocalPosition());
				transforms[3]->SetLocalRotation(transforms[1]->GetLocalRotation());
				p1BulletHit = 3;
			}			
			controlP2(transforms[2], dt);
			if (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
				p2Fired = true;
			if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
				p2Frwrds = true;
				p2Stopped = false;
			}
			else {
				p2Stopped = true;
			}
			if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
				p2Frwrds = false;
				p2Stopped = false;
			}
			else {
				p2Stopped = true;
			}
			if (p2Fired) {
				transforms[4]->MoveLocal(16.0f * dt, 0.0f, 0.0f);
				if (glm::distance(transforms[2]->GetLocalPosition(), transforms[4]->GetLocalPosition()) > 20.0f || p2BulletHit <= 0) {
					p2Fired = false;
				}
			}
			else {
				transforms[4]->SetLocalPosition(transforms[2]->GetLocalPosition());
				transforms[4]->SetLocalRotation(transforms[2]->GetLocalRotation());
				p2BulletHit = 3;
			}
		}
		for (int i = 5; i < 13; i++) {
			p1Theta = (glm::dot(transforms[3]->GetLocalPosition(), transforms[i]->GetLocalPosition())) / (glm::length(transforms[3]->GetLocalPosition()) * glm::length(transforms[i]->GetLocalPosition()));
			if (CheckCollision(transforms[1], transforms[i])) {
				if (p1Frwrds) {
					transforms[1]->MoveLocal(-4.0f * dt, 0.0f, 0.0f);
				}
				else {
					transforms[1]->MoveLocal(4.0f * dt, 0.0f, 0.0f);
				}
			}
			if (CheckCollision(transforms[2], transforms[i])) {
				if (p2Frwrds) {
					transforms[2]->MoveLocal(-4.0f * dt, 0.0f, 0.0f);
				}
				else {
					transforms[2]->MoveLocal(4.0f * dt, 0.0f, 0.0f);
				}
			}
			if (CheckCollision(transforms[3], transforms[i])) {
				//p1Fired = false;
				transforms[3]->RotateLocal(0.0f, -1.0f * transforms[3]->GetLocalRotation().y, 0.0f);
				//p1BulletHit--;
			}
			if (CheckCollision(transforms[4], transforms[i])) {
				//p2Fired = false;
				transforms[4]->RotateLocal(0.0f, -90.0f, 0.0f);
				//p2BulletHit--;
			}
		}
		WallCollision(transforms[1], dt, p1Frwrds);
		WallCollision(transforms[2], dt, p2Frwrds);
		if (CheckCollision(transforms[1], transforms[2])) {
			if (p1Frwrds && p2Frwrds) {
				transforms[1]->MoveLocal(-4.0f * dt, 0.0f, 0.0f);
				transforms[2]->MoveLocal(-4.0f * dt, 0.0f, 0.0f);
			}
			if (!p1Frwrds && !p2Frwrds) {
				transforms[1]->MoveLocal(4.0f * dt, 0.0f, 0.0f);
				transforms[2]->MoveLocal(4.0f * dt, 0.0f, 0.0f);
			}
			if (p1Frwrds && p2Stopped) {
				transforms[1]->MoveLocal(-4.0f * dt, 0.0f, 0.0f);
			}
			if (p1Stopped && p2Frwrds) {
				transforms[2]->MoveLocal(-4.0f * dt, 0.0f, 0.0f);
			}
		}
		if (CheckCollisionBulletToTank(transforms[3], transforms[2])) {
			p1Fired = false;
			p2Health--;
		}
		if (CheckCollisionBulletToTank(transforms[4], transforms[1])) {
			p2Fired = false;
			p1Health--;
		}
		if (p1Health <= 0 || p2Health <= 0)
		{
			p1Health = 5;
			p2Health = 5;
			transforms[1]->SetLocalPosition(2.0f, 0.0f, 0.5f)->SetLocalRotation(00.0f, 0.0f, 0.0f);
			transforms[2]->SetLocalPosition(-2.0f, 0.0f, 0.5f)->SetLocalRotation(00.0f, 180.0f, 0.0f);
		}
						
		glClearColor(0.08f, 0.17f, 0.31f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		shader->Bind();
		// These are the uniforms that update only once per frame
		shader->SetUniformMatrix("u_View", camera->GetView());
		shader->SetUniform("u_CamPos", camera->GetPosition());
		
		// Tell OpenGL that slot 0 will hold the diffuse, and slot 1 will hold the specular
		shader->SetUniform("s_Diffuse",  0);
		shader->SetUniform("s_Specular", 1); 

		arenaMat.Albedo->Bind(0);
		RenderVAO(shader, sceneVao, camera, transforms[0]);
		arenaMat.Albedo->UnBind(0);

		p1Mat.Albedo->Bind(0);
		RenderVAO(shader, tankVao, camera, transforms[1]);
		RenderVAO(shader, bulletVao, camera, transforms[3]);
		p1Mat.Albedo->UnBind(0);

		p2Mat.Albedo->Bind(0);
		RenderVAO(shader, tankVao, camera, transforms[2]);
		RenderVAO(shader, bulletVao, camera, transforms[4]);
		p2Mat.Albedo->UnBind(0);
				
		for (int i = 5; i < 13; i++)
		{
			obstacleMat.Albedo->Bind(0);
			RenderVAO(shader, obstacleVao, camera, transforms[i]);
			obstacleMat.Albedo->UnBind(0);
		}
		scoreMat[p1Health - 1].Albedo->Bind(0);
		RenderVAO(shader, scoreVao, camera, scoreTrans[0]);
		scoreMat[p1Health - 1].Albedo->UnBind(0);
		scoreMat[p2Health - 1].Albedo->Bind(0);
		RenderVAO(shader, scoreVao, camera, scoreTrans[1]);
		scoreMat[p2Health - 1].Albedo->UnBind(0);
		//RenderImGui();

		glfwSwapBuffers(window);
		lastFrame = thisFrame;
	}

	ShutdownImGui();

	// Clean up the toolkit logger so we don't leak memory
	Logger::Uninitialize();
	return 0;
}