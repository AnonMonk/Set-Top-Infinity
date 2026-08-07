#include <cstdio>
#include <cstdlib>
#include <cstring>
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
	#include <mmsystem.h>
	#include <GL/gl.h>
	#define strcasecmp _stricmp
#endif


#include "vipgfx.h"
#include "glTTF.h"
#include "gettime.h"

#include "EffectEndTitles.h"
#include "EffectLogo.h"
#include "EffectMandelbrot.h"
#include "EffectRotozoom.h"
#include "EffectStarfield.h"
#include "EffectTunnel.h"
#include "EffectTwister.h"

#define playaudio true

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

static const char* sceneNames[SCENE_COUNT] = {
	"logo",
	"wave",
	"rotozoom",
	"tunnel",
	"twister",
	"mandelbrot",
	"starfield",
	"credits",
	"greets"
};

int sceneLength[SCENE_COUNT];

/* Virtuelle Demo-Uhr (pausier- und springbar) */
long long demoClockMs = 0;
unsigned long long lastWallMs = 0;
bool paused = false;

/* Solo-Mode: nur eine Szene, geloopt (CLI oder per Taste umgeschaltet) */
bool soloMode = false;
int soloScene = 0;

/* Edge-Detection fuer Tasten */
bool prevKey[256];

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
#ifdef _WIN32
	/* PlaySound kann nur einen Sound — stoppt alles */
	(void)process;
	PlaySoundA(NULL, NULL, 0);
#else
	if (*process > 0) {
		kill(*process, SIGTERM);
		*process = -1;
	}
#endif
}

void stopAllAudio() {
#ifdef _WIN32
	PlaySoundA(NULL, NULL, 0);
	introAudioProcess = -1;
	mainMusicProcess = -1;
#else
	stopSound(&introAudioProcess);
	stopSound(&mainMusicProcess);
#endif
}


