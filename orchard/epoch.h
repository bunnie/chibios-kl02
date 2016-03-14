typedef struct {
  uint8_t second; // 0-59
  uint8_t minute; // 0-59
  uint8_t hour;   // 0-23
  uint8_t day;    // 1-31
  uint8_t month;  // 1-12
  uint8_t year;   // 0-99 (representing 2000-2099)
} date_time_t;

uint32_t date_time_to_epoch(date_time_t* date_time);
void epoch_to_date_time(date_time_t* date_time, uint32_t epoch);

