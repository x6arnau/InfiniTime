#include "displayapp/screens/WatchFaceCasioAE1200.h"

#include <lvgl/lvgl.h>
#include <cstdio>
#include <limits>
#include "displayapp/screens/Symbols.h"
#include "components/alarm/AlarmController.h"
#include "components/battery/BatteryController.h"
#include "components/ble/BleController.h"
#include "components/ble/NotificationManager.h"
#include "components/settings/Settings.h"

#include "displayapp/icons/ae1200/dial.c"
#include "displayapp/icons/ae1200/labels.c"
#include "displayapp/icons/ae1200/lcd.c"
#include "displayapp/icons/ae1200/worldmap.c"

using namespace Pinetime::Applications::Screens;

namespace {
  // Analog dial geometry. The bezel ring, ticks, LCD disc, inner circle and crosshair are part of the dial image.
  constexpr lv_coord_t dialCenterX = 56;
  constexpr lv_coord_t dialCenterY = 79;
  constexpr lv_coord_t dialImageSize = 100;
  constexpr int16_t minuteHandLength = 41;
  constexpr int16_t hourHandLength = 30;
  constexpr int16_t secondHandLength = 42;

  // World map image position and size, see displayapp/icons/ae1200/generate.js
  constexpr lv_coord_t mapX = 119;
  constexpr lv_coord_t mapY = 82;
  constexpr lv_coord_t mapWidth = 112;
  constexpr lv_coord_t mapHeight = 44;
  // About 30 degrees of longitude, centred on the local time zone
  constexpr lv_coord_t timeZoneHighlightWidth = 9;

  // sin(90) = 1 so the value of _lv_trigo_sin(90) is the scaling factor
  constexpr int32_t trigScale = std::numeric_limits<int16_t>::max();

  lv_point_t DialPoint(int16_t radius, int16_t angle) {
    return lv_point_t {.x = static_cast<lv_coord_t>(dialCenterX + radius * _lv_trigo_sin(angle) / trigScale),
                       .y = static_cast<lv_coord_t>(dialCenterY - radius * _lv_trigo_sin(angle + 90) / trigScale)};
  }

