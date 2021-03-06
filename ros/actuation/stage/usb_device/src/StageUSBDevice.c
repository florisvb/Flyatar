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
 *  Main source file for the GenericHID demo. This file contains the main tasks of the demo and
 *  is responsible for the initial application hardware configuration.
 */

#define INCLUDE_FROM_STAGEUSBDEVICE_C
#include "StageUSBDevice.h"

/* Scheduler Task List */
TASK_LIST
{
  { .Task = USB_USBTask          , .TaskStatus = TASK_STOP },
    { .Task = USB_ProcessPacket    , .TaskStatus = TASK_STOP },
      };

/* DFU Bootloader Declarations */
uint32_t  boot_key __attribute__ ((section (".noinit")));
typedef void (*AppPtr_t)(void) __attribute__ ((noreturn));
AppPtr_t Bootloader = (AppPtr_t)0xf000;

/** Main program entry point. This routine configures the hardware required by the application, then
 *  starts the scheduler to run the USB management task.
 */
int main(void)
{
  /* After reset start bootloader? */
  if ((AVR_IS_WDT_RESET()) && (boot_key == DFU_BOOT_KEY_VAL))
    {
      boot_key = 0;
      Bootloader();
    }

  /* Disable watchdog if enabled by bootloader/fuses */
  MCUSR &= ~(1 << WDRF);
  wdt_disable();

  /* Disable clock division */
  clock_prescale_set(clock_div_1);

  /* Hardware Initialization */
  LEDs_Init();

  /* Indicate USB not ready */
  UpdateStatus(Status_USBNotReady);

  /* Initialize Scheduler so that it can be used */
  Scheduler_Init();

  /* Initialize USB Subsystem */
  USB_Init();

  /* Initialize I/O lines */
  //IO_Init();

  /* Initialize Timers */
  Timer_Init();

  /* Initialize Motors */
  Motor_Init();

  /* Scheduling - routine never returns, so put this last in the main function */
  Scheduler_Start();
}

/** Event handler for the USB_Connect event. This indicates that the device is enumerating via the status LEDs and
 *  starts the library USB task to begin the enumeration and USB management process.
 */
void EVENT_USB_Connect(void)
{
  /* Start USB management task */
  Scheduler_SetTaskMode(USB_USBTask, TASK_RUN);

  /* Indicate USB enumerating */
  UpdateStatus(Status_USBEnumerating);
}

/** Event handler for the USB_Disconnect event. This indicates that the device is no longer connected to a host via
 *  the status LEDs and stops the USB management task.
 */
void EVENT_USB_Disconnect(void)
{
  /* Stop running HID reporting and USB management tasks */
  Scheduler_SetTaskMode(USB_ProcessPacket, TASK_STOP);
  Scheduler_SetTaskMode(USB_USBTask, TASK_STOP);

  /* Indicate USB not ready */
  UpdateStatus(Status_USBNotReady);
}

/** Event handler for the USB_ConfigurationChanged event. This is fired when the host sets the current configuration
 *  of the USB device after enumeration, and configures the generic HID device endpoints.
 */
void EVENT_USB_ConfigurationChanged(void)
{
  /* Setup USB In and Out Endpoints */
  Endpoint_ConfigureEndpoint(OUT_EPNUM, EP_TYPE_BULK,
                             ENDPOINT_DIR_OUT, OUT_EPSIZE,
                             ENDPOINT_BANK_SINGLE);

  Endpoint_ConfigureEndpoint(IN_EPNUM, EP_TYPE_BULK,
                             ENDPOINT_DIR_IN, IN_EPSIZE,
                             ENDPOINT_BANK_SINGLE);

  /* Indicate USB connected and ready */
  UpdateStatus(Status_USBReady);

  /* Start ProcessPacket task */
  Scheduler_SetTaskMode(USB_ProcessPacket, TASK_RUN);
}

/** Function to manage status updates to the user. This is done via LEDs on the given board, if available, but may be changed to
 *  log to a serial port, or anything else that is suitable for status updates.
 */
