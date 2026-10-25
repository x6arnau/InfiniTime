#include "displayapp/screens/WatchFaceRetro.h"

#include <cstdio>
#include <lvgl/lvgl.h>
#include "components/battery/BatteryController.h"
#include "components/heartrate/HeartRateController.h"
#include "components/motion/MotionController.h"
#include "components/settings/Settings.h"
#include "displayapp/icons/retro/retro_icons.c"

using namespace Pinetime::Applications::Screens;

namespace {
  // The pixel fonts leave one block of spacing after the last glyph. Centered labels are shifted by half a block
  // so that the visible text is centered.
  constexpr lv_coord_t timeBlock = 9;
  constexpr lv_coord_t valueBlock = 3;
  constexpr lv_coord_t smallBlock = 2;

  constexpr lv_coord_t columnOffset = 80;
  constexpr lv_coord_t iconCenterY = 22;
  constexpr lv_coord_t valueY = 42;
  constexpr lv_coord_t timeY = 80;
  constexpr lv_coord_t dateY = 140;
  constexpr lv_coord_t weatherTextX = 66;

  using Icons = Pinetime::Controllers::SimpleWeatherService::Icons;

  const lv_img_dsc_t* WeatherImage(Icons icon, bool night) {
    switch (icon) {
      case Icons::Sun:
        return night ? &retro_moon : &retro_sun;
      case Icons::CloudsSun:
        return night ? &retro_clouds_moon : &retro_clouds_sun;
      case Icons::Clouds:
        return &retro_clouds;
      case Icons::BrokenClouds:
        return &retro_broken_clouds;
      case Icons::CloudShowerHeavy:
        return &retro_shower;
      case Icons::CloudSunRain:
        return &retro_sun_rain;
      case Icons::Thunderstorm:
        return &retro_thunder;
      case Icons::Snow:
        return &retro_snow;
      case Icons::Smog:
        return &retro_mist;
      default:
        return &retro_unknown;
    }
  }

  const char* WeatherCondition(Icons icon, bool night) {
    switch (icon) {
      case Icons::Sun:
        return night ? "CLEAR" : "SUNNY";
      case Icons::CloudsSun:
        return "PARTLY CLOUDY";
      case Icons::Clouds:
        return "CLOUDY";
      case Icons::BrokenClouds:
        return "OVERCAST";
      case Icons::CloudShowerHeavy:
        return "SHOWERS";
      case Icons::CloudSunRain:
        return "RAIN";
      case Icons::Thunderstorm:
        return "STORM";
      case Icons::Snow:
        return "SNOW";
      case Icons::Smog:
        return "MIST";
      default:
        return "UNKNOWN";
    }
  }

  lv_obj_t* CreateLabel(const lv_font_t* font, lv_color_t color) {
    lv_obj_t* label = lv_label_create(lv_scr_act(), nullptr);
    lv_obj_set_style_local_text_font(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font);
    lv_obj_set_style_local_text_color(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, color);
    return label;
  }

