#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <limits>
#include <functional>

#include "Geometry.h"
#include "GLDebug.h"
#include "Log.h"
#include "ShaderProgram.h"
#include "Shader.h"
#include "Texture.h"
#include "Window.h"
#include "Camera.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <glm/gtx/transform.hpp>

using namespace std;
using namespace glm;

constexpr float PI = 3.14159265359f;

/*
Axial rotation is the rotation of the planet about its axis, independent of other planets.
Orbital rotation is the rotation of the entire planet about its parent (in orbit).
*/

// global constants -- scaled manually to fit the scene

// radius: km
const float sunRadius = 696340.0f;
const float earthRadius = 6371.0f * 30.0f;
const float moonRadius = 1737.4f * 60.0f;
const float backgroundRadius = sunRadius * 100.0f;

// distance btwn planets: km
const float earthToSun = 147.72e6f * 0.01f;
const float moonToEarth = 384400.0f * 1.10f;

// orbital inclination: radian
const float earthOrbitalInclination = (float)radians(23.4); // wrt sun
const float moonOrbitalInclination = (float)radians(5.15); // wrt earth

// axial tilt: radian
const float earthAxialTilt = (float)radians(23.4f); // wrt sun
const float moonAxialTilt = (float)radians(1.5); // wrt earth

// rotation speed: km/s
const float sunRotationSpeed = 1.997f;
const float earthRotationSpeed = 0.47f * 60.0f;
const float moonRotationSpeed = 0.004639f * 600.0f;

// orbit speed: km/s
const float earthOrbitSpeed = 30.0f / 10.0f;
const float moonOrbitSpeed = 1.022f * 10.0f;

vec3 xAxisOfRotation = vec3(1.0f, 0.0f, 0.0f);
vec3 yAxisOfRotation = vec3(0.0f, 1.0f, 0.0f);
vec4 xAxisMatrix = vec4(1.0f, 0.0f, 0.0f, 0.0f);
vec4 yAxisMatrix = vec4(0.0f, 1.0f, 0.0f, 0.0f);

const float modelScale = 0.5f / sunRadius; // let sun be unit size
const float uvInc = 0.1f;
float axialInc = 0.01f; // adjustable by animation speed
float animationSpeed = 1.0f;
bool isAnimating = true;
bool restartAnimation = false;

double lastUpdateTime;
double currUpdateTime;

class Planet {
public:
	Planet(float actualRadius, const string texturePath, float axialSpeed = 0.0f, float orbitSpeed = 0.0f, float orbitalIncl = 0.0f, float tilt = PI / 2, Planet* parentPtr = nullptr, float actualDistanceFromParent = 0.0f) :
		radius(actualRadius* modelScale), // scale -- constant
		texture(texturePath, GL_NEAREST),
		rotationSpeed(axialSpeed),
		orbitalSpeed(orbitSpeed),
		orbitalInclination(orbitalIncl), // constant
		axialTilt(tilt), // constant
		parent(parentPtr),
		distanceFromParent(actualDistanceFromParent* modelScale)
	{
		modelMatrix = mat4(1.0f);

		resetOrientation();
		updateLocation();
		updateTranslationMatrix();
		generateSphere();
	}

	void animate() {
		currUpdateTime = glfwGetTime(); // scaled to real time
		updateAxialRotation();
		updateOrbitalRotation();
		lastUpdateTime = currUpdateTime;
	}

	void draw(ShaderProgram& shader)
	{
		gpuGeom.bind();
		texture.bind();

		GLint uniformTransformationMatrix = glGetUniformLocation(shader, "transformationMatrix");
		glUniformMatrix4fv(uniformTransformationMatrix, 1, GL_FALSE, &translationMatrix[0][0]);

		GLint uniformRotationMatrix = glGetUniformLocation(shader, "rotationMatrix");
		glUniformMatrix4fv(uniformRotationMatrix, 1, GL_FALSE, &axialRotationMatrix[0][0]);

		GLint uniformNegRotationMatrix = glGetUniformLocation(shader, "negRotationMatrix");
		glUniformMatrix4fv(uniformNegRotationMatrix, 1, GL_FALSE, &negAxialRotationMatrix[0][0]);

		glDrawArrays(GL_TRIANGLES, 0, GLsizei(cpuGeom.verts.size()));

		texture.unbind();
	}

