#pragma once

#include <lvgl/lvgl.h>
#include <chrono>
#include <cstdint>
#include "displayapp/apps/Apps.h"
#include "displayapp/screens/Screen.h"
#include "displayapp/screens/BatteryIcon.h"
#include "displayapp/Controllers.h"
#include "components/datetime/DateTimeController.h"
#include "components/settings/Settings.h"
#include "utility/DirtyValue.h"

namespace Pinetime {
  namespace Controllers {
    class Battery;
    class Ble;
    class AlarmController;
    class NotificationManager;
  }

  namespace Applications {
    namespace Screens {

      class WatchFaceCasioAE1200 : public Screen {
      public:
        WatchFaceCasioAE1200(Controllers::DateTime& dateTimeController,
                             const Controllers::Battery& batteryController,
                             const Controllers::Ble& bleController,
                             const Controllers::AlarmController& alarmController,
                             Controllers::NotificationManager& notificationManager,
                             Controllers::Settings& settingsController);
        ~WatchFaceCasioAE1200() override;

        void Refresh() override;

        // Colours sampled from the reference design, shared with displayapp/icons/ae1200/generate.js
        static constexpr lv_color_t colorTile = LV_COLOR_MAKE(0x05, 0x05, 0x05);
        static constexpr lv_color_t colorTileBorder = LV_COLOR_MAKE(0x2E, 0x2E, 0x2E);
        static constexpr lv_color_t colorFrame = LV_COLOR_MAKE(0x16, 0x18, 0x18);
        static constexpr lv_color_t colorFrameRim = LV_COLOR_MAKE(0x34, 0x39, 0x3A);
        static constexpr lv_color_t colorLcd = LV_COLOR_MAKE(0x6E, 0x7C, 0x82);
        static constexpr lv_color_t colorSegment = LV_COLOR_MAKE(0x0A, 0x12, 0x16);
        static constexpr lv_color_t colorSegmentShadow = LV_COLOR_MAKE(0x5E, 0x6A, 0x70);
        static constexpr lv_color_t colorSegmentOff = LV_COLOR_MAKE(0x64, 0x72, 0x7A);
        static constexpr lv_color_t colorPrint = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF);
        static constexpr lv_color_t colorPrintRed = LV_COLOR_MAKE(0x92, 0x25, 0x28);
        static constexpr lv_color_t colorFooterLine = LV_COLOR_MAKE(0x26, 0x26, 0x26);
        static constexpr lv_color_t colorDotOff = LV_COLOR_MAKE(0x11, 0x11, 0x11);
        static constexpr lv_color_t colorDotBorder = LV_COLOR_MAKE(0x3A, 0x3A, 0x3A);
        static constexpr lv_color_t colorNotification = LV_COLOR_MAKE(0xF0, 0xA0, 0x30);
        static constexpr lv_color_t colorBle = LV_COLOR_MAKE(0x3A, 0x8D, 0xDD);

      private:
        void UpdateHands(uint8_t hour, uint8_t minute);
        void UpdateSecondHand(uint8_t second);
        void UpdateTimeZoneHighlight(int8_t offset);
        void SetLcdText(lv_obj_t* label, lv_obj_t* shadow, const char* text);

        Utility::DirtyValue<std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>> currentDateTime {};
        Utility::DirtyValue<std::chrono::time_point<std::chrono::system_clock, std::chrono::minutes>> currentMinute {};
        Utility::DirtyValue<std::chrono::time_point<std::chrono::system_clock, std::chrono::days>> currentDate {};
        Utility::DirtyValue<int8_t> utcOffset {};
        Utility::DirtyValue<uint8_t> batteryPercentRemaining {};
        Utility::DirtyValue<bool> powerPresent {};
        Utility::DirtyValue<bool> bleConnected {};
        Utility::DirtyValue<bool> notificationState {};
        Utility::DirtyValue<bool> alarmEnabled {};
        Utility::DirtyValue<bool> muted {};
        Utility::DirtyValue<bool> chimeEnabled {};

        // Analog dial
        lv_obj_t* minuteHand;
        lv_obj_t* hourHand;
        lv_obj_t* secondHand;
        lv_point_t minutePoints[2];
        lv_point_t hourPoints[2];
        lv_point_t secondPoints[2];

        // Indicator panel and world map
        lv_obj_t* indicatorMute;
        lv_obj_t* indicatorAlarm;
        lv_obj_t* indicatorSignal;
        lv_obj_t* timeZoneHighlight;
        lv_obj_t* timeZoneHighlightMap;

        // Status dots
        lv_obj_t* notificationDot;
        lv_obj_t* bleDot;

        // Main LCD, the time digits have a shadow copy drawn one pixel below and to the right
        lv_obj_t* labelDayOfWeek;
        lv_obj_t* labelDate;
        lv_obj_t* indicatorPm;
        lv_obj_t* labelTime;
        lv_obj_t* labelTimeShadow;
        lv_obj_t* labelSeconds;
        lv_obj_t* labelSecondsShadow;
        lv_obj_t* chargingIcon;
        BatteryIcon batteryIcon;

        static constexpr lv_point_t indicatorDividerPoints[2] {{0, 0}, {116, 0}};
        static constexpr lv_point_t footerLinePoints[2] {{0, 0}, {30, 0}};

        Controllers::DateTime& dateTimeController;
        const Controllers::Battery& batteryController;
        const Controllers::Ble& bleController;
        const Controllers::AlarmController& alarmController;
        Controllers::NotificationManager& notificationManager;
        Controllers::Settings& settingsController;

        lv_task_t* taskRefresh;
      };
    }

    template <>
    struct WatchFaceTraits<WatchFace::CasioAE1200> {
      static constexpr WatchFace watchFace = WatchFace::CasioAE1200;
      static constexpr const char* name = "Casio AE1200";

      static Screens::Screen* Create(AppControllers& controllers) {
        return new Screens::WatchFaceCasioAE1200(controllers.dateTimeController,
                                                 controllers.batteryController,
                                                 controllers.bleController,
                                                 controllers.alarmController,
                                                 controllers.notificationManager,
                                                 controllers.settingsController);
      }

      static bool IsAvailable(Pinetime::Controllers::FS& /*filesystem*/) {
        return true;
      }
    };
  }
}
