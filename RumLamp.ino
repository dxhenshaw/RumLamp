#pragma region Introduction
/*
* Salford Rum Lamp -- David Henshaw, July - August 2020
* v3 - xxx
* v2 - xxx
* v1 - xxx
* v0 - Core functionality of solid hue lamp and various animations
* Credit for animations to https://gist.github.com/StefanPetrick/c856b6d681ec3122e5551403aabfcc68
* and Fire2012 by Mark Kriegsman
* and Cine-Lights for multiple code examples - https://www.youtube.com/watch?v=64X5sJJ4YKM
* Rum Lamp has 192 pixels
*/
#pragma endregion
#pragma region Includes
#include <FastLED.h>
#pragma endregion
#pragma region Constants
#define MATRIX_HEIGHT 18
#define MATRIX_WIDTH 11

// following for candle and fire effect
#define FRAMES_PER_SECOND 60   // how fast should the led animation render
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 55, suggested range 20-100 
byte COOLING = 55;
// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 120
#pragma region NeoPixel
const byte neoPixelPin = 7;							// data wire for neopixels
#pragma endregion
#pragma region Animation Program List
const byte maxNumberAnimations = 13;                // How many animations are there? Now list them out...
const byte candleProgram = 0;
const byte fireProgram = 1;
const byte torchProgram = 2;
const byte rainbowNoiseProgram = 3;
const byte rainbowStripeNoiseProgram = 4;
const byte partyNoiseProgram = 5;
const byte forestNoiseProgram = 6;
const byte cloudNoiseProgram = 7;
const byte fireNoiseProgram = 8;
const byte lavaNoiseProgram = 9;
const byte oceanNoiseProgram = 10;
const byte blackAndBlueNoiseProgram = 11;
const byte cloudTwinklesProgram = 12;
const byte rainbowTwinklesProgram = 13;
#pragma endregion
#pragma region Buttons & Pots
const byte powerButton = 5;							// digital pin 5
const byte shiftFunctionButton = 9;                 // flip color hue & animations
const byte brightnessPotPin = A0;					// needs to be analog
const byte colorPotPin = A2;						// needs to be analog
#pragma endregion

#pragma endregion
#pragma region Variables
#pragma region NeoPixel
#define numberPixels (MATRIX_WIDTH * MATRIX_HEIGHT)
#pragma endregion
#pragma region Palettes
CRGBPalette16 currentPalette; // sets a variable for CRGBPalette16 which allows us to change this value later
// for twinkles...
CRGB w(85, 85, 85), W(CRGB::White);
CRGBPalette16 snowColors = CRGBPalette16(W, W, W, W, w, w, w, w, w, w, w, w, w, w, w, w);
CRGB l(0xE1A024);
CRGBPalette16 incandescentColors = CRGBPalette16(l, l, l, l, l, l, l, l, l, l, l, l, l, l, l, l);
CRGBPalette16 gPal;                         // for fire...
CRGBPalette16 blackAndBlueStripedPalette;   // for Noise
#pragma endregion
#pragma region Noise animations variables

#define MAX_DIMENSION ((MATRIX_WIDTH > MATRIX_HEIGHT) ? MATRIX_WIDTH : MATRIX_HEIGHT)

// The 16 bit version of our coordinates
static uint16_t noisex;
static uint16_t noisey;
static uint16_t noisez;

// We're using the x/y dimensions to map to the x/y pixels on the matrix.  We'll
// use the z-axis for "time".  speed determines how fast time moves forward.  Try
// 1 for a very slow moving effect, or 60 for something that ends up looking like
// water.
uint32_t noisespeedx = 1;
uint32_t noisespeedy = 1;
uint32_t noisespeedz = 1;

// Scale determines how far apart the pixels in our noise matrix are.  Try
// changing these values around to see how it affects the motion of the display.  The
// higher the value of scale, the more "zoomed out" the noise will be.  A value
// of 1 will be so zoomed in, you'll mostly see solid colors.
uint16_t noisescale = 30; // scale is set dynamically once we've started up

// This is the array that we keep our computed noise values in
uint8_t noise[MAX_DIMENSION][MAX_DIMENSION];

