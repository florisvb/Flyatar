/*
  LUFA Library
  Copyright (C) Dean Camera, 2009.

  dean [at] fourwalledcubicle [dot] com
  www.fourwalledcubicle.com
*/

/*
  Copyright 2009  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, and distribute this software
  and its documentation for any purpose and without fee is hereby
  granted, provided that the above copyright notice appear in all
  copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Header file for StageUSBDevice.c.
 */

#ifndef _STAGEUSBDEVICE_H_
#define _STAGEUSBDEVICE_H_

/* Includes: */
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <stdbool.h>
#include <string.h>

#include "Descriptors.h"

#include <LUFA/Version.h>                    // Library Version Information
#include <LUFA/Scheduler/Scheduler.h>        // Simple scheduler for task management
#include <LUFA/Drivers/USB/USB.h>            // USB Functionality
#include <LUFA/Drivers/Board/LEDs.h>         // LEDs driver

/* Macros: */
/* USB Commands */
#define USB_CMD_GET_STATE       1
#define USB_CMD_SET_STATE       2
#define USB_CMD_AVR_RESET       200
#define USB_CMD_AVR_DFU_MODE    201

/* Motor Parameter Values */
#define MOTOR_0_POSITION_HOME 1000
#define MOTOR_1_POSITION_HOME 1000
#define MOTOR_2_POSITION_HOME 1234
#define MOTOR_0_FREQUENCY_MAX 50000
#define MOTOR_1_FREQUENCY_MAX 50000
#define MOTOR_2_FREQUENCY_MAX 50000
#define MOTOR_0_DIRECTION_POS 0
#define MOTOR_0_DIRECTION_NEG 1
#define MOTOR_1_DIRECTION_POS 0
#define MOTOR_1_DIRECTION_NEG 1
#define MOTOR_2_DIRECTION_POS 0
#define MOTOR_2_DIRECTION_NEG 1
#define MOTOR_0_TIMER         1
#define MOTOR_1_TIMER         3
#define MOTOR_2_TIMER         2
#define MOTOR_0_INTERRUPT     TIMER1_OVF_vect
#define MOTOR_1_INTERRUPT     TIMER3_OVF_vect
#define MOTOR_2_INTERRUPT     TIMER2_OVF_vect

/* Software reset */
#define AVR_RESET() wdt_enable(WDTO_30MS); while(1) {}
#define AVR_IS_WDT_RESET()  ((MCUSR&(1<<WDRF)) ? 1:0)
#define DFU_BOOT_KEY_VAL 0xAA55AA55
#define MOTOR_NUM 3
#define TIMER_NUM 4
#define PRESCALER_NUM 5

/* Type Defines: */
typedef struct
{
  uint8_t     Timer;
  volatile uint8_t     *DirectionPort;
  uint8_t     DirectionPin;
  uint16_t    Frequency;
  uint16_t    FrequencyMax;
  uint8_t     Direction;
  uint8_t     DirectionPos;
  uint8_t     DirectionNeg;
  uint16_t    Position;
  uint16_t    PositionSetPoint;
  uint8_t     Update;
} MotorWrapper_t;

typedef struct
{
  struct
  {
    volatile uint16_t    *TOP;
    volatile uint8_t     *ClockSelect;
    volatile uint8_t     *PinPort;
  } Address;
  uint8_t     OutputPin;
  uint8_t     ClockSelect[PRESCALER_NUM + 1];
  uint8_t     Prescaler_N;
  uint8_t     ScaleFactor;
  uint16_t    TOPValue;
  uint16_t    TOPMax;
  uint8_t     OnOff;
} TimerWrapper_t;

typedef struct
{
  uint16_t   Frequency;
  uint16_t   Position;
} MotorStatus_t;

typedef struct
{
  uint8_t       CommandID;
  uint8_t       MotorUpdate;
  MotorStatus_t Setpoint[MOTOR_NUM];
} USBPacketOutWrapper_t;

typedef struct
{
  uint8_t       CommandID;
  MotorStatus_t MotorStatus[MOTOR_NUM];
} USBPacketInWrapper_t;

/* Enums: */
/** Enum for the possible status codes for passing to the UpdateStatus() function. */
enum USB_StatusCodes_t
  {
    Status_USBNotReady      = 0, /**< USB is not ready (disconnected from a USB host) */
    Status_USBEnumerating   = 1, /**< USB interface is enumerating */
    Status_USBReady         = 2, /**< USB interface is connected and ready */
    Status_ProcessingPacket = 3, /**< Processing packet */
  };

/* Global Variables: */
const  uint16_t         PrescalerArray16[PRESCALER_NUM] = {1, 8, 64, 256, 1024};
const  uint16_t         PrescalerArray8[PRESCALER_NUM] = {1, 8, 32, 64, 128};
MotorWrapper_t          Motor[MOTOR_NUM];
TimerWrapper_t          Timer[TIMER_NUM];
USBPacketOutWrapper_t   USBPacketOut;
USBPacketInWrapper_t    USBPacketIn;
uint8_t                 IO_Enabled=0;

/* Task Definitions: */
TASK(USB_ProcessPacket);

/* Function Prototypes: */
void EVENT_USB_Connect(void);
void EVENT_USB_Disconnect(void);
void EVENT_USB_ConfigurationChanged(void);
void EVENT_USB_UnhandledControlPacket(void);

void UpdateStatus(uint8_t CurrentStatus);

#if defined(INCLUDE_FROM_STAGEUSBDEVICE_C)
static void USBPacket_Read(void);
static void USBPacket_Write(void);
static void IO_Init(void);
static void IO_Disconnect(void);
static void Timer_Init(void);
static void Timer_On(uint8_t Timer_N);
static void Timer_Off(uint8_t Timer_N);
static void Motor_Init(void);
static void Motor_Update(uint8_t Motor_N);
static void Motor_Update_All(void);
//static void Position_Update(volatile uint8_t Motor_N);
#endif

#endif
