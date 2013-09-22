//GMCWipers class
// GMC motorhome Wiper motor controller
// This code runs a RC servo to control the Hydralic wipers on a GMC motorhome
//these wipers are variable speed, normally controlled via a cable
// the servo replaces the cable to provide both variable speed and
// intermittent operation.
//
// There are 3 inputs to the system;
// On/Off switch
// intermmitent enable switch
// Wipers parked switch
// delay potometer
//
// Copyright 2013 Keith Vasilakes
/*
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <Arduino.h> 
#include "GMCWipers.h"

static enum IStates_e {iSTATE_PARK, iSTATE_PARK_WAIT, iSTATE_DELAY, iSTATE_UNPARK, iSTATE_UNPARK_WAIT};

static enum MStates_e {mSTATE_SET, mSTATE_UNPARK, mSTATE_COUNT, mSTATE_WAIT, mSTATE_PARK};

GMCWipers::GMCWipers() :
potpin(0),
parkPin(12),
modePin(11),
servoPin(10),
mistPin(9),
DEADBAND(10),
SPEED_OFF(90),
UNPARK(150),
SPEED_MEDIUM(130),
SPEED_HIGH(179),
MISTSWEEPS(3)
{

}

//return true if it's been period ms since start
bool GMCWipers::IsTimedOut(unsigned long period, unsigned long start)
{
  unsigned long current = millis();
  return((current - start) > period);
}

void GMCWipers::Init()
{
  myservo.write(SPEED_OFF); //wipers off
  myservo.attach(servoPin);  // attaches the servo on pin 9 to the servo object

  pinMode(modePin, INPUT);
  pinMode(mistPin, INPUT);
  pinMode(parkPin, INPUT);

  digitalWrite(modePin, HIGH);       // turn on pullup resistors
  digitalWrite(mistPin, HIGH);       // turn on pullup resistors
  digitalWrite(parkPin, HIGH);       // turn on pullup resistors

  WipersMist.attach(mistPin);
  WiperMode.attach(modePin);
  WipersParked.attach(parkPin);

  Serial.println("GMC Wipers Init");
}

void GMCWipers::WiperSpeed(int val)
{
  val = map(val, DEADBAND, 1023, SPEED_OFF, SPEED_HIGH);     // scale it to use it with the servo
  myservo.write(val);                  // sets the servo position according to the scaled value
  delay(15);                           // waits for the servo to get there
}

void GMCWipers::WipersIntermitent(int val)
{
  unsigned long wipedelay;

  //Start by parking the wipers in case they are
  //already in motion
  static IStates_e state = iSTATE_PARK;

  switch(state)
  {
    case iSTATE_PARK:
      //turn off and let them park
      //about 3 seconds wiper sweep time
      myservo.write(SPEED_OFF);
      Serial.println("parking");
      state = iSTATE_PARK_WAIT;
      StartTime = millis();
      break;
    case iSTATE_PARK_WAIT:
      //...wait for park switch, or 5 seconds    
      if((LOW == WipersParked) || IsTimedOut(5000, StartTime))
      {
        StartTime = millis();
        Serial.println("Delaying");
        state = iSTATE_DELAY;
      }
      break;
    case iSTATE_DELAY:
      //wait according to pot value 6 seconds to 1/4 second between sweeps
      wipedelay = map(val, 0, 1023, 6000, 250);

      if(IsTimedOut(wipedelay, StartTime))
      {
        state = iSTATE_UNPARK;
      }
      break;
    case iSTATE_UNPARK:
      //unpark wipers so they make a sweep
      myservo.write(UNPARK);
      Serial.println("Unparking");
      state = iSTATE_UNPARK_WAIT;
      StartTime = millis();
      break;
    case iSTATE_UNPARK_WAIT:
      if(IsTimedOut(1000, StartTime))
      {
        state = iSTATE_PARK;
      }
      break;
    default:
      state = iSTATE_PARK;
  }
}

void GMCWipers::WipersMisting()
{
  static MStates_e state = mSTATE_SET;
  static int sweeps;

  switch(state)
  {
    //turn on the wipers
    case mSTATE_SET:
      myservo.write(SPEED_MEDIUM); //kinda medium speed
      sweeps = 0;
      StartTime = millis();
      state = mSTATE_UNPARK;
      break;
      
    case mSTATE_UNPARK:
      if( IsTimedOut(1000, StartTime))
      {
        state = mSTATE_COUNT;
      }
      break;
    //count sweeps
    case mSTATE_COUNT:
      if(LOW == WipersParked)
      {
        if(++sweeps > MISTSWEEPS)
        {
          state  = mSTATE_WAIT;
          StartTime = millis();
        }
        else
        {
          state = mSTATE_UNPARK;
        }
      }
      break;
      
    //Wait for the wipers to get the last sweep started
    case mSTATE_WAIT:
      if( IsTimedOut(1000, StartTime))
      {
        myservo.write(SPEED_OFF);
        state = mSTATE_PARK;
      }
      break;
      
    //done, back to normal operation
    case mSTATE_PARK:
      if(LOW == WipersParked)
      {
        misting = false;
        state = mSTATE_SET;
      }
      break;
  }
}

void GMCWipers::WipersDo()
{
    val = analogRead(potpin);
     
    //latch misting low on washer squirt
    //but not in speed mode
    if(LOW == WipersMist)
    {
        if(LOW == WiperMode)
		{
			//in speed mode but off
			if(val < DEADBAND)
			{
				misting = true;
			}
        }
		else //in intermitent mode switch to misting mode
		{
			misting = true;
		}
		else //in intermitent mode switch to misting mode
		{
			misting = true;
		}
    } 
    
    //if switch is in run mode speed follows pot
    if(0) //LOW == WiperMode)
    {
       WiperSpeed(val);
    }
    else if(misting)
    {
      WipersMisting();
    }
    else //pot controls time between sweeps
    {  
      WipersIntermitent(val);
    }
}

void GMCWipers::Update()
{
  WipersMist.Update();
  WiperMode.Update();
  WipersParked.Update();

  WipersDo();
}
