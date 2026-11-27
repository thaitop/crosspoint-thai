#include "HomeWidgets.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "UITheme.h"
#include "fontIds.h"
#include "icons/blocks.h"
#include "icons/folder.h"
#include "icons/library.h"
#include "icons/settings2.h"
#include "icons/transfer.h"

namespace {
constexpr int ICON_LABEL_GAP = 4;
constexpr int ICON_SIZE = 32;
constexpr int RADIUS = 6;
constexpr int SELECTION_LINE = 2;
constexpr int BUTTON_HEIGHT = 44;
constexpr int CAPSULE_BAR_HEIGHT = 56;
constexpr int CAPSULE_BAR_MARGIN = 8;
#ifndef OMIT_FONTS
constexpr int PERCENT_FONT_ID = NOTOSANS_18_FONT_ID;
#else
constexpr int PERCENT_FONT_ID = UI_12_FONT_ID;
#endif
}  // namespace

namespace homeui {

const uint8_t* toolIcon(const UIIcon icon) {
  switch (icon) {
    case UIIcon::Folder:
      return FolderIcon;
    case UIIcon::Library:
      return LibraryIcon;
    case UIIcon::Blocks:
      return BlocksIcon;
    case UIIcon::Transfer:
      return TransferIcon;
    case UIIcon::Settings:
      return Settings2Icon;
    default:
      return nullptr;
  }
}

void formatDuration(char* out, const size_t size, const uint32_t minutes) {
  if (minutes < 60) {
    snprintf(out, size, "%u %s", static_cast<unsigned>(minutes), tr(STR_UNIT_MINUTES));
  } else if (minutes % 60 == 0) {
    snprintf(out, size, "%u %s", static_cast<unsigned>(minutes / 60), tr(STR_UNIT_HOURS));
  } else {
    snprintf(out, size, "%u %s %u %s", static_cast<unsigned>(minutes / 60), tr(STR_UNIT_HOURS),
             static_cast<unsigned>(minutes % 60), tr(STR_UNIT_MINUTES));
  }
}

void drawToolBar(const GfxRenderer& renderer, const Tool* tools, const int count, const int firstIndex,
                 const int selected, const Rect rect, Rect* targets) {
  if (count <= 0) return;
  renderer.fillRect(rect.x, rect.y, rect.width, 2, true);
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int cellWidth = rect.width / count;
  for (int i = 0; i < count; ++i) {
    const Rect cell{rect.x + i * cellWidth, rect.y + 2, i == count - 1 ? rect.width - i * cellWidth : cellWidth,
                    rect.height - 2};
    if (i > 0) renderer.drawLine(cell.x, cell.y + 8, cell.x, cell.y + cell.height - 8, true);
    // Icon stacked above its label, the pair centered in the cell.
    const uint8_t* icon = toolIcon(tools[i].icon);
    const int contentH = (icon ? ICON_SIZE + ICON_LABEL_GAP : 0) + lineHeight;
    int y = cell.y + std::max(0, (cell.height - contentH) / 2);
    if (icon) {
      renderer.drawIcon(icon, cell.x + (cell.width - ICON_SIZE) / 2, y, ICON_SIZE);
      y += ICON_SIZE + ICON_LABEL_GAP;
    }
    const std::string label = renderer.truncatedText(UI_10_FONT_ID, tools[i].label, cell.width - 12);
    UITheme::drawCenteredText(renderer, cell, UI_10_FONT_ID, y, label.c_str());
    if (selected == firstIndex + i) {
      renderer.drawRoundedRect(cell.x + 4, cell.y + 4, cell.width - 8, cell.height - 8, SELECTION_LINE, RADIUS, true);
    }
    targets[firstIndex + i] = cell;
  }
}

Rect capsuleToolBarRect(const int left, const int width, const Rect safe) {
  return Rect{left, safe.y + safe.height - CAPSULE_BAR_HEIGHT - CAPSULE_BAR_MARGIN, width, CAPSULE_BAR_HEIGHT};
}

void drawCapsuleToolBar(const GfxRenderer& renderer, const Tool* tools, const int count, const int firstIndex,
                        const int selected, const Rect rect, Rect* targets) {
  if (count <= 0) return;
  renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, 2, rect.height / 2, true);
  const int inner = rect.width - 8;
  const int active = selected - firstIndex;
  const bool hasActive = active >= 0 && active < count;

