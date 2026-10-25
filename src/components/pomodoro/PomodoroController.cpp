#include "components/pomodoro/PomodoroController.h"

using namespace Pinetime::Controllers;

PomodoroController::PomodoroController(void* const timerData, TimerCallbackFunction_t timerCallbackFunction)
  : timer {timerData, timerCallbackFunction} {
}

void PomodoroController::Start() {
  phase = Phase::Work;
  state = State::Running;
  timer.StartTimer(workDuration);
}

void PomodoroController::Pause() {
  if (state != State::Running) {
    return;
  }
  pausedRemaining = Remaining();
  timer.StopTimer();
  state = State::Paused;
}

void PomodoroController::Resume() {
  if (state != State::Paused) {
    return;
  }
  state = State::Running;
  timer.StartTimer(pausedRemaining);
}

void PomodoroController::Reset() {
  timer.StopTimer();
  phase = Phase::Work;
  state = State::Stopped;
  pausedRemaining = workDuration;
}

void PomodoroController::AdvancePhase() {
  // The expiry message may arrive after the user paused or reset the cycle
  if (state != State::Running) {
    return;
  }
  phase = phase == Phase::Work ? Phase::Break : Phase::Work;
  timer.StartTimer(PhaseDuration());
}

std::chrono::milliseconds PomodoroController::Remaining() {
  switch (state) {
    case State::Running: {
      // Once the timer has expired (and until AdvancePhase runs) GetTimerState counts up, so report 0 instead
      if (!timer.IsRunning()) {
        return std::chrono::milliseconds::zero();
      }
      auto status = timer.GetTimerState();
      return status ? status->distanceToExpiry : std::chrono::milliseconds::zero();
    }
    case State::Paused:
      return pausedRemaining;
    case State::Stopped:
    default:
      return workDuration;
  }
}
