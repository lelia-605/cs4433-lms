#include "lms/domain/Time.hpp"

#include <cstdio>

#include "lms/Errors.hpp"

namespace lms {

namespace {

// Days since 1970-01-01 for a civil date (proleptic Gregorian calendar).
long long daysFromCivil(long long y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<long long>(doe) - 719468;
}

void civilFromDays(long long z, long long& y, unsigned& m, unsigned& d) {
  z += 719468;
  const long long era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  y = static_cast<long long>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  y += m <= 2;
}

}  // namespace

std::string toIso(TimePoint tp) {
  using namespace std::chrono;
  const long long secs = duration_cast<seconds>(tp.time_since_epoch()).count();
  long long days = secs / 86400;
  long long rem = secs % 86400;
  if (rem < 0) { rem += 86400; --days; }
  long long y; unsigned m, d;
  civilFromDays(days, y, m, d);
  char buf[64];  // room for any year, so the compiler cannot warn about truncation
  std::snprintf(buf, sizeof buf, "%04lld-%02u-%02uT%02lld:%02lld:%02lldZ", y, m, d,
                rem / 3600, (rem % 3600) / 60, rem % 60);
  return buf;
}

TimePoint fromIso(const std::string& text) {
  int y, mo, d, h, mi, s;
  char tail = 0;
  if (std::sscanf(text.c_str(), "%d-%d-%dT%d:%d:%d%c", &y, &mo, &d, &h, &mi, &s, &tail) != 7 || tail != 'Z')
    throw RuleViolation("invalid date and time: " + text);
  if (mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 || s > 59)
    throw RuleViolation("invalid date and time: " + text);
  const long long days = daysFromCivil(y, static_cast<unsigned>(mo), static_cast<unsigned>(d));
  const TimePoint tp = TimePoint{} + std::chrono::seconds(days * 86400 + h * 3600 + mi * 60 + s);
  if (toIso(tp) != text) throw RuleViolation("invalid date and time: " + text);  // rejects e.g. 30 February
  return tp;
}

}  // namespace lms