uint8_t colorLoop = 0;
boolean initialized = false;

#pragma endregion
#pragma region Torch Variables
uint16_t cycle_wait = 1; // 0..255

byte flame_min = 100; // 0..255
byte flame_max = 220; // 0..255

byte random_spark_probability = 2; // 0..100
byte spark_min = 200; // 0..255
byte spark_max = 255; // 0..255

byte spark_tfr = 40; // 0..256 how much energy is transferred up for a spark per cycle
uint16_t spark_cap = 200; // 0..255: spark cells: how much energy is retained from previous cycle

uint16_t up_rad = 40; // up radiation
uint16_t side_rad = 35; // sidewards radiation
uint16_t heat_cap = 0; // 0..255: passive cells: how much energy is retained from previous cycle

byte red_bg = 0;
byte green_bg = 0;
byte blue_bg = 0;
byte red_bias = 10;
byte green_bias = 0;
byte blue_bias = 0;
int red_energy = 180;
int green_energy = 20; // 145;
int blue_energy = 0;

byte upside_down = 0; // if set, flame (or rather: drop) animation is upside down. Text remains as-is

#define numLeds numberPixels
#define ledsPerLevel 11 //MATRIX_WIDTH
#define levels 18 // MATRIX_HEIGHT

byte currentEnergy[numLeds]; // current energy level
byte nextEnergy[numLeds]; // next energy level
byte energyMode[numLeds]; // mode how energy is calculated for this point

enum {
    torch_passive = 0, // just environment, glow from nearby radiation
    torch_nop = 1, // no processing
    torch_spark = 2, // slowly looses energy, moves up
    torch_spark_temp = 3, // a spark still getting energy from the level below
};
#pragma endregion
#pragma region Twinkles variables
#define STARTING_BRIGHTNESS 64
#define FADE_IN_SPEED       32
#define FADE_OUT_SPEED      20
uint8_t DENSITY = 255;
// Compact implementation of
// the directionFlags array, using just one BIT of RAM
// per pixel.  This requires a bunch of bit wrangling,
// but conserves precious RAM.  The cost is a few
// cycles and about 100 bytes of flash program memory.
uint8_t  directionFlags[(numberPixels + 7) / 8];
enum { GETTING_DARKER = 0, GETTING_BRIGHTER = 1 };
#pragma endregion


//CRGB leds_plus_safety_pixel[numberPixels + 1];
//CRGB* const leds(leds_plus_safety_pixel + 1); 
CRGB leds[numberPixels+1];  
// it took a 8 hours+ of debugging to figure out why the Noise animations were overwriting global variables
// turns out I needed the "safety pixel" +1 to be declared in this array

#pragma region lamp control
byte value = 0;                                     // Brightness dial
byte program = 0;                                   // Program dial
byte lampFunction;
byte valuePrevious = 0;
byte programPrevious = 0;
boolean lampPowerStatus = true;						// lamp is turned on by default
byte activeProgram = 0;                             // when animations are active, which one is running
#pragma endregion
#pragma endregion

void setup() {
Serial.begin(115200);								// start serial comms for terminal
#pragma region Initialize buttons
pinMode(powerButton, INPUT);						// set button to input
digitalWrite(powerButton, HIGH);					// turn on pullup resistors
pinMode(shiftFunctionButton, INPUT);				// set button to input
digitalWrite(shiftFunctionButton, HIGH);			// turn on pullup resistors
#pragma endregion
#pragma region FastLED NeoPixel wire setup
FastLED.addLeds<NEOPIXEL, neoPixelPin>(leds, numberPixels);
FastLED.setBrightness(255);							// 0 - 255
#pragma endregion
currentPalette = HeatColors_p;                      // for candle effect
gPal = HeatColors_p;                                // for fire effect
}


