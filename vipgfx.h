#pragma once

#ifdef _WIN32
#define importdecl __declspec(dllimport)
#define ref &
#else
#define importdecl
#define ref
#endif


#ifdef __cplusplus
extern "C" {
#endif


#define gfxDeviceBest 0
#define gfxDeviceGUI 1
#define gfxDeviceDDraw 2
#define gfxDeviceOpenGL 3
#define gfxDeviceSVGAlib 4
#define gfxDeviceCanvas 5
#define gfxDeviceOpenGLcontext 6
#define gfxDeviceOpenGLStretch 7
#define gfxDeviceGUIStretch 8
#define gfxDeviceXCB 9
#define gfxDeviceMetal 10


extern importdecl int gfxDeviceMode;
extern importdecl bool gfxCenterWindow;
extern importdecl int gfxWindowPosX;
extern importdecl int gfxWindowPosY;
extern importdecl bool gfxUsingThreads;
extern importdecl int gfxPixelSizeX;
extern importdecl int gfxPixelSizeY;

extern void InitGFXsystem(unsigned int width, unsigned int height, bool fullscreen);
extern void FinishGFXsystem();
extern void UpdateGFXsystem();

extern importdecl float gfxFPS;
extern void StartFPScounter();
extern void ReturnFPSstring();
extern void updateTimer();

extern importdecl unsigned int gfxRealScreenWidth;
extern importdecl unsigned int gfxRealScreenHeight;
extern importdecl bool gfxWindowMinimized;

extern importdecl bool gfxFullScreen;
extern importdecl bool gfxDone;


extern void openGLcontext(unsigned int width, unsigned int height, bool fullscreen);
extern void closeGLcontext();
extern void openGLswapBuffers();

extern void FlipScreen();
extern void UpdateInternals();

extern void SwitchFullScreen();
extern void SwitchWindowed();

extern void MinimizeGFXwindow();
extern void RestoreGFXwindow();

extern void SetGFXwindowCaption(const char* name);
extern void SetGFXwindowPos(unsigned int x, unsigned int y);
extern void GetGFXwindowPos(unsigned int &x, unsigned int &y);

extern void GetDesktopResolution(unsigned int &x, unsigned int &y);

typedef struct gfxImage {
	void* data;
	unsigned int width;
	unsigned int height;
} gfxImage;

typedef struct gfxColor {
	char b;
	char g;
	char r;
	char a;
} gfxColor;

 extern importdecl gfxImage vscreen;


#define KEY_A 1
#define KEY_S 2
#define KEY_D 3
#define KEY_F 4
#define KEY_H 5
#define KEY_G 6
#define KEY_Z 7
#define KEY_X 8
#define KEY_C 9
#define KEY_V 10
#define KEY_B 11
#define KEY_Q 12
#define KEY_W 13
#define KEY_E 14
#define KEY_R 15
#define KEY_Y 16
#define KEY_T 17
#define KEY_O 18
#define KEY_U 19
#define KEY_I 20
#define KEY_P 21
#define KEY_L 22
#define KEY_J 23
#define KEY_K 24
#define KEY_N 25
#define KEY_M 26
#define KEY_1 27
#define KEY_2 28
#define KEY_3 29
#define KEY_4 30
#define KEY_5 31
#define KEY_6 32
#define KEY_7 33
#define KEY_8 34
#define KEY_9 35
#define KEY_0 36
#define KEY_F1 37
#define KEY_F2 38
#define KEY_F3 39
#define KEY_F4 40
#define KEY_F5 41
#define KEY_F6 42
#define KEY_F7 43
#define KEY_F8 44
#define KEY_F9 45
#define KEY_F10 46
#define KEY_F11 47
#define KEY_F12 48
#define KEY_RETURN 49
#define KEY_TAB 50
#define KEY_SPACE 51
#define KEY_BACKSPACE 52
#define KEY_ESCAPE 53
#define KEY_CAPSLOCK 54
#define KEY_PRINTSCREEN 55
#define KEY_SCROLLLOCK 56
#define KEY_PAUSE 57
#define KEY_RGUI 58
#define KEY_LGUI 59
#define KEY_RSHIFT 60
#define KEY_LSHIFT 61
#define KEY_RALT 62
#define KEY_LALT 63
#define KEY_RCONTROL 64
#define KEY_LCONTROL 65  
#define KEY_NUMPAD_NUMLOCK 66
#define KEY_NUMPAD_MUL 67
#define KEY_NUMPAD_ADD 68
#define KEY_NUMPAD_DIV 69
#define KEY_NUMPAD_SUB 70
#define KEY_NUMPAD_ENTER 71
#define KEY_NUMPAD_0 72
#define KEY_NUMPAD_1 73
#define KEY_NUMPAD_2 74
#define KEY_NUMPAD_3 75
#define KEY_NUMPAD_4 76
#define KEY_NUMPAD_5 77
#define KEY_NUMPAD_6 78
#define KEY_NUMPAD_7 79
#define KEY_NUMPAD_8 80
#define KEY_NUMPAD_9 81
#define KEY_NUMPAD_DECIMAL 82
#define KEY_INSERT 83
#define KEY_DELETE 84
#define KEY_HOME 85
#define KEY_END 86
#define KEY_PAGEUP 87
#define KEY_PAGEDOWN 88
#define KEY_LEFT 89
#define KEY_RIGHT 90
#define KEY_DOWN 91
#define KEY_UP 92
#define KEY_RAW_1 93     // de =  ^   en = ~
#define KEY_RAW_2 94     // de =  ß   en = -
#define KEY_RAW_3 95     // de =  ´   en = =
#define KEY_RAW_4 96     // de =  ü   en = [
#define KEY_RAW_5 97     // de =  +   en = ]
#define KEY_RAW_6 98     // de =  ö   en = ;
#define KEY_RAW_7 99     // de =  ä   en = '
#define KEY_RAW_8 100    // de =  #   en = backslash
#define KEY_RAW_9 101    // de =  <   en = <
#define KEY_RAW_10 102   // de =  ,   en = ,
#define KEY_RAW_11 103   // de =  .   en = .
#define KEY_RAW_12 104   // de =  -   en = /
#define keyboardlayout_english 0
#define keyboardlayout_german 1

extern importdecl bool keyboard[256];
extern importdecl int keyboardlayout;
extern const char* doReadKey();
extern bool KeyPressed();
extern void SetNumLock(bool state);
extern void SetCapsLock(bool state);
extern void SetScrollLock(bool state);


extern importdecl unsigned int gfxMouseClickDelay;
extern importdecl unsigned int gfxKeyboardHitDelay;

extern importdecl int mouseX;
extern importdecl int mouseY;
extern importdecl bool mouseL;
extern importdecl bool mouseM;
extern importdecl bool mouseR;
extern importdecl int mouseWheel;

extern importdecl int mouse2X;
extern importdecl int mouse2X;

extern void SetMouseCursor(bool show);
extern void SetMousePos(unsigned int x,  unsigned int y);

extern void DelayMouse();
extern bool mouseClickable();
extern bool mouseLclick();
extern bool mouseRclick();
extern bool mouseLDoubleClick();

extern void delayKeyboard();
extern bool allowKeyboardHit();


extern void AllocImage(gfxImage &pic, unsigned int width, unsigned int height);
extern void FreeImage(gfxImage &pic);


extern void FastMove(void* src, void* dist, unsigned int size);
extern void FastFill(void *src, unsigned int size, unsigned int what);


extern importdecl int clipX1;
extern importdecl int clipX2;
extern importdecl int clipY1;
extern importdecl int clipY2;
extern void setClipRectangle(int theClipX1, int theClipY1, int theClipX2, int theClipY2);


extern unsigned int getPixel(const gfxImage ref where, unsigned int x, unsigned int y);
extern unsigned int getPixelClip(const gfxImage ref where, int x, int y);
extern gfxColor getPixelColor(const gfxImage ref where, unsigned int x, unsigned int y);
extern gfxColor getPixelColorClip(const gfxImage ref where, int x, int y);
extern void getPixelRGBA(const gfxImage ref where, unsigned int x, unsigned int y, char &r, char &g, char &b, char &a);
extern void getPixelRGBAClip(const gfxImage ref where, int x, int y, char &r, char &g, char &b, char &a);


extern void putPixel(const gfxImage ref where, unsigned int x, unsigned int y, unsigned int color);
extern void putPixelClip(const gfxImage where, int x, int y, unsigned int color);
extern void putPixelColor(const gfxImage where, unsigned int x, unsigned int y, const gfxColor ref color);
extern void putPixelColorClip(const gfxImage ref where, int x, int y, const gfxColor ref color);
extern void putPixelRGBA(const gfxImage ref where, unsigned int x, unsigned int y, char r, char g, char b, char a);
extern void putPixelRGBAClip(const gfxImage ref where, int x, int y, char r, char g, char b, char a);


extern unsigned int RGBA(char r, char g, char b, char a);
extern void getRGBA(unsigned int color, char &r, char &g, char &b, char &a);
extern char getRed(unsigned int color);
extern char getGreen(unsigned int color);
extern char getBlue(unsigned int color);
extern char getAlpha(unsigned int color);
extern unsigned int Average(unsigned int color1, unsigned int color2);
extern unsigned int SaturatedAdd(unsigned int color1, unsigned int color2);
extern unsigned int SaturatedSub(unsigned int color1, unsigned int color2);


extern bool pngLoad(gfxImage &pic, const char* name);
extern bool pngLoadMem(gfxImage &pic, const void *data, const unsigned dataSize);
extern bool jpegLoad(gfxImage &pic, const char* name);
extern bool jpegLoadMem(gfxImage &pic, const void *data, const unsigned dataSize);
extern bool bmpLoad(gfxImage &pic, const char* name);
extern bool pcxLoad(gfxImage &pic, const char* name);
extern bool imLoad(gfxImage &pic, const char* name);


extern void putImage(const gfxImage ref where, unsigned int x, int y, const gfxImage ref what);
extern void putImageClip(const gfxImage ref where, int x, int y, const gfxImage ref what);
extern void putSprite(const gfxImage ref where, unsigned int x, unsigned int y, const gfxImage ref what, unsigned int mask);
extern void putSpriteClip(const gfxImage ref where, int x, int y, const gfxImage ref what, unsigned int mask);


extern void putSpriteAlphaValue(const gfxImage ref where, unsigned int x, unsigned int y, const gfxImage ref what, char alphaVal);
extern void putSpriteAlphaValueClip(const gfxImage ref where, int x, int y, const gfxImage ref what, char alphaVal);
extern void putSpriteAlpha(const gfxImage ref where, unsigned int x, unsigned int y, const gfxImage ref what);
extern void putSpriteAlphaClip(const gfxImage ref where, int x, int y, const gfxImage ref what);


extern void putSpriteAverage(const gfxImage ref where, unsigned int x, unsigned int y, const gfxImage ref what);
extern void putSpriteAverageClip(const gfxImage ref where, int x, int y, const gfxImage ref what);
extern void putSpriteSaturatedAdd(const gfxImage ref where, unsigned int x, unsigned int y, const gfxImage ref what);
extern void putSpriteSaturatedAddClip(const gfxImage ref where, int x, int y, const gfxImage ref what);
extern void putSpriteSaturatedSub(const gfxImage ref where, unsigned int x, unsigned int y, const gfxImage ref what);
extern void putSpriteSaturatedSubClip(const gfxImage ref where, int x, int y, const gfxImage ref what);


extern void getImage(const gfxImage ref sprite, unsigned int x, unsigned int y, const gfxImage ref where);


extern void ScaleImage(const gfxImage ref where, int x1, int y1, int x2, int y2, const gfxImage ref sprite);
extern void ScaleSprite(const gfxImage ref where, int x1, int y1, int x2, int y2, unsigned int mask, const gfxImage ref sprite);
extern void ScaleSpriteAlphaValue(const gfxImage ref where, int x1, int  y1, int x2, int y2, char alphaVal, const gfxImage ref sprite);
extern void ScaleSpriteAlpha(const gfxImage ref where, int x1, int y1, int x2, int y2, const gfxImage ref sprite);
extern void ScaleSpriteAverage(const gfxImage ref where, int x1, int y1, int x2, int y2, const gfxImage ref sprite);
extern void ScaleSpriteSaturatedAdd( gfxImage ref where, int x1, int y1, int x2, int y2, const gfxImage ref sprite);
extern void ScaleSpriteSaturatedSub( gfxImage ref where, int x1, int y1, int x2, int y2, const gfxImage ref sprite);


extern void RotateImage(const gfxImage ref where, int x, int y, int angle, const gfxImage ref what);
extern void RotateSprite(const gfxImage ref where, int x, int y, int angle, unsigned int mask, const gfxImage ref what);
extern void RotateSpriteAlpha(const gfxImage ref where, int x, int y, int angle, const gfxImage ref what);
extern void RotateSpriteAlphaValue(const gfxImage ref where, int x, int y, int angle, char alphaVal, const gfxImage ref what);
extern void RotateSpriteAverage(const gfxImage ref where, int x, int y, int angle, const gfxImage ref what);
extern void RotateSpriteSaturatedAdd(const gfxImage ref where, int x, int y, int angle, const gfxImage ref what);
extern void RotateSpriteSaturatedSub(const gfxImage ref where, int x, int y, int angle, const gfxImage ref what);


extern void DrawLine(const gfxImage ref where, unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, unsigned int color);
extern void DrawLineClip(const gfxImage ref where, int x1, int y1, int x2, int y2, unsigned int color);

extern void DrawCircle(const gfxImage ref where, unsigned int x, unsigned int y, unsigned int r, unsigned int color);
extern void DrawCircleClip(const gfxImage ref where, int x, int y, int r, unsigned int color);
extern void DrawFilledCircle(const gfxImage ref where, unsigned int x, unsigned int y, unsigned int r, unsigned int color);
extern void DrawFilledCircleClip(const gfxImage ref where, int x, int y, int r, unsigned int color);

extern void DrawFilledTriangle(const gfxImage ref where, unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, unsigned int x3, unsigned int y3, unsigned int color);
extern void DrawFilledTriangleClip(const gfxImage ref where, int x1, int y1, int x2, int y2, int x3, int y3, unsigned int color);

extern void DrawHLine(const gfxImage ref where, unsigned int x1, unsigned int x2, unsigned int y, unsigned int color);
extern void DrawHLineClip(const gfxImage ref where, int x1, int x2, int y, unsigned int color);
extern void DrawVLine(const gfxImage ref where, unsigned int y1, unsigned int y2, unsigned int x, unsigned int color);
extern void DrawVLineClip(const gfxImage ref where, int y1, int y2, int x, unsigned int color);

extern void DrawRectangle(const gfxImage ref where, unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, unsigned int color);
extern void DrawRectangleClip(const gfxImage ref where, int x1, int y1, int x2, int y2, unsigned int color);
extern void DrawBar(const gfxImage ref where, unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, unsigned int color);
extern void DrawBarClip(const gfxImage ref where, int x1, int y1, int x2, int y2, unsigned int color);

extern void  putSpriteClipInRectangle(const gfxImage ref where, int x, int y, int xx1, int yy1, int xx2, int yy2, const gfxImage ref what, unsigned int mask);

extern void RGB2HSV(char r, char g, char b, char &h, char &s, char &v);
extern void HSV2RGB(char h, char s, char v, char &r, char &g, char &b);
extern void RGB2CYM(char r, char g, char b, char &c, char &y, char &m);
extern void CYM2RGB(char c, char y, char m, char &r, char &g, char &b);
extern void RGB2CYMK(char r, char g, char b, char &c, char &y, char &m, char &k);
extern void CYMK2RGB(char c, char y, char m,  char k, char &r, char &g, char &b);

extern unsigned int AlphaValue(unsigned int Color1, unsigned int Color2, char AlphaVal);
extern unsigned int Alpha(unsigned int Color1, unsigned int Color2);
extern gfxColor AlphaValueColor(const gfxColor ref Color1, const gfxColor ref Color2, char alphaVal);
extern gfxColor AlphaColor(const gfxColor ref Color1, const gfxColor ref Color2);

extern void Fade2BlackImage(const gfxImage ref where, char fade);
extern void Fade2WhiteImage(const gfxImage ref where, char fade);
extern void Fade2WhiteImage2Screen(const gfxImage ref where, const gfxImage ref what, char fade);

extern void AlphaImage(const gfxImage ref where, const gfxImage ref pic1, const gfxImage ref pic2, char alphaVal);
extern void CreateAlphaFromImage(const gfxImage ref where, const gfxImage ref alphapic);

extern void RotoZoomImage(const gfxImage ref where, const gfxImage ref pic, float rotangle, float zoom);
extern void RotoZoomImageNoRepeat(const gfxImage ref where, const gfxImage ref pic, float rotangle, float zoom);

//extern unsigned int CosineInterpolate(var o : array of dword; x, y : single) : dword;
extern void SubPlasmaImage(const gfxImage ref where, int dist, int seed, int amplitude, bool rgb);
extern void ParticleImage(const gfxImage ref where, float f);
extern void SinePlasmaImage(const gfxImage ref where, float dx, float dy, float amplitude);

extern void InvertImageColors(const gfxImage ref where);

extern void AverageImage(const gfxImage ref where, const gfxImage ref pic1, const gfxImage ref pic2);
extern void SaturatedImageAdd(const gfxImage ref where, const gfxImage ref pic1, const gfxImage ref pic2);
extern void SaturatedImageSub(const gfxImage ref where, const gfxImage ref pic1, const gfxImage ref pic2);

extern void BlurImage(const gfxImage &where);

extern void cloneImage(const gfxImage ref src, const gfxImage &dst);

#ifdef __cplusplus
}
#endif
