#include "led_handler.h"

#include <Arduino.h>
#include <math.h>

#include "constants.h"
#include "logging.h"

Adafruit_NeoPixel *strip = nullptr;
uint16_t numLeds = 0;

namespace
{
	bool initialized = false;
	Command activeCmd;
	uint32_t step = 0;
	uint32_t lastUpdate = 0;

	bool identifyActive = false;
	uint32_t identifyEnd = 0;

	bool emergencyActive = false;

	bool heartbeatFlashActive = false;
	uint32_t heartbeatFlashEnd = 0;
	constexpr uint32_t kHeartbeatFlashDuration = 80;

	bool standbyNeedsInit = true;

	const char *GetEffectName(uint8_t effect)
	{
		switch (effect)
		{
		case Cmd::kEffectSolid:
			return "Solid";
		case Cmd::kEffectBlink:
			return "Blink";
		case Cmd::kEffectRainbow:
			return "Rainbow";
		case Cmd::kEffectRainbowCycle:
			return "Rainbow Cycle";
		case Cmd::kEffectChase:
			return "Chase";
		case Cmd::kEffectTheaterChase:
			return "Theater Chase";
		case Cmd::kEffectTwinkle:
			return "Twinkle";
		case Cmd::kEffectFire:
			return "Fire";
		case Cmd::kEffectPulse:
			return "Pulse";
		case Cmd::kEffectGradient:
			return "Gradient";
		case Cmd::kEffectWave:
			return "Wave";
		case Cmd::kEffectMeteor:
			return "Meteor";
		case Cmd::kEffectDna:
			return "DNA Helix";
		case Cmd::kEffectBounce:
			return "Bounce";
		case Cmd::kEffectColorWipe:
			return "Color Wipe";
		case Cmd::kEffectScanner:
			return "Scanner";
		case Cmd::kEffectConfetti:
			return "Confetti";
		case Cmd::kEffectLightning:
			return "Lightning";
		case Cmd::kEffectPolice:
			return "Police";
		case Cmd::kEffectStacking:
			return "Stacking";
		case Cmd::kEffectMarquee:
			return "Marquee";
		case Cmd::kEffectRipple:
			return "Ripple";
		case Cmd::kEffectPlasma:
			return "Plasma";
		default:
			return "Unknown";
		}
	}
}

uint32_t WheelColor(uint8_t pos)
{
	pos = 255 - pos;
	if (pos < 85)
	{
		return strip->Color(255 - pos * 3, 0, pos * 3);
	}
	if (pos < 170)
	{
		pos -= 85;
		return strip->Color(0, pos * 3, 255 - pos * 3);
	}
	pos -= 170;
	return strip->Color(pos * 3, 255 - pos * 3, 0);
}

uint32_t ApplyIntensity(uint32_t color, uint8_t intensity)
{
	uint8_t r = ((color >> 16) & 0xFF) * intensity / 255;
	uint8_t g = ((color >> 8) & 0xFF) * intensity / 255;
	uint8_t b = (color & 0xFF) * intensity / 255;
	return strip->Color(r, g, b);
}

void InitializeLeds()
{
	LOG("Initializing LEDs");

	numLeds = config.ledCount;
	LOGF("LED count: %u on pin %u\n", numLeds, config.ledPin);

	if (strip != nullptr)
	{
		delete strip;
	}

	strip = new Adafruit_NeoPixel(numLeds, config.ledPin, kLedType);
	strip->begin();
	strip->clear();
	strip->show();

	initialized = true;
	step = 0;
	lastUpdate = millis();
	activeCmd.effect = Cmd::kNop;
}

void SetLedColor(uint8_t r, uint8_t g, uint8_t b)
{
	if (!initialized)
		return;

	uint32_t color = strip->Color(r, g, b);
	strip->fill(color, 0, numLeds);
	strip->show();
}

void TurnOffLeds()
{
	if (!initialized)
		return;
	strip->clear();
	strip->show();
	standbyNeedsInit = true;
}

void TurnOffLedsImmediate()
{
	TurnOffLeds();
}

void SetLedCount(uint8_t count)
{
	config.ledCount = count;
	SaveConfig();

	SetLedColor(255, 0, 0);
	delay(500);
	TurnOffLeds();

	InitializeLeds();
	LOGF("LED count set to %u\n", count);
}

