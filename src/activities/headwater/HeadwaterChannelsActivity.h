#pragma once
#include <string>

#include "HeadwaterChannelIndex.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Two-level browser: channels list → per-channel summary entries.
// Selecting a summary deep-links into the owning digest EPUB at that anchor.
// Launched via pushActivity so Back returns to HeadwaterAppActivity.
class HeadwaterChannelsActivity final : public Activity {
 public:
  explicit HeadwaterChannelsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("HeadwaterChannels", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Mode { CHANNELS, ITEMS };

  headwater::ChannelIndex index;
  Mode mode = Mode::CHANNELS;
  size_t channelIndex = 0;
  size_t itemIndex = 0;
  bool loading = true;
  bool empty = false;

  ButtonNavigator buttonNavigator;

  void openSelected();
  int totalItems() const;
};