	void resetOrientation() {
		orbitalAngle = PI / 2;
		axialAngle = PI / 2;

		float initAxialAngle = orbitalInclination + axialAngle + axialTilt;
		axialRotationMatrix = rotate(modelMatrix, initAxialAngle, xAxisOfRotation);
		negAxialRotationMatrix = modelMatrix;
		rotationAxis = vec3(axialRotationMatrix * yAxisMatrix);
	}

private:
	float getElapsedTime() {
		return (float)(currUpdateTime - lastUpdateTime);
	}

	vec3 getPosition() {
		return position;
	}

	void updateGPUGeom(GPU_Geometry& gpuGeom, CPU_Geometry const& cpuGeom) {
		gpuGeom.bind();
		gpuGeom.setVerts(cpuGeom.verts);
		gpuGeom.setTexCoords(cpuGeom.texCoords);
		gpuGeom.setNormals(cpuGeom.normals);
	}

	void updateAxialRotation() {
		axialAngle += rotationSpeed * animationSpeed * getElapsedTime();
		axialRotationMatrix = rotate(modelMatrix, axialAngle, rotationAxis);
		negAxialRotationMatrix = rotate(modelMatrix, -axialAngle, rotationAxis);
	}

	void updateOrbitalRotation() {
		orbitalAngle += orbitalSpeed * animationSpeed * getElapsedTime();
		updateLocation();
		updateNormals();
		updateTranslationMatrix();
	}

	void updateTranslationMatrix() {
		if (parent == nullptr) {
			translationMatrix = modelMatrix;
		}
		else {
			translationMatrix = translate(modelMatrix, position);
		}
	}

	void updateLocation() {
		if (parent == nullptr) {
			position = vec3(0.0f);
		}
		else {
			vec3 relativePositionFromParent = distanceFromParent * vec3(sin(orbitalAngle), sin(orbitalAngle) * sin(orbitalInclination), cos(orbitalAngle));
			position = parent->getPosition() + relativePositionFromParent;
		}
	}

	void updateNormals() {
		cpuGeom.normals.clear();
		for (vec3 vertex : cpuGeom.verts) {
			cpuGeom.normals.push_back(getNormal(vertex));
		}
		updateGPUGeom(gpuGeom, cpuGeom);
	}

	vec3 getNormal(vec3 vertex) { // simpler for spheres
		return normalize(vertex - position);
	}

	//vec3 getVertexNormal(float phi, float theta) { // unused
	//	/*
	//	Qu(u,v) = [rcos(u)cos(v), rcos(u)sin(v), -rsin(u)]
	//	Qv(u,v) = [-rsin(u)sin(v), rsin(u)cos(v), 0]
	//	n(u, v) = Qu(u, v) x Qv(u, v)
	//	*/
	//	vec3 Qu = vec3(radius * cos(phi) * cos(theta), radius * cos(phi) * sin(theta), -radius * sin(phi));
	//	vec3 Qv = vec3(-radius * sin(phi) * sin(theta), radius * sin(phi) * cos(theta), 0);
	//	return normalize(cross(Qu, Qv));
	//}