  lv_obj_t* CreateRect(lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height, lv_coord_t radius, lv_color_t color) {
    lv_obj_t* rect = lv_obj_create(lv_scr_act(), nullptr);
    lv_obj_set_pos(rect, x, y);
    lv_obj_set_size(rect, width, height);
    lv_obj_set_style_local_radius(rect, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, radius);
    lv_obj_set_style_local_bg_color(rect, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, color);
    lv_obj_set_style_local_bg_opa(rect, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_COVER);
    lv_obj_set_style_local_border_width(rect, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    return rect;
  }

  // LCD panel with a dark frame and a lighter rim around it
  void CreatePanel(lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height) {
    lv_obj_t* panel = CreateRect(x, y, width, height, 9, WatchFaceCasioAE1200::colorLcd);
    lv_obj_set_style_local_border_width(panel, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 3);
    lv_obj_set_style_local_border_color(panel, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, WatchFaceCasioAE1200::colorFrame);
    lv_obj_set_style_local_outline_width(panel, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_outline_color(panel, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, WatchFaceCasioAE1200::colorFrameRim);
    lv_obj_set_style_local_outline_opa(panel, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_COVER);
  }

  lv_obj_t* CreateDot(lv_coord_t x, lv_coord_t y) {
    lv_obj_t* dot = CreateRect(x, y, 7, 7, LV_RADIUS_CIRCLE, WatchFaceCasioAE1200::colorDotOff);
    lv_obj_set_style_local_border_width(dot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_border_color(dot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, WatchFaceCasioAE1200::colorDotBorder);
    return dot;
  }

  lv_obj_t* CreateImage(const lv_img_dsc_t* src, lv_coord_t x, lv_coord_t y) {
    lv_obj_t* img = lv_img_create(lv_scr_act(), nullptr);
    lv_img_set_src(img, src);
    lv_obj_set_pos(img, x, y);
    return img;
  }

  // Alpha mask image (printed text), drawn in the given colour. LVGL only reads the recolor colour when its
  // opacity is not zero.
  lv_obj_t* CreateMask(const lv_img_dsc_t* src, lv_align_t align, lv_coord_t x, lv_coord_t y, lv_color_t color) {
    lv_obj_t* img = lv_img_create(lv_scr_act(), nullptr);
    lv_img_set_src(img, src);
    lv_obj_set_style_local_image_recolor(img, LV_IMG_PART_MAIN, LV_STATE_DEFAULT, color);
    lv_obj_set_style_local_image_recolor_opa(img, LV_IMG_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_COVER);
    lv_obj_align(img, nullptr, align, x, y);
    return img;
  }

  lv_obj_t* CreateLabel(const lv_font_t* font, lv_color_t color, const char* text, lv_align_t align, lv_coord_t x, lv_coord_t y) {
    lv_obj_t* label = lv_label_create(lv_scr_act(), nullptr);
    lv_obj_set_style_local_text_font(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font);
    lv_obj_set_style_local_text_color(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, color);
    lv_label_set_text_static(label, text);
    lv_obj_align(label, nullptr, align, x, y);
    return label;
  }

  // Segment text with its shadow; the shadow is created first so it ends up below
  lv_obj_t* CreateLcdLabel(const lv_font_t* font, const char* text, lv_align_t align, lv_coord_t x, lv_coord_t y, lv_obj_t** shadow) {
    *shadow = CreateLabel(font, WatchFaceCasioAE1200::colorSegmentShadow, text, align, x + 1, y + 1);
    return CreateLabel(font, WatchFaceCasioAE1200::colorSegment, text, align, x, y);
  }

  lv_obj_t* CreateLine(const lv_point_t* points, lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_color_t color) {
    lv_obj_t* line = lv_line_create(lv_scr_act(), nullptr);
    lv_line_set_points(line, points, 2);
    lv_obj_set_style_local_line_width(line, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, width);
    lv_obj_set_style_local_line_color(line, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, color);
    lv_obj_set_style_local_line_rounded(line, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, width > 2);
    lv_obj_set_pos(line, x, y);
    return line;
  }

  lv_obj_t* CreateHand(lv_coord_t width, bool rounded) {
    lv_obj_t* hand = lv_line_create(lv_scr_act(), nullptr);
    lv_obj_set_style_local_line_width(hand, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, width);
    lv_obj_set_style_local_line_color(hand, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, WatchFaceCasioAE1200::colorSegment);
    lv_obj_set_style_local_line_rounded(hand, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, rounded);
    return hand;
  }

  void SetIndicator(lv_obj_t* indicator, bool active) {
    lv_obj_set_style_local_image_recolor(indicator,
                                         LV_IMG_PART_MAIN,
                                         LV_STATE_DEFAULT,
                                         active ? WatchFaceCasioAE1200::colorSegment : WatchFaceCasioAE1200::colorSegmentOff);
  }

  void SetDotColor(lv_obj_t* dot, bool active, lv_color_t activeColor) {
    lv_obj_set_style_local_bg_color(dot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, active ? activeColor : WatchFaceCasioAE1200::colorDotOff);
  }

  // '!' is a blank of digit width in the segment fonts, used instead of a leading zero
  const char* Blank(uint8_t value) {
    return value < 10 ? "!" : "";
  }
}

WatchFaceCasioAE1200::WatchFaceCasioAE1200(Controllers::DateTime& dateTimeController,
                                           const Controllers::Battery& batteryController,
                                           const Controllers::Ble& bleController,
                                           const Controllers::AlarmController& alarmController,
                                           Controllers::NotificationManager& notificationManager,
                                           Controllers::Settings& settingsController)
  : batteryIcon(false),
    dateTimeController {dateTimeController},
    batteryController {batteryController},
    bleController {bleController},
    alarmController {alarmController},
    notificationManager {notificationManager},
    settingsController {settingsController} {

  // ---- Header ------------------------------------------------------------
  CreateMask(&ae1200_label_alarms, LV_ALIGN_IN_TOP_MID, dialCenterX - 120, 6, colorPrint);
  notificationDot = CreateDot(108, 8);
  CreateMask(&ae1200_label_casio, LV_ALIGN_IN_TOP_MID, 55, 4, colorPrint);

  // ---- Analog dial -------------------------------------------------------
  lv_obj_t* dialTile = CreateRect(4, 27, 104, 104, 18, colorTile);
  lv_obj_set_style_local_border_width(dialTile, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 2);
  lv_obj_set_style_local_border_color(dialTile, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, colorTileBorder);
  CreateImage(&ae1200_dial, dialCenterX - dialImageSize / 2, dialCenterY - dialImageSize / 2);

  minuteHand = CreateHand(3, true);
  hourHand = CreateHand(4, true);
  secondHand = CreateHand(2, false);
  CreateRect(dialCenterX - 12, dialCenterY - 12, 24, 24, LV_RADIUS_CIRCLE, LV_COLOR_BLACK);

  // ---- Indicator panel ---------------------------------------------------
  CreatePanel(114, 27, 122, 46);
  indicatorMute = CreateMask(&ae1200_label_mute, LV_ALIGN_IN_TOP_MID, 55, 32, colorSegmentOff);
  CreateLine(indicatorDividerPoints, 117, 48, 1, colorSegment);
  indicatorAlarm = CreateMask(&ae1200_label_alm, LV_ALIGN_IN_TOP_LEFT, 123, 54, colorSegmentOff);
  indicatorSignal = CreateMask(&ae1200_label_sig, LV_ALIGN_IN_TOP_RIGHT, -13, 54, colorSegmentOff);

  // ---- World map ---------------------------------------------------------
  CreatePanel(114, 77, 122, 54);
  CreateImage(&ae1200_worldmap, mapX, mapY);

  // The land of the local time zone is darker: a window onto a copy of the map where only the land is opaque
  timeZoneHighlight = lv_obj_create(lv_scr_act(), nullptr);
  lv_obj_set_size(timeZoneHighlight, timeZoneHighlightWidth, mapHeight);
  lv_obj_set_style_local_bg_opa(timeZoneHighlight, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
  lv_obj_set_style_local_border_width(timeZoneHighlight, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_style_local_radius(timeZoneHighlight, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
  timeZoneHighlightMap = lv_img_create(timeZoneHighlight, nullptr);
  lv_img_set_src(timeZoneHighlightMap, &ae1200_worldmap_highlight);

  // ---- Main LCD ----------------------------------------------------------
  // The image holds the raised date block, the time area, the curve joining them, their frame, the date
  // divider and the underline. Its corners are plain background, so the model text is drawn on top of it.
  CreateImage(&ae1200_lcd, 3, 134);

  // ---- Model text --------------------------------------------------------
  CreateMask(&ae1200_label_model, LV_ALIGN_IN_TOP_MID, dialCenterX - 120, 137, colorPrintRed);
  CreateMask(&ae1200_label_wr100m, LV_ALIGN_IN_TOP_MID, dialCenterX - 120, 148, colorPrint);
  bleDot = CreateDot(9, 142);

  // The thin date segments get no shadow: on this screen it would look like a doubled text
  labelDayOfWeek = CreateLabel(&lcd_segment_16, colorSegment, "---", LV_ALIGN_IN_TOP_LEFT, 112, 143);
  labelDate = CreateLabel(&lcd_segment_16, colorSegment, "!1-!1", LV_ALIGN_IN_TOP_RIGHT, -11, 143);

  indicatorPm = CreateMask(&ae1200_label_pm, LV_ALIGN_IN_TOP_LEFT, 13, 177, colorSegmentOff);
  labelTime = CreateLcdLabel(&lcd_segment_bold_40, "00:00", LV_ALIGN_IN_TOP_LEFT, 44, 174, &labelTimeShadow);
  labelSeconds = CreateLcdLabel(&lcd_segment_bold_18, "00", LV_ALIGN_IN_TOP_LEFT, 190, 196, &labelSecondsShadow);

  batteryIcon.Create(lv_scr_act());
  batteryIcon.SetColor(colorSegment);
  lv_obj_set_pos(batteryIcon.GetObject(), 16, 196);
  chargingIcon = CreateLabel(&jetbrains_mono_bold_20, colorSegment, Symbols::plug, LV_ALIGN_IN_TOP_LEFT, 30, 196);
  lv_obj_set_hidden(chargingIcon, true);

  // ---- Footer ------------------------------------------------------------
  CreateMask(&ae1200_label_water, LV_ALIGN_IN_TOP_MID, 0, 229, colorPrint);
  CreateLine(footerLinePoints, 26, 233, 1, colorFooterLine);
  CreateLine(footerLinePoints, 184, 233, 1, colorFooterLine);

  taskRefresh = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);
  Refresh();
}

WatchFaceCasioAE1200::~WatchFaceCasioAE1200() {
  lv_task_del(taskRefresh);
  lv_obj_clean(lv_scr_act());
}

void WatchFaceCasioAE1200::UpdateHands(uint8_t hour, uint8_t minute) {
  minutePoints[0] = DialPoint(0, 0);
  minutePoints[1] = DialPoint(minuteHandLength, minute * 6);
  lv_line_set_points(minuteHand, minutePoints, 2);

  hourPoints[0] = DialPoint(0, 0);
  hourPoints[1] = DialPoint(hourHandLength, (hour % 12) * 30 + minute / 2);
  lv_line_set_points(hourHand, hourPoints, 2);
}

void WatchFaceCasioAE1200::UpdateSecondHand(uint8_t second) {
  secondPoints[0] = DialPoint(0, 0);
  secondPoints[1] = DialPoint(secondHandLength, second * 6);
  lv_line_set_points(secondHand, secondPoints, 2);
}

void WatchFaceCasioAE1200::UpdateTimeZoneHighlight(int8_t offset) {
  // UtcOffset() is expressed in quarters of an hour, each one is 3.75 degrees of longitude
  int16_t longitude = offset * 15 / 4;
  if (longitude >= 180) {
    longitude -= 360;
  }
  lv_coord_t center = mapX + (longitude + 180) * mapWidth / 360;
  lv_coord_t x = center - timeZoneHighlightWidth / 2;
  if (x < mapX) {
    x = mapX;
  } else if (x > mapX + mapWidth - timeZoneHighlightWidth) {
    x = mapX + mapWidth - timeZoneHighlightWidth;
  }
  lv_obj_set_pos(timeZoneHighlight, x, mapY);
  lv_obj_set_pos(timeZoneHighlightMap, mapX - x, 0);
}

void WatchFaceCasioAE1200::SetLcdText(lv_obj_t* label, lv_obj_t* shadow, const char* text) {
  lv_label_set_text(label, text);
  lv_label_set_text(shadow, text);
}

void WatchFaceCasioAE1200::Refresh() {
  // ---- Status --------------------------------------------------------------
  notificationState = notificationManager.AreNewNotificationsAvailable();
  if (notificationState.IsUpdated()) {
    SetDotColor(notificationDot, notificationState.Get(), colorNotification);
  }

  bleConnected = bleController.IsConnected();
  if (bleConnected.IsUpdated()) {
    SetDotColor(bleDot, bleConnected.Get(), colorBle);
  }

  muted = settingsController.GetNotificationStatus() != Controllers::Settings::Notification::On;
  if (muted.IsUpdated()) {
    SetIndicator(indicatorMute, muted.Get());
  }

  alarmEnabled = alarmController.IsEnabled();
  if (alarmEnabled.IsUpdated()) {
    SetIndicator(indicatorAlarm, alarmEnabled.Get());
  }

  chimeEnabled = settingsController.GetChimeOption() != Controllers::Settings::ChimesOption::None;
  if (chimeEnabled.IsUpdated()) {
    SetIndicator(indicatorSignal, chimeEnabled.Get());
  }

  batteryPercentRemaining = batteryController.PercentRemaining();
  if (batteryPercentRemaining.IsUpdated()) {
    batteryIcon.SetBatteryPercentage(batteryPercentRemaining.Get());
  }

  powerPresent = batteryController.IsPowerPresent();
  if (powerPresent.IsUpdated()) {
    lv_obj_set_hidden(chargingIcon, !powerPresent.Get());
  }

  utcOffset = dateTimeController.UtcOffset();
  if (utcOffset.IsUpdated()) {
    UpdateTimeZoneHighlight(utcOffset.Get());
  }

  // ---- Time ----------------------------------------------------------------
  currentDateTime = std::chrono::time_point_cast<std::chrono::seconds>(dateTimeController.CurrentDateTime());
  if (!currentDateTime.IsUpdated()) {
    return;
  }
  char text[8];
  uint8_t second = dateTimeController.Seconds();
  snprintf(text, sizeof(text), "%02d", second);
  SetLcdText(labelSeconds, labelSecondsShadow, text);
  UpdateSecondHand(second);

  currentMinute = std::chrono::time_point_cast<std::chrono::minutes>(currentDateTime.Get());
  if (!currentMinute.IsUpdated()) {
    return;
  }
  uint8_t hour = dateTimeController.Hours();
  uint8_t minute = dateTimeController.Minutes();
  bool clock12h = settingsController.GetClockType() == Controllers::Settings::ClockType::H12;

  if (clock12h) {
    SetIndicator(indicatorPm, hour >= 12);
    uint8_t hour12 = hour % 12 == 0 ? 12 : hour % 12;
    snprintf(text, sizeof(text), "%s%d:%02d", Blank(hour12), hour12, minute);
  } else {
    SetIndicator(indicatorPm, false);
    snprintf(text, sizeof(text), "%02d:%02d", hour, minute);
  }
  SetLcdText(labelTime, labelTimeShadow, text);
  UpdateHands(hour, minute);

  currentDate = std::chrono::time_point_cast<std::chrono::days>(currentMinute.Get());
  if (currentDate.IsUpdated()) {
    auto month = static_cast<uint8_t>(dateTimeController.Month());
    uint8_t day = dateTimeController.Day();
    // Month first with the 12 hour clock, like the original watch; day first otherwise
    uint8_t firstField = clock12h ? month : day;
    uint8_t secondField = clock12h ? day : month;
    lv_label_set_text_fmt(labelDate, "%s%d-%s%d", Blank(firstField), firstField, Blank(secondField), secondField);
    lv_label_set_text_static(labelDayOfWeek, dateTimeController.DayOfWeekShortToString());
  }
}
