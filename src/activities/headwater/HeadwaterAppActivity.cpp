#include "HeadwaterAppActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <string_view>

#include "HeadwaterPaths.h"
#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr size_t NAME_BUFFER_SIZE = 256;

// Strip the ".epub" (or other) extension for display. Until the backend
// manifest ships clean titles, the filename stem is the best label we have.
std::string displayName(const std::string& fileName) {
  const auto pos = fileName.rfind('.');
  return pos == std::string::npos ? fileName : fileName.substr(0, pos);
}
}  // namespace

void HeadwaterAppActivity::loadIssues() {
  issues.clear();

  // RAII: the directory handle closes when it leaves scope (HalFile destructor).
  auto dir = Storage.open(headwater::ISSUES_DIR);
  if (!dir || !dir.isDirectory()) {
    return;  // No sync has run yet; render() shows the empty state.
  }

  char nameBuf[NAME_BUFFER_SIZE];
  dir.rewindDirectory();
  for (auto file = dir.openNextFile(); file; file = file.openNextFile()) {
    if (file.isDirectory()) continue;
    file.getName(nameBuf, NAME_BUFFER_SIZE);
    const std::string_view name{nameBuf};
    if (FsHelpers::hasEpubExtension(name)) {
      issues.emplace_back(name);
    }
  }

  // Issue titles are date-stamped, so descending lexicographic order puts the
  // newest issue first.
  std::sort(issues.begin(), issues.end(), std::greater<std::string>());
}

void HeadwaterAppActivity::onEnter() {
  Activity::onEnter();

  selectorIndex = 0;
  // Launched from the Home menu with Confirm held: swallow that release so we
  // don't immediately open the first issue.
  lockNextConfirmRelease = mappedInput.isPressed(MappedInputManager::Button::Confirm);

  loadIssues();
  requestUpdate();
}

void HeadwaterAppActivity::onExit() {
  Activity::onExit();
  issues.clear();
}

void HeadwaterAppActivity::onSelectIssue(const std::string& fileName) {
  activityManager.goToReader(std::string(headwater::ISSUES_DIR) + "/" + fileName);
}

void HeadwaterAppActivity::loop() {
  const int pageItems = UITheme::getNumberOfItemsPerPage(renderer, true, false, true, false);
  const int listSize = static_cast<int>(issues.size());

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (lockNextConfirmRelease) {
      lockNextConfirmRelease = false;
      return;
    }
    if (!issues.empty()) {
      onSelectIssue(issues[selectorIndex]);
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }

  buttonNavigator.onNextRelease([this, listSize] {
    selectorIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this, listSize] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });
  buttonNavigator.onNextContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });
  buttonNavigator.onPreviousContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });
}

void HeadwaterAppActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_HEADWATER));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (issues.empty()) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_HEADWATER_NO_ISSUES));
  } else {
    GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, issues.size(), selectorIndex,
                 [this](int index) { return displayName(issues[index]); });
  }

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), issues.empty() ? "" : tr(STR_OPEN),
                                            issues.empty() ? "" : tr(STR_DIR_UP), issues.empty() ? "" : tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