bool playSound(const string& filename, pid_t* process) {

if (!playaudio) {
	if (process != NULL)
		*process = -1;
	return true;
}

#ifdef _WIN32
	/* WAV asynchron abspielen — kein Preload, niedrige Latenz */
	if (!PlaySoundA(filename.c_str(), NULL, SND_ASYNC | SND_FILENAME | SND_NODEFAULT)) {
		printf("PlaySound failed: %s\n", filename.c_str());
		return false;
	}
	if (process != NULL)
		*process = 1;
	return true;
#endif

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
		0x80E1, //GL_BGRA
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


int sceneStartFrame(int scene)
{
	int start = 0;
	int i;
	for (i = 0; i < scene; i++)
		start += sceneLength[i];
	return start;
}


void seekToScene(int scene)
{
	if (scene < 0 || scene >= SCENE_COUNT)
		return;
	demoClockMs = (long long)sceneStartFrame(scene) * 1000 / DEMO_FPS;
}


void tickDemoClock()
{
	unsigned long long wall = GetMilliseconds();
	if (!paused)
		demoClockMs += (long long)(wall - lastWallMs);
	lastWallMs = wall;
	if (demoClockMs < 0)
		demoClockMs = 0;
}


bool keyJustPressed(int key)
{
	return keyboard[key] && !prevKey[key];
}


void storeKeyState()
{
	int i;
	for (i = 0; i < 256; i++)
		prevKey[i] = keyboard[i];
}


void printUsage()
{
	int i;
	printf("Usage: demo [scene]\n");
	printf("  without args  = full demo\n");
	printf("  with scene    = solo loop that effect\n");
	printf("Scenes:\n");
	for (i = 0; i < SCENE_COUNT; i++)
		printf("  %d  %s\n", i + 1, sceneNames[i]);
	printf("Also: logo_wave, roto, mandel, stars, greetz, intro, ...\n");
	printf("Keys: 1-9 scene, R restart, Space pause, ESC quit\n");
}


int parseSceneName(const char* name)
{
	int n;

	if (name == NULL || name[0] == '\0')
		return -1;

	/* Zahlen 1-9 oder 0-8 */
	if (name[0] >= '1' && name[0] <= '9' && name[1] == '\0')
		return name[0] - '1';
	if (name[0] >= '0' && name[0] <= '8' && name[1] == '\0')
		return name[0] - '0';

	if (strcasecmp(name, "logo") == 0 || strcasecmp(name, "intro") == 0)
		return SCENE_LOGO;
	if (strcasecmp(name, "wave") == 0 ||
		strcasecmp(name, "logo_wave") == 0 ||
		strcasecmp(name, "logowave") == 0 ||
		strcasecmp(name, "transition") == 0)
		return SCENE_LOGO_WAVE;
	if (strcasecmp(name, "roto") == 0 ||
		strcasecmp(name, "rotozoom") == 0 ||
		strcasecmp(name, "rotozoomer") == 0)
		return SCENE_ROTOZOOM;
	if (strcasecmp(name, "tunnel") == 0)
		return SCENE_TUNNEL;
	if (strcasecmp(name, "twister") == 0)
		return SCENE_TWISTER;
	if (strcasecmp(name, "mandelbrot") == 0 || strcasecmp(name, "mandel") == 0)
		return SCENE_MANDELBROT;
	if (strcasecmp(name, "starfield") == 0 ||
		strcasecmp(name, "stars") == 0 ||
		strcasecmp(name, "star") == 0)
		return SCENE_STARFIELD;
	if (strcasecmp(name, "credits") == 0 || strcasecmp(name, "credit") == 0)
		return SCENE_CREDITS;
	if (strcasecmp(name, "greets") == 0 ||
		strcasecmp(name, "greet") == 0 ||
		strcasecmp(name, "greetz") == 0)
		return SCENE_GREETS;

	/* exakter Listenname */
	for (n = 0; n < SCENE_COUNT; n++) {
		if (strcasecmp(name, sceneNames[n]) == 0)
			return n;
	}

	return -1;
}


void jumpToScene(int scene)
{
	if (scene < 0 || scene >= SCENE_COUNT)
		return;

	if (soloMode) {
		soloScene = scene;
		demoClockMs = 0;
		printf("Solo -> %s\n", sceneNames[scene]);
	} else {
		seekToScene(scene);
		printf("Seek -> %s\n", sceneNames[scene]);
	}
}


void restartCurrentScene(int scene)
{
	if (soloMode) {
		demoClockMs = 0;
		printf("Restart solo %s\n", sceneNames[soloScene]);
	} else {
		seekToScene(scene);
		printf("Restart %s\n", sceneNames[scene]);
	}
}


void handleDevKeys(int currentScene)
{
	if (keyJustPressed(KEY_SPACE)) {
		paused = !paused;
		printf(paused ? "Pause\n" : "Resume\n");
	}

	if (keyJustPressed(KEY_R))
		restartCurrentScene(currentScene);

	if (keyJustPressed(KEY_1)) jumpToScene(SCENE_LOGO);
	if (keyJustPressed(KEY_2)) jumpToScene(SCENE_LOGO_WAVE);
	if (keyJustPressed(KEY_3)) jumpToScene(SCENE_ROTOZOOM);
	if (keyJustPressed(KEY_4)) jumpToScene(SCENE_TUNNEL);
	if (keyJustPressed(KEY_5)) jumpToScene(SCENE_TWISTER);
	if (keyJustPressed(KEY_6)) jumpToScene(SCENE_MANDELBROT);
	if (keyJustPressed(KEY_7)) jumpToScene(SCENE_STARFIELD);
	if (keyJustPressed(KEY_8)) jumpToScene(SCENE_CREDITS);
	if (keyJustPressed(KEY_9)) jumpToScene(SCENE_GREETS);
}


void drawScene(
	int scene,
	float animTime,
	float rotoAnimTime,
	float tunnelAnimTime,
	float creditsAnimTime,
	float sceneDuration)
{
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
				sceneDuration,
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
				sceneDuration
			);
			break;

		case SCENE_STARFIELD:
			drawStarfieldEffect(animTime, sceneDuration);
			break;

		case SCENE_CREDITS:
			drawCreditsEffect(creditsAnimTime);
			break;

		case SCENE_GREETS:
		default:
			drawGreetsEffect(animTime);
			break;
	}
}