  lv_obj_t* CreateRectangle(lv_coord_t width, lv_coord_t height, lv_color_t color) {
    lv_obj_t* rectangle = lv_obj_create(lv_scr_act(), nullptr);
    lv_obj_set_size(rectangle, width, height);
    lv_obj_set_style_local_bg_color(rectangle, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, color);
    lv_obj_set_style_local_radius(rectangle, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_border_width(rectangle, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    return rectangle;
  }

  // Icons of the top row are centered on their column and vertically on iconCenterY
  lv_obj_t* CreateTopIcon(const lv_img_dsc_t* src, lv_coord_t x) {
    lv_obj_t* image = lv_img_create(lv_scr_act(), nullptr);
    lv_img_set_src(image, src);
    lv_obj_align(image, nullptr, LV_ALIGN_IN_TOP_MID, x, iconCenterY - src->header.h / 2);
    return image;
  }
}

WatchFaceRetro::WatchFaceRetro(Controllers::DateTime& dateTimeController,
                               const Controllers::Battery& batteryController,
                               Controllers::Settings& settingsController,
                               Controllers::HeartRateController& heartRateController,
                               Controllers::MotionController& motionController,
                               Controllers::SimpleWeatherService& weatherService)
  : dateTimeController {dateTimeController},
    batteryController {batteryController},
    settingsController {settingsController},
    heartRateController {heartRateController},
    motionController {motionController},
    weatherService {weatherService} {

  // ---- Top row: steps, heart rate and battery ----
  CreateTopIcon(&retro_shoe, -columnOffset);
  CreateTopIcon(&retro_heart, 0);
  batteryIcon = CreateTopIcon(&retro_battery, columnOffset);
  for (uint8_t i = 0; i < nBatteryBars; i++) {
    batteryBars[i] = CreateRectangle(4, 12, colorGreen);
    lv_obj_align(batteryBars[i], batteryIcon, LV_ALIGN_IN_TOP_LEFT, 4 + i * 5, 4);
  }

  stepsValue = CreateLabel(&retro_18, colorWhite);
  heartRateValue = CreateLabel(&retro_18, colorWhite);
  batteryValue = CreateLabel(&retro_18, colorWhite);

  lv_obj_set_pos(CreateRectangle(1, 52, colorDivider), 80, 6);
  lv_obj_set_pos(CreateRectangle(1, 52, colorDivider), 159, 6);
  lv_obj_set_pos(CreateRectangle(224, 1, colorDivider), 8, 66);

  // ---- Time and date ----
  // All digits have the same advance, so the time keeps its width and is only aligned here
  labelTime = CreateLabel(&retro_54, colorWhite);
  lv_label_set_text_static(labelTime, timeText);
  lv_obj_align(labelTime, nullptr, LV_ALIGN_IN_TOP_MID, timeBlock / 2, timeY);
  labelAmPm = CreateLabel(&retro_12, colorWhite);
  lv_label_set_text_static(labelAmPm, "AM");
  lv_obj_align(labelAmPm, labelTime, LV_ALIGN_OUT_RIGHT_BOTTOM, -timeBlock + 3, -timeBlock + smallBlock);
  labelDate = CreateLabel(&retro_12, colorWhite);
  lv_label_set_recolor(labelDate, true);

  lv_obj_set_pos(CreateRectangle(224, 1, colorDivider), 8, 160);

  // ---- Weather ----
  weatherIcon = lv_img_create(lv_scr_act(), nullptr);
  lv_obj_set_pos(weatherIcon, 10, 184);
  temperature = CreateLabel(&retro_24, colorWhite);
  lv_obj_set_pos(temperature, weatherTextX, 175);
  condition = CreateLabel(&retro_12, colorBlue);
  lv_obj_set_pos(condition, weatherTextX, 203);
  minMax = CreateLabel(&retro_12, colorWhite);
  lv_label_set_recolor(minMax, true);
  lv_obj_set_pos(minMax, weatherTextX, 219);

  Refresh();
  taskRefresh = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);
}

WatchFaceRetro::~WatchFaceRetro() {
  lv_task_del(taskRefresh);
  lv_obj_clean(lv_scr_act());
}

void WatchFaceRetro::Refresh() {
  // ---- Steps ----
  stepCount = motionController.NbSteps();
  if (stepCount.IsUpdated()) {
    snprintf(stepsText, sizeof(stepsText), "%lu", static_cast<unsigned long>(stepCount.Get()));
    lv_label_set_text_static(stepsValue, stepsText);
    lv_obj_align(stepsValue, nullptr, LV_ALIGN_IN_TOP_MID, -columnOffset + valueBlock / 2, valueY);
  }

  // ---- Heart rate ----
  heartbeat = heartRateController.HeartRate();
  heartbeatRunning = heartRateController.State() != Controllers::HeartRateController::States::Stopped;
  if (heartbeat.IsUpdated() || heartbeatRunning.IsUpdated()) {
    if (heartbeatRunning.Get() && heartbeat.Get() > 0) {
      snprintf(heartRateText, sizeof(heartRateText), "%d", heartbeat.Get());
    } else {
      snprintf(heartRateText, sizeof(heartRateText), "--");
    }
    lv_label_set_text_static(heartRateValue, heartRateText);
    lv_obj_align(heartRateValue, nullptr, LV_ALIGN_IN_TOP_MID, valueBlock / 2, valueY);
  }

  // ---- Battery ----
  batteryPercentRemaining = batteryController.PercentRemaining();
  powerPresent = batteryController.IsPowerPresent();
  if (batteryPercentRemaining.IsUpdated() || powerPresent.IsUpdated()) {
    uint8_t remaining = batteryPercentRemaining.Get();
    snprintf(batteryText, sizeof(batteryText), "%d%%", remaining);
    lv_label_set_text_static(batteryValue, batteryText);
    lv_obj_align(batteryValue, nullptr, LV_ALIGN_IN_TOP_MID, columnOffset + valueBlock / 2, valueY);

    // One bar per started 20 %, as on the reference design (71 % shows 3 of 5 bars)
    uint8_t bars = remaining / 20;
    if (bars == 0 && remaining > 0) {
      bars = 1;
    }
    lv_color_t color = powerPresent.Get() ? colorYellow : remaining <= 20 ? colorRed : colorGreen;
    for (uint8_t i = 0; i < nBatteryBars; i++) {
      lv_obj_set_hidden(batteryBars[i], i >= bars);
      lv_obj_set_style_local_bg_color(batteryBars[i], LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, color);
    }
  }

  // ---- Time and date ----
  currentDateTime = std::chrono::time_point_cast<std::chrono::minutes>(dateTimeController.CurrentDateTime());
  if (currentDateTime.IsUpdated()) {
    uint8_t hour = dateTimeController.Hours();
    uint8_t minute = dateTimeController.Minutes();

    if (settingsController.GetClockType() == Controllers::Settings::ClockType::H12) {
      const char* ampm = "AM";
      if (hour == 0) {
        hour = 12;
      } else if (hour == 12) {
        ampm = "PM";
      } else if (hour > 12) {
        hour = hour - 12;
        ampm = "PM";
      }
      lv_label_set_text_static(labelAmPm, ampm);
    } else {
      lv_label_set_text_static(labelAmPm, "");
    }
    snprintf(timeText, sizeof(timeText), "%02d:%02d", hour, minute);
    lv_label_set_text_static(labelTime, timeText);

    currentDate = std::chrono::time_point_cast<std::chrono::days>(currentDateTime.Get());
    if (currentDate.IsUpdated()) {
      snprintf(dateText,
               sizeof(dateText),
               "%s #808080 |# #5AD82E %d %s#",
               dateTimeController.DayOfWeekToString(),
               dateTimeController.Day(),
               dateTimeController.MonthShortToString());
      lv_label_set_text_static(labelDate, dateText);
      lv_obj_align(labelDate, nullptr, LV_ALIGN_IN_TOP_MID, smallBlock / 2, dateY);
    }

    // Night only changes the weather icon at sunrise and sunset: checking it once a minute is enough
    isNight = weatherService.IsNight();
  }

  // ---- Weather ----
  currentWeather = weatherService.Current();
  if (currentWeather.IsUpdated() || isNight.IsUpdated()) {
    RefreshWeather();
  }
}

void WatchFaceRetro::RefreshWeather() {
  const auto& optCurrentWeather = currentWeather.Get();
  if (!optCurrentWeather) {
    lv_img_set_src(weatherIcon, &retro_unknown);
    lv_label_set_text_static(temperature, "--\xC2\xB0");
    lv_label_set_text_static(condition, "NO DATA");
    lv_label_set_text_static(minMax, "");
    return;
  }

  int16_t temp;
  int16_t minTemp;
  int16_t maxTemp;
  if (settingsController.GetWeatherFormat() == Controllers::Settings::WeatherFormat::Imperial) {
    temp = optCurrentWeather->temperature.Fahrenheit();
    minTemp = optCurrentWeather->minTemperature.Fahrenheit();
    maxTemp = optCurrentWeather->maxTemperature.Fahrenheit();
  } else {
    temp = optCurrentWeather->temperature.Celsius();
    minTemp = optCurrentWeather->minTemperature.Celsius();
    maxTemp = optCurrentWeather->maxTemperature.Celsius();
  }

  lv_img_set_src(weatherIcon, WeatherImage(optCurrentWeather->iconId, isNight.Get()));
  snprintf(temperatureText, sizeof(temperatureText), "%d\xC2\xB0", temp);
  lv_label_set_text_static(temperature, temperatureText);
  lv_label_set_text_static(condition, WeatherCondition(optCurrentWeather->iconId, isNight.Get()));
  snprintf(minMaxText, sizeof(minMaxText), "H: %d\xC2\xB0  #4080F8 L:# %d\xC2\xB0", maxTemp, minTemp);
  lv_label_set_text_static(minMax, minMaxText);
}