void mapNoiseToLEDsUsingPalette(CRGBPalette16 palette)
{
    static uint8_t ihue = 0;

    for (int i = 0; i < MATRIX_WIDTH; i++) {
        for (int j = 0; j < MATRIX_HEIGHT; j++) {
            // We use the value at the (i,j) coordinate in the noise
            // array for our brightness, and the flipped value from (j,i)
            // for our pixel's index into the color palette.

            uint8_t index = noise[j][i];
            uint8_t bri = noise[i][j];

            // if this palette is a 'loop', add a slowly-changing base value
            if (colorLoop) {
                index += ihue;
            }

            // brighten up, as the color palette itself often contains the
            // light/dark dynamic range desired
            if (bri > 127) {
                bri = 255;
            }
            else {
                bri = dim8_raw(bri * 2);
            }

            CRGB color = ColorFromPalette(palette, index, bri);
            uint16_t n = XY(i, j);

            //leds[50] = color;   // works
            leds[XY(i, j)] = ColorFromPalette(palette, index, bri); //fails - works when array is +1
            //leds[XY(i, j)] = CRGB::Aquamarine;  // fails

        }
    }

    ihue += 1;
}

void loop() {
	random16_add_entropy(random());

	if (digitalRead(powerButton) == false) {
        lampPowerStatus = !lampPowerStatus;				// flip value from on to off, or off to on

        if (lampPowerStatus == true) {					// turn on lamp
            valuePrevious--;                            // hack to force hue/animation section to execute
        }
        else {											// turn off lamp
            fadeLamp();
        }
        FastLED.delay(500);     // debounce time
    }

	if (lampPowerStatus == true) {      // everything after this point is where LEDs are on and we pay attention to dials & buttons
		value = brightnessDial();
		program = programDial();

        if (digitalRead(shiftFunctionButton) == false) {
            
            if (lampFunction == 0) {
                lampFunction = 1;   //animation
            }
            else {
                lampFunction = 0;   //lamp
            }
            valuePrevious--;
            fadeLamp();
            if (lampFunction == 0) { FastLED.setBrightness(255); }
            FastLED.delay(150);
            //Serial.println("");
            //Serial.print("S");
           // Serial.println(lampFunction);
        }

		if ((value != valuePrevious) || (program != programPrevious)) {	// if either dial changed (or hack earlier triggers this)

            //if (value != valuePrevious) FastLED.delay(1);
            //if (program != programPrevious) FastLED.delay(1);

            valuePrevious = value;			                    // remember for next time round
            programPrevious = program;

			if (lampFunction == 0) {				// basic single color lamp
				setLampColor(); 
			}
            if (lampFunction == 1) {  // based on program dial, figure out which special program will now run
                activeProgram = map(program, 0, 254, 0, maxNumberAnimations);
                FastLED.setBrightness(value);               // set led strip overall brightness
			}
		}
        
		if (lampFunction == 1) {   // animation section. This executes repeatedly, rapidly in order to see animations
            switch (activeProgram)
			{
			case candleProgram:
				candleEffect();
				FastLED.delay(1000 / FRAMES_PER_SECOND); // sets a delay using the number 1000 devided by the predetermined frames per second.
				break;

			case fireProgram:
                Fire2012WithPalette(); // run simulation frame, using palette colors
                FastLED.delay(1000 / FRAMES_PER_SECOND);
				break;

			case torchProgram:
                torch();
				break;

            case rainbowNoiseProgram:
                rainbowNoise();
                break;

            case rainbowStripeNoiseProgram: // not as nice as rainbownoise
                rainbowStripeNoise();
                break;

            case partyNoiseProgram:
                partyNoise();
                break;

            case forestNoiseProgram:
                forestNoise();
                break;

            case cloudNoiseProgram:
                cloudNoise();
                break;

            case fireNoiseProgram:
                fireNoise();
                break;

            case lavaNoiseProgram: 
                lavaNoise();
                FastLED.delay(1000 / FRAMES_PER_SECOND);
                break;

            case oceanNoiseProgram: 
                oceanNoise();
                FastLED.delay(1000 / FRAMES_PER_SECOND);
                break;

            case blackAndBlueNoiseProgram:
                blackAndBlueNoise();
                break;

            case cloudTwinklesProgram: 
                cloudTwinkles();
                FastLED.delay(1000 / FRAMES_PER_SECOND);
                break;

            case rainbowTwinklesProgram: 
                rainbowTwinkles();
                FastLED.delay(1000 / FRAMES_PER_SECOND);
                break;

			default:
				break;
			}
		}
        FastLED.show();
	}
}
byte brightnessDial() {
    byte response = 0;
    int valuePotRead = analogRead(brightnessPotPin);		// read the value from the pot
	byte proposedValue = map(valuePotRead, 0, 1023, 255, 5);// map to brightness for fastled
    if (abs(proposedValue - value) > 1) {
        response = proposedValue;
    }
    else { response = value; }
    return response;
}
byte programDial() {
    byte response = 0;
	int valuePotRead = analogRead(colorPotPin);			    // read the value from the pot
	byte proposedProgram = map(valuePotRead, 0, 1023, 255, 0);		
    if (abs(proposedProgram - program) > 1) {
        response = proposedProgram;
    }
    else { response = program; }
	return response;							
}
void fadeLamp() {
    for (byte counter = 0; counter < 50; counter++) {
        fadeToBlackBy(leds, numberPixels, 20);
        FastLED.delay(20);
    }
}
void setLampColor() {                               // Set solid hue of lamp
	byte paletteIndex = map(program, 0, 255, 67, 235);  // map program dial to a range we know looks good in the heat palette
	fill_palette(leds, numberPixels, paletteIndex, 0, currentPalette, value, LINEARBLEND);
    // leds = array for the NeoPixels
    // numberPixels = how many pixels are there (192 but it calculates a little more than that)
    // paletteIndex = how far along the palette do we want to sample
    // value = brightness, based on brightness dial
}

