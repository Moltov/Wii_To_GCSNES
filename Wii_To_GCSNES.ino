#include "Wire.h"
#include "WiiClassic.h"
#include "crc_table.h"
//#include <pthread.h>
//#include <stdio.h>
#include <EEPROM.h>

#define GC_PIN 8
#define GC_HIGH DDRB &= ~0x01
#define GC_LOW DDRB |= 0x01
#define GC_QUERY (PINB & 0x01)


//Look up table for N64 DeadZone
static const uint8_t oot_mapping[64] = {57, 58, 59, 60, 61, 63, 65, 66, 68, 70, 72, 74, 76, 78, 80, 82, 84, 86, 88, 90, 92, 94, 97, 98, 101, 102, 104, 105, 107, 109, 111, 145, 147, 149, 151, 152, 154, 155, 158, 159, 162, 164, 166, 168, 170, 172, 174, 176, 178, 180, 182, 184, 186, 188, 190, 191, 193, 195, 196, 197, 198, 199};

unsigned char gc_Buffer2[33];
unsigned char gc_Buffer[33];
unsigned char cc_Neutral[4];

//bool printFlag = false;
bool needCommand = false;

char gc_Raw_Dump[281];
unsigned char gc_Command;
void get_GC_Command();
void gc_Send(unsigned char *buffer, char length, bool wide_stop);

//Initialize the wii classic pro class
WiiClassic myClassic = WiiClassic();

//Multiplier for the joystick
double multiplier = 4;

double gcUpdate = 0;
bool deadZone = true;

bool dUpPressed = false;
bool dDownPressed = false;
bool dLeftPressed = false;
bool dRightPressed = false;

void setup() {
  // Set up the serial connection for the classic pro
  Wire.begin();
  Serial.begin(9600);
  deadZone = EEPROM.read(0);
  //if(readValue != 255)
  //multiplier = (readValue + 200) / 100;
  myClassic.begin();
  myClassic.update();

  //init gc buffer
  memset(gc_Buffer, 0, sizeof(gc_Buffer));
  memset(gc_Buffer, 0, sizeof(gc_Buffer2));

  //Get the nuetral position
  cc_Neutral[0] = myClassic.leftStickX();
  cc_Neutral[1] = myClassic.leftStickY();
  cc_Neutral[2] = myClassic.rightStickX();
  cc_Neutral[3] = myClassic.rightStickY();

  digitalWrite(GC_PIN, LOW);
  pinMode(GC_PIN, INPUT);
}

void loop() {
  //unsigned char data, addr;

  if (needCommand == false) {
    // put your main code here, to run repeatedly:
    //Wait until the next update is about 1 ms away with this delay
    delay(10);
    get_Wii_Input();
    //Cant update the output fast enough so send half of it
    //over serial and alternate.
    /*if (printFlag) {
      uint32_t temp = gc_Buffer[0] - '0';
      temp = temp + (gc_Buffer[1] - '0') * 1000;
      temp = temp + (gc_Buffer[2] - '0') * 1000000;
      temp = temp;
      Serial.print("a");
      Serial.println(temp);
      printFlag = false;
    } else {
      uint32_t temp = temp + (gc_Buffer[3] - '0') * 1;
      temp = temp + (gc_Buffer[4] - '0') * 1000;
      temp = temp + (gc_Buffer[5] - '0') * 1000000;
      Serial.println(temp);
      printFlag = true;
    }*/
    noInterrupts();
    get_GC_Command(0);
  }

  // Wait for incomming GC command
  // this will block until the GC sends us a command

  if (needCommand == true) {
    //Serial.println(gc_Command, HEX);
    needCommand = false;
  }



  switch (gc_Command) {
    case 0x00:
    case 0xff:
      //ID what controller you are using, return 0x090000
      gc_Buffer[0] = 0x29;
      gc_Buffer[1] = 0x00;
      gc_Buffer[2] = 0x20;

      gc_Buffer[3] = 0x00;

      gc_Send(gc_Buffer, 3, 0);
      needCommand = true;

      get_GC_Command(0);

      //Serial.println("ID command:");
      //Serial.println(gc_Command, HEX);
      break;
    case 0x40:
      //Return the data input from the classic controller
      gc_Send(gc_Buffer, 8, 0);
      get_GC_Command(1);
      //Serial.println("Input Request command:");
      //Serial.println(gc_Command, HEX);
      break;
    case 0x41:
    case 0x42:
      //
      gc_Buffer[0] = 0x00;
      gc_Buffer[1] = 0x80;
      gc_Buffer[2] = 0x80;
      gc_Buffer[3] = 0x80;
      gc_Buffer[4] = 0x80;
      gc_Buffer[5] = 0x80;
      gc_Buffer[6] = 0x00;
      gc_Buffer[7] = 0x00;
      gc_Buffer[8] = 0x02;
      gc_Buffer[9] = 0x02;
      gc_Send(gc_Buffer, 10, 0);
      //Serial.println("Origin command:");
      //Serial.println(gc_Command, HEX);
      break;

    default:
      //
      Serial.println("Error, unknown command from console.");
      Serial.println(gc_Command, HEX);
      break;
  }



  interrupts();

}

