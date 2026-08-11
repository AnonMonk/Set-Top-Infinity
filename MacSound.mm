#include "MacSound.h"

#ifdef __APPLE__

#import <Cocoa/Cocoa.h>

#include <unistd.h>

MacSound::MacSound()
	: sound(0), ready(false)
{
}

MacSound::~MacSound()
{
	shutdown();
}

bool MacSound::prepare(const char* filename)
{
	shutdown();
	if (filename == 0 || filename[0] == '\0')
		return false;

	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	NSString* path = [[NSString alloc] initWithUTF8String:filename];
	NSData* data = path != 0
		? [[NSData alloc] initWithContentsOfFile:path]
		: 0;
	NSSound* loadedSound = data != 0
		? [[NSSound alloc] initWithData:data]
		: 0;

	[path release];
	[data release];

	if (loadedSound == 0)
	{
		[pool drain];
		return false;
	}

	sound = loadedSound;
	[loadedSound setVolume:0.0f];
	BOOL started = [loadedSound play];

	/* Maximal zwei Sekunden auf den tatsaechlichen Audio-Start warten. */
	int attempts = 0;
	while (started && [loadedSound currentTime] < 0.01 && attempts < 400)
	{
		usleep(5000);
		attempts++;
	}

	BOOL advanced = [loadedSound currentTime] >= 0.01;
	BOOL paused = advanced ? [loadedSound pause] : NO;
	if (paused)
	{
		[loadedSound setCurrentTime:0.0];
		[loadedSound setVolume:1.0f];
		ready = true;
	}
	else
	{
		[loadedSound stop];
		[loadedSound release];
		sound = 0;
	}

	[pool drain];
	return ready;
}

bool MacSound::start()
{
	if (!ready || sound == 0)
		return false;

	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	BOOL result = [(NSSound*)sound resume];
	[pool drain];
	return result == YES;
}

void MacSound::shutdown()
{
	ready = false;
	if (sound == 0)
		return;

	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	NSSound* loadedSound = (NSSound*)sound;
	sound = 0;
	[loadedSound stop];
	[loadedSound release];
	[pool drain];
}

#endif