#pragma region Animation functions
void candleEffect()
{
    COOLING = map(value, 1, 255, 20, 100);
    static byte heat[numberPixels]; // Array of temperature readings at each simulation cell

    // Step 1.  Cool down every cell a little
    for (int i = 0; i < numberPixels; i++) {
        heat[i] = qsub8(heat[i], random8(0, ((COOLING * 10) / numberPixels) + 2));
    }

    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for (int k = numberPixels - 1; k >= 2; k--) {
        heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }

    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if (random8() < SPARKING) {
        int y = random8(7);
        heat[y] = qadd8(heat[y], random8(160, 255));
    }

    // Step 4.  Map from heat cells to LED colors
    for (int j = 0; j < numberPixels; j++) {
        // Scale the heat value from 0-255 down to 0-240
        // for best results with color palettes.
        byte colorindex = scale8(heat[j], 248);
        leds[j] = ColorFromPalette(currentPalette, colorindex);
    }
}
#pragma region Torch
void torch() {
    injectRandom();
    calcNextEnergy();
    calcNextColors();
}
inline void reduce(byte& aByte, byte aAmount, byte aMin = 0)
{
    int r = aByte - aAmount;
    if (r < aMin)
        aByte = aMin;
    else
        aByte = (byte)r;
}
inline void increase(byte& aByte, byte aAmount, byte aMax = 255)
{
    int r = aByte + aAmount;
    if (r > aMax)
        aByte = aMax;
    else
        aByte = (byte)r;
}
uint16_t random2(uint16_t aMinOrMax, uint16_t aMax = 0)
{
    if (aMax == 0) {
        aMax = aMinOrMax;
        aMinOrMax = 0;
    }
    uint32_t r = aMinOrMax;
    aMax = aMax - aMinOrMax + 1;
    r += rand() % aMax;
    return r;
}
void resetEnergy()
{
    for (int i = 0; i < numLeds; i++) {
        currentEnergy[i] = 0;
        nextEnergy[i] = 0;
        energyMode[i] = torch_passive;
    }
}
void calcNextEnergy()
{
    int i = 0;
    for (int y = 0; y < levels; y++) {
        for (int x = 0; x < ledsPerLevel; x++) {
            byte e = currentEnergy[i];
            byte m = energyMode[i];
            switch (m) {
            case torch_spark: {
                // loose transfer up energy as long as the is any
                reduce(e, spark_tfr);
                // cell above is temp spark, sucking up energy from this cell until empty
                if (y < levels - 1) {
                    energyMode[i + ledsPerLevel] = torch_spark_temp;
                }
                break;
            }
            case torch_spark_temp: {
                // just getting some energy from below
                byte e2 = currentEnergy[i - ledsPerLevel];
                if (e2 < spark_tfr) {
                    // cell below is exhausted, becomes passive
                    energyMode[i - ledsPerLevel] = torch_passive;
                    // gobble up rest of energy
                    increase(e, e2);
                    // loose some overall energy
                    e = ((int)e * spark_cap) >> 8;
                    // this cell becomes active spark
                    energyMode[i] = torch_spark;
                }
                else {
                    increase(e, spark_tfr);
                }
                break;
            }
            case torch_passive: {
                e = ((int)e * heat_cap) >> 8;
                increase(e, ((((int)currentEnergy[i - 1] + (int)currentEnergy[i + 1]) * side_rad) >> 9) + (((int)currentEnergy[i - ledsPerLevel] * up_rad) >> 8));
            }
            default:
                break;
            }
            nextEnergy[i++] = e;
        }
    }
}
const uint8_t energymap[32] = { 0, 64, 96, 112, 128, 144, 152, 160, 168, 176, 184, 184, 192, 200, 200, 208, 208, 216, 216, 224, 224, 224, 232, 232, 232, 240, 240, 240, 240, 248, 248, 248 };

