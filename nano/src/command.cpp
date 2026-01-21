#include "command.h"

#include "logging.h"

Command DecodeCommand(uint8_t *message_buffer)
{
  Command message;
  message.effect = message_buffer[0];
  message.duration = (message_buffer[1] << 8) | message_buffer[2];
  message.intensity = message_buffer[3];
  message.red = message_buffer[4];
  message.green = message_buffer[5];
  message.blue = message_buffer[6];
  message.rainbow = message_buffer[7];
  message.speed = (message_buffer[8] << 8) | message_buffer[9];
  message.length = message_buffer[10];

  // Print out the decoded values
  LOG("Decoded Command:");
  LOGF("  Effect: %d\n", message.effect);
  LOGF("  Duration: %d\n", message.duration);
  LOGF("  Intensity: %d\n", message.intensity);
  LOGF("  Red: %d\n", message.red);
  LOGF("  Green: %d\n", message.green);
  LOGF("  Blue: %d\n", message.blue);
  LOGF("  Rainbow: %d\n", message.rainbow);
  LOGF("  Speed: %d\n", message.speed);
  LOGF("  Length: %d\n", message.length);

  return message;
}

bool IsLedEffect(Command command)
{
  return (command.effect >= 20 && command.effect <= 49) || command.effect == 100 ||
         (command.effect >= 103 && command.effect <= 109);
}
