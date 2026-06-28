#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// The Headwater "app": an offline browser over the issues the sync downloaded
// into /Headwater. v1 is a flat, newest-first list of issues that opens the
// selected issue in the reader. The cross-day channels view (manifest-driven)
// layers on top of this once the backend ships the per-issue manifest.
class HeadwaterAppActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  size_t selectorIndex = 0;
  // Issue file names (no path), newest first. Path is HEADWATER_DIR + "/" + name.
  std::vector<std::string> issues;
  // True when entered while Confirm was held (launched from the Home menu); we
  // swallow the next release so we don't immediately open the first issue.
  bool lockNextConfirmRelease = false;

  void loadIssues();
  void onSelectSync();
  void onSelectIssue(const std::string& fileName);

 public:
  explicit HeadwaterAppActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("HeadwaterApp", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