	vec3 getVertexCoord(float phi, float theta) {
		/* sphere parametric representation:
		Q(u,v) = [rsin(u)cos(v), rsin(u)sin(v), rcos(u)] , 0 <= u <= PI, 0 <= v <= 2PI
		u = phi, v = theta
		*/
		return radius * vec3(sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi));
	}

	vec2 getTextureCoord(float phi, float theta) {
		return vec2(theta / (2 * PI), phi / PI);
	}

	void drawPoint(float phi, float theta) {
		cpuGeom.verts.push_back(getVertexCoord(phi, theta));
		cpuGeom.texCoords.push_back(getTextureCoord(phi, theta));
	}

	void generateSphere() { // init verts, textures, normals
		cpuGeom.verts.clear();
		cpuGeom.texCoords.clear();

		for (float u = 0.0f; u <= PI; u += uvInc) { // u
			for (float v = 0.0f; v <= 2 * PI; v += uvInc) { // v
				// triangle #1: |\ 
				drawPoint(u, v); // u, v
				drawPoint(u + uvInc, v); // u + 1, v
				drawPoint(u, v + uvInc); // u, v + 1

				// triangle #2: \|
				drawPoint(u + uvInc, v); // u + 1, v
				drawPoint(u + uvInc, v + uvInc); // u + 1, v + 1
				drawPoint(u, v + uvInc); // u, v + 1
			}
		}
		updateNormals();
		updateGPUGeom(gpuGeom, cpuGeom);
	}

	const float orbitalInclination;
	const float axialTilt;
	const float distanceFromParent;
	const float orbitalSpeed;
	const float rotationSpeed;

	float radius;
	float orbitalAngle;
	float axialAngle;

	Planet* parent;

	vec3 position;
	vec3 rotationAxis;

	CPU_Geometry cpuGeom;
	GPU_Geometry gpuGeom;
	Texture texture;

	mat4 modelMatrix;
	mat4 translationMatrix;
	mat4 axialRotationMatrix;
	mat4 negAxialRotationMatrix;
};

// EXAMPLE CALLBACKS
class Assignment4 : public CallbackInterface {

public:
	Assignment4()
		: camera(glm::radians(45.f), glm::radians(45.f), 3.0)
		, aspect(1.0f)
		, rightMouseDown(false)
		, mouseOldX(0.0)
		, mouseOldY(0.0)
	{}

	virtual void keyCallback(int key, int scancode, int action, int mods) {
		if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
			// pause/unpause animation
			isAnimating = !isAnimating;
		}
		else if (key == GLFW_KEY_UP && action == GLFW_PRESS && isAnimating) {
			// increase animation speed
			animationSpeed += 0.1f;
		}
		else if (key == GLFW_KEY_DOWN && action == GLFW_PRESS && isAnimating) {
			// decrease animation speed
			animationSpeed = std::max(0.0f, animationSpeed - 0.1f);
		}
		else if (key == GLFW_KEY_R && action == GLFW_PRESS) {
			// restart animation
			restartAnimation = true;
		}
	}
	virtual void mouseButtonCallback(int button, int action, int mods) {
		if (button == GLFW_MOUSE_BUTTON_RIGHT) {
			if (action == GLFW_PRESS)			rightMouseDown = true;
			else if (action == GLFW_RELEASE)	rightMouseDown = false;
		}
	}
	
	virtual void cursorPosCallback(double xpos, double ypos) {
		if (rightMouseDown) {
			camera.incrementTheta((float)(ypos - mouseOldY));
			camera.incrementPhi((float)(xpos - mouseOldX));
		}
		mouseOldX = xpos;
		mouseOldY = ypos;
	}
	virtual void scrollCallback(double xoffset, double yoffset) {
		camera.incrementR((float)yoffset);
	}
	virtual void windowSizeCallback(int width, int height) {
		// The CallbackInterface::windowSizeCallback will call glViewport for us
		CallbackInterface::windowSizeCallback(width,  height);
		aspect = float(width)/float(height);
	}

	void viewPipeline(ShaderProgram &sp) {
		mat4 M = mat4(1.0);
		mat4 V = camera.getView();
		mat4 P = perspective(radians(45.0f), aspect, 0.01f, 1000.f);

		GLint location = glGetUniformLocation(sp, "lightPos");
		vec3 lightPos = { 0.0f, 0.0f, 0.0f };
		glUniform3fv(location, 1, value_ptr(lightPos));

		GLint viewLocation = glGetUniformLocation(sp, "viewPos");
		glUniform3fv(viewLocation, 1, value_ptr(camera.getPos()));

		GLint uniMat = glGetUniformLocation(sp, "M");
		glUniformMatrix4fv(uniMat, 1, GL_FALSE, value_ptr(M));
		uniMat = glGetUniformLocation(sp, "V");
		glUniformMatrix4fv(uniMat, 1, GL_FALSE, value_ptr(V));
		uniMat = glGetUniformLocation(sp, "P");
		glUniformMatrix4fv(uniMat, 1, GL_FALSE, value_ptr(P));
	}

	Camera camera;

