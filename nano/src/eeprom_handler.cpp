#include "eeprom_handler.h"
#include "logging.h"

void InitializeEEPROM()
{
   if (!EEPROM.begin(EEPROM_SIZE))
   {
      LOG("Failed to initialize EEPROM");
      return;
   }
   LOG("EEPROM initialized");
}

void SaveLedCount(uint8_t count)
{
   // Save magic number for validation
   EEPROM.write(LED_COUNT_MAGIC_ADDR, LED_COUNT_MAGIC_NUMBER);

   // Save LED count
   EEPROM.write(LED_COUNT_ADDR, count);

   // Commit changes
   EEPROM.commit();

   LOGF("Saved LED count: %d\n", count);
}

uint8_t LoadLedCount()
{
   // Check for valid data
   if (EEPROM.read(LED_COUNT_MAGIC_ADDR) != LED_COUNT_MAGIC_NUMBER)
   {
      LOG("No valid LED count found, using default");
      return 70;
   }

   uint8_t count = EEPROM.read(LED_COUNT_ADDR);
   LOGF("Loaded LED count: %d\n", count);
   return count;
}

void SaveStandbyColors(uint8_t r, uint8_t g, uint8_t b)
{
   // Save magic number for validation
   EEPROM.write(COLOR_MAGIC_ADDR, COLOR_MAGIC_NUMBER);

   // Save RGB values
   EEPROM.write(COLOR_R_ADDR, r);
   EEPROM.write(COLOR_G_ADDR, g);
   EEPROM.write(COLOR_B_ADDR, b);

   // Commit changes
   EEPROM.commit();

   LOGF("Saved standby colors: R=%d, G=%d, B=%d\n", r, g, b);
}

void LoadStandbyColors(uint8_t &r, uint8_t &g, uint8_t &b)
{
   // Check for valid data
   if (EEPROM.read(COLOR_MAGIC_ADDR) != COLOR_MAGIC_NUMBER)
   {
      LOG("No valid colors found, using defaults");
      r = DEFAULT_STANDBY_R;
      g = DEFAULT_STANDBY_G;
      b = DEFAULT_STANDBY_B;
      return;
   }

   r = EEPROM.read(COLOR_R_ADDR);
   g = EEPROM.read(COLOR_G_ADDR);
   b = EEPROM.read(COLOR_B_ADDR);
}