void UpdateStatus(uint8_t CurrentStatus)
{
  uint8_t LEDMask = LEDS_NO_LEDS;

  /* Set the LED mask to the appropriate LED mask based on the given status code */
  switch (CurrentStatus)
    {
    case Status_USBNotReady:
      LEDMask = (LEDS_LED1);
      break;
    case Status_USBEnumerating:
      LEDMask = (LEDS_LED1 | LEDS_LED2);
      break;
    case Status_USBReady:
      LEDMask = (LEDS_LED2 | LEDS_LED4);
      break;
    case Status_ProcessingPacket:
      LEDMask = (LEDS_LED1 | LEDS_LED2);
      break;
    }

  /* Set the board LEDs to the new LED mask */
  LEDs_SetAllLEDs(LEDMask);
}

TASK(USB_ProcessPacket)
{
  /* Check if the USB System is connected to a Host */
  if (USB_IsConnected)
    {
      /* Select the Data Out Endpoint */
      Endpoint_SelectEndpoint(OUT_EPNUM);

      /* Check if OUT Endpoint contains a packet */
      if (Endpoint_IsOUTReceived())
        {
          /* Check to see if a command from the host has been issued */
          if (Endpoint_IsReadWriteAllowed())
            {
              /* Indicate busy */
              UpdateStatus(Status_ProcessingPacket);

              /* Read USB packet from the host */
              USBPacket_Read();

              /* Return the same CommandID that was received */
              USBPacketIn.CommandID = USBPacketOut.CommandID;

              /* Process USB packet */
              switch (USBPacketOut.CommandID)
                {
                case USB_CMD_AVR_RESET:
                  {
                    USBPacket_Write();
                    AVR_RESET();
                  }
                  break;
                case USB_CMD_AVR_DFU_MODE:
                  {
                    USBPacket_Write();
                    boot_key = DFU_BOOT_KEY_VAL;
                    AVR_RESET();
                  }
                  break;
                case USB_CMD_GET_STATE:
                  {
                  }
                  break;
                case USB_CMD_SET_STATE:
                  {
                      if (!IO_Enabled)
                      {
                      IO_Init();
                      }
                    for ( uint8_t Motor_N=0; Motor_N<MOTOR_NUM; Motor_N++ )
                      {
                        Motor[Motor_N].Update = (USBPacketOut.MotorUpdate & (1<<Motor_N));
                        if (Motor[Motor_N].Update)
                          {
                            ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
                            {
                              if (USBPacketOut.Setpoint[Motor_N].Frequency > Motor[Motor_N].FrequencyMax)
                                {
                                  Motor[Motor_N].Frequency = Motor[Motor_N].FrequencyMax;
                                }
                              else
                                {
                                  Motor[Motor_N].Frequency = USBPacketOut.Setpoint[Motor_N].Frequency;
                                }
                              Motor[Motor_N].PositionSetPoint = USBPacketOut.Setpoint[Motor_N].Position;
                              if (Motor[Motor_N].PositionSetPoint > Motor[Motor_N].Position)
                                {
                                  Motor[Motor_N].Direction = Motor[Motor_N].DirectionPos;
                                }
                              else if (Motor[Motor_N].PositionSetPoint < Motor[Motor_N].Position)
                                {
                                  Motor[Motor_N].Direction = Motor[Motor_N].DirectionNeg;
                                }
                              else
                                {
                                  Motor[Motor_N].Frequency = 0;
                                }
                            }
                          }
                      }
                      Motor_Update_All();
                  }
                  break;
                default:
                  {
                  }
                }

              /* Write the return USB packet */
              for ( uint8_t Motor_N=0; Motor_N<MOTOR_NUM; Motor_N++ )
                {
                  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
                  {
                    USBPacketIn.MotorStatus[Motor_N].Frequency = Motor[Motor_N].Frequency;
                    USBPacketIn.MotorStatus[Motor_N].Position = Motor[Motor_N].Position;
                  }
                }
              USBPacket_Write();

              /* Indicate ready */
              LEDs_SetAllLEDs(LEDS_LED2 | LEDS_LED4);
            }
        }
    }
}

static void USBPacket_Read(void)
{
  uint8_t* USBPacketOutPtr = (uint8_t*)&USBPacketOut;

  /* Select the Data Out endpoint */
  Endpoint_SelectEndpoint(OUT_EPNUM);

  /* Read in USB packet header */
  Endpoint_Read_Stream_LE(USBPacketOutPtr, sizeof(USBPacketOut));

  /* Finalize the stream transfer to send the last packet */
  Endpoint_ClearOUT();
}