void gc_Send(unsigned char *buffer, char length, bool wide_stop)
{
  asm volatile (";Starting GC Send Routine");
  // Send these bytes
  char bits;

  bool bit;

  //Serial.println("test");

  // This routine is very carefully timed by examining the assembly output.
  // Do not change any statements, it could throw the timings off
  //
  // We get 16 cycles per microsecond, which should be plenty, but we need to
  // be conservative. Most assembly ops take 1 cycle, but a few take 2
  //
  // I use manually constructed for-loops out of gotos so I have more control
  // over the outputted assembly. I can insert nops where it was impossible
  // with a for loop

  asm volatile (";Starting outer for loop");
outer_loop:
  {
    asm volatile (";Starting inner for loop");
    bits = 8;
inner_loop:
    {
      // Starting a bit, set the line low
      asm volatile (";Setting line to low");
      GC_LOW; // 1 op, 2 cycles

      asm volatile (";branching");
      if (*buffer >> 7) {
        asm volatile (";Bit is a 1");
        // 1 bit
        // remain low for 1us, then go high for 3us
        // nop block 1
        asm volatile ("nop\nnop\nnop\nnop\nnop\n"
                      "nop\nnop\nnop\n");

        asm volatile (";Setting line to high");
        GC_HIGH;

        // nop block 2
        // we'll wait only 2us to sync up with both conditions
        // at the bottom of the if statement
        asm volatile ("nop\nnop\nnop\nnop\nnop\n"
                      "nop\nnop\nnop\nnop\nnop\n"
                      "nop\nnop\nnop\nnop\nnop\n"
                      "nop\nnop\nnop\nnop\nnop\n"
                      "nop\nnop\nnop\nnop\nnop\n"
                      "nop\nnop\nnop\nnop\nnop\n"
                      "nop\nnop\nnop\nnop\n"
                      "nop\nnop\nnop\nnop\n"
                     );

      } else {
        asm volatile (";Bit is a 0");
        // 0 bit
        // remain low for 3us, then go high for 1us
        // nop block 3
        asm volatile ("nop\nnop\nnop\nnop\nnop\n"
                      "nop\nnop\nnop\nnop\nnop\n"
                      "nop\nnop\nnop\nnop\nnop\n"
                      "nop\nnop\nnop\nnop\nnop\n"
                      "nop\nnop\nnop\nnop\nnop\n"
                      "nop\nnop\nnop\nnop\nnop\n"
                      "nop\nnop\nnop\nnop\nnop\n"
                      "nop\n"
                      "nop\nnop\nnop\nnop\n"
                      "nop\nnop\nnop\nnop\n"
                      "nop\nnop\nnop\n");

        asm volatile (";Setting line to high");
        GC_HIGH;

        // wait for 1us
        asm volatile ("; end of conditional branch, need to wait 1us more before next bit");

      }
      // end of the if, the line is high and needs to remain
      // high for exactly 16 more cycles, regardless of the previous
      // branch path

      asm volatile (";finishing inner loop body");
      --bits;
      if (bits != 0) {
        // nop block 4
        // this block is why a for loop was impossible
        asm volatile ("nop\nnop\nnop\nnop\nnop\n"
                      "nop\nnop\nnop\nnop\n");
        // rotate bits
        asm volatile (";rotating out bits");
        *buffer <<= 1;

        goto inner_loop;
      } // fall out of inner loop

    }
    asm volatile (";continuing outer loop");
    asm volatile ("nop\nnop\nnop\nnop\n");
    // In this case: the inner loop exits and the outer loop iterates,
    // there are /exactly/ 16 cycles taken up by the necessary operations.
    // So no nops are needed here (that was lucky!)
    --length;
    if (length != 0) {
      asm volatile ("nop\nnop\nnop\nnop\n");
      ++buffer;
      goto outer_loop;
    } // fall out of outer loop
  }

  // send a single stop (1) bit
  // nop block 5
  asm volatile ("nop\nnop\nnop\nnop\n"
                "nop\nnop\nnop\nnop\n");
  GC_LOW;
  // wait 1 us, 16 cycles, then raise the line
  // take another 3 off for the wide_stop check
  // 16-2-3=11
  // nop block 6
  asm volatile ("nop\nnop\nnop\nnop\nnop\n"
                "nop\nnop\nnop\nnop\nnop\n"
                "nop\n"
                "nop\nnop\nnop\nnop\n");
  if (wide_stop) {
    asm volatile (";another 1us for extra wide stop bit\n"
                  "nop\nnop\nnop\nnop\nnop\n"
                  "nop\nnop\nnop\nnop\nnop\n"
                  "nop\nnop\nnop\nnop\n"
                  "nop\nnop\nnop\nnop\n");
  }

  GC_HIGH;
}