void SetLedEffect(const Command &cmd)
{
	if (!initialized)
		return;

	LOGF("Effect: %s | RGB(%u,%u,%u) | Brightness: %u%% | Speed: %u | Duration: %u | Length: %u | Rainbow: %u\n",
		  GetEffectName(cmd.effect), cmd.r, cmd.g, cmd.b, (cmd.intensity * 100) / 255, cmd.speed, cmd.duration, cmd.length, cmd.rainbow);

	step = 0;
	lastUpdate = millis();

	activeCmd = cmd;
	identifyActive = false;
	emergencyActive = false;
	standbyNeedsInit = true;

	if (cmd.effect == Cmd::kEffectSolid)
	{
		uint32_t color = strip->Color(cmd.r, cmd.g, cmd.b);
		color = ApplyIntensity(color, cmd.intensity);
		strip->fill(color, 0, numLeds);
		strip->show();
	}
}

void UpdateLedEffect()
{
	if (!initialized)
		return;

	if (identifyActive)
	{
		if (millis() >= identifyEnd)
		{
			identifyActive = false;
			TurnOffLeds();
			return;
		}
		bool on = ((millis() / 200) % 2) == 0;
		if (on)
		{
			SetLedColor(255, 255, 255);
		}
		else
		{
			TurnOffLeds();
		}
		return;
	}

	if (emergencyActive)
	{
		bool on = ((millis() / 100) % 2) == 0;
		if (on)
		{
			SetLedColor(255, 0, 0);
		}
		else
		{
			TurnOffLeds();
		}
		return;
	}

	uint32_t now = millis();
	uint16_t speed = activeCmd.speed > 0 ? activeCmd.speed : 50;

	if (now - lastUpdate < speed)
		return;
	lastUpdate = now;
	step++;

	switch (activeCmd.effect)
	{
	case Cmd::kEffectSolid:
		break;

	case Cmd::kEffectBlink:
	{
		bool on = (step % 2) == 0;
		if (on)
		{
			uint32_t color = strip->Color(activeCmd.r, activeCmd.g, activeCmd.b);
			color = ApplyIntensity(color, activeCmd.intensity);
			strip->fill(color, 0, numLeds);
		}
		else
		{
			strip->clear();
		}
		strip->show();
		break;
	}


	case Cmd::kEffectRainbow:
	{
		for (uint16_t i = 0; i < numLeds; i++)
		{
			uint32_t color = WheelColor(((i * 256 / numLeds) + step) & 255);
			color = ApplyIntensity(color, activeCmd.intensity);
			strip->setPixelColor(i, color);
		}
		strip->show();
		break;
	}

	case Cmd::kEffectRainbowCycle:
	{
		for (uint16_t i = 0; i < numLeds; i++)
		{
			uint32_t color = WheelColor((step + i) & 255);
			color = ApplyIntensity(color, activeCmd.intensity);
			strip->setPixelColor(i, color);
		}
		strip->show();
		break;
	}

	case Cmd::kEffectChase:
	{
		strip->clear();
		uint8_t len = activeCmd.length > 0 ? activeCmd.length : 3;
		uint32_t color = strip->Color(activeCmd.r, activeCmd.g, activeCmd.b);
		color = ApplyIntensity(color, activeCmd.intensity);

		for (uint8_t j = 0; j < len; j++)
		{
			int pos = (step + j) % numLeds;
			strip->setPixelColor(pos, color);
		}
		strip->show();
		break;
	}

	case Cmd::kEffectTheaterChase:
	{
		strip->clear();
		uint32_t color = strip->Color(activeCmd.r, activeCmd.g, activeCmd.b);
		color = ApplyIntensity(color, activeCmd.intensity);

		for (uint16_t i = 0; i < numLeds; i += 3)
		{
			int pos = i + (step % 3);
			if (pos < numLeds)
			{
				strip->setPixelColor(pos, color);
			}
		}
		strip->show();
		break;
	}

	case Cmd::kEffectTwinkle:
	{
		uint8_t probability = activeCmd.length > 0 ? activeCmd.length : 10;
		for (uint16_t i = 0; i < numLeds; i++)
		{
			if (random(100) < probability)
			{
				float randInt = 0.6f + (random(40) / 100.0f);
				uint8_t intensity = activeCmd.intensity * randInt;
				uint32_t color = strip->Color(activeCmd.r, activeCmd.g, activeCmd.b);
				color = ApplyIntensity(color, intensity);
				strip->setPixelColor(i, color);
			}
			else
			{
				strip->setPixelColor(i, 0);
			}
		}
		strip->show();
		break;
	}


	case Cmd::kEffectFire:
	{
		for (uint16_t i = 0; i < numLeds; i++)
		{
			uint8_t flickerBrightness = random(40, 100);
			uint8_t pixelIntensity = (activeCmd.intensity * flickerBrightness) / 100;
			uint32_t color = strip->Color(activeCmd.r, activeCmd.g, activeCmd.b);
			color = ApplyIntensity(color, pixelIntensity);
			strip->setPixelColor(i, color);
		}
		strip->show();
		break;
	}

	case Cmd::kEffectPulse:
	{
		float phase = ((float)step / 12.75f) * 2.0f * PI;
		float minBrightness = (activeCmd.length > 0 ? activeCmd.length / 100.0f : 0.4f);
		float pulse = minBrightness + (1.0f - minBrightness) * ((sin(phase) + 1.0f) / 2.0f);
		uint8_t intensity = pulse * activeCmd.intensity;
		uint32_t color = strip->Color(activeCmd.r, activeCmd.g, activeCmd.b);
		color = ApplyIntensity(color, intensity);
		strip->fill(color, 0, numLeds);
		strip->show();
		break;
	}


	case Cmd::kEffectGradient:
	{
		for (uint16_t i = 0; i < numLeds; i++)
		{
			float ratio = (float)i / (float)numLeds;
			uint8_t r, g, b;

			if (activeCmd.rainbow)
			{
				uint32_t endColor = WheelColor((step + 128) & 255);
				uint8_t er = (endColor >> 16) & 0xFF;
				uint8_t eg = (endColor >> 8) & 0xFF;
				uint8_t eb = endColor & 0xFF;
				r = activeCmd.r + ratio * (er - activeCmd.r);
				g = activeCmd.g + ratio * (eg - activeCmd.g);
				b = activeCmd.b + ratio * (eb - activeCmd.b);
			}
			else
			{
				r = activeCmd.r * (1.0f - ratio);
				g = activeCmd.g * (1.0f - ratio);
				b = activeCmd.b * (1.0f - ratio);
			}

			uint32_t color = strip->Color(r, g, b);
			color = ApplyIntensity(color, activeCmd.intensity);
			strip->setPixelColor(i, color);
		}
		strip->show();
		break;
	}

	case Cmd::kEffectWave:
	{
		uint8_t len = activeCmd.length > 0 ? activeCmd.length : 10;
		for (uint16_t i = 0; i < numLeds; i++)
		{
			float wave = sin(2.0f * PI * ((float)i / len + (float)step / 20.0f));
			float brightness = (wave + 1.0f) / 2.0f;
			uint8_t intensity = brightness * activeCmd.intensity;
			uint32_t color = strip->Color(activeCmd.r, activeCmd.g, activeCmd.b);
			color = ApplyIntensity(color, intensity);
			strip->setPixelColor(i, color);
		}
		strip->show();
		break;
	}

	case Cmd::kEffectMeteor:
	{
		strip->clear();
		uint8_t meteorLength = activeCmd.length > 0 ? activeCmd.length : 4;
		uint8_t gapLength = meteorLength;
		float fadeRate = 0.8f;

		for (uint16_t j = 0; j < numLeds; j += (meteorLength + gapLength))
		{
			for (uint8_t i = 0; i < meteorLength; i++)
			{
				int pos = (step - i + j + numLeds) % numLeds;
				float fade = pow(fadeRate, i);
				uint8_t intensity = activeCmd.intensity * fade;
				uint32_t color = strip->Color(activeCmd.r, activeCmd.g, activeCmd.b);
				color = ApplyIntensity(color, intensity);
				strip->setPixelColor(pos, color);
			}
		}

		for (uint16_t i = 0; i < numLeds; i++)
		{
			if (random(10) == 0)
			{
				uint32_t curCol = strip->getPixelColor(i);
				uint8_t r = ((curCol >> 16) & 0xFF) * 0.7f;
				uint8_t g = ((curCol >> 8) & 0xFF) * 0.7f;
				uint8_t b = (curCol & 0xFF) * 0.7f;
				strip->setPixelColor(i, strip->Color(r, g, b));
			}
		}
		strip->show();
		break;
	}


	case Cmd::kEffectDna:
	{
		uint8_t waveLen = activeCmd.length > 0 ? activeCmd.length : 10;
		for (uint16_t i = 0; i < numLeds; i++)
		{
			float phase = 2.0f * PI * ((float)i / waveLen + (float)step / 20.0f);
			float mix = (sin(phase) + 1.0f) / 2.0f;

			uint8_t r = activeCmd.r + (uint8_t)((255 - activeCmd.r) * (1.0f - mix));
			uint8_t g = activeCmd.g + (uint8_t)((255 - activeCmd.g) * (1.0f - mix));
			uint8_t b = activeCmd.b + (uint8_t)((255 - activeCmd.b) * (1.0f - mix));

			uint32_t color = strip->Color(r, g, b);
			color = ApplyIntensity(color, activeCmd.intensity);
			strip->setPixelColor(i, color);
		}
		strip->show();
		break;
	}

	case Cmd::kEffectBounce:
	{
		strip->clear();
		uint8_t len = activeCmd.length > 0 ? activeCmd.length : 3;
		uint32_t color = strip->Color(activeCmd.r, activeCmd.g, activeCmd.b);
		color = ApplyIntensity(color, activeCmd.intensity);

		uint16_t maxPos = (numLeds > len) ? (numLeds - len) : 0;
		uint16_t bouncePos = 0;
		if (maxPos > 0)
		{
			uint16_t cycleLen = maxPos * 2;
			uint16_t phase = step % cycleLen;
			bouncePos = (phase <= maxPos) ? phase : cycleLen - phase;
		}

		for (uint8_t j = 0; j < len && (bouncePos + j) < numLeds; j++)
		{
			strip->setPixelColor(bouncePos + j, color);
		}
		strip->show();
		break;
	}

	case Cmd::kEffectColorWipe:
	{
		if (numLeds == 0)
			break;
		uint32_t color = strip->Color(activeCmd.r, activeCmd.g, activeCmd.b);
		color = ApplyIntensity(color, activeCmd.intensity);

		uint16_t cycleLen = numLeds * 2;
		uint16_t phase = step % cycleLen;

		if (phase < numLeds)
		{
			strip->setPixelColor(phase, color);
		}
		else
		{
			strip->setPixelColor(phase - numLeds, 0);
		}
		strip->show();
		break;
	}

	case Cmd::kEffectScanner:
	{
		if (numLeds < 2)
			break;
		strip->clear();
		uint8_t trailLen = activeCmd.length > 0 ? activeCmd.length : 5;
		uint32_t color = strip->Color(activeCmd.r, activeCmd.g, activeCmd.b);

		uint16_t maxPos = numLeds - 1;
		uint16_t pos = 0;
		if (maxPos > 0)
		{
			uint16_t cycleLen = maxPos * 2;
			uint16_t phase = step % cycleLen;
			pos = (phase <= maxPos) ? phase : cycleLen - phase;
		}

		strip->setPixelColor(pos, ApplyIntensity(color, activeCmd.intensity));

		for (uint8_t i = 1; i <= trailLen; i++)
		{
			float fade = pow(0.6f, (float)i);
			uint8_t trailIntensity = activeCmd.intensity * fade;
			uint32_t trailColor = ApplyIntensity(color, trailIntensity);

			int leftPos = (int)pos - i;
			int rightPos = (int)pos + i;
			if (leftPos >= 0)
				strip->setPixelColor(leftPos, trailColor);
			if (rightPos < numLeds)
				strip->setPixelColor(rightPos, trailColor);
		}
		strip->show();
		break;
	}

	case Cmd::kEffectConfetti:
	{
		for (uint16_t i = 0; i < numLeds; i++)
		{
			uint32_t curColor = strip->getPixelColor(i);
			uint8_t r = ((curColor >> 16) & 0xFF);
			uint8_t g = ((curColor >> 8) & 0xFF);
			uint8_t b = (curColor & 0xFF);
			uint8_t fadeAmt = 10;
			r = (r > fadeAmt) ? r - fadeAmt : 0;
			g = (g > fadeAmt) ? g - fadeAmt : 0;
			b = (b > fadeAmt) ? b - fadeAmt : 0;
			strip->setPixelColor(i, strip->Color(r, g, b));
		}

		uint8_t numNew = 1 + random(2);
		for (uint8_t n = 0; n < numNew; n++)
		{
			uint16_t pos = random(numLeds);
			uint32_t randColor = WheelColor(random(256));
			randColor = ApplyIntensity(randColor, activeCmd.intensity);
			strip->setPixelColor(pos, randColor);
		}
		strip->show();
		break;
	}

	case Cmd::kEffectLightning:
	{
		// Fade all pixels toward black
		for (uint16_t i = 0; i < numLeds; i++)
		{
			uint32_t curColor = strip->getPixelColor(i);
			uint8_t r = ((curColor >> 16) & 0xFF);
			uint8_t g = ((curColor >> 8) & 0xFF);
			uint8_t b = (curColor & 0xFF);
			r = r > 40 ? r - 40 : 0;
			g = g > 40 ? g - 40 : 0;
			b = b > 40 ? b - 40 : 0;
			strip->setPixelColor(i, strip->Color(r, g, b));
		}

		uint8_t chance = activeCmd.length > 0 ? activeCmd.length : 8;
		if (random(100) < chance)
		{
			uint16_t flashPos = random(numLeds > 5 ? numLeds - 5 : 0);
			uint8_t flashLen = 3 + random(5);
			for (uint8_t i = 0; i < flashLen && (flashPos + i) < numLeds; i++)
			{
				uint32_t color = strip->Color(activeCmd.r, activeCmd.g, activeCmd.b);
				color = ApplyIntensity(color, activeCmd.intensity);
				strip->setPixelColor(flashPos + i, color);
			}
		}
		strip->show();
		break;
	}

	case Cmd::kEffectPolice:
	{
		uint16_t half = numLeds / 2;
		bool phase = ((step / 3) % 2) == 0;

		uint32_t colorA = strip->Color(activeCmd.r, activeCmd.g, activeCmd.b);
		colorA = ApplyIntensity(colorA, activeCmd.intensity);
		uint32_t colorB = ApplyIntensity(strip->Color(255, 255, 255), activeCmd.intensity);

		for (uint16_t i = 0; i < numLeds; i++)
		{
			if (i < half)
				strip->setPixelColor(i, phase ? colorA : 0);
			else
				strip->setPixelColor(i, phase ? 0 : colorB);
		}
		strip->show();
		break;
	}

	case Cmd::kEffectStacking:
	{
		static uint16_t stackHeight = 0;
		static uint16_t dotPos = 0;

		if (step == 1)
		{
			stackHeight = 0;
			dotPos = 0;
			strip->clear();
		}

		uint32_t color = strip->Color(activeCmd.r, activeCmd.g, activeCmd.b);
		color = ApplyIntensity(color, activeCmd.intensity);

		// Clear previous dot position
		if (dotPos < numLeds)
			strip->setPixelColor(dotPos, 0);

		dotPos++;
		uint16_t landingPos = numLeds - 1 - stackHeight;

		if (dotPos >= landingPos)
		{
			strip->setPixelColor(landingPos, color);
			stackHeight++;
			dotPos = 0;

			if (stackHeight >= numLeds)
			{
				stackHeight = 0;
				strip->clear();
			}
		}
		else
		{
			strip->setPixelColor(dotPos, color);
		}
		strip->show();
		break;
	}

	case Cmd::kEffectMarquee:
	{
		uint8_t spacing = activeCmd.length > 0 ? activeCmd.length : 5;
		uint32_t color = strip->Color(activeCmd.r, activeCmd.g, activeCmd.b);
		color = ApplyIntensity(color, activeCmd.intensity);

		for (uint16_t i = 0; i < numLeds; i++)
		{
			if ((i + step) % spacing == 0)
				strip->setPixelColor(i, color);
			else
				strip->setPixelColor(i, 0);
		}
		strip->show();
		break;
	}

	case Cmd::kEffectRipple:
	{
		strip->clear();
		uint16_t center = numLeds / 2;
		uint16_t maxRadius = center;
		uint16_t radius = step % (maxRadius + 5); // +5 for gap between ripples

		uint32_t color = strip->Color(activeCmd.r, activeCmd.g, activeCmd.b);

		if (radius <= maxRadius)
		{
			uint8_t trailLen = activeCmd.length > 0 ? activeCmd.length : 3;
			for (uint8_t t = 0; t < trailLen; t++)
			{
				int r = (int)radius - t;
				if (r < 0) break;
				float fade = pow(0.7f, (float)t);
				uint8_t trailIntensity = activeCmd.intensity * fade;
				uint32_t trailColor = ApplyIntensity(color, trailIntensity);

				int posLeft = center - r;
				int posRight = center + r;
				if (posLeft >= 0 && posLeft < numLeds)
					strip->setPixelColor(posLeft, trailColor);
				if (posRight >= 0 && posRight < numLeds && posRight != posLeft)
					strip->setPixelColor(posRight, trailColor);
			}
		}
		strip->show();
		break;
	}

	case Cmd::kEffectPlasma:
	{
		for (uint16_t i = 0; i < numLeds; i++)
		{
			float v1 = sin((float)i / 3.0f + (float)step / 7.0f);
			float v2 = sin((float)i / 5.0f - (float)step / 11.0f);
			float v3 = sin(((float)i + (float)step) / 9.0f);
			float val = (v1 + v2 + v3 + 3.0f) / 6.0f; // 0.0 - 1.0

			uint8_t hue = (uint8_t)(val * 255.0f);
			uint32_t color = WheelColor(hue);
			color = ApplyIntensity(color, activeCmd.intensity);
			strip->setPixelColor(i, color);
		}
		strip->show();
		break;
	}

	default:
		break;
	}
}

