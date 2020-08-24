/*
 Name:		RumLapTesting.ino
 Created:	7/28/2020 1:38:19 PM
 Author:	david
*/

#include <FastLED.h>

#define neoPixelPin  7
#define BRIGHTNESS 50
#define numberPixels 192

// matrix size
#define MATRIX_WIDTH 11
#define MATRIX_HEIGHT 18


// NUM_LEDS = Width * Height
#define NUM_LEDS      192
CRGB leds[NUM_LEDS];

// for twinkles
CRGB w(85, 85, 85), W(CRGB::White);
CRGBPalette16 snowColors = CRGBPalette16(W, W, W, W, w, w, w, w, w, w, w, w, w, w, w, w);

CRGB l(0xE1A024);
CRGBPalette16 incandescentColors = CRGBPalette16(l, l, l, l, l, l, l, l, l, l, l, l, l, l, l, l);



void setup() {
#pragma region FastLED NeoPixel wire setup
	FastLED.addLeds<NEOPIXEL, neoPixelPin>(leds, numberPixels);
	FastLED.setBrightness(BRIGHTNESS);							// 0 - 255
#pragma endregion

}

void loop() {
    //byte result = torch();
    byte result = cloudNoise();
    FastLED.show();
    FastLED.delay(40);
}


#pragma region Torch
// torch parameters
// set brightness by adjusting entire strip

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

// torch mode
// ==========

#define numLeds NUM_LEDS
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

uint16_t torch() {
    injectRandom();
    calcNextEnergy();
    calcNextColors();
    return 1;
}
#pragma endregion
#pragma region Noise

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
//uint8_t noise[MATRIX_HEIGHT][MATRIX_WIDTH];

uint8_t colorLoop = 0;

CRGBPalette16 blackAndWhiteStripedPalette;

// This function sets up a palette of black and white stripes,
// using code.  Since the palette is effectively an array of
// sixteen CRGB colors, the various fill_* functions can be used
// to set them up.
void SetupBlackAndWhiteStripedPalette()
{
    // 'black out' all 16 palette entries...
    fill_solid(blackAndWhiteStripedPalette, 16, CRGB::Black);
    // and set every fourth one to white.
    blackAndWhiteStripedPalette[0] = CRGB::White;
    blackAndWhiteStripedPalette[4] = CRGB::White;
    blackAndWhiteStripedPalette[8] = CRGB::White;
    blackAndWhiteStripedPalette[12] = CRGB::White;

}

CRGBPalette16 blackAndBlueStripedPalette;

// This function sets up a palette of black and blue stripes,
// using code.  Since the palette is effectively an array of
// sixteen CRGB colors, the various fill_* functions can be used
// to set them up.
void SetupBlackAndBlueStripedPalette()
{
    // 'black out' all 16 palette entries...
    fill_solid(blackAndBlueStripedPalette, 16, CRGB::Black);

    for (uint8_t i = 0; i < 6; i++) {
        blackAndBlueStripedPalette[i] = CRGB::Blue;
    }
}

// There are several different palettes of colors demonstrated here.
//
// FastLED provides several 'preset' palettes: RainbowColors_p, RainbowStripeColors_p,
// OceanColors_p, CloudColors_p, LavaColors_p, ForestColors_p, and PartyColors_p.
//
// Additionally, you can manually define your own color palettes, or you can write
// code that creates color palettes on the fly.

boolean initialized = false;

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

void mapNoiseToLEDsUsingPalette(CRGBPalette16 palette, uint8_t hueReduce = 0)
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

            if (hueReduce > 0) {
                if (index < hueReduce) index = 0;
                else index -= hueReduce;
            }

            CRGB color = ColorFromPalette(palette, index, bri);
            uint16_t n = XY(i, j);

            leds[n] = color;
        }
    }

    ihue += 1;
}

uint16_t drawNoise(CRGBPalette16 palette, uint8_t hueReduce = 0) {
    // generate noise data
    fillnoise8();

    // convert the noise data to colors in the LED array
    // using the current palette
    mapNoiseToLEDsUsingPalette(palette, hueReduce);

    return 10;
}

uint16_t rainbowNoise() {
    noisespeedx = 9;
    noisespeedy = 0;
    noisespeedz = 0;
    noisescale = 30;
    colorLoop = 0;
    return drawNoise(RainbowColors_p);
}

