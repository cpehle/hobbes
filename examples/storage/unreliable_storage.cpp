#include <hobbes/storage.H>
#include <stdlib.h>

DEFINE_STORAGE_GROUP(
  WeatherMonitor,              /* an arbitrary name for our data set */
  3000,                        /* our maximum expected throughput in system pages (if we record up to this limit, we either drop or block) */
  hobbes::storage::Unreliable, /* we'd rather drop than block */
  hobbes::storage::AutoCommit  /* we don't need to correlate multiple records in a batch (non-transactional) */
);

// a set of sensor readings
DEFINE_HSTORE_STRUCT(
  Sensor,
  (double, temp),
  (double, humidity)
);

int main() {
  while (true) {
    // record a random sensor reading from somewhere
    const char* locations[] = { "AZ", "CO", "HI" };
    const char* location    = locations[rand() % 3];

    Sensor s;
    s.temp     = 1.0 + 50.0 * ((double)(rand() % 100)) / 100.0;
    s.humidity = ((double)(rand() % 100)) / 100.0;

    HSTORE(WeatherMonitor, sensor, location, s);

    // sometimes record a passing car
    if (rand() % 100 == 0) {
      HSTORE(WeatherMonitor, carPassed, location);
    }
  }
  return 0;
}

