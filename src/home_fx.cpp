/*
 * wled-home-fx – WLED effects for houses, driveways and garages
 *
 *   Advent Wreath  candles with a real flicker, optional wreath between them, can follow the calendar
 *   Runway         approach lights: a strobe "rabbit" runs to the end, steady edge and threshold lights
 *   Battery        fill level gauge (e.g. home battery or car), colour by level, charging/discharging flow
 *   Garage Door    timed fill that follows the travel time of a door, blinking leading edge
 *   Notify         pulses a message colour a few times, then settles on a steady colour
 *
 * Built for WLED 16 as an external usermod:  custom_usermods = https://github.com/djlex83/wled-home-fx
 * Licensed under the EUPL-1.2, like WLED itself.
 */

#include "wled.h"

namespace homefx {

/* ------------------------------------------------------------------------------------------------ */
/*  helpers                                                                                         */
/* ------------------------------------------------------------------------------------------------ */

static void fallbackStatic() { SEGMENT.fill(SEGCOLOR(0)); }

static inline uint32_t hash32(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dUL;
  x ^= x >> 15; x *= 0x846ca68bUL;
  x ^= x >> 16;
  return x;
}

// triangle 0 -> 254 -> 0 over one byte
static inline uint8_t tri8(uint8_t x) { return x < 128 ? x << 1 : 255 - ((x - 128) << 1); }

// State for effects that play once and then hold (Garage Door, Notify).
struct OneShot {
  uint32_t start;     // strip.now when the run started
  uint32_t sig;       // fingerprint of the settings of the current run
  uint8_t  armed;     // segment or light was dark: start again when it comes back
};

static uint32_t settingsSignature() {
  uint32_t s = uint32_t(SEGMENT.speed) | (uint32_t(SEGMENT.intensity) << 8)
             | (uint32_t(SEGMENT.custom1) << 16) | (uint32_t(SEGMENT.custom2) << 24);
  const uint32_t opts = uint32_t(SEGMENT.custom3) | (uint32_t(SEGMENT.check1) << 5)
                      | (uint32_t(SEGMENT.check2) << 6) | (uint32_t(SEGMENT.check3) << 7);
  s = hash32(s ^ (opts * 2654435761UL));
  for (unsigned i = 0; i < 3; i++) s = hash32(s ^ SEGMENT.colors[i]);
  return s;
}

// Milliseconds since the run started. A new run starts when the effect is selected, when one of its
// settings or colours changes, or when the segment or the whole light is switched on again. WLED itself
// does not restart an effect that is already selected, so this makes "apply preset" behave like
// "play it now". Turning the light off is seen as brightness 0: WLED keeps calling the effect for 600 ms
// before it stops the strip. A pause of the loop (frozen segment, slow file write) does not restart the
// run, the effect simply continues where the elapsed time says.
static uint32_t oneShotElapsed(OneShot *d) {
  const uint32_t now = strip.now;
  const bool dark = !SEGMENT.on || strip.getBrightness() == 0;
  const uint32_t sig = settingsSignature();
  if (SEGENV.call == 0 || sig != d->sig || (d->armed && !dark)) {
    d->start = now;
    d->sig = sig;
    d->armed = 0;
  }
  if (dark) d->armed = 1;
  return now - d->start;
}

/* ------------------------------------------------------------------------------------------------ */
/*  Advent Wreath                                                                                   */
/*  sx flicker speed, ix flicker depth, c1 lit candles (0 = none ... 255 = all), c2 candle size,     */
/*  c3 number of candles (0 = 4), o1 follow the calendar, o2 wreath between candles, o3 wax glow     */
/* ------------------------------------------------------------------------------------------------ */

static int weekdayOfDate(int y, int m, int d) {  // 0 = Sunday
  static const uint8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

// Candles that burn today on a four-candle Advent wreath, -1 while the clock is not set.
// First Advent is the fourth Sunday before Christmas; all four burn until Epiphany (6 January).
static int adventCandlesToday() {
  if (localTime < 1600000000L) return -1;
  const int y = year(localTime), m = month(localTime), d = day(localTime);
  if (m == 1) return d <= 6 ? 4 : 0;
  if (m != 11 && m != 12) return 0;
  const int fourth = 24 - weekdayOfDate(y, 12, 24);  // day in December, 18..24
  const int today = (m == 12) ? d : d - 30;      // November as days before 1 December
  int lit = 0;
  for (int n = 1; n <= 4; n++) if (today >= fourth - (4 - n) * 7) lit = n;
  return lit;
}

static void mode_advent(void) {
  const int len = SEGLEN;
  int candles = SEGMENT.custom3 ? SEGMENT.custom3 : 4;
  if (candles > len) candles = len;
  int lit = (int(SEGMENT.custom1) * (candles + 1)) >> 8;
  if (SEGMENT.check1) {
    const int byDate = adventCandlesToday();
    if (byDate >= 0) lit = byDate < candles ? byDate : candles;
  }

  const int slot = len / candles;
  const int offset = (len - slot * candles) / 2;
  int width = (slot * (16 + ((SEGMENT.custom2 * 3) >> 2))) >> 8;
  if (width < 1) width = 1;

  const uint32_t flame   = SEGCOLOR(0);
  const uint8_t  emberR  = R(flame) > W(flame) ? R(flame) : W(flame);  // a white-only flame dips to red, too
  const uint32_t ember   = RGBW32(emberR, G(flame) / 4, 0, 0);
  const uint32_t wreath  = SEGCOLOR(1) ? SEGCOLOR(1) : RGBW32(0, 48, 10, 0);
  const uint32_t sparkle = SEGCOLOR(2) ? SEGCOLOR(2) : RGBW32(255, 150, 30, 0);
  const uint32_t t = (strip.now * (16 + SEGMENT.speed)) >> 6;

  for (int i = 0; i < len; i++) {
    int k = (i < offset) ? 0 : (i - offset) / slot;
    if (k >= candles) k = candles - 1;
    const int start = offset + k * slot + slot / 2 - width / 2;
    uint32_t c = BLACK;

    if (i >= start && i < start + width) {
      // flame profile: bright core, softer edges
      const int pos2 = 2 * i - (2 * start + width - 1);
      const int profile = 255 - (abs(pos2) * 220) / width;
      if (k < lit) {
        const uint8_t common = perlin8(uint16_t(k * 3001 + 977), uint16_t(t));
        const uint8_t local  = perlin8(uint16_t(i * 45), uint16_t(t + 7777));
        int v = (int(common) * 3 + local) >> 2;
        v = ((v - 40) * 255) / 176;  // perlin8 rarely leaves 40..216
        v = v < 0 ? 0 : (v > 255 ? 255 : v);
        const uint8_t level = 255 - scale8(255 - v, SEGMENT.intensity);
        c = color_fade(color_blend(ember, flame, level), scale8(level, profile), true);
      } else if (SEGMENT.check3) {
        c = color_fade(flame, scale8(14, profile), true);
      }
    } else if (SEGMENT.check2) {
      const uint32_t h = hash32(uint32_t(i) * 2654435761UL);
      const uint32_t per = 1200 + (h & 1023);
      const uint32_t tt = strip.now + (h >> 12);
      c = wreath;
      if (((tt / per + (h >> 24)) % 7) == 0) c = color_blend(wreath, sparkle, tri8(uint8_t(((tt % per) * 255) / per)));
    }
    SEGMENT.setPixelColor(i, c);
  }
}
static const char _data_FX_MODE_ADVENT[] PROGMEM =
  "Advent Wreath@Flicker speed,Flicker depth,Lit candles,Candle size,Candles,By date,Wreath,Wax;Flame,Wreath,Sparkle;;1;"
  "sx=96,ix=170,c1=64,c2=110,c3=4,o2=1,o3=1";

/* ------------------------------------------------------------------------------------------------ */
/*  Runway                                                                                          */
/*  sx strobe speed, ix light spacing, c1 edge light brightness, c2 flash length,                   */
/*  o1 threshold light at the end, o2 steady edge lights. Use "reverse" to change the direction.    */
/* ------------------------------------------------------------------------------------------------ */

static void mode_runway(void) {
  const int len = SEGLEN;
  const int spacing = 2 + ((SEGMENT.intensity * 30) >> 8);   // about 2..31 LEDs between lights
  int lampW = 1 + (spacing >= 10) + (spacing >= 20);
  if (lampW > len) lampW = len;
  // the lights are spread evenly: the first starts at the beginning, the last ends at the end of the segment
  const int lights = (len - lampW + spacing / 2) / spacing + 1;
  const uint32_t msPerLight = 20 + (((255 - SEGMENT.speed) * 140) >> 8);
  const uint32_t cycle = uint32_t(lights) * msPerLight + 300 + msPerLight * 2;
  const uint32_t pos256 = ((strip.now % cycle) * 256) / msPerLight;  // strobe head in 1/256 light
  const uint32_t flash256 = 64 + SEGMENT.custom2 * 3;               // 0.25 .. 3.2 lights

  const uint32_t strobe = SEGCOLOR(0);
  const uint32_t edge = color_fade(SEGCOLOR(1) ? SEGCOLOR(1) : RGBW32(255, 150, 40, 0), SEGMENT.custom1, true);
  const uint32_t threshold = SEGCOLOR(2) ? SEGCOLOR(2) : RGBW32(0, 255, 40, 0);

  SEGMENT.fill(BLACK);
  for (int k = 0; k < lights; k++) {
    uint32_t c = SEGMENT.check2 ? edge : BLACK;
    if (SEGMENT.check1 && k == lights - 1) {
      c = threshold;
    } else {
      const int32_t d = int32_t(pos256) - k * 256;
      if (d >= 0 && uint32_t(d) < flash256) {
        const uint32_t f = 255 - (uint32_t(d) * 255) / flash256;
        c = color_blend(c, strobe, (f * f) >> 8);
      }
    }
    const int p = lights > 1 ? (k * (len - lampW)) / (lights - 1) : len - lampW;  // a single light is the threshold
    for (int w = 0; w < lampW; w++) SEGMENT.setPixelColor(p + w, c);
  }
}
static const char _data_FX_MODE_RUNWAY[] PROGMEM =
  "Runway@Strobe speed,Light spacing,Edge brightness,Flash length,,Threshold,Edge lights;Strobe,Edge,Threshold;;1;"
  "sx=170,ix=80,c1=70,c2=96,o1=1,o2=1";

/* ------------------------------------------------------------------------------------------------ */
/*  Battery                                                                                         */
/*  ix level in percent (0..100, like WLED's Percent), sx fill and flow speed, c1 low warning level, */
/*  c2 flow length, o1 charging flow, o2 discharging flow, o3 colour from palette instead of         */
/*  red-yellow-green. Colour 2 is the empty part.                                                    */
/* ------------------------------------------------------------------------------------------------ */

struct BatteryData {
  uint32_t shown;  // displayed fill in 1/256 LED
  uint32_t last;   // strip.now of the previous frame
};

static void mode_battery(void) {
  if (!SEGENV.allocateData(sizeof(BatteryData))) { fallbackStatic(); return; }
  BatteryData *d = reinterpret_cast<BatteryData *>(SEGENV.data);

  const uint32_t len = SEGLEN;
  const uint32_t level = SEGMENT.intensity > 100 ? 100 : SEGMENT.intensity;
  const uint32_t target = (len * 256 * level) / 100;
  // the bar moves by time, not by frame: an empty bar fills in about 6 s (sx = 0) to 0.4 s (sx = 255)
  uint32_t dt = SEGENV.call == 0 ? 0 : strip.now - d->last;
  if (dt > 250) dt = 250;  // no jump after a pause
  d->last = strip.now;
  uint32_t step = (len * (16 + SEGMENT.speed) * dt) / 384;
  if (step == 0) step = 1;
  if (d->shown < target)      d->shown = (d->shown + step < target) ? d->shown + step : target;
  else if (d->shown > target) d->shown = (d->shown > target + step) ? d->shown - step : target;

  uint32_t fillC;
  if (SEGMENT.check3)  fillC = SEGMENT.color_from_palette((level * 255) / 100, false, false, 0);
  else if (level < 50) fillC = color_blend(RGBW32(255, 0, 0, 0), RGBW32(255, 170, 0, 0), (level * 255) / 50);
  else                 fillC = color_blend(RGBW32(255, 170, 0, 0), RGBW32(0, 255, 30, 0), ((level - 50) * 255) / 50);

  const uint32_t low = (uint32_t(SEGMENT.custom1) * 100) >> 8;  // 0..99 %
  if (level <= low) fillC = color_fade(fillC, 64 + scale8(sin8_t(uint8_t(strip.now >> 2)), 191), true);

  const uint32_t empty = SEGCOLOR(1);
  const uint32_t full = d->shown >> 8, frac = d->shown & 255;

  const bool charging = SEGMENT.check1, discharging = SEGMENT.check2 && !charging;
  const bool flowing = (charging || discharging) && full > 0;
  const int32_t flowLen = 2 + int32_t((len * SEGMENT.custom2) >> 9);
  int32_t head = 0;  // charging: band runs towards the tip, discharging: back to the start
  if (flowing) {
    const uint32_t period = 600 + (255 - SEGMENT.speed) * 12;
    const uint32_t span = full + flowLen;
    const uint32_t p = ((strip.now % period) * span) / period;
    head = charging ? int32_t(p) : int32_t(span) - 1 - int32_t(p) - flowLen;
  }

  for (int i = 0; i < int(len); i++) {  // int index: setPixelColor(uint32_t, ...) is ambiguous on newer WLED
    const uint32_t u = uint32_t(i);
    uint32_t c;
    if (u < full)                 c = fillC;
    else if (u == full && frac)   c = color_blend(empty, fillC, uint8_t(frac));
    else                          c = empty;
    if (flowing && u < full) {
      const int32_t dd = charging ? head - i : i - head;  // distance behind the head
      if (dd >= 0 && dd < flowLen) c = color_blend(c, WHITE, uint8_t((255 - (dd * 255) / flowLen) / 3));
    }
    SEGMENT.setPixelColor(i, c);
  }
}
static const char _data_FX_MODE_BATTERY[] PROGMEM =
  "Battery@Speed,Level %,Low warning,Flow length,,Charging,Discharging,Use palette;,Empty;!;1;"
  "sx=64,ix=72,c1=26,c2=96";

/* ------------------------------------------------------------------------------------------------ */
/*  Garage Door                                                                                     */
/*  ix travel time (3..60 s), sx blink speed of the leading edge, c1 edge width,                    */
/*  o1 closing (empties instead of fills), o2 from both ends, o3 moving warning stripes.             */
/*  Colours: edge, door (filled part), ahead (not yet reached).                                      */
/* ------------------------------------------------------------------------------------------------ */

static void mode_garage_door(void) {
  if (!SEGENV.allocateData(sizeof(OneShot))) { fallbackStatic(); return; }
  OneShot *d = reinterpret_cast<OneShot *>(SEGENV.data);
  const uint32_t elapsed = oneShotElapsed(d);

  const uint32_t travel = 3000 + uint32_t(SEGMENT.intensity) * 57000 / 255;
  const uint32_t prog = elapsed >= travel ? 65535 : uint32_t((uint64_t(elapsed) * 65535) / travel);
  const bool closing = SEGMENT.check1, both = SEGMENT.check2, stripes = SEGMENT.check3;
  const uint32_t open = closing ? 65535 - prog : prog;

  const int len = SEGLEN;
  const int span = both ? (len + 1) / 2 : len;
  const uint32_t filled256 = (uint32_t(span) * open) >> 8;  // fill in 1/256 LED, no overflow for long segments
  const int filled = filled256 >> 8;

  const uint32_t edgeC  = SEGCOLOR(0);
  const uint32_t doorC  = SEGCOLOR(1) ? SEGCOLOR(1) : color_fade(SEGCOLOR(0), 48, true);
  const uint32_t aheadC = SEGCOLOR(2);
  const bool moving = elapsed < travel;
  const int edgeW = 1 + (SEGMENT.custom1 >> 3);
  const uint32_t blinkPeriod = 200 + (255 - SEGMENT.speed) * 4;
  const bool blinkOn = (strip.now % blinkPeriod) < blinkPeriod / 2;
  const uint32_t shift = (strip.now / 60) & 0xFFFF;

  for (int i = 0; i < len; i++) {
    const int j = (both && i >= span) ? len - 1 - i : i;  // distance from where the travel starts
    uint32_t c;
    if (j < filled) {
      c = doorC;
      if (stripes) {
        const uint32_t q = closing ? uint32_t(j) + shift : uint32_t(j) + 0x10000 - shift;
        if ((q >> 2) & 1) c = color_fade(doorC, 70, true);
      }
    } else if (j == filled && (filled256 & 255)) {
      c = color_blend(aheadC, doorC, filled256 & 255);
    } else {
      c = aheadC;
    }
    if (moving && blinkOn && j <= filled && j > filled - edgeW) c = edgeC;
    SEGMENT.setPixelColor(i, c);
  }
}
static const char _data_FX_MODE_GARAGE_DOOR[] PROGMEM =
  "Garage Door@Blink speed,Travel time,Edge width,,,Closing,From both ends,Warning stripes;Edge,Door,Ahead;;1;"
  "sx=180,ix=95,c1=24";

/* ------------------------------------------------------------------------------------------------ */
/*  Notify                                                                                          */
/*  c3 number of pulses, sx pulse speed, ix pulse depth, c2 fade to the steady colour,              */
/*  o1 alternate message colours 1 and 3, o2 repeat the message, c1 repeat interval (10..120 s).    */
/*  Colours: message, steady (black = off afterwards), message 2.                                    */
/* ------------------------------------------------------------------------------------------------ */

static void mode_notify(void) {
  if (!SEGENV.allocateData(sizeof(OneShot))) { fallbackStatic(); return; }
  OneShot *d = reinterpret_cast<OneShot *>(SEGENV.data);
  uint32_t elapsed = oneShotElapsed(d);

  const uint32_t pulses = SEGMENT.custom3 ? SEGMENT.custom3 : 1;
  const uint32_t period = 300 + (255 - SEGMENT.speed) * 8;
  const uint32_t fade = uint32_t(SEGMENT.custom2) * 10;
  const uint32_t msgTime = pulses * period;
  if (SEGMENT.check2) {
    uint32_t every = 10000 + uint32_t(SEGMENT.custom1) * 431;
    if (every < msgTime + fade + 1000) every = msgTime + fade + 1000;
    elapsed %= every;
  }

  const uint32_t steady = SEGCOLOR(1);
  const uint8_t floorLvl = 255 - SEGMENT.intensity;
  uint32_t c;
  if (elapsed < msgTime) {
    const uint32_t n = elapsed / period;
    const uint8_t wave = 255 - cos8_t(uint8_t(((elapsed % period) * 255) / period));  // 0 -> 255 -> 0
    const uint32_t msg = (SEGMENT.check1 && (n & 1)) ? SEGCOLOR(2) : SEGCOLOR(0);
    c = color_fade(msg, 255 - scale8(255 - wave, SEGMENT.intensity), true);
  } else {
    const uint32_t lastMsg = (SEGMENT.check1 && ((pulses - 1) & 1)) ? SEGCOLOR(2) : SEGCOLOR(0);
    const uint32_t from = color_fade(lastMsg, floorLvl, true);
    const uint32_t since = elapsed - msgTime;
    c = (fade && since < fade) ? color_blend(from, steady, (since * 255) / fade) : steady;
  }
  SEGMENT.fill(c);
}
static const char _data_FX_MODE_NOTIFY[] PROGMEM =
  "Notify@Pulse speed,Pulse depth,Repeat every,Fade to steady,Pulses,Alternate colours,Repeat;Message,Steady,Message 2;;1;"
  "sx=128,ix=230,c1=50,c2=80,c3=3";

}  // namespace homefx

/* ------------------------------------------------------------------------------------------------ */
/*  usermod                                                                                         */
/* ------------------------------------------------------------------------------------------------ */

class HomeFxUsermod : public Usermod {
 public:
  void setup() override {
    // 255 = next free effect id; the final id depends on the build, so look effects up by name
    strip.addEffect(255, &homefx::mode_advent,      homefx::_data_FX_MODE_ADVENT);
    strip.addEffect(255, &homefx::mode_runway,      homefx::_data_FX_MODE_RUNWAY);
    strip.addEffect(255, &homefx::mode_battery,     homefx::_data_FX_MODE_BATTERY);
    strip.addEffect(255, &homefx::mode_garage_door, homefx::_data_FX_MODE_GARAGE_DOOR);
    strip.addEffect(255, &homefx::mode_notify,      homefx::_data_FX_MODE_NOTIFY);
  }
  void loop() override {}
};

static HomeFxUsermod home_fx;
REGISTER_USERMOD(home_fx);