static void USBPacket_Write(void)
{
  uint8_t* USBPacketInPtr = (uint8_t*)&USBPacketIn;

  /* Select the Data In endpoint */
  Endpoint_SelectEndpoint(IN_EPNUM);

  /* Wait until read/write to IN data endpoint allowed */
  while (!(Endpoint_IsReadWriteAllowed() && Endpoint_IsINReady()));

  /* Write the return data to the endpoint */
  Endpoint_Write_Stream_LE(USBPacketInPtr, sizeof(USBPacketIn));

  /* Finalize the stream transfer to send the last packet */
  Endpoint_ClearIN();
}

static void IO_Init(void)
{
  /* Input lines initialization */


  /* Output lines initialization */

  /* Set data direction of Timer 0 PWM output A to output (PORTB pin 7) */
  DDRB |= (1<<DDB7);

  /* Set data direction of Timer 1 PWM output A to output (PORTB pin 5) */
  DDRB |= (1<<DDB5);

  /* Set data direction of Timer 2 PWM output A to output (PORTB pin 4) */
  DDRB |= (1<<DDB4);

  /* Set data direction of Timer 3 PWM output A to output (PORTC pin 6) */
  DDRC |= (1<<DDC6);

  /* Set data direction of Direction pins to output (PORTC pin 0:2) */
  DDRC |= ((1<<DDC0) | (1<<DDC1) | (1<<DDC2));

  /* Set Timer PWM and Direction pins low to start (PORTB pins 4,5,7 PORTC pin 0:2,6) */
  PORTB &= ~((1<<PB4) | (1<<PB5) | (1<<PB7));
  PORTC &= ~((1<<PC0) | (1<<PC1) | (1<<PC2) | (1<<PC6));

  IO_Enabled = 1;
}

