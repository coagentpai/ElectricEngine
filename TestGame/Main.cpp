#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <GL/glew.h>
#define degreesToRadians(x) x*(3.141592f/180.0f)
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <map>

#include "Timings.h"
#include "Overlay.h"
#include "Skybox.h"
#include "Terrain.h"
#include "Shader.h"

using namespace std;

int windowHeight = 1024;
int windowWidth = 1280;
float cameraSpeed = 5.0;
float skyboxRotation = 0;
glm::vec3 cameraPosition = glm::vec3(284, 14,225);
glm::vec3 cameraRotation = glm::vec3(0, 0, 0);

enum class GameState { PLAY, EXIT };
GameState curGameState = GameState::PLAY;
SDL_Window * window;
Mix_Chunk * spellsound;
Timings * timings;
Overlay * overlay;
Skybox * skybox;
Terrain * terrain;
Shader * terrainShader;
noise::module::Perlin * perlin;

void processinput();
void gameloop();
void drawgame();

int main(int argc, char ** argv){
	SDL_Init(SDL_INIT_EVERYTHING);

	window = SDL_CreateWindow("Test Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_OPENGL);// | SDL_WINDOW_FULLSCREEN);
	if (window == nullptr){
		printf("SDL_CreateWindow failed");
		exit(1);
	}
	auto glcontext = SDL_GL_CreateContext(window);
	if (glcontext == nullptr){
		printf("SDL_GL context could not be created");
		exit(1);
	}
	auto glewinit = glewInit();
	if (glewinit != GLEW_OK){
		printf("glewInit context could not be created");
		exit(1);
	}

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_SetRelativeMouseMode(SDL_bool::SDL_TRUE);


	glClearColor(0.1,0.1,0.1,1.0);

	if (Mix_Init(MIX_INIT_MP3) != MIX_INIT_MP3){
		printf("failed to init mp3");
		exit(1);
	}

	if (Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 1024) == -1){
		printf("Mix_OpenAudio %s\n", Mix_GetError());
		exit(1);
	}
	/*
	auto music = Mix_LoadMUS("assets/TownTheme.mp3");
	if (!music) {
		printf("Mix_LoadMUS %s\n", Mix_GetError());
		exit(1);
	}

	spellsound = Mix_LoadWAV("assets/MagicSmite.wav");
	if (!spellsound) {
		printf("Mix_LoadWAV %s\n", Mix_GetError());
		exit(1);
	}*/

	//Mix_PlayMusic(music, 1);

	perlin =  new noise::module::Perlin();
	perlin->SetOctaveCount(8);
	perlin->SetFrequency(0.15);
	terrain = new Terrain(500,500, perlin);
	timings = new Timings();
	overlay = new Overlay();
	skybox = new Skybox();

	gameloop();
	return 0;
}

void gameloop(){

	while (curGameState != GameState::EXIT){

		float delta = timings->FrameUpdate();
		//skyboxRotation += delta*10.0;
		SDL_Event evnt;

		while (SDL_PollEvent(&evnt)){
			switch (evnt.type){
			case SDL_QUIT:
				curGameState = GameState::EXIT;
				break;
			case SDL_KEYDOWN:
				switch (evnt.key.keysym.sym)
				{
				case SDLK_ESCAPE:
					curGameState = GameState::EXIT;
				case SDLK_1:
					Mix_PlayChannel(-1, spellsound, 0);
					break;
				}
				break;
			case SDL_MOUSEMOTION:
				cameraRotation.y += 0.1f * delta * evnt.motion.yrel;
				cameraRotation.x -= 0.1f * delta* evnt.motion.xrel;
				if (cameraRotation.y < degreesToRadians(-75)){
					cameraRotation.y = degreesToRadians(-75);
				}
				if (cameraRotation.y> degreesToRadians(89)){
					cameraRotation.y = degreesToRadians(89);
				}
				break;
			}
		}

		auto moveVector = glm::vec3(0.0, 0.0, 0.0);
		auto keystates = SDL_GetKeyboardState(NULL);

		auto moveMap2 = map<SDL_Scancode, glm::vec3> {
				{ SDL_SCANCODE_W, glm::vec3(0.0, 0.0, 1.0) },
				{ SDL_SCANCODE_A, glm::vec3(1.0, 0.0, 0.0) },
				{ SDL_SCANCODE_S, glm::vec3(0.0, 0.0, -1.0) },
				{ SDL_SCANCODE_D, glm::vec3(-1.0, 0.0, 0.0) },
				{ SDL_SCANCODE_Q, glm::vec3(0.0, 1.0, 0.0) },
				{ SDL_SCANCODE_Z, glm::vec3(0.0, -1.0, 0.0) }
		};
		for(auto var : moveMap2)
		{
			if (keystates[var.first]){
				moveVector += var.second;
			}
		}
		if (glm::length(moveVector) > 0){
			auto normalizedMoveVector = glm::normalize(moveVector);
			auto xRotate = glm::rotate(cameraRotation.x, glm::vec3(0, -1, 0));
			auto movement = glm::vec4(normalizedMoveVector * delta * cameraSpeed, 0) * xRotate;
		  auto proposed_location = cameraPosition + glm::vec3( movement);

			// The user can go wherever they want currently.
				bool xOkay = true;
				bool yOkay = true;
				bool zOkay = true;

				cameraPosition += glm::vec3(xOkay ? movement.x : 0, yOkay ? movement.y : 0, zOkay ? movement.z : 0);
				cameraPosition.y = 7 + perlin->GetValue(cameraPosition.x / 25.5, 0, (cameraPosition.z / 25.5)) * 15;
		}
		drawgame();
	}
}

void drawgame(){
	glClearDepth(1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	auto xRotate = glm::rotate(cameraRotation.x, glm::vec3(0, 1, 0));
	auto yRotate = glm::rotate(cameraRotation.y, glm::vec3(1, 0, 0));
	auto bothRotate = xRotate * yRotate;
	auto lookVector = bothRotate * glm::vec4(0, 0, 1, 0);
	auto lookat = glm::lookAt(cameraPosition, glm::vec3(lookVector) + cameraPosition, glm::vec3(0.0, 1.0, 0.0));

// Broken	auto rotate = glm::rotate(lookat, skyboxRotation, glm::vec3(0, 1, 0));

	glLoadMatrixf(glm::value_ptr(lookat));
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	auto perspective = glm::perspectiveFov<float>(1.27, windowWidth, windowHeight, 0.1f, 5000.0f);
	glLoadMatrixf(glm::value_ptr(perspective));

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glPolygonMode(GL_FRONT, GL_FILL);

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	terrain->Render(glm::value_ptr(lookat), glm::value_ptr(perspective));

	glMatrixMode(GL_MODELVIEW);
	auto skyboxPosition = glm::translate( cameraPosition* glm::vec3(1,1,1));
	auto combinedSkyboxMat = lookat*skyboxPosition;
	glLoadMatrixf(glm::value_ptr(combinedSkyboxMat));
	glEnable(GL_TEXTURE_2D);
	skybox->Render();

	// text
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glOrtho(0, windowWidth, windowHeight, 0, -10, 10);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	std::stringstream stringstream;
	stringstream.precision(2);
	stringstream << "x: " << fixed << cameraPosition.x << " y: " << fixed << cameraPosition.y << " z: " << fixed << cameraPosition.z;
	overlay->Render(stringstream.str(), SDL_Color{ 255, 255, 255 }, 10, 10);
	overlay->Render("Hello World", SDL_Color{ 255,255, 25 }, 10, 40);

	SDL_GL_SwapWindow(window);
}