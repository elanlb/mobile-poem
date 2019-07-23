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
AudioConnection patchCord5(mixer, 0, audioOutput, 0); // connection from mixer to output
AudioConnection patchCord6(mixer, 1, audioOutput, 0); // connection from mixer to output

AudioControlSGTL5000 sgtl5000_1; // controller for the audio chip on the shield

#define SDCARD_CS_PIN 10
#define SDCARD_MOSI_PIN 7
#define SDCARD_SCK_PIN 14

#define AUDIO_INPUT 7 // pin A7/21

#define MUSIC "MUSIC.WAV"
#define DIALTONE "DIALTONE.WAV"

const int DIAL_THRESHOLD = 310; // 1 volt
const long DEBOUNCE_TIME = 10; // wait before counting the pulse
const int MIN_PULSE_LENGTH = 20; // the minimum duration of a pulse
const long DIAL_WAIT = 500; // wait 500 ms before confirming the number dialed

bool riseCounted = 0; // has the pulse or spike been counted yet
bool fallCounted = 0;
unsigned long riseTime = 0; // keep track of when the pulses happen
unsigned long fallTime = 0;

uint8_t phoneStatus = 0; // 0: hung up, 1: waiting for dial, 2: dialing, 3: ringing, 4: playing audio, 5: ringing bells
uint8_t number = 10;

String currentSong = "";

void setup ()
{
    Serial.begin(9600);

    AudioMemory(20); // allocate some memory for the audio processing
    sgtl5000_1.enable(); // enable the audio processing chip and set the volume
    sgtl5000_1.volume(1);
    mixer.gain(0, 1); // set mixer to maximum volume
    SPI.setMOSI(SDCARD_MOSI_PIN); // enable the SD card communication interface
    SPI.setSCK(SDCARD_SCK_PIN);
    if (!(SD.begin(SDCARD_CS_PIN)))
    {
        Serial.println("Unable to access the SD card");
    }
}

void playFile (const char *filename)
{
    if ((String) filename != currentSong || !playWav1.isPlaying())
    {
        playWav1.play(filename);
        currentSong = (String) filename;
    }
}

void playFileAndWait (const char *filename)
{
    playWav1.play(filename);
    delay(5);
    while (playWav1.isPlaying())
    {}
}

void stopPlaying ()
{
    playWav1.stop();
}

void loop ()
{
    if (analogRead(AUDIO_INPUT) > DIAL_THRESHOLD && !riseCounted)
    {
        riseTime = millis();
        riseCounted = 1;
    }
    if (analogRead(AUDIO_INPUT) < DIAL_THRESHOLD && !fallCounted)
    {
        fallTime = millis();
        fallCounted = 1;
    }
    if (millis() - riseTime > DEBOUNCE_TIME)
    {
        fallCounted = 0;
    }
    if (millis() - fallTime > DEBOUNCE_TIME)
    {
        riseCounted = 0;
    }

    // 0: hung up, 1: waiting for dial, 2: dialing, 3: ringing, 4: playing audio, 5: ringing bells
    if (phoneStatus == 0) // hung up
    {
        // play a dialtone so that we can detect if the phone is picked up
        playFile(DIALTONE);

        if (analogRead(AUDIO_INPUT) < DIAL_THRESHOLD)
        {
            phoneStatus = 1; // phone has been picked up
            riseCounted = 0;
            fallCounted = 0;
        }
    }
    else if (phoneStatus == 1) // waiting for dial
    {
        // play a dialtone until the first dialing is detected
        playFile(DIALTONE);

        if (analogRead(AUDIO_INPUT) > DIAL_THRESHOLD && !riseCounted)
        {
            riseTime = millis();
            riseCounted = 1;
            fallCounted = 0;
        }
        if (riseCounted && millis() - riseTime > DEBOUNCE_TIME)
        {
            phoneStatus = 2;
        }
    }
    else if (phoneStatus == 2) // dialing
    {
        // make the dialing logic
        
        if (!riseCounted && fallCounted)
        {
            if (fallTime - riseTime > MIN_PULSE_LENGTH)
            {
                number++;
                fallCounted = 0;
            }
        }
    }
    else if (phoneStatus == 3) // ringing on other end
    {
        // play a ringing sound for a specified amount of time
    }
    else if (phoneStatus == 4) // playing sound
    {
        // play the audio which corresponds to the number dialed
    }
    else if (phoneStatus == 5) // ringing bells
    {
        // isolate the electronics and activate the ringer
    }
}