void UpdateStandbyAnimation()
{
	if (!initialized)
		return;

	static uint32_t lastStandbyUpdate = 0;

	constexpr uint32_t kUpdateInterval = 1500;
	constexpr float kMinBrightness = 0.01f;  // 1%
	constexpr float kMaxBrightness = 0.04f;  // 4%
	constexpr float kColorVariation = 0.20f; // 20% color deviation
	constexpr float kUpdateChanceMin = 0.10f; // 10% of LEDs
	constexpr float kUpdateChanceMax = 0.20f; // 20% of LEDs

	uint32_t now = millis();

	// Initialize all LEDs when entering standby animation
	if (standbyNeedsInit)
	{
		for (uint16_t i = 0; i < numLeds; i++)
		{
			float brightness = kMinBrightness + (random(100) / 100.0f) * (kMaxBrightness - kMinBrightness);
			float colorVarR = 1.0f + ((random(200) - 100) / 100.0f) * kColorVariation;
			float colorVarG = 1.0f + ((random(200) - 100) / 100.0f) * kColorVariation;
			float colorVarB = 1.0f + ((random(200) - 100) / 100.0f) * kColorVariation;

			uint8_t r = constrain((int)(config.standbyR * brightness * colorVarR), 0, 255);
			uint8_t g = constrain((int)(config.standbyG * brightness * colorVarG), 0, 255);
			uint8_t b = constrain((int)(config.standbyB * brightness * colorVarB), 0, 255);

			strip->setPixelColor(i, strip->Color(r, g, b));
		}
		strip->show();
		standbyNeedsInit = false;
		lastStandbyUpdate = now;
		return;
	}

	if (now - lastStandbyUpdate < kUpdateInterval)
		return;
	lastStandbyUpdate = now;

	// Randomly update 10-20% of LEDs
	float updateChance = kUpdateChanceMin + (random(100) / 100.0f) * (kUpdateChanceMax - kUpdateChanceMin);
	int updateThreshold = (int)(updateChance * 100);

	for (uint16_t i = 0; i < numLeds; i++)
	{
		if (random(100) < updateThreshold)
		{
			float brightness = kMinBrightness + (random(100) / 100.0f) * (kMaxBrightness - kMinBrightness);
			float colorVarR = 1.0f + ((random(200) - 100) / 100.0f) * kColorVariation;
			float colorVarG = 1.0f + ((random(200) - 100) / 100.0f) * kColorVariation;
			float colorVarB = 1.0f + ((random(200) - 100) / 100.0f) * kColorVariation;

			uint8_t r = constrain((int)(config.standbyR * brightness * colorVarR), 0, 255);
			uint8_t g = constrain((int)(config.standbyG * brightness * colorVarG), 0, 255);
			uint8_t b = constrain((int)(config.standbyB * brightness * colorVarB), 0, 255);

			strip->setPixelColor(i, strip->Color(r, g, b));
		}
	}

	strip->show();
}