void get_GC_Command(int ms) {
  gc_Command = 0x00;
  int bitcount;
  char *bitbin = gc_Raw_Dump;
  int idle_wait;
  int test = 2500;



func_top:
  gc_Command = 0;

  bitcount = 8;

  if (ms > 0) {
    while (test > 0) {
      if (!GC_QUERY) {
        needCommand = true;
        goto read_jump;
      }
      test--;
    }
    if (needCommand == false) {
      return;
    }

  }

  // wait to make sure the line is idle before
  // we begin listening
  if (needCommand == false) {
    for (idle_wait = 32; idle_wait > 0; --idle_wait) {
      if (!GC_QUERY) {
        idle_wait = 32;
      }
    }
  }

read_loop:
  // wait for the line to go low
  while (GC_QUERY) {}
read_jump:
  // wait approx 2.5us and poll the line
  asm volatile (
    "nop\nnop\nnop\nnop\nnop\n"
    "nop\nnop\nnop\nnop\nnop\n"
    "nop\nnop\nnop\nnop\nnop\n"
    "nop\nnop\nnop\nnop\nnop\n"
    "nop\nnop\nnop\nnop\nnop\n"
    "nop\nnop\nnop\nnop\nnop\n"
    "nop\nnop\nnop\nnop\nnop\n"
    "nop\nnop\nnop\n"
  );
  if (GC_QUERY)
    gc_Command |= 0x01;
  //Serial.println("test1");
  --bitcount;
  if (bitcount == 0)
    goto read_more;

  gc_Command <<= 1;

  // wait for line to go high again
  // I don't want this to execute if the loop is exiting, so
  // I couldn't use a traditional for-loop
  while (!GC_QUERY) {}
  goto read_loop;

read_more:
  if (gc_Command == 0x40) {
    bitcount = 1;
  } else {
    bitcount = 1;
  }

  // make sure the line is high. Hopefully we didn't already
  // miss the high-to-low transition
  while (!GC_QUERY) {}
read_loop2:
  for (idle_wait = 12; idle_wait > 0; --idle_wait) {
    if (!GC_QUERY) {
      idle_wait = 12;
    }
  }
  return;
  // wait for the line to go low
  while (GC_QUERY) {}

  // wait approx 2.5us and poll the line
  asm volatile (
    "nop\nnop\nnop\nnop\nnop\n"
    "nop\nnop\nnop\nnop\nnop\n"
    "nop\nnop\nnop\nnop\nnop\n"
    "nop\nnop\nnop\nnop\nnop\n"
    "nop\nnop\nnop\nnop\nnop\n"
    "nop\nnop\nnop\nnop\nnop\n"
    "nop\nnop\nnop\nnop\nnop\n"
    "nop\nnop\nnop\n"
  );
  if (GC_QUERY) {
    //gc_Command |= 0x01;
    //goto bit_test;
  }
  //Serial.println("test2");
  --bitcount;
  if (bitcount == 0) {
    //wait for the line to go high
    while (!GC_QUERY) {}
    return;
  }
  gc_Command <<= 1;

  // wait for line to go high again
  while (!GC_QUERY) {}
  goto read_loop2;

bit_test:

  if (!GC_QUERY) {
    Serial.println(bitcount);
    gc_Command = 0xEE;
    return;
  }
  test--;
  if (test > 0) {
    goto bit_test;
  }
  return;
}