void calcNextColors()
{
    for (int i = 0; i < numLeds; i++) {
        int ei; // index into energy calculation buffer
        if (upside_down)
            ei = numLeds - i;
        else
            ei = i;
        uint16_t e = nextEnergy[ei];
        currentEnergy[ei] = e;
        if (e > 250)
            leds[i] = CRGB(170, 170, e); // blueish extra-bright spark
        else {
            if (e > 0) {
                // energy to brightness is non-linear
                byte eb = energymap[e >> 3];
                byte r = red_bias;
                byte g = green_bias;
                byte b = blue_bias;
                increase(r, (eb * red_energy) >> 8);
                increase(g, (eb * green_energy) >> 8);
                increase(b, (eb * blue_energy) >> 8);
                leds[i] = CRGB(r, g, b);
            }
            else {
                // background, no energy
                leds[i] = CRGB(red_bg, green_bg, blue_bg);
            }
        }
    }
}
void injectRandom()
{
    // random flame energy at bottom row
    for (int i = 0; i < ledsPerLevel; i++) {
        currentEnergy[i] = random2(flame_min, flame_max);
        energyMode[i] = torch_nop;
    }
    // random sparks at second row
    for (int i = ledsPerLevel; i < 2 * ledsPerLevel; i++) {
        if (energyMode[i] != torch_spark && random2(100) < random_spark_probability) {
            currentEnergy[i] = random2(spark_min, spark_max);
            energyMode[i] = torch_spark;
        }
    }
}
#pragma endregion
#pragma region Noise

// Fill the x/y array of 8-bit noise values using the inoise8 function.
void fillnoise8() {

    if (!initialized) {
        initialized = true;
        // Initialize our coordinates to some random values
        noisex = random16();
        noisey = random16();
        noisez = random16();
    }

    // If we're runing at a low "speed", some 8-bit artifacts become visible
    // from frame-to-frame.  In order to reduce this, we can do some fast data-smoothing.
    // The amount of data smoothing we're doing depends on "speed".
    uint8_t dataSmoothing = 0;
    uint16_t lowestNoise = noisespeedx < noisespeedy ? noisespeedx : noisespeedy;
    lowestNoise = lowestNoise < noisespeedz ? lowestNoise : noisespeedz;
    if (lowestNoise < 8) {
        dataSmoothing = 200 - (lowestNoise * 4);
    }

    for (int i = 0; i < MAX_DIMENSION; i++) {
        int ioffset = noisescale * i;
        for (int j = 0; j < MAX_DIMENSION; j++) {
            int joffset = noisescale * j;

            uint8_t data = inoise8(noisex + ioffset, noisey + joffset, noisez);

            // The range of the inoise8 function is roughly 16-238.
            // These two operations expand those values out to roughly 0..255
            // You can comment them out if you want the raw noise data.
            data = qsub8(data, 16);
            data = qadd8(data, scale8(data, 39));

            if (dataSmoothing) {
                uint8_t olddata = noise[i][j];
                uint8_t newdata = scale8(olddata, dataSmoothing) + scale8(data, 256 - dataSmoothing);
                data = newdata;
            }

            noise[i][j] = data;
        }
    }

    noisex += noisespeedx;
    noisey += noisespeedy;
    noisez += noisespeedz;
}

