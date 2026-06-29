#pragma once
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Lists all Headwater issues except the newest (which stays on the main app
// page as "Today"). Short Select → open in reader. Long-hold Select → delete
// with a warning that the issue's Channels summaries will also be removed.
// Launched via pushActivity so Back returns to HeadwaterAppActivity.
class HeadwaterArchiveActivity final : public Activity {
 public:
  explicit HeadwaterArchiveActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("HeadwaterArchive", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // All issues in /Headwater sorted newest-first. Archived items are issues[1..N].
  std::vector<std::string> issues;
  size_t selectorIndex = 0;
  bool lockNextConfirmRelease = false;
  ButtonNavigator buttonNavigator;

  void loadIssues();

  // Convenience: map display index i to issues[i+1].
  const std::string& archivedAt(size_t i) const { return issues[i + 1]; }
  int archivedCount() const { return std::max(0, static_cast<int>(issues.size()) - 1); }
};
