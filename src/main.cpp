#include <Arduino.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

AudioPlaySdWav playWav1; // one player for the wav files
//AudioPlaySdWav playWav2; // add as many players as needed
AudioMixer4 mixer; // mixer to combine the outputs from multiple sources
AudioOutputI2S audioOutput; // the audio output

AudioConnection patchCord1(playWav1, 0, mixer, 0); // connection from player to mixer
//AudioConnection patchCord2(playWav2, 0, mixer, 1); // add connections for the various players
AudioConnection patchCord5(mixer, audioOutput); // connection from mixer to output

AudioControlSGTL5000 sgtl5000_1; // controller for the audio chip on the shield

#define SDCARD_CS_PIN 10
#define SDCARD_MOSI_PIN 7
#define SDCARD_SCK_PIN 14


void setup ()
{
    Serial.begin(9600);

    AudioMemory(20); // allocate some memory for the audio processing

    sgtl5000_1.enable(); // enable the audio processing chip and set the volume
    sgtl5000_1.volume(0.5);

    // enable the SD card communication interface
    SPI.setMOSI(SDCARD_MOSI_PIN);
    SPI.setSCK(SDCARD_SCK_PIN);
    if (!(SD.begin(SDCARD_CS_PIN)))
    {
        // print an error if the SD card cannot be accessed
        while (1)
        {
            Serial.println("Unable to access the SD card");
            delay(500);
        }
    }
}

void playFile (const char *filename)
{
    if (!playWav1.isPlaying())
    {
        playWav1.play(filename);
    }
    //else if (!playWav2.isPlaying())
}

void loop ()
{
    delay(100);
    playFile("TEST_1.WAV");
}