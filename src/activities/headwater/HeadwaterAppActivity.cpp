#include "HeadwaterAppActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <string_view>

#include "HeadwaterChannelsActivity.h"
#include "HeadwaterPaths.h"
#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "activities/ActivityManager.h"
#include "activities/network/OpdsSyncActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr size_t NAME_BUFFER_SIZE = 256;

std::string displayName(const std::string& fileName) {
  const auto pos = fileName.rfind('.');
  return pos == std::string::npos ? fileName : fileName.substr(0, pos);
}
}  // namespace

// Row layout (all rows pre-computed):
//   0 issues:  [Sync now]                                     totalItems=1
//   1 issue:   [Today] [Sync now] [Channels]                  totalItems=3
//   2+ issues: [Today] [Sync now] [Channels] [Archived]       totalItems=4
//
// "Today" = issues[0] (newest by ISO-date sort). Everything else lives in
// HeadwaterArchiveActivity, which the user reaches via "Archived".

namespace {
// Row indices when at least one issue exists.
constexpr int ROW_TODAY    = 0;
constexpr int ROW_SYNC     = 1;
constexpr int ROW_CHANNELS = 2;
constexpr int ROW_ARCHIVED = 3;
}  // namespace

void HeadwaterAppActivity::loadIssues() {
  issues.clear();
  auto dir = Storage.open(headwater::ISSUES_DIR);
  if (!dir || !dir.isDirectory()) return;

  char nameBuf[NAME_BUFFER_SIZE];
  dir.rewindDirectory();
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (file.isDirectory()) continue;
    file.getName(nameBuf, NAME_BUFFER_SIZE);
    const std::string_view name{nameBuf};
    if (FsHelpers::hasEpubExtension(name)) issues.emplace_back(name);
  }
  std::sort(issues.begin(), issues.end(), std::greater<std::string>());
}

void HeadwaterAppActivity::onEnter() {
  Activity::onEnter();
  // Pre-select today's issue when one exists; otherwise Sync now.
  selectorIndex = 0;
  // Launched from the Home menu with Confirm held: swallow that release so we
  // don't immediately open today's issue.
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

void HeadwaterAppActivity::onSelectChannels() { activityManager.goToHeadwaterChannels(); }

void HeadwaterAppActivity::onSelectArchive() { activityManager.goToHeadwaterArchive(); }

void HeadwaterAppActivity::onSelectIssue(const std::string& fileName) {
  activityManager.goToReader(std::string(headwater::ISSUES_DIR) + "/" + fileName);
}

void HeadwaterAppActivity::loop() {
  const bool hasIssues  = !issues.empty();
  const bool hasArchive = issues.size() > 1;
  const int totalItems  = hasIssues ? (hasArchive ? 4 : 3) : 1;
  const int pageItems   = UITheme::getNumberOfItemsPerPage(renderer, true, false, true, false);

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (lockNextConfirmRelease) {
      lockNextConfirmRelease = false;
      return;
    }
    if (!hasIssues) {
      onSelectSync();
    } else {
      switch (static_cast<int>(selectorIndex)) {
        case ROW_TODAY:    onSelectIssue(issues[0]); break;
        case ROW_SYNC:     onSelectSync();            break;
        case ROW_CHANNELS: onSelectChannels();        break;
        case ROW_ARCHIVED: onSelectArchive();         break;
        default:                                      break;
      }
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
  const auto pageWidth  = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics   = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_HEADWATER));

  const int contentTop    = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  const bool hasIssues  = !issues.empty();
  const bool hasArchive = issues.size() > 1;
  const int totalItems  = hasIssues ? (hasArchive ? 4 : 3) : 1;

  GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, totalItems,
               static_cast<int>(selectorIndex), [this, hasIssues, hasArchive](int i) -> std::string {
                 if (!hasIssues) return tr(STR_HEADWATER_SYNC_NOW);
                 switch (i) {
                   case ROW_TODAY:    return displayName(issues[0]);
                   case ROW_SYNC:     return tr(STR_HEADWATER_SYNC_NOW);
                   case ROW_CHANNELS: return tr(STR_HEADWATER_CHANNELS);
                   case ROW_ARCHIVED: return tr(STR_HEADWATER_ARCHIVE);
                   default:           return {};
                 }
               });

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_SELECT),
                                            totalItems > 1 ? tr(STR_DIR_UP) : "",
                                            totalItems > 1 ? tr(STR_DIR_DOWN) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
