#ifndef _AUDIO_H_
#define _AUDIO_H_

#define byte win_byte_override
#include <windows.h>
#include <mmsystem.h>
#undef byte

#include <string>

// Enlazar la librería de Windows Multimedia automáticamente en MSVC
#pragma comment(lib, "winmm.lib")

inline int initaudio(void)
{
    // No se necesita inicialización especial para la API MCI de Windows,
    // pero retornamos 1 para indicar éxito.
    return 1;
}

inline int playsound(const char* sound, int repeat)
{
    // Detener y cerrar el alias anterior para liberar recursos
    mciSendStringA("stop sfxsound", NULL, 0, NULL);
    mciSendStringA("close sfxsound", NULL, 0, NULL);

    // Abrir el nuevo archivo
    std::string command = "open \"" + std::string(sound) + "\" type mpegvideo alias sfxsound";
    MCIERROR err = mciSendStringA(command.c_str(), NULL, 0, NULL);
    if (err != 0) {
        // Reintento alternativo sin tipo explícito
        command = "open \"" + std::string(sound) + "\" alias sfxsound";
        err = mciSendStringA(command.c_str(), NULL, 0, NULL);
    }

    if (err == 0) {
        if (repeat) {
            mciSendStringA("play sfxsound repeat", NULL, 0, NULL);
        } else {
            mciSendStringA("play sfxsound", NULL, 0, NULL);
        }
        return 1;
    }
    return 0;
}

inline int playmusic(const char* musicfile)
{
    // Detener y cerrar la música de fondo anterior
    mciSendStringA("stop bgmusic", NULL, 0, NULL);
    mciSendStringA("close bgmusic", NULL, 0, NULL);

    // Abrir el nuevo archivo de música de fondo
    std::string command = "open \"" + std::string(musicfile) + "\" type mpegvideo alias bgmusic";
    MCIERROR err = mciSendStringA(command.c_str(), NULL, 0, NULL);
    if (err != 0) {
        // Reintento alternativo sin tipo explícito
        command = "open \"" + std::string(musicfile) + "\" alias bgmusic";
        err = mciSendStringA(command.c_str(), NULL, 0, NULL);
    }

    if (err == 0) {
        mciSendStringA("play bgmusic repeat", NULL, 0, NULL);
        return 1;
    }
    return 0;
}

inline int haltmusic()
{
    mciSendStringA("stop bgmusic", NULL, 0, NULL);
    mciSendStringA("close bgmusic", NULL, 0, NULL);
    return 1;
}

inline int haltsound()
{
    mciSendStringA("stop sfxsound", NULL, 0, NULL);
    mciSendStringA("close sfxsound", NULL, 0, NULL);
    return 1;
}

#endif