private:
	bool rightMouseDown = false;
	float aspect;
	double mouseOldX;
	double mouseOldY;
};

int main() {
	Log::debug("Starting main");

	// WINDOW
	glfwInit();
	Window window(800, 800, "CPSC 453"); // can set callbacks at construction if desired


	GLDebug::enable();

	// CALLBACKS
	//auto a4 = make_shared<Assignment4>(&earthOrbitalRotationIncrement, &moonOrbitalRotationIncrement);
	auto a4 = make_shared<Assignment4>();
	window.setCallbacks(a4);

	ShaderProgram shader("shaders/test.vert", "shaders/test.frag");

	lastUpdateTime = glfwGetTime();

	Planet sun(sunRadius, "textures/2k_sun.jpg", sunRotationSpeed);
	Planet earth(earthRadius, "textures/2k_earth_daymap.jpg", earthRotationSpeed, earthOrbitSpeed, earthOrbitalInclination, earthAxialTilt, &sun, earthToSun);
	Planet moon(moonRadius, "textures/2k_moon.jpg", moonRotationSpeed, moonOrbitSpeed, moonOrbitalInclination, moonAxialTilt, &earth, moonToEarth);
	Planet starBackground(backgroundRadius, "textures/2k_stars.jpg");

	// RENDER LOOP
	while (!window.shouldClose()) {
		glfwPollEvents();

		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_FRAMEBUFFER_SRGB);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL /*GL_LINE*/);

		shader.use();

		a4->viewPipeline(shader);

		if (restartAnimation) {
			sun.resetOrientation();
			earth.resetOrientation();
			moon.resetOrientation();
			restartAnimation = false;
		}

		sun.draw(shader);
		earth.draw(shader);
		moon.draw(shader);
		starBackground.draw(shader);

		if (isAnimating) {
			sun.animate();
			earth.animate();
			moon.animate();
		}

		glDisable(GL_FRAMEBUFFER_SRGB); // disable sRGB for things like imgui

		// Starting the new ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		// Putting the text-containing window in the top-left of the screen.
		ImGui::SetNextWindowPos(ImVec2(5, 5));

		// Setting flags
		ImGuiWindowFlags textWindowFlags =
			ImGuiWindowFlags_NoMove |				// text "window" should not move
			ImGuiWindowFlags_NoResize |				// should not resize
			ImGuiWindowFlags_NoCollapse |			// should not collapse
			ImGuiWindowFlags_NoSavedSettings |		// don't want saved settings mucking things up
			ImGuiWindowFlags_AlwaysAutoResize |		// window should auto-resize to fit the text
			ImGuiWindowFlags_NoBackground |			// window should be transparent; only the text should be visible
			ImGuiWindowFlags_NoDecoration |			// no decoration; only the text should be visible
			ImGuiWindowFlags_NoTitleBar;			// no title; only the text should be visible

		// Begin a new window with these flags. (bool *)0 is the "default" value for its argument.
		ImGui::Begin("scoreText", (bool*)0, textWindowFlags);

		// Scale up text a little, and set its value
		ImGui::SetWindowFontScale(2.5f);

		if (isAnimating) {
			ImGui::Text("Animation is playing.");
		}
		else {
			ImGui::Text("Animation is paused.");
		}

		ImGui::End();
		ImGui::Render(); // Render the ImGui window
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		window.swapBuffers();
	}

	glfwTerminate();
	return 0;
}
