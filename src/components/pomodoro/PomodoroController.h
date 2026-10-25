#pragma once

#include <FreeRTOS.h>
#include <timers.h>

#include <chrono>
#include <cstdint>

#include "components/timer/Timer.h"

namespace Pinetime {
  namespace Controllers {
    // Fixed 50/10 pomodoro cycle. A single one-shot FreeRTOS timer covers the current phase, so nothing runs
    // periodically while the cycle is in progress; its callback (owned by DisplayApp) chains the next phase.
    class PomodoroController {
    public:
      enum class Phase : uint8_t { Work, Break };
      enum class State : uint8_t { Stopped, Running, Paused };

      static constexpr std::chrono::milliseconds workDuration = std::chrono::minutes(50);
      static constexpr std::chrono::milliseconds breakDuration = std::chrono::minutes(10);

      PomodoroController(void* timerData, TimerCallbackFunction_t timerCallbackFunction);

      void Start();
      void Pause();
      void Resume();
      void Reset();
      // Called when the phase timer expires: switches phase and starts the next one
      void AdvancePhase();

      std::chrono::milliseconds Remaining();

      Phase GetPhase() const {
        return phase;
      }

      State GetState() const {
        return state;
      }

      std::chrono::milliseconds PhaseDuration() const {
        return phase == Phase::Work ? workDuration : breakDuration;
      }

    private:
      Timer timer;
      Phase phase = Phase::Work;
      State state = State::Stopped;
      std::chrono::milliseconds pausedRemaining {workDuration};
    };
  }
}
