#include "espnow_handler.h"

#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>

#include "constants.h"
#include "eeprom_handler.h"
#include "logging.h"
#include "states.h"

namespace
{
   uint16_t seqBuffer[kIdempotencyBufferSize];
   uint8_t seqBufferIndex = 0;
   bool seqBufferFull = false;

   volatile bool commandPending = false;
   uint8_t receiveBuffer[kFrameSize];
   Command pendingCommand;

   uint32_t lastHeartbeat = 0;
   uint32_t lastRebroadcast = 0;

   uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
   bool peerAdded = false;

   volatile bool pairingMessagePending = false;
   uint8_t pairingBuffer[8];
   int pairingBufferLen = 0;

   // Non-blocking rebroadcast state
   bool rebroadcastPending = false;
   uint8_t rebroadcastData[kFrameSize];
   uint32_t rebroadcastTime = 0;

   bool MatchesMac(const Command &cmd)
   {
      uint8_t myMac[6];
      WiFi.macAddress(myMac);

      uint8_t cmdMac[6];
      cmdMac[0] = cmd.r;
      cmdMac[1] = cmd.g;
      cmdMac[2] = cmd.b;
      cmdMac[3] = (cmd.speed >> 8) & 0xFF;
      cmdMac[4] = cmd.speed & 0xFF;
      cmdMac[5] = cmd.intensity;

      return memcmp(myMac, cmdMac, 6) == 0;
   }

   void OnDataReceived(const uint8_t *mac, const uint8_t *data, int len)
   {
      if (len >= 1 && IsPairingCommand(data[0]))
      {
         if (len <= 8)
         {
            memcpy(pairingBuffer, data, len);
            pairingBufferLen = len;
            pairingMessagePending = true;
         }
         return;
      }

      if (len != kFrameSize)
      {
         LOGF("Invalid frame size: %d\n", len);
         return;
      }

      memcpy(receiveBuffer, data, kFrameSize);
      commandPending = true;
   }

   void OnDataSent(const uint8_t *mac, esp_now_send_status_t status)
   {
      if (status != ESP_NOW_SEND_SUCCESS)
      {
         LOG("ESP-NOW send failed");
      }
   }

   bool AddBroadcastPeer()
   {
      if (peerAdded)
         return true;

      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, broadcastMac, 6);
      peerInfo.channel = config.channel;
      peerInfo.encrypt = false;

      if (esp_now_add_peer(&peerInfo) != ESP_OK)
      {
         LOG("Failed to add broadcast peer");
         return false;
      }

      peerAdded = true;
      return true;
   }

   void ScheduleRebroadcast(const uint8_t *data, uint8_t ttl)
   {
      // Don't rebroadcast if TTL is 0 or mesh is disabled
      if (ttl == 0 || config.meshTTL == 0)
         return;

      // Don't schedule if another rebroadcast is pending
      if (rebroadcastPending)
         return;

      uint32_t now = millis();
      if (now - lastRebroadcast < kRebroadcastMinGap)
         return;

      // Copy data and decrement TTL
      memcpy(rebroadcastData, data, kFrameSize);
      uint8_t newTTL = ttl - 1;
      uint8_t flags = GetFlags(rebroadcastData[2]);
      rebroadcastData[2] = MakeFlagsByte(newTTL, flags);

      // Schedule for later (non-blocking)
      uint32_t jitter = random(kRebroadcastJitterMax);
      rebroadcastTime = now + jitter;
      rebroadcastPending = true;
   }

   void ProcessPendingRebroadcast()
   {
      if (!rebroadcastPending)
         return;

      if (millis() < rebroadcastTime)
         return;

      SendBroadcast(rebroadcastData, kFrameSize);
      lastRebroadcast = millis();
      rebroadcastPending = false;
   }
}

bool InitializeEspNow()
{
   WiFi.mode(WIFI_STA);
   WiFi.disconnect();
   esp_wifi_set_channel(config.channel, WIFI_SECOND_CHAN_NONE);

   esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
   LOG("ESP-NOW Pure Long Range mode enabled");

   LOGF("WiFi STA mode on channel %u\n", config.channel);
   LOGF("MAC: %s\n", WiFi.macAddress().c_str());

   if (esp_now_init() != ESP_OK)
   {
      LOG("ESP-NOW init failed");
      return false;
   }

   esp_now_register_recv_cb(OnDataReceived);
   esp_now_register_send_cb(OnDataSent);

   if (!AddBroadcastPeer())
   {
      return false;
   }

   ClearKnownSeqs();
   lastHeartbeat = millis();

   LOG("ESP-NOW initialized");
   return true;
}

