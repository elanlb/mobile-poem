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
#define OUTPUT_RELAY 2 // to isolate the electronics from 48v
#define RINGER_RELAY 3 // to activate the ringer
#define RINGER_BUTTON 4 // to initiate the ringing

const int DIAL_THRESHOLD = 310; // 1 volt
const long DEBOUNCE_TIME = 20; // wait before counting the event
const int MIN_PULSE_LENGTH = 40; // the minimum duration of a pulse
const int MAX_PULSE_LENGTH = 100; // the maximum duration of a pulse
const long DIAL_TIMEOUT = 300; // wait 500 ms before confirming the number dialed
const long HANG_UP_TIME = 300; // time before confirming that the phone has been hung up
const long RINGER_DELAY = 500; // wait a bit after isolating the electronics before ringing

bool eventCounted = 0;
bool signalUp = 0; // has the pulse or spike been counted yet
bool fallCounted = 0;
bool pulseCounted = 0;
unsigned long riseTime = 0; // keep track of when the pulses happen
unsigned long fallTime = 0;
unsigned long ringTime = 0; // time when the ring button is pressed

uint8_t phoneState = 0; // 0: hung up, 1: waiting for dial, 2: dialing, 3: ringing, 4: playing audio, 5: ringing bells
uint8_t number = 0;

String currentSong = "";

void setup ()
{
    Serial.begin(9600);
    pinMode(RINGER_RELAY, OUTPUT);
    pinMode(RINGER_BUTTON, INPUT);
    pinMode(OUTPUT_RELAY, OUTPUT);
    digitalWrite(RINGER_RELAY, LOW);
    digitalWrite(OUTPUT_RELAY, LOW);

    AudioMemory(8); // allocate some memory for the audio processing
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
    if (analogRead(AUDIO_INPUT) > DIAL_THRESHOLD && !eventCounted) // signal rised
    {
        riseTime = millis(); // set the time where the rise occurred
        eventCounted = 1; // set the current state to high
    }
    if (analogRead(AUDIO_INPUT) < DIAL_THRESHOLD && eventCounted) // signal fall
    {
        fallTime = millis(); // set the time where the fall occurred
        eventCounted = 0; // set the current state to low
    }
    if (eventCounted && millis() - riseTime > DEBOUNCE_TIME) // high state held for set time
    {
        signalUp = 1; // confirm the debouncing
    }
    if (!eventCounted && millis() - fallTime > DEBOUNCE_TIME) // low state held for set time
    {
        signalUp = 0; // confirm the debouncing
    }

    // 0: hung up, 1: waiting for dial, 2: dialing, 3: ringing, 4: playing audio

    if (signalUp && millis() - riseTime > HANG_UP_TIME) // signal high shows phone is up
    {
        phoneState = 0; // set the phone status to 0 if the signal was high for a certain time
    }

    if (phoneState == 0) // hung up
    {
        playFile("DIALTONE.WAV"); // play a dialtone so that we can detect if the phone is picked up

        if (!signalUp) // signal drops when phone is picked up
        {
            phoneState = 1; // phone has been picked up
        }
    }
    else if (phoneState == 1) // waiting for dial
    {
        playFile("DIALTONE.WAV"); // play a dialtone until the first dialing is detected
        number = 0; // reset the number on a new call

        if (signalUp) // rising signal
        {
            stopPlaying(); // stop the dialtone as soon as the first pulse is detected
            phoneState = 2; // rise detected so switch to dialing state
        }
    }
    else if (phoneState == 2) // dialing
    {
        if (signalUp) // reset the pulse on rise
        {
            pulseCounted = 0;
        }
        if (!pulseCounted && !signalUp) // count the pulse on fall
        {            
            // check that the pulse lasted the correct amount of time
            if (fallTime - riseTime >= MIN_PULSE_LENGTH && fallTime - riseTime <= MAX_PULSE_LENGTH)
            {
                pulseCounted = 1; // reset so that the message is not repeated
                number++; // increment the number
            }
        }

        if (millis() - fallTime >= DIAL_TIMEOUT) // no pulse was detected for a certain time
        {
            if (number != 0) // confirm a real number
            {
                Serial.print("Number: ");
                Serial.println(number);
                phoneState = 3; // move to ringing
            }
        }
    }
    else if (phoneState == 3) // ringing on other end
    {
        for (int i = 0; i < 1; i++) // ring a set number of times
        {
            playFileAndWait("RINGING.WAV"); // play a ringing sound
        }

        phoneState = 4; // play the audio
    }
    else if (phoneState == 4) // playing sound
    {
        // play the audio which corresponds to the number dialed
        switch (number)
        {
        case 1:
            playFile("TRACK1.WAV");
            break;
        case 2:
            playFile("TRACK2.WAV");
            break;
        case 3:
            playFile("TRACK3.WAV");
            break;
        case 4:
            playFile("TRACK4.WAV");
            break;
        case 5:
            playFile("TRACK5.WAV");
            break;
        case 6:
            playFile("TRACK6.WAV");
            break;
        case 7:
            playFile("TRACK7.WAV");
            break;
        case 8:
            playFile("TRACK8.WAV");
            break;
        case 9:
            playFile("TRACK9.WAV");
            break;
        case 10:
            playFile("TRACK10.WAV");
            break;
        }
    }

    if (digitalRead(RINGER_BUTTON) == HIGH)
    {
        digitalWrite(OUTPUT_RELAY, HIGH);
        if (ringTime == 0)
        {
            ringTime = millis();
        }
        if (millis() - ringTime >= RINGER_DELAY)
        {
            digitalWrite(RINGER_RELAY, HIGH);
            ringTime = 0;
        }
    }
    else
    {
        digitalWrite(RINGER_RELAY, LOW);
        if (ringTime == 0)
        {
            ringTime = millis();
        }
        if (millis() - ringTime >= RINGER_DELAY)
        {
            digitalWrite(OUTPUT_RELAY, LOW);
            ringTime = 0;
        }
    }
    

    delay(5);
}