uint16_t rainbowStripeNoise() {
    noisespeedx = 9;
    noisespeedy = 0;
    noisespeedz = 0;
    noisescale = 20;
    colorLoop = 0;
    return drawNoise(RainbowStripeColors_p);
}

uint16_t partyNoise() {
    noisespeedx = 9;
    noisespeedy = 0;
    noisespeedz = 0;
    noisescale = 30;
    colorLoop = 0;
    return drawNoise(PartyColors_p);
}

uint16_t forestNoise() {
    noisespeedx = 9;
    noisespeedy = 0;
    noisespeedz = 0;
    noisescale = 120;
    colorLoop = 0;
    return drawNoise(ForestColors_p);
}

uint16_t cloudNoise() {
    noisespeedx = 9;
    noisespeedy = 0;
    noisespeedz = 0;
    noisescale = 30;
    colorLoop = 0;
    return drawNoise(CloudColors_p);
}

uint16_t fireNoise() {
    noisespeedx = 8; // 24;
    noisespeedy = 0;
    noisespeedz = 8;
    noisescale = 50;
    colorLoop = 0;
    return drawNoise(HeatColors_p, 60);
}

uint16_t lavaNoise() {
    noisespeedx = 32;
    noisespeedy = 0;
    noisespeedz = 16;
    noisescale = 50;
    colorLoop = 0;
    return drawNoise(LavaColors_p);
}

uint16_t oceanNoise() {
    noisespeedx = 9;
    noisespeedy = 0;
    noisespeedz = 0;
    noisescale = 90;
    colorLoop = 0;
    return drawNoise(OceanColors_p);
}

uint16_t blackAndWhiteNoise() {
    SetupBlackAndWhiteStripedPalette();
    noisespeedx = 9;
    noisespeedy = 0;
    noisespeedz = 0;
    noisescale = 30;
    colorLoop = 0;
    return drawNoise(blackAndWhiteStripedPalette);
}

uint16_t blackAndBlueNoise() { // like a blue lava lamp
    SetupBlackAndBlueStripedPalette();
    noisespeedx = 9;
    noisespeedy = 0;
    noisespeedz = 0;
    noisescale = 30;
    colorLoop = 0;
    return drawNoise(blackAndBlueStripedPalette);
}
uint16_t XY(uint8_t x, uint8_t y) // maps the matrix to the strip
{
    uint16_t i;
    i = (y * MATRIX_WIDTH) + (MATRIX_WIDTH - x);

    i = (NUM_LEDS - 1) - i;

    if (i > NUM_LEDS)
        i = NUM_LEDS;

    return i;
}
#pragma endregion
#pragma region Twinkles

#define STARTING_BRIGHTNESS 64
#define FADE_IN_SPEED       32
#define FADE_OUT_SPEED      20
uint8_t DENSITY = 255;

uint16_t cloudTwinkles()
{
    DENSITY = 255;
    colortwinkles(CloudColors_p);
    return 20;
}

uint16_t rainbowTwinkles()
{
    DENSITY = 255;
    colortwinkles(RainbowColors_p);
    return 20;
}

uint16_t snowTwinkles()
{
    DENSITY = 255;
    colortwinkles(snowColors);
    return 20;
}

uint16_t incandescentTwinkles()
{
    DENSITY = 255;
    colortwinkles(incandescentColors);
    return 20;
}

uint16_t fireflies()
{
    DENSITY = 16;
    colortwinkles(incandescentColors);
    return 20;
}

enum { GETTING_DARKER = 0, GETTING_BRIGHTER = 1 };

void colortwinkles(CRGBPalette16 palette)
{
    // Make each pixel brighter or darker, depending on
    // its 'direction' flag.
    brightenOrDarkenEachPixel(FADE_IN_SPEED, FADE_OUT_SPEED);

    // Now consider adding a new random twinkle
    if (random8() < DENSITY) {
        int pos = random16(NUM_LEDS);
        if (!leds[pos]) {
            leds[pos] = ColorFromPalette(palette, random8(), STARTING_BRIGHTNESS, NOBLEND);
            setPixelDirection(pos, GETTING_BRIGHTER);
        }
    }
}

void brightenOrDarkenEachPixel(fract8 fadeUpAmount, fract8 fadeDownAmount)
{
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
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

// Compact implementation of
// the directionFlags array, using just one BIT of RAM
// per pixel.  This requires a bunch of bit wrangling,
// but conserves precious RAM.  The cost is a few
// cycles and about 100 bytes of flash program memory.
uint8_t  directionFlags[(NUM_LEDS + 7) / 8];

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