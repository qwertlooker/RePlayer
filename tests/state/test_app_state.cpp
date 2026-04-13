#include <gtest/gtest.h>

#include "state/app_state.h"

namespace replayer {
namespace {

TEST(AppStateTest, DefaultsAreConsistent) {
    AppState state;
    EXPECT_EQ(state.playback_mode, PlaybackMode::Normal);
    EXPECT_EQ(state.player_state, PlayerVisualState::Stopped);
    EXPECT_FALSE(state.is_recording);
    EXPECT_EQ(state.auto_pause_delay_ms, 1000);
}

} // namespace
} // namespace replayer
