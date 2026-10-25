#include "displayapp/screens/Pomodoro.h"

#include <algorithm>
#include <cstdio>
#include "components/brightness/BrightnessController.h"
#include "components/motor/MotorController.h"
#include "displayapp/InfiniTimeTheme.h"

using namespace Pinetime::Applications::Screens;

namespace {
  constexpr lv_coord_t timeY = 58;
  constexpr lv_coord_t barWidth = 200;
  constexpr lv_coord_t barHeight = 6;
  constexpr lv_coord_t barY = 158;
  constexpr lv_coord_t buttonY = 190;
  constexpr lv_coord_t buttonHeight = 50;
  constexpr lv_coord_t halfButtonWidth = 115;

  constexpr uint8_t pulseCount = 3;
  constexpr uint8_t pulseDuration = 60;   // ms
  constexpr uint32_t pulseInterval = 300; // ms

  void MainButtonEventHandler(lv_obj_t* obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
      static_cast<Pomodoro*>(obj->user_data)->OnMainButton();
    }
  }

  void ResetButtonEventHandler(lv_obj_t* obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
      static_cast<Pomodoro*>(obj->user_data)->OnResetButton();
    }
  }

  void PulseTaskCallback(lv_task_t* task) {
    static_cast<Pomodoro*>(task->user_data)->OnPulse();
  }

  lv_obj_t* CreateLabel(const lv_font_t* font, const char* text) {
    lv_obj_t* label = lv_label_create(lv_scr_act(), nullptr);
    lv_obj_set_style_local_text_font(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font);
    lv_label_set_text_static(label, text);
    return label;
  }

  lv_obj_t* CreateButton(lv_event_cb_t eventHandler, void* userData, const char* text, lv_obj_t** label) {
    lv_obj_t* button = lv_btn_create(lv_scr_act(), nullptr);
    button->user_data = userData;
    lv_obj_set_style_local_radius(button, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_RADIUS_CIRCLE);
    lv_obj_set_event_cb(button, eventHandler);
    lv_obj_t* buttonLabel = lv_label_create(button, nullptr);
    lv_label_set_text_static(buttonLabel, text);
    if (label != nullptr) {
      *label = buttonLabel;
    }
    return button;
  }
}

Pomodoro::Pomodoro(Controllers::PomodoroController& pomodoroController,
                   Controllers::MotorController& motorController,
                   const Controllers::BrightnessController& brightnessController)
  : pomodoroController {pomodoroController}, motorController {motorController}, brightnessController {brightnessController} {

  phaseLabel = CreateLabel(&jetbrains_mono_bold_20, "Focus");
  lv_obj_align(phaseLabel, nullptr, LV_ALIGN_IN_TOP_MID, 0, 20);

  // Minutes in the large font and seconds in a smaller one: only the small seconds label is redrawn every second.
  // Both fonts are monospaced, so the labels keep their width and are placed once.
  minutesLabel = CreateLabel(&jetbrains_mono_76, "50");
  secondsLabel = CreateLabel(&jetbrains_mono_42, ":00");
  const lv_coord_t timeWidth = lv_obj_get_width(minutesLabel) + lv_obj_get_width(secondsLabel);
  lv_obj_set_pos(minutesLabel, (LV_HOR_RES - timeWidth) / 2, timeY);
  // Bottom aligned, then raised by the difference of the baselines so that both texts share the same baseline
  lv_obj_align(secondsLabel,
               minutesLabel,
               LV_ALIGN_OUT_RIGHT_BOTTOM,
               0,
               static_cast<lv_coord_t>(jetbrains_mono_42.base_line - jetbrains_mono_76.base_line));

  progressBar = lv_bar_create(lv_scr_act(), nullptr);
  lv_obj_set_size(progressBar, barWidth, barHeight);
  lv_obj_set_pos(progressBar, (LV_HOR_RES - barWidth) / 2, barY);
  lv_bar_set_range(progressBar, 0, barWidth);
  // The theme leaves the background of bars transparent
  lv_obj_set_style_local_bg_opa(progressBar, LV_BAR_PART_BG, LV_STATE_DEFAULT, LV_OPA_COVER);
  lv_obj_set_style_local_bg_color(progressBar, LV_BAR_PART_BG, LV_STATE_DEFAULT, Colors::bgAlt);
  lv_obj_set_style_local_radius(progressBar, LV_BAR_PART_BG, LV_STATE_DEFAULT, LV_RADIUS_CIRCLE);

  resetButton = CreateButton(ResetButtonEventHandler, this, "Reset", nullptr);
  lv_obj_set_style_local_bg_color(resetButton, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, Colors::bgAlt);
  lv_obj_set_size(resetButton, halfButtonWidth, buttonHeight);
  lv_obj_set_pos(resetButton, 0, buttonY);

  mainButton = CreateButton(MainButtonEventHandler, this, "Start", &mainButtonLabel);

  UpdateState();
  taskRefresh = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);
  Refresh();
}

