#ifndef MAC_SOUND_H
#define MAC_SOUND_H

#ifdef __APPLE__

class MacSound
{
public:
	MacSound();
	~MacSound();

	bool prepare(const char* filename);
	bool start();
	void shutdown();

private:
	MacSound(const MacSound&);
	MacSound& operator=(const MacSound&);

	void* sound;
	bool ready;
};

#endif

#endif