static void Timer_Init(void)
{
  /* Timers hardware initialization */

  /* Toggle OC0A on compare match, OC0B disconnected */
  /* Set Phase Correct PWM Mode on Timer 0 */
  /* Top @ OCR0A, Update of OCR0x @ TOP, TOV0 Flag Set on BOTTOM */
  TCCR0A = ((1<<COM0A0) | (1<<WGM00));
  TCCR0B = (1<<WGM02);

  /* Toggle OC1A on compare match, OC1B/OC1C disconnected */
  /* Set Phase and Frequency Correct PWM Mode on Timer 1 */
  /* Top @ OCR1A, Update of OCR1x @ BOTTOM, TOV1 Flag Set on BOTTOM */
  TCCR1A = ((1<<COM1A0) | (1<<COM1B0) | (1<<COM1C0) | (1<<WGM10));
  TCCR1B = (1<<WGM13);

  /* Toggle OC2A on compare match, OC2B disconnected */
  /* Set Phase Correct PWM Mode on Timer 2 */
  /* Top @ OCR2A, Update of OCR2x @ TOP, TOV2 Flag Set on BOTTOM */
  TCCR2A = ((1<<COM2A0) | (1<<WGM20));
  TCCR2B = (1<<WGM22);

  /* Toggle OC3A on compare match, OC3B/OC3C disconnected */
  /* Set Phase and Frequency Correct PWM Mode on Timer 3 */
  /* Top @ OCR3A, Update of OCR3x @ BOTTOM, TOV3 Flag Set on BOTTOM */
  TCCR3A = ((1<<COM3A0) | (1<<COM3B0) | (1<<COM3C0) | (1<<WGM30));
  TCCR3B = (1<<WGM33);

  /* Enable Timer Interrupts */
  TIMSK1 = (1<<TOIE1);
  TIMSK2 = (1<<TOIE2);
  TIMSK3 = (1<<TOIE3);

  /* Store Timer Addresses used for setting Timer frequency and prescaler */
  Timer[0].Address.TOP = (uint16_t*)&OCR0A;
  Timer[0].Address.ClockSelect = &TCCR0B;
  Timer[0].Address.PinPort = &PINB;
  Timer[0].OutputPin = DDB7;

  Timer[1].Address.TOP = &OCR1A;
  Timer[1].Address.ClockSelect = &TCCR1B;
  Timer[1].Address.PinPort = &PINB;
  Timer[1].OutputPin = DDB5;

  Timer[2].Address.TOP = (uint16_t*)&OCR2A;
  Timer[2].Address.ClockSelect = &TCCR2B;
  Timer[2].Address.PinPort = &PINB;
  Timer[2].OutputPin = DDB4;

  Timer[3].Address.TOP = &OCR3A;
  Timer[3].Address.ClockSelect = &TCCR3B;
  Timer[3].Address.PinPort = &PINC;
  Timer[3].OutputPin = DDC6;

  /* Store ClockSelect Values for each Prescaler_N */
  Timer[0].ClockSelect[0] = (1<<CS00);
  Timer[0].ClockSelect[1] = (1<<CS01);
  Timer[0].ClockSelect[2] = ((1<<CS01)|(1<<CS00));
  Timer[0].ClockSelect[3] = (1<<CS02);
  Timer[0].ClockSelect[4] = ((1<<CS02)|(1<<CS00));
  Timer[0].ClockSelect[5] = ~((1<<CS02)|(1<<CS01)|(1<<CS00));

  Timer[1].ClockSelect[0] = (1<<CS10);
  Timer[1].ClockSelect[1] = (1<<CS11);
  Timer[1].ClockSelect[2] = ((1<<CS11)|(1<<CS10));
  Timer[1].ClockSelect[3] = (1<<CS12);
  Timer[1].ClockSelect[4] = ((1<<CS12)|(1<<CS10));
  Timer[1].ClockSelect[5] = ~((1<<CS12)|(1<<CS11)|(1<<CS10));

  Timer[2].ClockSelect[0] = (1<<CS20);
  Timer[2].ClockSelect[1] = (1<<CS21);
  Timer[2].ClockSelect[2] = ((1<<CS21)|(1<<CS20));
  Timer[2].ClockSelect[3] = (1<<CS22);
  Timer[2].ClockSelect[4] = ((1<<CS22)|(1<<CS20));
  Timer[2].ClockSelect[5] = ~((1<<CS22)|(1<<CS21)|(1<<CS20));

  Timer[3].ClockSelect[0] = (1<<CS30);
  Timer[3].ClockSelect[1] = (1<<CS31);
  Timer[3].ClockSelect[2] = ((1<<CS31)|(1<<CS30));
  Timer[3].ClockSelect[3] = (1<<CS32);
  Timer[3].ClockSelect[4] = ((1<<CS32)|(1<<CS30));
  Timer[3].ClockSelect[5] = ~((1<<CS32)|(1<<CS31)|(1<<CS30));

  /* Set TOPMax values for each Timer */
  Timer[0].TOPMax = 255;
  Timer[1].TOPMax = 65535;
  Timer[2].TOPMax = 255;
  Timer[3].TOPMax = 65535;

  /* Set ScaleFactor for each timer
     Freq_out = F_CLOCK/(Prescaler*ScaleFactor*TOP) */
  Timer[0].ScaleFactor = 4;
  Timer[1].ScaleFactor = 4;
  Timer[2].ScaleFactor = 4;
  Timer[3].ScaleFactor = 4;
}

