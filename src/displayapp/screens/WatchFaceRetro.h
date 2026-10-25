#pragma once

#include <lvgl/lvgl.h>
#include <chrono>
#include <cstdint>
#include <optional>
#include "displayapp/apps/Apps.h"
#include "displayapp/screens/Screen.h"
#include "displayapp/Controllers.h"
#include "components/datetime/DateTimeController.h"
#include "components/ble/SimpleWeatherService.h"
#include "utility/DirtyValue.h"

namespace Pinetime {
  namespace Controllers {
    class Settings;
    class Battery;
    class HeartRateController;
    class MotionController;
  }

  namespace Applications {
    namespace Screens {

      // Pixel art watch face: steps, heart rate and battery on top, the time and date in the middle and the
      // weather at the bottom. Fonts and icons are 1 or 2 bits per pixel and every value is only redrawn when it
      // changes.
      class WatchFaceRetro : public Screen {
      public:
        WatchFaceRetro(Controllers::DateTime& dateTimeController,
                       const Controllers::Battery& batteryController,
                       Controllers::Settings& settingsController,
                       Controllers::HeartRateController& heartRateController,
                       Controllers::MotionController& motionController,
                       Controllers::SimpleWeatherService& weatherService);
        ~WatchFaceRetro() override;

        void Refresh() override;

      private:
        static constexpr uint8_t nBatteryBars = 5;

        void RefreshWeather();

        Utility::DirtyValue<uint8_t> batteryPercentRemaining {};
        Utility::DirtyValue<bool> powerPresent {};
        Utility::DirtyValue<uint32_t> stepCount {};
        Utility::DirtyValue<uint8_t> heartbeat {};
        Utility::DirtyValue<bool> heartbeatRunning {};
        Utility::DirtyValue<std::chrono::time_point<std::chrono::system_clock, std::chrono::minutes>> currentDateTime {};
        Utility::DirtyValue<std::chrono::time_point<std::chrono::system_clock, std::chrono::days>> currentDate {};
        Utility::DirtyValue<std::optional<Controllers::SimpleWeatherService::CurrentWeather>> currentWeather {};
        Utility::DirtyValue<bool> isNight {};

        lv_obj_t* stepsValue;
        lv_obj_t* heartRateValue;
        lv_obj_t* batteryIcon;
        lv_obj_t* batteryBars[nBatteryBars];
        lv_obj_t* batteryValue;
        lv_obj_t* labelTime;
        lv_obj_t* labelAmPm;
        lv_obj_t* labelDate;
        lv_obj_t* weatherIcon;
        lv_obj_t* temperature;
        lv_obj_t* condition;
        lv_obj_t* minMax;

        // Texts are formatted into these buffers and set with lv_label_set_text_static, so updating a value does
        // not allocate memory.
        char stepsText[11];
        char heartRateText[4];
        char batteryText[5];
        char timeText[8] = "00:00";
        char dateText[40];
        char temperatureText[10];
        char minMaxText[40];

        static constexpr lv_color_t colorWhite = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF);
        static constexpr lv_color_t colorDivider = LV_COLOR_MAKE(0x80, 0x80, 0x80);
        static constexpr lv_color_t colorGreen = LV_COLOR_MAKE(0x5A, 0xD8, 0x2E);
        static constexpr lv_color_t colorRed = LV_COLOR_MAKE(0xE8, 0x30, 0x30);
        static constexpr lv_color_t colorYellow = LV_COLOR_MAKE(0xF8, 0xC8, 0x20);
        static constexpr lv_color_t colorBlue = LV_COLOR_MAKE(0x40, 0x80, 0xF8);

        Controllers::DateTime& dateTimeController;
        const Controllers::Battery& batteryController;
        Controllers::Settings& settingsController;
        Controllers::HeartRateController& heartRateController;
        Controllers::MotionController& motionController;
        Controllers::SimpleWeatherService& weatherService;

        lv_task_t* taskRefresh;
      };
    }

    template <>
    struct WatchFaceTraits<WatchFace::Retro> {
      static constexpr WatchFace watchFace = WatchFace::Retro;
      static constexpr const char* name = "Retro";

      static Screens::Screen* Create(AppControllers& controllers) {
        return new Screens::WatchFaceRetro(controllers.dateTimeController,
                                           controllers.batteryController,
                                           controllers.settingsController,
                                           controllers.heartRateController,
                                           controllers.motionController,
                                           *controllers.weatherController);
      }

      static bool IsAvailable(Pinetime::Controllers::FS& /*filesystem*/) {
        return true;
      }
    };
  }
}