Pomodoro::~Pomodoro() {
  lv_task_del(taskRefresh);
  if (pulseTask != nullptr) {
    lv_task_del(pulseTask);
  }
  lv_obj_clean(lv_scr_act());
}

void Pomodoro::Refresh() {
  // In always on mode the seconds are hidden, so that the screen is only redrawn once a minute
  alwaysOn = brightnessController.Level() == Controllers::BrightnessController::Levels::AlwaysOn;
  if (alwaysOn.IsUpdated()) {
    lv_obj_set_hidden(secondsLabel, alwaysOn.Get());
  }
  UpdateTime();
}

void Pomodoro::UpdateTime() {
  const auto remaining = pomodoroController.Remaining();
  const auto remainingSeconds = std::chrono::duration_cast<std::chrono::seconds>(remaining).count();

  displayMinutes = static_cast<uint8_t>(std::min<int64_t>(remainingSeconds / 60, 99));
  if (displayMinutes.IsUpdated()) {
    snprintf(minutesText, sizeof(minutesText), "%02d", displayMinutes.Get());
    lv_label_set_text_static(minutesLabel, minutesText);
  }

  if (!alwaysOn.Get()) {
    displaySeconds = static_cast<uint8_t>(remainingSeconds % 60);
    if (displaySeconds.IsUpdated()) {
      snprintf(secondsText, sizeof(secondsText), ":%02d", displaySeconds.Get());
      lv_label_set_text_static(secondsLabel, secondsText);
    }
  }

  const int64_t duration = pomodoroController.PhaseDuration().count();
  const int64_t elapsed = duration - remaining.count();
  displayProgress = static_cast<lv_coord_t>(std::clamp<int64_t>(elapsed * barWidth / duration, 0, barWidth));
  if (displayProgress.IsUpdated()) {
    lv_bar_set_value(progressBar, displayProgress.Get(), LV_ANIM_OFF);
  }
}

// Phase and state only change on user input or at the end of a phase, so the rest of the UI is updated here only
void Pomodoro::UpdateState() {
  using State = Controllers::PomodoroController::State;
  const bool work = pomodoroController.GetPhase() == Controllers::PomodoroController::Phase::Work;
  const lv_color_t phaseColor = work ? Colors::deepOrange : Colors::green;

  lv_label_set_text_static(phaseLabel, work ? "Focus" : "Break");
  lv_obj_set_style_local_text_color(phaseLabel, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, phaseColor);
  lv_obj_align(phaseLabel, nullptr, LV_ALIGN_IN_TOP_MID, 0, 20);
  lv_obj_set_style_local_bg_color(progressBar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, phaseColor);

  const State state = pomodoroController.GetState();
  const bool paused = state == State::Paused;
  lv_obj_set_hidden(resetButton, !paused);
  if (paused) {
    lv_obj_set_size(mainButton, halfButtonWidth, buttonHeight);
    lv_obj_set_pos(mainButton, LV_HOR_RES - halfButtonWidth, buttonY);
  } else {
    lv_obj_set_size(mainButton, LV_HOR_RES, buttonHeight);
    lv_obj_set_pos(mainButton, 0, buttonY);
  }
  lv_label_set_text_static(mainButtonLabel, state == State::Running ? "Pause" : (paused ? "Resume" : "Start"));
  lv_obj_set_style_local_bg_color(mainButton,
                                  LV_BTN_PART_MAIN,
                                  LV_STATE_DEFAULT,
                                  state == State::Running ? Colors::bgAlt : Colors::highlight);

  UpdateTime();
}

void Pomodoro::OnMainButton() {
  using State = Controllers::PomodoroController::State;
  switch (pomodoroController.GetState()) {
    case State::Stopped:
      pomodoroController.Start();
      break;
    case State::Running:
      pomodoroController.Pause();
      break;
    case State::Paused:
      pomodoroController.Resume();
      break;
  }
  UpdateState();
}

void Pomodoro::OnResetButton() {
  pomodoroController.Reset();
  UpdateState();
}

void Pomodoro::NotifyPhaseChange() {
  UpdateState();
  motorController.RunForDuration(pulseDuration);
  pulsesLeft = pulseCount - 1;
  if (pulseTask == nullptr) {
    pulseTask = lv_task_create(PulseTaskCallback, pulseInterval, LV_TASK_PRIO_MID, this);
  }
}

void Pomodoro::OnPulse() {
  motorController.RunForDuration(pulseDuration);
  if (--pulsesLeft == 0) {
    lv_task_del(pulseTask);
    pulseTask = nullptr;
  }
}
