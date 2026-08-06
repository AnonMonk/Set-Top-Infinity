#include <cstdio>
#include <cstdlib>
#include <limits.h>
#include <stdint.h>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>


#ifdef __linux__
#include <GL/gl.h>
#endif
#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <mach-o/dyld.h>
#endif
#ifdef _WIN32
    #include <windows.h>
    #include <GL/gl.h>
#endif


#include "vipgfx.h"
#include "glTTF.h"
#include "gettime.h"

#include "EndTitlesEffect.h"
#include "LogoEffect.h"
#include "MandelbrotEffect.h"
#include "RotozoomEffect.h"
#include "StarfieldEffect.h"
#include "TunnelEffect.h"
#include "TwisterEffect.h"

#define playaudio false

const int DEMO_FPS = 60;

using namespace std;

enum Scene
{
	SCENE_LOGO = 0,
	SCENE_LOGO_WAVE = 1,
	SCENE_ROTOZOOM = 2,
	SCENE_TUNNEL = 3,
	SCENE_TWISTER = 4,
	SCENE_MANDELBROT = 5,
	SCENE_STARFIELD = 6,
	SCENE_CREDITS = 7,
	SCENE_GREETS = 8,
	SCENE_COUNT = 9
};

int sceneLength[SCENE_COUNT];
int demoStartMilliseconds = 0;

GLuint rotoTexture = 0;
GLuint tunnelTexture = 0;
GLuint fishHeadTexture = 0;
GLuint fishTailTexture = 0;
GLuint mandelbrotTextures[MANDELBROT_TEXTURE_COUNT];

string executableDirectory;
bool mainMusicStarted = false;

pid_t introAudioProcess = -1;
pid_t mainMusicProcess = -1;


string getExecutableDir(char** argv) {

#ifdef __APPLE__
	char path[PATH_MAX];
	uint32_t size = sizeof(path);
	if (_NSGetExecutablePath(path, &size) == 0)
	{
		string fullPath = path;
		size_t pos = fullPath.find_last_of("/\\");
		if (pos != string::npos)
			return fullPath.substr(0, pos);
	}
#endif

	return ".";
}


void stopSound(pid_t* process) {
#ifndef _WIN32
	if (*process > 0) {
		kill(*process, SIGTERM);
		*process = -1;
	}
#endif
}

void stopAllAudio() {
	stopSound(&introAudioProcess);
	stopSound(&mainMusicProcess);
}


