#pragma once
#include <OpdsParser.h>

#include <string>
#include <vector>

#include "OpdsServerStore.h"
#include "activities/Activity.h"

/**
 * Headless one-press sync of the Headwater OPDS feed.
 *
 * Connects Wi-Fi from saved credentials, fetches the feed, atomically downloads
 * any issue not already on the SD card, then silently restarts back to Home.
 *
 * This is launched ONLY by an explicit user action (the Home "Check Headwater"
 * entry), never from setup(). That is deliberate: nothing here runs during boot,
 * so a crash or hang just reboots to Home with no automatic retry — there is no
 * boot-loop to guard against. Wi-Fi teardown + silentRestart() happen in onExit
 * (same pattern as OpdsBookBrowserActivity) to clear post-Wi-Fi heap
 * fragmentation before normal reading resumes.
 */
class OpdsSyncActivity final : public Activity {
 public:
  enum class State { CONNECTING, FETCHING, DOWNLOADING, ERROR, DONE };

  explicit OpdsSyncActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, OpdsServer server)
      : Activity("OpdsSync", renderer, mappedInput), server(std::move(server)) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // An issue advertised by the feed that is not yet on disk.
  struct PendingIssue {
    std::string url;
    std::string path;
    std::string title;
  };

  State state = State::CONNECTING;
  OpdsServer server;  // Copied at construction — safe even if the store changes during sync

  std::vector<PendingIssue> pending;
  size_t currentIssue = 0;
  int downloadedCount = 0;

  unsigned long connectStartMs = 0;
  unsigned long syncStartMs = 0;
  unsigned long doneAtMs = 0;  // when the DONE state was entered; gates the completion hold
  bool cancelRequested = false;

  std::string statusMessage;
  std::string errorMessage;
  size_t downloadProgress = 0;
  size_t downloadTotal = 0;

  void startConnect();
  void fetchAndQueue();
  void downloadNext();
  void finishToApp();
  bool checkSkip();

  bool preventAutoSleep() override { return true; }
};
