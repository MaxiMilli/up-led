#include "command.h"

#include "logging.h"

Command ParseCommand(const uint8_t *buffer)
{
  Command cmd;

  cmd.seq = (static_cast<uint16_t>(buffer[0]) << 8) | buffer[1];
  cmd.flags = buffer[2];
  cmd.effect = buffer[3];
  cmd.groups = (static_cast<uint16_t>(buffer[4]) << 8) | buffer[5];
  cmd.duration = (static_cast<uint16_t>(buffer[6]) << 8) | buffer[7];
  cmd.length = buffer[8];
  cmd.rainbow = buffer[9];
  cmd.r = buffer[10];
  cmd.g = buffer[11];
  cmd.b = buffer[12];
  cmd.speed = (static_cast<uint16_t>(buffer[13]) << 8) | buffer[14];
  cmd.intensity = buffer[15];

  LOGF("CMD seq=%u fx=0x%02X grp=0x%04X dur=%u rgb=%u,%u,%u spd=%u int=%u\n",
       cmd.seq, cmd.effect, cmd.groups, cmd.duration,
       cmd.r, cmd.g, cmd.b, cmd.speed, cmd.intensity);

  return cmd;
}

bool MatchesGroup(const Command &cmd, uint16_t myGroups)
{
  return (cmd.groups & myGroups) != 0;
}