  // The selected tool widens to fit its label; the others share the rest.
  int activeW = 0;
  std::string activeLabel;
  if (hasActive) {
    const int maxW = inner / 2;
    activeLabel =
        renderer.truncatedText(UI_10_FONT_ID, tools[active].label, maxW - ICON_SIZE - 34, EpdFontFamily::BOLD);
    const int contentW = ICON_SIZE + 6 + renderer.getTextWidth(UI_10_FONT_ID, activeLabel.c_str(), EpdFontFamily::BOLD);
    activeW = std::max(inner / count, std::min(maxW, contentW + 28));
  }
  const int otherCount = hasActive ? count - 1 : count;
  const int otherW = otherCount > 0 ? (inner - activeW) / otherCount : 0;

  int x = rect.x + 4;
  const int cellY = rect.y + 4;
  const int cellH = rect.height - 8;
  for (int i = 0; i < count; ++i) {
    const bool isActive = hasActive && i == active;
    // The last cell takes the rounding remainder so the bar has no dead edge.
    const int cellW = i == count - 1 ? rect.x + 4 + inner - x : (isActive ? activeW : otherW);
    const uint8_t* icon = toolIcon(tools[i].icon);
    const int iconY = cellY + (cellH - ICON_SIZE) / 2;
    if (isActive) {
      renderer.fillRoundedRect(x, cellY, cellW, cellH, cellH / 2, Color::Black);
      const int labelW = renderer.getTextWidth(UI_10_FONT_ID, activeLabel.c_str(), EpdFontFamily::BOLD);
      const int contentW = (icon ? ICON_SIZE + 6 : 0) + labelW;
      int cx = x + (cellW - contentW) / 2;
      if (icon) {
        renderer.drawIcon(icon, cx, iconY, ICON_SIZE, false);
        cx += ICON_SIZE + 6;
      }
      renderer.drawText(UI_10_FONT_ID, cx, cellY + (cellH - renderer.getLineHeight(UI_10_FONT_ID)) / 2,
                        activeLabel.c_str(), false, EpdFontFamily::BOLD);
    } else if (icon) {
      renderer.drawIcon(icon, x + (cellW - ICON_SIZE) / 2, iconY, ICON_SIZE);
    } else {
      const std::string label = renderer.truncatedText(UI_10_FONT_ID, tools[i].label, cellW - 8);
      UITheme::drawCenteredText(renderer, Rect{x, cellY, cellW, cellH}, UI_10_FONT_ID,
                                cellY + (cellH - renderer.getLineHeight(UI_10_FONT_ID)) / 2, label.c_str());
    }
    targets[firstIndex + i] = Rect{x, rect.y, cellW, rect.height};
    x += cellW;
  }
}

void drawProgressTrack(const GfxRenderer& renderer, const Rect rect, const int percent) {
  renderer.drawRect(rect.x, rect.y, rect.width, rect.height, true);
  const int fill = (rect.width - 2) * std::clamp(percent, 0, 100) / 100;
  if (fill > 0) renderer.fillRect(rect.x + 1, rect.y + 1, fill, rect.height - 2, true);
}