void SetIdentifyEffect(uint16_t durationMs)
{
	identifyActive = true;
	identifyEnd = millis() + durationMs;
	LOG("Identify effect started");
}

void SetEmergencyEffect()
{
	emergencyActive = true;
	LOG("Emergency effect started");
}

void UpdateUnconfiguredAnimation()
{
	if (!initialized)
		return;

	static uint32_t lastUpdate = 0;
	static float phase = 0;

	uint32_t now = millis();
	if (now - lastUpdate < 30)
		return;
	lastUpdate = now;

	phase += 0.03f;
	if (phase > 2.0f * PI)
		phase -= 2.0f * PI;

	float brightness = 0.2f + 0.3f * (1.0f + sin(phase)) / 2.0f;

	uint8_t r = static_cast<uint8_t>(50 * brightness);
	strip->fill(strip->Color(r, 0, 0), 0, numLeds);
	strip->show();
}

void UpdatePairingAnimation()
{
	if (!initialized)
		return;

	static uint32_t lastBlink = 0;
	static bool ledOn = false;

	uint32_t now = millis();
	if (now - lastBlink < 150)
		return;
	lastBlink = now;

	ledOn = !ledOn;
	if (ledOn)
	{
		strip->fill(strip->Color(0, 0, 100), 0, numLeds);
	}
	else
	{
		strip->clear();
	}
	strip->show();
}