void ProcessEspNow()
{
   // Process any pending rebroadcast (non-blocking)
   ProcessPendingRebroadcast();

   if (pairingMessagePending)
   {
      pairingMessagePending = false;
      ProcessPairingMessage(pairingBuffer, pairingBufferLen);
   }

   if (!commandPending)
      return;

   commandPending = false;

   pendingCommand = ParseCommand(receiveBuffer);

   bool forceFlag = HasForceFlag(pendingCommand);
   bool isDuplicate = IsKnownSeq(pendingCommand.seq);

   if (isDuplicate && !forceFlag)
   {
      LOGF("Duplicate SEQ %u ignored\n", pendingCommand.seq);
      pendingCommand.effect = Cmd::kNop;
      return;
   }

   AddKnownSeq(pendingCommand.seq);

   if (pendingCommand.effect == Cmd::kPairingAckRecv || pendingCommand.effect == Cmd::kConfigSetRecv)
   {
      if (MatchesMac(pendingCommand))
      {
         LOGF("Pairing message for this device (fx=0x%02X)\n", pendingCommand.effect);

         if (pendingCommand.effect == Cmd::kPairingAckRecv)
         {
            if (!IsPairingActive())
            {
               LOG("Received PAIRING_ACK but not in pairing mode");
            }
            else
            {
               OnPairingAckReceived();
            }
         }
         else if (pendingCommand.effect == Cmd::kConfigSetRecv)
         {
            uint8_t deviceRegister = pendingCommand.length;
            uint16_t ledCount = pendingCommand.duration;
            // Standby color is encoded in: flags = B, groups = (R << 8) | G
            uint8_t standbyR = (pendingCommand.groups >> 8) & 0xFF;
            uint8_t standbyG = pendingCommand.groups & 0xFF;
            uint8_t standbyB = pendingCommand.flags;

            LOGF("CONFIG_SET: register=%u, ledCount=%u, standby=(%u,%u,%u)\n",
                 deviceRegister, ledCount, standbyR, standbyG, standbyB);
            bool success = OnConfigSetReceived(deviceRegister, ledCount, standbyR, standbyG, standbyB);
            SendConfigAck(success);
         }
      }
      else
      {
         LOGF("Pairing message for different MAC (fx=0x%02X)\n", pendingCommand.effect);
      }

      pendingCommand.effect = Cmd::kNop;
      return;
   }

   // Extract TTL from flags byte
   uint8_t ttl = GetTTL(receiveBuffer[2]);

   if (!MatchesGroup(pendingCommand, config.groups))
   {
      LOGF("Group mismatch: cmd=0x%04X my=0x%04X\n", pendingCommand.groups, config.groups);

      if (!HasNoRebroadcastFlag(pendingCommand))
      {
         ScheduleRebroadcast(receiveBuffer, ttl);
      }

      pendingCommand.effect = Cmd::kNop;
      return;
   }

   if (pendingCommand.effect == Cmd::kHeartbeat)
   {
      lastHeartbeat = millis();
      LOGF("Heartbeat received (seq=%u)\n", pendingCommand.seq);
   }

   if (!HasNoRebroadcastFlag(pendingCommand))
   {
      ScheduleRebroadcast(receiveBuffer, ttl);
   }
}

bool SendBroadcast(const uint8_t *data, size_t len)
{
   if (!peerAdded && !AddBroadcastPeer())
   {
      return false;
   }

   esp_err_t result = esp_now_send(broadcastMac, data, len);
   return result == ESP_OK;
}

bool IsKnownSeq(uint16_t seq)
{
   size_t count = seqBufferFull ? kIdempotencyBufferSize : seqBufferIndex;

   for (size_t i = 0; i < count; i++)
   {
      if (seqBuffer[i] == seq)
      {
         return true;
      }
   }

   return false;
}

void AddKnownSeq(uint16_t seq)
{
   seqBuffer[seqBufferIndex] = seq;
   seqBufferIndex = (seqBufferIndex + 1) % kIdempotencyBufferSize;

   if (seqBufferIndex == 0)
   {
      seqBufferFull = true;
   }
}

void ClearKnownSeqs()
{
   seqBufferIndex = 0;
   seqBufferFull = false;
}

Command *GetPendingCommand()
{
   return &pendingCommand;
}

void ClearPendingCommand()
{
   pendingCommand.effect = Cmd::kNop;
}

uint32_t GetLastHeartbeatTime()
{
   return lastHeartbeat;
}

bool IsHeartbeatTimedOut()
{
   return (millis() - lastHeartbeat) > kHeartbeatTimeout;
}

void SendPairingRequest()
{
   uint8_t mac[6];
   WiFi.macAddress(mac);

   uint8_t frame[7];
   frame[0] = Cmd::kPairingRequest;
   memcpy(&frame[1], mac, 6);

   if (SendBroadcast(frame, sizeof(frame)))
   {
      LOGF("Pairing request sent (MAC: %02X:%02X:%02X:%02X:%02X:%02X)\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
   }
   else
   {
      LOG("Failed to send pairing request");
   }
}

void SendConfigAck(bool success)
{
   uint8_t frame[2];
   frame[0] = Cmd::kConfigAck;
   frame[1] = success ? 1 : 0;

   if (SendBroadcast(frame, sizeof(frame)))
   {
      LOGF("Config ACK sent: %s\n", success ? "success" : "failed");
   }
   else
   {
      LOG("Failed to send config ACK");
   }
}

bool ProcessPairingMessage(const uint8_t *data, int len)
{
   if (len < 1)
      return false;

   uint8_t cmd = data[0];

   if (cmd == Cmd::kPairingAckRecv)
   {
      if (!IsPairingActive())
      {
         LOG("Received PAIRING_ACK but not in pairing mode");
         return true;
      }
      OnPairingAckReceived();
      return true;
   }

   if (cmd == Cmd::kConfigSetRecv)
   {
      if (len < 7)
      {
         LOG("CONFIG_SET too short");
         SendConfigAck(false);
         return true;
      }

      uint8_t deviceRegister = data[1];
      uint16_t ledCount = static_cast<uint16_t>(data[2]) | (static_cast<uint16_t>(data[3]) << 8);
      uint8_t standbyR = data[4];
      uint8_t standbyG = data[5];
      uint8_t standbyB = data[6];

      bool success = OnConfigSetReceived(deviceRegister, ledCount, standbyR, standbyG, standbyB);
      SendConfigAck(success);
      return true;
   }

   return false;
}
