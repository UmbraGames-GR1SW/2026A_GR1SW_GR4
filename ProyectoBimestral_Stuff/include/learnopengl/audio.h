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

inline int& get_global_volume() {
    static int volume = 50; // 0 to 100, default 50
    return volume;
}

inline void setvolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    get_global_volume() = volume;
    int mciVol = volume * 2;
    std::string command = "setaudio sfxsound volume to " + std::to_string(mciVol);
    mciSendStringA(command.c_str(), NULL, 0, NULL);
    std::string command2 = "setaudio bgmusic volume to " + std::to_string(mciVol);
    mciSendStringA(command2.c_str(), NULL, 0, NULL);
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
        // Apply current volume
        std::string volCommand = "setaudio sfxsound volume to " + std::to_string(get_global_volume() * 2);
        mciSendStringA(volCommand.c_str(), NULL, 0, NULL);
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
        // Apply current volume
        std::string volCommand = "setaudio bgmusic volume to " + std::to_string(get_global_volume() * 2);
        mciSendStringA(volCommand.c_str(), NULL, 0, NULL);
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

// =========================================================
// NUEVO: Precarga de sonidos para eliminar el retraso al reproducir
// =========================================================

// Abre el archivo de audio UNA SOLA VEZ, dejándolo listo en memoria/MCI
// bajo un alias personalizado, sin reproducirlo todavía.
inline int preloadSound(const char* sound, const char* aliasName)
{
    std::string command = "open \"" + std::string(sound) + "\" type mpegvideo alias " + std::string(aliasName);
    MCIERROR err = mciSendStringA(command.c_str(), NULL, 0, NULL);
    if (err != 0) {
        command = "open \"" + std::string(sound) + "\" alias " + std::string(aliasName);
        err = mciSendStringA(command.c_str(), NULL, 0, NULL);
    }
    if (err == 0) {
        std::string volCommand = "setaudio " + std::string(aliasName) + " volume to " + std::to_string(get_global_volume() * 2);
        mciSendStringA(volCommand.c_str(), NULL, 0, NULL);
        return 1;
    }
    return 0;
}

// Reproduce instantáneamente un sonido ya precargado con preloadSound().
// Rebobina al inicio antes de reproducir, para poder llamarlo varias veces.
inline void playPreloaded(const char* aliasName)
{
    std::string seek = "seek " + std::string(aliasName) + " to start";
    mciSendStringA(seek.c_str(), NULL, 0, NULL);
    std::string play = "play " + std::string(aliasName);
    mciSendStringA(play.c_str(), NULL, 0, NULL);
}

// Cierra un sonido precargado (llamar al terminar el programa, opcional pero recomendable)
inline void closePreloaded(const char* aliasName)
{
    std::string stop = "stop " + std::string(aliasName);
    mciSendStringA(stop.c_str(), NULL, 0, NULL);
    std::string close = "close " + std::string(aliasName);
    mciSendStringA(close.c_str(), NULL, 0, NULL);
}

#endif