void SetPairingSuccessFeedback()
{
	if (!initialized)
		return;

	for (int i = 0; i < 3; i++)
	{
		strip->fill(strip->Color(0, 100, 0), 0, numLeds);
		strip->show();
		delay(100);
		strip->clear();
		strip->show();
		delay(100);
	}
}

void SetPairingFailedFeedback()
{
	if (!initialized)
		return;

	for (int i = 0; i < 5; i++)
	{
		strip->fill(strip->Color(100, 0, 0), 0, numLeds);
		strip->show();
		delay(80);
		strip->clear();
		strip->show();
		delay(80);
	}
}

void SetConfigSuccessFeedback()
{
	if (!initialized)
		return;

	strip->fill(strip->Color(0, 150, 0), 0, numLeds);
	strip->show();
	delay(1000);
	strip->clear();
	strip->show();
	LOG("Config success feedback shown");
}

void SetConfigFailedFeedback()
{
	if (!initialized)
		return;

	for (int i = 0; i < 3; i++)
	{
		strip->fill(strip->Color(150, 0, 0), 0, numLeds);
		strip->show();
		delay(300);
		strip->clear();
		strip->show();
		delay(300);
	}
	LOG("Config failed feedback shown");
}

void TriggerHeartbeatFlash()
{
	heartbeatFlashActive = true;
	heartbeatFlashEnd = millis() + kHeartbeatFlashDuration;
}

bool UpdateHeartbeatFlash()
{
	if (!initialized || !heartbeatFlashActive)
		return false;

	if (millis() >= heartbeatFlashEnd)
	{
		heartbeatFlashActive = false;
		// Restore first LED to standby color
		standbyNeedsInit = true;
		return false;
	}

	// Only flash the first LED
	strip->setPixelColor(0, strip->Color(50, 50, 50));
	strip->show();
	return true;
}

void ShowDimWhiteStandby()
{
	if (!initialized)
		return;

	static uint32_t lastDimUpdate = 0;
	uint32_t now = millis();
	if (now - lastDimUpdate < 100)
		return;
	lastDimUpdate = now;

	strip->fill(strip->Color(5, 5, 5), 0, numLeds);
	strip->show();
}