static void Motor_Init(void)
{
  /* Initialize Motor Variables */
  Motor[0].Timer = MOTOR_0_TIMER;
  Motor[0].DirectionPort = &PORTC;
  Motor[0].DirectionPin = PC0;
  Motor[0].Frequency = 0;
  Motor[0].FrequencyMax = MOTOR_0_FREQUENCY_MAX;
  Motor[0].Direction = 0;
  Motor[0].DirectionPos = MOTOR_0_DIRECTION_POS;
  Motor[0].DirectionNeg = MOTOR_0_DIRECTION_NEG;
  Motor[0].Position = MOTOR_0_POSITION_HOME;
  Motor[0].PositionSetPoint = MOTOR_0_POSITION_HOME;
  Motor[0].Update = 1;

  Motor[1].Timer = MOTOR_1_TIMER;
  Motor[1].DirectionPort = &PORTC;
  Motor[1].DirectionPin = PC1;
  Motor[1].Frequency = 0;
  Motor[1].FrequencyMax = MOTOR_1_FREQUENCY_MAX;
  Motor[1].Direction = 0;
  Motor[1].DirectionPos = MOTOR_1_DIRECTION_POS;
  Motor[1].DirectionNeg = MOTOR_1_DIRECTION_NEG;
  Motor[1].Position = MOTOR_1_POSITION_HOME;
  Motor[1].PositionSetPoint = MOTOR_1_POSITION_HOME;
  Motor[1].Update = 1;

  Motor[2].Timer = MOTOR_2_TIMER;
  Motor[2].DirectionPort = &PORTC;
  Motor[2].DirectionPin = PC2;
  Motor[2].Frequency = 0;
  Motor[2].FrequencyMax = MOTOR_2_FREQUENCY_MAX;
  Motor[2].Direction = 0;
  Motor[2].DirectionPos = MOTOR_2_DIRECTION_POS;
  Motor[2].DirectionNeg = MOTOR_2_DIRECTION_NEG;
  Motor[2].Position = MOTOR_2_POSITION_HOME;
  Motor[2].PositionSetPoint = MOTOR_2_POSITION_HOME;
  Motor[2].Update = 1;

  /* Update Motors */
  Motor_Update_All();
}

static void IO_Disconnect(void)
{
  /* Set data direction of PORTB pins 4,5,7 to input to reduce current draw */
  DDRB &= ~((1<<DDB4) | (1<<DDB5) | (1<<DDB7));

  /* Set data direction of PORTC pins 0:2,6 to input to reduce current draw */
  DDRC &= ~((1<<DDC0) | (1<<DDC1) | (1<<DDC2) | (1<<DDC6));
}

static void Timer_On(uint8_t Timer_N)
{
  /* Turn on Timer_N by writing appropriate ClockSelect value
     to the ClockSelect Register */
  *Timer[Timer_N].Address.ClockSelect |= Timer[Timer_N].ClockSelect[Timer[Timer_N].Prescaler_N];

  /* Timer[Timer_N] is turned on */
  Timer[Timer_N].OnOff = 1;
}

static void Timer_Off(uint8_t Timer_N)
{
  /* Turn off Timer_N by writing appropriate ClockSelect value
     to the ClockSelect Register */
  *Timer[Timer_N].Address.ClockSelect &= Timer[Timer_N].ClockSelect[PRESCALER_NUM];

  /* Timer[Timer_N] is turned off */
  Timer[Timer_N].OnOff = 0;
}

static void Motor_Update(uint8_t Motor_N)
{
  uint32_t TOPValue;
  uint8_t  Prescaler_N;
  uint16_t Prescaler=1;
  uint32_t ScaleFactor;
  uint8_t  Timer_N;
  uint16_t Freq;

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    Freq = Motor[Motor_N].Frequency;
  }

  Timer_N = Motor[Motor_N].Timer;
  if (Motor[Motor_N].Update && (Freq == 0) && Timer[Timer_N].OnOff)
    {
      Timer_Off(Timer_N);
    }
  else if (Motor[Motor_N].Update && (Freq > 0))
    {
      if (Freq > Motor[Motor_N].FrequencyMax)
        {
          Freq = Motor[Motor_N].FrequencyMax;
        }
      ScaleFactor = Timer[Timer_N].ScaleFactor;
      TOPValue = (uint32_t)F_CLOCK/(ScaleFactor*(uint32_t)Freq);
      Prescaler_N = 0;
      while ((TOPValue > Timer[Timer_N].TOPMax) && (Prescaler_N < (PRESCALER_NUM-1)))
        {
          Prescaler_N++;
          if (Timer_N == 2)
            {
              Prescaler = PrescalerArray8[Prescaler_N];
            }
          else
            {
              Prescaler = PrescalerArray16[Prescaler_N];
            }
          TOPValue = (uint32_t)F_CLOCK/((uint32_t)Freq*(uint32_t)ScaleFactor*Prescaler);
          if ((Prescaler_N == (PRESCALER_NUM-1)) && (TOPValue > Timer[Timer_N].TOPMax))
            {
              TOPValue = Timer[Timer_N].TOPMax;
            }
        }
      Timer[Timer_N].TOPValue = (uint16_t)TOPValue;
      Freq = F_CLOCK/(Timer[Timer_N].TOPValue*ScaleFactor*Prescaler);
      Timer[Timer_N].Prescaler_N = Prescaler_N;

      Timer_Off(Timer_N);

      if ((Timer_N == 1) || (Timer_N == 3))
        {
          /* Set PWM frequency for the 16-bit Motor Timer */
          ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
          {
            *Timer[Timer_N].Address.TOP = Timer[Timer_N].TOPValue;
          }
        }
      else
        {
          /* Set PWM frequency for the 8-bit Motor Timer */
          *Timer[Timer_N].Address.TOP = (uint8_t)Timer[Timer_N].TOPValue;
        }

      /* Set Motor Direction pin */
      if (Motor[Motor_N].Direction)
        {
          *Motor[Motor_N].DirectionPort |= (1<<Motor[Motor_N].DirectionPin);
        }
      else
        {
          *Motor[Motor_N].DirectionPort &= ~(1<<Motor[Motor_N].DirectionPin);
        }

      /* Turn on Timer */
      Timer_On(Timer_N);
    }
  Motor[Motor_N].Update = 0;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    Motor[Motor_N].Frequency = Freq;
  }

}

