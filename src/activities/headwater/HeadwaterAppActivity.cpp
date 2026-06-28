#include "HeadwaterAppActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <string_view>

#include "HeadwaterPaths.h"
#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "activities/ActivityManager.h"
#include "activities/network/OpdsSyncActivity.h"
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
    return;  // No sync has run yet; the sync item still shows.
  }

  char nameBuf[NAME_BUFFER_SIZE];
  dir.rewindDirectory();
  // Advance with dir.openNextFile() (the directory's iterator), not
  // file.openNextFile() — the latter would only ever yield the first entry.
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
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

  selectorIndex = 0;  // pre-select the sync item
  // Launched from the Home menu with Confirm held: swallow that release so we
  // don't immediately trigger sync.
  lockNextConfirmRelease = mappedInput.isPressed(MappedInputManager::Button::Confirm);

  loadIssues();
  requestUpdate();
}

void HeadwaterAppActivity::onExit() {
  Activity::onExit();
  issues.clear();
}

void HeadwaterAppActivity::onSelectSync() {
  const OpdsServer* server = OPDS_STORE.getHeadwaterServer();
  if (server) {
    activityManager.replaceActivity(std::make_unique<OpdsSyncActivity>(renderer, mappedInput, *server));
  }
}

void HeadwaterAppActivity::onSelectIssue(const std::string& fileName) {
  activityManager.goToReader(std::string(headwater::ISSUES_DIR) + "/" + fileName);
}

void HeadwaterAppActivity::loop() {
  // Index 0 is the sync item; real issues occupy indices 1..N.
  const int totalItems = static_cast<int>(issues.size()) + 1;
  const int pageItems = UITheme::getNumberOfItemsPerPage(renderer, true, false, true, false);

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (lockNextConfirmRelease) {
      lockNextConfirmRelease = false;
      return;
    }
    if (selectorIndex == 0) {
      onSelectSync();
    } else {
      onSelectIssue(issues[selectorIndex - 1]);
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }

  buttonNavigator.onNextRelease([this, totalItems] {
    selectorIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), totalItems);
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this, totalItems] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), totalItems);
    requestUpdate();
  });
  buttonNavigator.onNextContinuous([this, totalItems, pageItems] {
    selectorIndex = ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex), totalItems, pageItems);
    requestUpdate();
  });
  buttonNavigator.onPreviousContinuous([this, totalItems, pageItems] {
    selectorIndex = ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex), totalItems, pageItems);
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

  // Index 0 = sync item; indices 1..N = downloaded issues.
  const int totalItems = static_cast<int>(issues.size()) + 1;
  GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, totalItems, selectorIndex,
               [this](int index) -> std::string {
                 if (index == 0) return tr(STR_HEADWATER_SYNC_NOW);
                 return displayName(issues[index - 1]);
               });

  // Confirm hint mirrors the Home screen ("Select") regardless of row; the row
  // itself (Sync now vs. an issue) tells the user what Select will do.
  const bool hasIssues = !issues.empty();
  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_SELECT), hasIssues ? tr(STR_DIR_UP) : "",
                                            hasIssues ? tr(STR_DIR_DOWN) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