bool playSound(const string& filename, pid_t* process) {

if (!playaudio) {
	process = NULL;
	return true;
}

#ifdef __APPLE__
	*process = fork();
	if (*process < 0) return false;

	if (*process == 0) {
		execl("/usr/bin/afplay", "afplay", filename.c_str(), (char*)0);
		_exit(127);
	}
#endif

#ifdef __linux__
    *process = fork();
    if (*process < 0) return false;

    if (*process == 0) {
        execlp("pw-play", "pw-play", filename.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
#endif

	return true;
}


GLuint loadTextureRGBA(const string& filename, bool repeatMode) {

	gfxImage pic;
	pngLoad(pic, filename.c_str());

	GLuint texture;

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if (repeatMode) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGBA,
		pic.width,
		pic.height,
		0,
		0x80E1, //GL_ARGBA
		GL_UNSIGNED_BYTE,
		pic.data
	);

	return texture;
}


void getSceneState(int totalFrame, int* scene, int* localFrame)
{
	int frame = totalFrame;
	for (int i = 0; i < SCENE_COUNT; i++)
	{
		if (sceneLength[i] < 0)
		{
			*scene = i;
			*localFrame = frame;
			return;
		}

		if (frame < sceneLength[i])
		{
			*scene = i;
			*localFrame = frame;
			return;
		}
		frame -= sceneLength[i];
	}

	*scene = SCENE_COUNT - 1;
	*localFrame = sceneLength[SCENE_COUNT - 1] - 1;
}


int totalDemoFrames() {
	int total = 0;
	for (int i = 0; i < SCENE_COUNT; i++) {
		if (sceneLength[i] > 0)
			total += sceneLength[i];
	}
	return total;
}


bool update() {

	float now = (GetMilliseconds() - demoStartMilliseconds) * 0.001f;
	if (now < 0.0f) now = 0.0f;

	float mainMusicStart = (float)sceneLength[SCENE_LOGO] / (float)DEMO_FPS;
	if (!mainMusicStarted && now >= mainMusicStart) {
		mainMusicStarted = true;

		if (!playSound(executableDirectory + "/assets/neon.aiff", &mainMusicProcess)) printf("Warnung: neon.aiff konnte nicht gestartet werden.\n");
	}

	int totalFrame = (int)(now * DEMO_FPS);
	if (totalFrame >= totalDemoFrames()) return true;

	int scene = 0;
	int localFrame = 0;
	getSceneState(totalFrame, &scene, &localFrame);
	float animTime = (float)localFrame / (float)DEMO_FPS;

	int rotoStartFrame = sceneLength[SCENE_LOGO];
	int rotoFrame = totalFrame - rotoStartFrame;
	if (rotoFrame < 0) rotoFrame = 0;
	float rotoAnimTime = (float)rotoFrame / (float)DEMO_FPS;

	int tunnelStartFrame =
		sceneLength[SCENE_LOGO]
		+ sceneLength[SCENE_LOGO_WAVE]
		+ sceneLength[SCENE_ROTOZOOM];
	int tunnelFrame = totalFrame - tunnelStartFrame;
	if (tunnelFrame < 0) tunnelFrame = 0;
	float tunnelAnimTime = (float)tunnelFrame / (float)DEMO_FPS;

	int creditsStartFrame =
		sceneLength[SCENE_LOGO]
		+ sceneLength[SCENE_LOGO_WAVE]
		+ sceneLength[SCENE_ROTOZOOM]
		+ sceneLength[SCENE_TUNNEL]
		+ sceneLength[SCENE_TWISTER]
		+ sceneLength[SCENE_MANDELBROT]
		+ sceneLength[SCENE_STARFIELD];
	int creditsFrame = totalFrame - creditsStartFrame;
	if (creditsFrame < 0) creditsFrame = 0;
	float creditsAnimTime = (float)creditsFrame / (float)DEMO_FPS;


	switch (scene)
	{
		case SCENE_LOGO:
			drawLogoIntroEffect(
				fishHeadTexture,
				fishTailTexture,
				animTime
			);
			break;

		case SCENE_LOGO_WAVE:
			drawLogoTransitionEffect(
				fishHeadTexture,
				fishTailTexture,
				rotoTexture,
				animTime,
				(float)sceneLength[SCENE_LOGO_WAVE]
					/ (float)DEMO_FPS,
				rotoAnimTime
			);
			break;

		case SCENE_ROTOZOOM:
			drawRotozoomerEffect(rotoTexture, rotoAnimTime);
			break;

		case SCENE_TUNNEL:
			drawTunnelEffect(tunnelTexture, tunnelAnimTime);
			break;

		case SCENE_TWISTER:
			drawTwisterEffect(animTime);
			break;

		case SCENE_MANDELBROT:
			drawMandelbrotEffect(
				mandelbrotTextures,
				MANDELBROT_TEXTURE_COUNT,
				animTime,
				(float)sceneLength[SCENE_MANDELBROT]
					/ (float)DEMO_FPS
			);
			break;

		case SCENE_STARFIELD:
			drawStarfieldEffect(
				animTime,
				(float)sceneLength[SCENE_STARFIELD]
					/ (float)DEMO_FPS
			);
			break;

		case SCENE_CREDITS:
			drawCreditsEffect(creditsAnimTime);
			break;

		case SCENE_GREETS:
		default:
			drawGreetsEffect(animTime);
			break;
	}

	return false;
}


void loadDemoTextures(const string& directory)
{
	
	glEnable(GL_TEXTURE_2D);

	rotoTexture = loadTextureRGBA(directory + "/assets/Testbild.png", true);
	if (rotoTexture == 0) {
		printf("can't load Testbild.png\n");
		exit(0);
	}

	tunnelTexture = loadTextureRGBA(directory + "/assets/bdl.png", true);
	if (tunnelTexture == 0) {
		printf("can't load bdl.png\n");
		exit(0);
	}

	fishHeadTexture = loadTextureRGBA(directory + "/assets/Kopf_transparent.png", false);
	if (fishHeadTexture == 0) {
		printf("can't load Kopf_transparent.png\n");
		exit(0);
	}

	fishTailTexture = loadTextureRGBA(directory + "/assets/Schwanz_transparent.png", false);
	if (fishTailTexture == 0) {
		printf("can't load Schwanz_transparent.png\n");
		exit(0);
	}

	for (int i = 0; i < MANDELBROT_TEXTURE_COUNT; i++) {

		char fileName[64];
		sprintf(fileName, "/assets/mandelbrot/Mandelbrot%02d.png", i);

		mandelbrotTextures[i] = loadTextureRGBA(directory + fileName, false);

		if (mandelbrotTextures[i] == 0) {
			printf("can't load Mandelbrot%02d.png\n", i);
			exit(0);
		}
			
	}

}



int main(int argc, char** argv) {

	sceneLength[SCENE_LOGO] = 60 * 3;
	sceneLength[SCENE_LOGO_WAVE] = 60 * 2;
	sceneLength[SCENE_ROTOZOOM] = 60 * 8;
	sceneLength[SCENE_TUNNEL] = 60 * 10;
	sceneLength[SCENE_TWISTER] = 60 * 15;
	sceneLength[SCENE_MANDELBROT] = 60 * 14;
	sceneLength[SCENE_STARFIELD] = 60 * 20;
	sceneLength[SCENE_CREDITS] = 60 * 9;
	sceneLength[SCENE_GREETS] = 60 * 9;

	int demoSeconds = totalDemoFrames() / DEMO_FPS;
	printf(
		"Gesamtlaenge der Demo: %d Sekunden (%d:%02d)\n",
		demoSeconds,
		demoSeconds / 60,
		demoSeconds % 60
	);

	openGLcontext(1280, 720, false);


	executableDirectory = getExecutableDir(argv);
	
	loadDemoTextures(executableDirectory);

	loadFontLogo();
	loadFontEndTitles();


	if (!playSound(executableDirectory + "/assets/Introsound.aiff", &introAudioProcess)) printf("Warnung: Introsound.aiff konnte nicht gestartet werden.\n");
	

	demoStartMilliseconds = GetMilliseconds();

	// Wichtig: && nicht ||
	// Alt: while (!ESC || quit) → sobald update() true liefert, läuft die
	// Schleife endlos weiter (quit bleibt true, Bedingung immer erfüllt).
	bool quit = false;
	while (!keyboard[KEY_ESCAPE] && !quit) {
		quit = update();
		UpdateGFXsystem();
	}

	
	FinishGFXsystem();
    ReturnFPSstring();

    stopAllAudio();

	return 0;
}