static void Motor_Update_All(void)
{
  for ( uint8_t Motor_N=0; Motor_N<MOTOR_NUM; Motor_N++ )
    {
      Motor_Update(Motor_N);
    }
}

/*
  static void Position_Update(volatile uint8_t Motor_N)
  {
  uint8_t Timer_N;

  Timer_N = Motor[Motor_N].Timer;
  if (*Timer[Timer_N].Address.PinPort & (1<<Timer[Timer_N].OutputPin))
  {
  if (Motor[Motor_N].Direction == Motor[Motor_N].DirectionPos)
  {
  Motor[Motor_N].Position += 1;
  }
  else
  {
  Motor[Motor_N].Position -= 1;
  }
  if (Motor[Motor_N].Position == Motor[Motor_N].PositionSetPoint)
  {
  Motor[Motor_N].Frequency = 0;
  Timer_Off(Motor[Motor_N].Timer);
  }
  }
  }

  ISR(MOTOR_0_INTERRUPT)
  {
  Position_Update(0);
  return;
  }

  ISR(MOTOR_1_INTERRUPT)
  {
  Position_Update(1);
  return;
  }

  ISR(MOTOR_2_INTERRUPT)
  {
  Position_Update(2);
  return;
  }
*/

ISR(MOTOR_0_INTERRUPT)
{
  if (PINB & (1<<DDB5))
    {
      if (Motor[0].Direction == Motor[0].DirectionPos)
        {
          Motor[0].Position += 1;
        }
      else
        {
          Motor[0].Position -= 1;
        }
      if (Motor[0].Position == Motor[0].PositionSetPoint)
        {
          Motor[0].Frequency = 0;
          Timer_Off(Motor[0].Timer);
          /* Add this to test drift problem... */
          *Motor[0].DirectionPort &= ~(1<<Motor[0].DirectionPin);
        }
    }
  return;
}

ISR(MOTOR_1_INTERRUPT)
{
  if (PINC & (1<<DDC6))
    {
      if (Motor[1].Direction == Motor[1].DirectionPos)
        {
          Motor[1].Position += 1;
        }
      else
        {
          Motor[1].Position -= 1;
        }
      if (Motor[1].Position == Motor[1].PositionSetPoint)
        {
          Motor[1].Frequency = 0;
          Timer_Off(Motor[1].Timer);
          /* Add this to test drift problem... */
          *Motor[1].DirectionPort &= ~(1<<Motor[1].DirectionPin);
        }
    }
  return;
}

ISR(MOTOR_2_INTERRUPT)
{
  if (PINB & (1<<DDB4))
    {
      if (Motor[2].Direction == Motor[2].DirectionPos)
        {
          Motor[2].Position += 1;
        }
      else
        {
          Motor[2].Position -= 1;
        }
      if (Motor[2].Position == Motor[2].PositionSetPoint)
        {
          Motor[2].Frequency = 0;
          Timer_Off(Motor[2].Timer);
          /* Add this to test drift problem... */
          *Motor[2].DirectionPort &= ~(1<<Motor[2].DirectionPin);
        }
    }
  return;
}
