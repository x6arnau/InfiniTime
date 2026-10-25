#pragma once

#include <lvgl/lvgl.h>
#include <chrono>
#include <cstdint>

#include "displayapp/apps/Apps.h"
#include "displayapp/Controllers.h"
#include "displayapp/screens/Screen.h"
#include "displayapp/screens/Symbols.h"
#include "components/pomodoro/PomodoroController.h"
#include "utility/DirtyValue.h"

namespace Pinetime {
  namespace Controllers {
    class MotorController;
    class BrightnessController;
  }

  namespace Applications {
    namespace Screens {
      class Pomodoro : public Screen {
      public:
        Pomodoro(Controllers::PomodoroController& pomodoroController,
                 Controllers::MotorController& motorController,
                 const Controllers::BrightnessController& brightnessController);
        ~Pomodoro() override;

        void Refresh() override;

        // Called by DisplayApp when a phase has ended and the next one has started
        void NotifyPhaseChange();

        void OnMainButton();
        void OnResetButton();
        void OnPulse();

      private:
        void UpdateState();
        void UpdateTime();

        Controllers::PomodoroController& pomodoroController;
        Controllers::MotorController& motorController;
        const Controllers::BrightnessController& brightnessController;

        lv_obj_t* phaseLabel;
        lv_obj_t* minutesLabel;
        lv_obj_t* secondsLabel;
        lv_obj_t* progressBar;
        lv_obj_t* mainButton;
        lv_obj_t* mainButtonLabel;
        lv_obj_t* resetButton;

        lv_task_t* taskRefresh;
        lv_task_t* pulseTask = nullptr;
        uint8_t pulsesLeft = 0;

        // Each value is redrawn only when it changes: the seconds once a second (hidden in always on mode), the
        // minutes once a minute and the progress bar once per pixel of progress
        Utility::DirtyValue<uint8_t> displayMinutes;
        Utility::DirtyValue<uint8_t> displaySeconds;
        Utility::DirtyValue<lv_coord_t> displayProgress;
        Utility::DirtyValue<bool> alwaysOn;

        // Sized for any uint8_t value so that snprintf can never truncate them
        char minutesText[4];
        char secondsText[5];
      };
    }

    template <>
    struct AppTraits<Apps::Pomodoro> {
      static constexpr Apps app = Apps::Pomodoro;
      static constexpr const char* icon = Screens::Symbols::tomato;

      static Screens::Screen* Create(AppControllers& controllers) {
        return new Screens::Pomodoro(controllers.pomodoroController, controllers.motorController, controllers.brightnessController);
      };

      static bool IsAvailable(Pinetime::Controllers::FS& /*filesystem*/) {
        return true;
      };
    };
  }
}