void get_Wii_Input() {
  myClassic.update();
  gcUpdate++;

  gc_Buffer[0] = gc_Buffer2[0];
  gc_Buffer[1] = gc_Buffer2[1];

  gc_Buffer2[0] = 0x00;
  gc_Buffer2[1] = 0x00;

  /***********
    ** BYTE 0 **
    ***********/
  // 0, 0, 0, Start, Y, X, B, A

  // Classic Start to N64 Start
  gc_Buffer2[0] |= (myClassic.startPressed() << 4);

  // Classic Y to GC C-Left
  gc_Buffer2[0] |= (myClassic.yPressed() << 3);

  // Classic X to GC C-Right
  gc_Buffer2[0] |= (myClassic.xPressed() << 2);

  // Classic B to GC B
  gc_Buffer2[0] |= (myClassic.bPressed() << 1);

  // Classic A to GC A
  gc_Buffer2[0] |= (myClassic.aPressed() << 0);

  /***********
  ** BYTE 1 **
  ***********/
  // 1, L, R, Z,D-up, D-down, D-right, D-left

  // Set Bit 7 to 1 (not sure why)
  gc_Buffer2[1] |= (1 << 7);

  // Classic L to GC L (Z for n64)
  gc_Buffer2[1] |= myClassic.leftShoulderPressed() << 6;

  // Classic R to GC R
  gc_Buffer2[1] |= myClassic.rightShoulderPressed() << 5;

  // Classic Dup to to GC D-Up (N64 L)
  gc_Buffer2[1] |= myClassic.upDPressed() << 3;

  // Classic Ddown to to GC D-Down (N64 L)
  gc_Buffer2[1] |= myClassic.downDPressed() << 2;

  // Classic Dright to GC D-right (N64 L)
  gc_Buffer2[1] |= myClassic.rightDPressed() << 1;

  // Classic Dleft to GC D-Left (N64 L)
  gc_Buffer2[1] |= myClassic.leftDPressed() << 0;

  /***************
  ** BYTE 2 & 3 **
  ***************/
  //JOYSTICK DEADZONE DATA FOR OOT
  //ESS End - 23
  //ESS Start - 26
  //Center of Controller - 32
  //ESS Start - 38
  //ESS End - 41
  //X axis:
  //HOLDING LEFT 0--23--ESS--26--neutral--38--ESS--41--HOLDING RIGHT
  //Y axis:
  //HOLDING DOWN 0--23--ESS--26--neutral--38--ESS--41--HOLDING UP

  gc_Buffer[2] = myClassic.leftStickX();
  gc_Buffer[3] = myClassic.leftStickY();

  //ADJUST DEADZONE
  //This left the use adjust the deadzone of the controller
  //Press HOME + DUP to toggle between dead zones
  //The default is n64 dead zone and D + up toggles to VC deadzone.
  if (deadZone == false) {
    //VC Dead Zone
    if (gc_Buffer[2] == 27 || gc_Buffer[2] == 28) {
      gc_Buffer[2] = 26;
    }
    if (gc_Buffer[2] == 36 || gc_Buffer[2] == 37) {
      gc_Buffer[2] = 38;
    }
    if (gc_Buffer[3] == 27 || gc_Buffer[3] == 30) {
      gc_Buffer[3] = 26;
    }
    if (gc_Buffer[3] == 36 || gc_Buffer[3] == 37) {
      gc_Buffer[3] = 38;
    }
  } else {
    //N64 DeadZone (Default)
    gc_Buffer[2] = oot_mapping[gc_Buffer[2]];
    gc_Buffer[3] = oot_mapping[gc_Buffer[3]];
    /*
    if (gc_Buffer[2] >= 23 && gc_Buffer[2] <= 30) {
      gc_Buffer[2] = 26;
    }
    if (gc_Buffer[2] >= 34 && gc_Buffer[2] <= 41) {
      gc_Buffer[2] = 38;
    }
    if (gc_Buffer[3] >= 23 && gc_Buffer[3] <= 30) {
      gc_Buffer[3] = 26;
    }
    if (gc_Buffer[3] >= 34 && gc_Buffer[3] <= 41) {
      gc_Buffer[3] = 38;
    }*/
  }

  //Apply the multiplier for GC if VC deadzone
  if (deadZone == false) {
    gc_Buffer[2] = (gc_Buffer[2] * multiplier);
    gc_Buffer[3] = (gc_Buffer[3] * multiplier);
  }

  /***************
  ** BYTE 4 & 5 **
  ***************/

  gc_Buffer[4] = ((myClassic.rightStickX() - 32) * 8);
  gc_Buffer[5] = ((myClassic.rightStickY() - 32) * 8);
  if ((gc_Buffer[4] >= -24) && (gc_Buffer[5] <= 24)) {
    gc_Buffer[4] = 0;
  }
  if ((gc_Buffer[5] >= -24) && (gc_Buffer[5] <= 24)) {
    gc_Buffer[5] = 0;
  }

  // Classic RZ to N64 C-Down
  if (myClassic.rzPressed()) {
    gc_Buffer[5] = 0x00000000;
  }

  // Classic LZ to N64 C-Down
  if (myClassic.lzPressed()) {
    gc_Buffer[5] = 0x00000000;
  }
  /***************
  ** BYTE 6 & 7 **
  ***************/
  //unused atm
  gc_Buffer[6] = 0x00;
  gc_Buffer[7] = 0x00;

  // in order to make the controller a bit more dynamic, let the user increase or decrease the senstivitiy by .5
  // if the minus button and the dpad is used together
  /*if(myClassic.rightDPressed() && myClassic.selectPressed())
    if(multiplier < 5.5)
      multiplier += .5;

  if(myClassic.leftDPressed() && myClassic.selectPressed())
    if(multiplier > 2.5)
      multiplier -= .5;

  if(myClassic.upDPressed() && myClassic.selectPressed())
  {
    //multiplier = ((EEPROM.read(0)) + 200) / 100;
  }*/

  //if(myClassic.downDPressed() && myClassic.selectPressed())
  //EEPROM.write(0, ((multiplier * 100) - 200));

  if (myClassic.upDPressed()) {
    if (myClassic.homePressed() && dUpPressed == false) {
      deadZone = !deadZone;
      EEPROM.write(0, deadZone);
    }
    dUpPressed = true;
  } else {
    dUpPressed = false;
  }
  if (myClassic.homePressed() && myClassic.selectPressed()) {
    multiplier = 4;
    //Get the nuetral position
    cc_Neutral[0] = myClassic.leftStickX();
    cc_Neutral[1] = myClassic.leftStickY();
    cc_Neutral[2] = myClassic.rightStickX();
    cc_Neutral[3] = myClassic.rightStickY();
  }

  //Serial.println(gc_Buffer[2], HEX);
  //GC_LOW;
  //asm volatile ("nop\nnop\nnop\nnop\n"
  //             "nop\nnop\nnop\nnop\n");
  //GC_HIGH;
}