void drawCover(const GfxRenderer& renderer, const RecentBook& book, const int thumbHeight, const Rect slot,
               const int radius) {
  bool drawn = false;
  if (!book.coverBmpPath.empty()) {
    HalFile file;
    if (Storage.openFileForRead("HOME", UITheme::getCoverThumbPath(book.coverBmpPath, thumbHeight), file)) {
      Bitmap bitmap(file);
      drawn = bitmap.parseHeaders() == BmpReaderError::Ok && BaseTheme::drawCoverThumbFill(renderer, bitmap, slot);
    }
  }
  if (!drawn) BaseTheme::drawCoverPlaceholder(renderer, slot);
  if (radius <= 0) {
    renderer.drawRect(slot.x, slot.y, slot.width, slot.height, true);
    return;
  }
  // Clear the image outside each quarter circle, then outline the rounded shape.
  const int r = std::min(radius, std::min(slot.width, slot.height) / 2);
  for (int dy = 0; dy < r; ++dy) {
    for (int dx = 0; dx < r; ++dx) {
      const int ox = r - dx;
      const int oy = r - dy;
      if (ox * ox + oy * oy <= r * r) continue;
      renderer.drawPixel(slot.x + dx, slot.y + dy, false);
      renderer.drawPixel(slot.x + slot.width - 1 - dx, slot.y + dy, false);
      renderer.drawPixel(slot.x + dx, slot.y + slot.height - 1 - dy, false);
      renderer.drawPixel(slot.x + slot.width - 1 - dx, slot.y + slot.height - 1 - dy, false);
    }
  }
  renderer.drawRoundedRect(slot.x, slot.y, slot.width, slot.height, 1, r, true);
}

void drawFeaturedBook(const GfxRenderer& renderer, const RecentBook& book, const int percent, const int thumbHeight,
                      const Rect rect, const bool selected, const bool percentText) {
  const int coverHeight = std::min(thumbHeight, rect.height);
  const int coverWidth = coverHeight * 2 / 3;
  drawCover(renderer, book, thumbHeight, Rect{rect.x, rect.y, coverWidth, coverHeight});

  const int x = rect.x + coverWidth + 20;
  const int w = rect.x + rect.width - x;
  if (w <= 0) return;
  const int titleHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int smallHeight = renderer.getLineHeight(UI_10_FONT_ID);

  // Laid out bottom-up: button, percent and bar are fixed; the title gets
  // whatever lines remain above them and ellipsizes past that.
  const int buttonY = rect.y + coverHeight - BUTTON_HEIGHT;
  const int percentY = buttonY - 10 - renderer.getLineHeight(PERCENT_FONT_ID);
  const int barY = percentText ? percentY - 18 : buttonY - 22;
  const int textBottom = percent >= 0 ? barY - 12 : buttonY - 12;
  const bool showAuthor = !book.author.empty() && textBottom - rect.y >= titleHeight + smallHeight + 2;
  const int titleLines =
      std::clamp((textBottom - rect.y - (showAuthor ? smallHeight + 2 : 0)) / std::max(1, titleHeight), 1, 3);

  int y = rect.y;
  for (const auto& line : renderer.wrappedText(UI_12_FONT_ID, book.title.c_str(), w, titleLines, EpdFontFamily::BOLD)) {
    renderer.drawText(UI_12_FONT_ID, x, y, line.c_str(), true, EpdFontFamily::BOLD);
    y += titleHeight;
  }
  if (showAuthor) {
    renderer.drawText(UI_10_FONT_ID, x, y + 2, renderer.truncatedText(UI_10_FONT_ID, book.author.c_str(), w).c_str(),
                      true);
    y += smallHeight + 2;
  }

  if (percent >= 0) {
    // Without the percent figure the bar sits right under the title and author.
    drawProgressTrack(renderer, Rect{x, percentText ? barY : y + 12, w, 10}, percent);
    if (percentText) {
      char text[8];
      snprintf(text, sizeof(text), "%d%%", percent);
      renderer.drawText(PERCENT_FONT_ID, x, percentY, text, true, EpdFontFamily::BOLD);
    }
  }

  // The resume button doubles as the selection indicator: filled when selected.
  const int labelWidth = renderer.getTextWidth(UI_12_FONT_ID, tr(STR_CONTINUE_READING), EpdFontFamily::BOLD);
  const int labelY = buttonY + (BUTTON_HEIGHT - titleHeight) / 2;
  if (selected) {
    renderer.fillRoundedRect(x, buttonY, w, BUTTON_HEIGHT, RADIUS, Color::Black);
  } else {
    renderer.drawRoundedRect(x, buttonY, w, BUTTON_HEIGHT, SELECTION_LINE, RADIUS, true);
  }
  renderer.drawText(UI_12_FONT_ID, x + (w - labelWidth) / 2, labelY, tr(STR_CONTINUE_READING), !selected,
                    EpdFontFamily::BOLD);
}

}  // namespace homeui
