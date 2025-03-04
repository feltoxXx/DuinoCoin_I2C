/*
  DuinoCoin_Wire.ino
  created 10 05 2021
  by Luiz H. Cassettari

  Some changes to get a good performance in H/s. 
  Modified by feltoxxx
  01/03/2025
*/

#include <Wire.h>

//T-Display S3
#define SDA 43
#define SCL 44
// ESP32 WROOM
// #define SDA 21
// #define SCL 22



#define WIRE_CLOCK 400000
#define WIRE_MAX 30
#define WIRE_MIN 8

void wire_setup()
{
  wire_start();
  wire_readAll();
}

void wire_start()
{
  Wire.begin(SDA, SCL);
  Wire.setClock(WIRE_CLOCK);
                 
}

void wire_readAll()
{
  for (byte address = WIRE_MIN; address < WIRE_MAX; address++ )
  {
    if (wire_exists(address))
    {
      Serial.print("Address: ");
      Serial.println(address);
      wire_readLine(address);
    }
  }
}

void wire_SendAll(String message)
{
  for (byte address = WIRE_MIN; address < WIRE_MAX; address++ )
  {
    if (wire_exists(address))
    {
      Serial.print("Address: ");
      Serial.println(address);
      Wire_sendln(address, message);
    }
  }
}

boolean wire_exists(byte address)
{
  //wire_start();
  Wire.beginTransmission(address);
  byte error = Wire.endTransmission();
  return (error == 0);
}

void wire_sendJob(byte address, String lastblockhash, String newblockhash, int difficulty)
{
  String job = lastblockhash + "," + newblockhash + "," + difficulty;
  Wire_sendln(address, job);
}

void Wire_sendln(byte address, String message)
{
  Wire_send(address, message + "\n");
}

void Wire_sendCmd(byte address, String message)
{
  Wire_send(address, message + '$');
}

void Wire_send(byte address, String message)
{
  //wire_start();
  
  int i=0;
  while (i < message.length())
  {
    if (wire_runEveryMicro(400)) {
      Wire.beginTransmission(address);
      for (int j=0; j < REPEATED_WIRE_SEND_COUNT; j++) {
        Wire.write(message.charAt(i));
      }
      Wire.endTransmission();
      i++;
    }
  }
}

String wire_readLine(int address)
{
  wire_runEvery(0);
  char end = '\n';
  String str = "";
  boolean done = false;
  //wire_start();
  while (!done)
  {
    Wire.requestFrom(address, 1);
    if (Wire.available())
    {
      char c = Wire.read();
      //Serial.print(c);
      if (c == end)
      {
        done = true;
        break;
      }
      str += c;
    }
    if (wire_runEvery(2000)) break;
  }
  //str += end;
  return str;
}

boolean wire_runEvery(unsigned long interval)
{
  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}

boolean wire_runEveryMicro(unsigned long interval)
{
  static unsigned long previousMicros = 0;
  unsigned long currentMicros = micros();
  if (currentMicros - previousMicros >= interval)
  {
    previousMicros = currentMicros;
    return true;
  }
  return false;
}