void drawNoise(CRGBPalette16 palette) {
    // generate noise data
    fillnoise8();

    // convert the noise data to colors in the LED array
    // using the current palette
    mapNoiseToLEDsUsingPalette(palette);
}

void cloudNoise() {
    noisespeedx = 9;
    noisespeedy = 0;
    noisespeedz = 0;
    noisescale = 30;
    colorLoop = 0;
    drawNoise(CloudColors_p);
}

void rainbowNoise() {
    noisespeedx = 9;
    noisespeedy = 0;
    noisespeedz = 0;
    noisescale = 30;
    colorLoop = 0;
    drawNoise(RainbowColors_p);
}

void rainbowStripeNoise() {
    noisespeedx = 9;
    noisespeedy = 0;
    noisespeedz = 0;
    noisescale = 20;
    colorLoop = 0;
    drawNoise(RainbowStripeColors_p);
}

void partyNoise() {
    noisespeedx = 9;
    noisespeedy = 0;
    noisespeedz = 0;
    noisescale = 30;
    colorLoop = 0;
    drawNoise(PartyColors_p);
}

void forestNoise() {
    noisespeedx = 9;
    noisespeedy = 0;
    noisespeedz = 0;
    noisescale = 120;
    colorLoop = 0;
    drawNoise(ForestColors_p);
}

void fireNoise() {
    noisespeedx = 8; // 24;
    noisespeedy = 0;
    noisespeedz = 8;
    noisescale = 50;
    colorLoop = 0;
    drawNoise(HeatColors_p);
}

void lavaNoise() {
    noisespeedx = 32;
    noisespeedy = 0;
    noisespeedz = 16;
    noisescale = 50;
    colorLoop = 0;
    drawNoise(LavaColors_p);
}

void oceanNoise() {
    noisespeedx = 9;
    noisespeedy = 0;
    noisespeedz = 0;
    noisescale = 90;
    colorLoop = 0;
    drawNoise(OceanColors_p);
}

void blackAndBlueNoise() { // like a blue lava lamp
    SetupBlackAndBlueStripedPalette();
    noisespeedx = 9;
    noisespeedy = 0;
    noisespeedz = 0;
    noisescale = 30;
    colorLoop = 0;
    drawNoise(blackAndBlueStripedPalette);
}
void SetupBlackAndBlueStripedPalette()
// This function sets up a palette of black and blue stripes,
// using code.  Since the palette is effectively an array of
// sixteen CRGB colors, the various fill_* functions can be used
// to set them up.
{
    // 'black out' all 16 palette entries...
    fill_solid(blackAndBlueStripedPalette, 16, CRGB::Black);

    for (uint8_t i = 0; i < 6; i++) {
        blackAndBlueStripedPalette[i] = CRGB::Blue;
    }
}

uint16_t XY(uint8_t x, uint8_t y) // maps the matrix to the strip
{
    uint16_t i;
    i = (y * MATRIX_WIDTH) + (MATRIX_WIDTH - x);

    i = (numberPixels - 1) - i;

    if (i > numberPixels)
        i = numberPixels;

    return i;
}
#pragma endregion
#pragma region Twinkles
void cloudTwinkles()
{
    DENSITY = 255;
    colortwinkles(CloudColors_p);
}

void rainbowTwinkles()
{
    DENSITY = 255;
    colortwinkles(RainbowColors_p);
}