bool update() {

	tickDemoClock();

	float now = (float)demoClockMs * 0.001f;
	if (now < 0.0f) now = 0.0f;

	float mainMusicStart = (float)sceneLength[SCENE_LOGO] / (float)DEMO_FPS;
	if (!mainMusicStarted && now >= mainMusicStart) {
		mainMusicStarted = true;

#ifdef _WIN32
		if (!playSound(executableDirectory + "/assets/neon.wav", &mainMusicProcess))
			printf("Warnung: neon.wav konnte nicht gestartet werden.\n");
#else
		if (!playSound(executableDirectory + "/assets/neon.aiff", &mainMusicProcess))
			printf("Warnung: neon.aiff konnte nicht gestartet werden.\n");
#endif
	}

	int totalFrame = (int)(now * DEMO_FPS);

	int scene = 0;
	int localFrame = 0;
	float animTime = 0.0f;
	float rotoAnimTime = 0.0f;
	float tunnelAnimTime = 0.0f;
	float creditsAnimTime = 0.0f;
	float sceneDuration = 0.0f;

	if (soloMode) {
		scene = soloScene;
		int len = sceneLength[scene];
		if (len < 1)
			len = DEMO_FPS * 10;

		localFrame = totalFrame % len;
		animTime = (float)localFrame / (float)DEMO_FPS;
		sceneDuration = (float)len / (float)DEMO_FPS;

		/* Solo: alle Effekt-Zeiten lokal ab 0 */
		rotoAnimTime = animTime;
		tunnelAnimTime = animTime;
		creditsAnimTime = animTime;
	} else {
		if (totalFrame >= totalDemoFrames())
			return true;

		getSceneState(totalFrame, &scene, &localFrame);
		animTime = (float)localFrame / (float)DEMO_FPS;
		sceneDuration = (float)sceneLength[scene] / (float)DEMO_FPS;

		int rotoStartFrame = sceneLength[SCENE_LOGO];
		int rotoFrame = totalFrame - rotoStartFrame;
		if (rotoFrame < 0) rotoFrame = 0;
		rotoAnimTime = (float)rotoFrame / (float)DEMO_FPS;

		int tunnelStartFrame =
			sceneLength[SCENE_LOGO]
			+ sceneLength[SCENE_LOGO_WAVE]
			+ sceneLength[SCENE_ROTOZOOM];
		int tunnelFrame = totalFrame - tunnelStartFrame;
		if (tunnelFrame < 0) tunnelFrame = 0;
		tunnelAnimTime = (float)tunnelFrame / (float)DEMO_FPS;

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
		creditsAnimTime = (float)creditsFrame / (float)DEMO_FPS;
	}

	handleDevKeys(scene);

	drawScene(
		scene,
		animTime,
		rotoAnimTime,
		tunnelAnimTime,
		creditsAnimTime,
		sceneDuration
	);

	storeKeyState();

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

	int i;

	sceneLength[SCENE_LOGO] = 60 * 3;
	sceneLength[SCENE_LOGO_WAVE] = 60 * 2;
	sceneLength[SCENE_ROTOZOOM] = 60 * 8;
	sceneLength[SCENE_TUNNEL] = 60 * 10;
	sceneLength[SCENE_TWISTER] = 60 * 15;
	sceneLength[SCENE_MANDELBROT] = 60 * 14;
	sceneLength[SCENE_STARFIELD] = 60 * 20;
	sceneLength[SCENE_CREDITS] = 60 * 9;
	sceneLength[SCENE_GREETS] = 60 * 9;

	for (i = 0; i < 256; i++)
		prevKey[i] = false;

	/* CLI: demo tunnel  /  demo 4  /  demo --help */
	if (argc >= 2) {
		if (strcasecmp(argv[1], "-h") == 0 ||
			strcasecmp(argv[1], "--help") == 0 ||
			strcasecmp(argv[1], "help") == 0) {
			printUsage();
			return 0;
		}

		int scene = parseSceneName(argv[1]);
		if (scene < 0) {
			printf("Unknown scene: %s\n\n", argv[1]);
			printUsage();
			return 1;
		}

		soloMode = true;
		soloScene = scene;
		printf("Solo mode: %s (loop)\n", sceneNames[scene]);
	}

	int demoSeconds = totalDemoFrames() / DEMO_FPS;
	if (!soloMode) {
		printf(
			"Gesamtlaenge der Demo: %d Sekunden (%d:%02d)\n",
			demoSeconds,
			demoSeconds / 60,
			demoSeconds % 60
		);
	}
	printf("Keys: 1-9 scene, R restart, Space pause, ESC quit\n");

	openGLcontext(1280, 720, false);


	executableDirectory = getExecutableDir(argv);

	loadDemoTextures(executableDirectory);

	loadFontLogo();
	loadFontEndTitles();

	/* Windows: WAV + PlaySound | Apple/Linux: AIFF + afplay/pw-play */
#ifdef _WIN32
	if (!playSound(executableDirectory + "/assets/Introsound.wav", &introAudioProcess))
		printf("Warnung: Introsound.wav konnte nicht gestartet werden.\n");
#else
	if (!playSound(executableDirectory + "/assets/Introsound.aiff", &introAudioProcess))
		printf("Warnung: Introsound.aiff konnte nicht gestartet werden.\n");
#endif

	lastWallMs = GetMilliseconds();
	demoClockMs = 0;

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
