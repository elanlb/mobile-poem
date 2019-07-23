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
const long DEBOUNCE_TIME = 20; // wait before counting the event
const int MIN_PULSE_LENGTH = 40; // the minimum duration of a pulse
const int MAX_PULSE_LENGTH = 100; // the maximum duration of a pulse
const long DIAL_TIMEOUT = 300; // wait 500 ms before confirming the number dialed
const long HANG_UP_TIME = 300; // time before confirming that the phone has been hung up

bool eventCounted = 0;
bool signalUp = 0; // has the pulse or spike been counted yet
bool fallCounted = 0;
bool pulseCounted = 0;
unsigned long riseTime = 0; // keep track of when the pulses happen
unsigned long fallTime = 0;

uint8_t phoneState = 0; // 0: hung up, 1: waiting for dial, 2: dialing, 3: ringing, 4: playing audio, 5: ringing bells
uint8_t number = 0;

String currentSong = "";

void setup ()
{
    Serial.begin(9600);
    pinMode(13, OUTPUT);

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
    if (analogRead(AUDIO_INPUT) > DIAL_THRESHOLD && !eventCounted)
    {
        riseTime = millis(); // set the time where the rise occurred
        eventCounted = 1;
    }
    if (analogRead(AUDIO_INPUT) < DIAL_THRESHOLD && eventCounted)
    {
        fallTime = millis(); // set the time where the fall occurred
        eventCounted = 0;
    }
    if (eventCounted && millis() - riseTime > DEBOUNCE_TIME)
    {
        signalUp = 1; // confirm the debouncing
    }
    if (!eventCounted && millis() - fallTime > DEBOUNCE_TIME)
    {
        signalUp = 0; // confirm the debouncing
    }

    // 0: hung up, 1: waiting for dial, 2: dialing, 3: ringing, 4: playing audio, 5: ringing bells

    if (signalUp && millis() - riseTime > HANG_UP_TIME)
    {
        phoneState = 0; // set the phone status to 0 if the signal was high for a certain time
    }

    if (phoneState == 0) // hung up
    {
        // play a dialtone so that we can detect if the phone is picked up
        playFile(DIALTONE);

        if (!signalUp)
        {
            phoneState = 1; // phone has been picked up
        }
    }
    else if (phoneState == 1) // waiting for dial
    {
        // play a dialtone until the first dialing is detected
        playFile(DIALTONE);
        number = 0;

        if (signalUp) // rising signal
        {
            phoneState = 2; // rise detected so switch to dialing state
        }
    }
    else if (phoneState == 2) // dialing
    {
        // make the dialing logic
        if (signalUp) // rising signal
        {
            pulseCounted = 0;
        }
        if (!pulseCounted && !signalUp) // falling signal
        {            
            // check that the pulse lasted the correct amount of time
            if (fallTime - riseTime >= MIN_PULSE_LENGTH && fallTime - riseTime <= MAX_PULSE_LENGTH)
            {
                pulseCounted = 1; // reset so that the message is not repeated
                number++;
            }
        }

        if (millis() - fallTime >= DIAL_TIMEOUT)
        {
            if (number != 0)
            {
                Serial.print("Number: ");
                Serial.println(number);
                phoneState = 3;
            }
        }
    }
    else if (phoneState == 3) // ringing on other end
    {
        // play a ringing sound for a specified amount of time
    }
    else if (phoneState == 4) // playing sound
    {
        // play the audio which corresponds to the number dialed
    }
    else if (phoneState == 5) // ringing bells
    {
        // isolate the electronics and activate the ringer
    }

    delay(5);
}