void colortwinkles(CRGBPalette16 palette)
{
    // Make each pixel brighter or darker, depending on
    // its 'direction' flag.
    brightenOrDarkenEachPixel(FADE_IN_SPEED, FADE_OUT_SPEED);

    // Now consider adding a new random twinkle
    if (random8() < DENSITY) {
        int pos = random16(numberPixels);
        if (!leds[pos]) {
            leds[pos] = ColorFromPalette(palette, random8(), STARTING_BRIGHTNESS, NOBLEND);
            setPixelDirection(pos, GETTING_BRIGHTER);
        }
    }
}
void brightenOrDarkenEachPixel(fract8 fadeUpAmount, fract8 fadeDownAmount)
{
    for (uint16_t i = 0; i < numberPixels; i++) {
        if (getPixelDirection(i) == GETTING_DARKER) {
            // This pixel is getting darker
            leds[i] = makeDarker(leds[i], fadeDownAmount);
        }
        else {
            // This pixel is getting brighter
            leds[i] = makeBrighter(leds[i], fadeUpAmount);
            // now check to see if we've maxxed out the brightness
            if (leds[i].r == 255 || leds[i].g == 255 || leds[i].b == 255) {
                // if so, turn around and start getting darker
                setPixelDirection(i, GETTING_DARKER);
            }
        }
    }
}
CRGB makeBrighter(const CRGB& color, fract8 howMuchBrighter)
{
    CRGB incrementalColor = color;
    incrementalColor.nscale8(howMuchBrighter);
    return color + incrementalColor;
}
CRGB makeDarker(const CRGB& color, fract8 howMuchDarker)
{
    CRGB newcolor = color;
    newcolor.nscale8(255 - howMuchDarker);
    return newcolor;
}
bool getPixelDirection(uint16_t i) {
    uint16_t index = i / 8;
    uint8_t  bitNum = i & 0x07;

    uint8_t  andMask = 1 << bitNum;
    return (directionFlags[index] & andMask) != 0;
}
void setPixelDirection(uint16_t i, bool dir) {
    uint16_t index = i / 8;
    uint8_t  bitNum = i & 0x07;

    uint8_t  orMask = 1 << bitNum;
    uint8_t andMask = 255 - orMask;
    uint8_t value = directionFlags[index] & andMask;
    if (dir) {
        value += orMask;
    }
    directionFlags[index] = value;
}
#pragma endregion
#pragma region Fire
void Fire2012WithPalette()
// Fire2012 by Mark Kriegsman, July 2012
// as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY
//// 
// This basic one-dimensional 'fire' simulation works roughly as follows:
// There's a underlying array of 'heat' cells, that model the temperature
// at each point along the line.  Every cycle through the simulation, 
// four steps are performed:
//  1) All cells cool down a little bit, losing heat to the air
//  2) The heat from each cell drifts 'up' and diffuses a little
//  3) Sometimes randomly new 'sparks' of heat are added at the bottom
//  4) The heat from each cell is rendered as a color into the leds array
//     The heat-to-color mapping uses a black-body radiation approximation.
//
// Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
//
// This simulation scales it self a bit depending on NUM_LEDS; it should look
// "OK" on anywhere from 20 to 100 LEDs without too much tweaking. 
//
// I recommend running this simulation at anywhere from 30-100 frames per second,
// meaning an interframe delay of about 10-35 milliseconds.
//
// Looks best on a high-density LED setup (60+ pixels/meter).
//
//
// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above), and SPARKING (used
// in step 3 above).
{
#define PIXELCOUNT 85   // Due to Arduino running out of memory with all the other animations,
    // pixelcount has to be set lower than actual number of pixels!
    // Array of temperature readings at each simulation cell
    static byte heat[PIXELCOUNT];

    // Step 1.  Cool down every cell a little
    for (int i = 0; i < PIXELCOUNT; i++) {
        heat[i] = qsub8(heat[i], random8(0, ((COOLING * 10) / PIXELCOUNT) + 2));
    }

    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for (int k = PIXELCOUNT - 1; k >= 2; k--) {
        heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }

    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if (random8() < SPARKING) {
        int y = random8(7);
        heat[y] = qadd8(heat[y], random8(160, 255));
    }

    // Step 4.  Map from heat cells to LED colors
    for (int j = 0; j < PIXELCOUNT; j++) {
        // Scale the heat value from 0-255 down to 0-240
        // for best results with color palettes.
        byte colorindex = scale8(heat[j], 240);
        CRGB color = ColorFromPalette(gPal, colorindex);
        int pixelnumber;

        pixelnumber = j;

        leds[pixelnumber] = color;
    }
}
#pragma endregion
#pragma endregion