#pragma once

#include <ctime>

class GfxRenderer;

// Draws the Thai calendar sleep screen for `date` into the frame buffer.
// Needs DIGITS_LARGE_FONT_ID, so callers skip it on OMIT_FONTS builds.
void drawThaiCalendarSleepScreen(const GfxRenderer& renderer, const struct tm& date);
