/*

    This code is the final version of the BIS sprite code. It will cycle through 
    a)transmitting the temperature b)startup info including number of startups 
    and duration of the minimum, maximum and average sessions c)the results of 
    a memory integrity (radiation) test.
    It will also print the hexadecimal code as it will do in the final code.
    
    
    When transmitting it will send the hexadecimal representation of the desired
    message. The characters transmitted are as follows:
      1. T - The header code for temperature information
      Up to four characters - Hexadecimal representation of processor temperature in kelvin (leading zeros ommitted)
      Up to four characters - Hexadecimal representation of gyro temperature in kelvin (leading zeros ommitted)
      ------------------------------------------------------------------------------------------------------------------------
      1. S - The header code for session information
      Four characters - Hexadecimal representation of minimum duration
      Four characters - Hexadecimal representation of maximum duration
      Four characters - Hexadecimal representation of average duration
      Up to four characters - Hexadecimal representation of number of startups (leading zeros ommitted)
      ------------------------------------------------------------------------------------------------------------------------      
      1. R - The header code for the memory integrity test results
      Up to four characters - Hexadecimal representation of number of currupted bits due to radiation (leading zeros ommitted)
      
      This code takes roughly 30 seconds to transmit.
    
*/


#include "CCFlash.h" //To allow the use of the flash memory
#include "temp.h" //To allow the use of the temperature sensor
int32_t TempM;
int32_t TempG;
#include "radtest.h" //To allow the use of the radiation memory integrity test
#define flash_C SEGMENT_C //To use segment C
#define flash_D SEGMENT_D //To use segment D
unsigned char p = 0; //Set p as a variable which we will use later for segment C for logging session info
unsigned char q = 0; //Set q as a variable which we will use later for segment D for looping throug 3 sections in turn
unsigned int errors = 0; //This will be used for the memory integrity test

char HexChar[] ="0123456789ABCDEF";

#include <SpriteRadio.h>
unsigned char prn2[80] = {
  0b00111010, 0b00010010, 0b01111101, 0b10011010, 0b01010000, 0b10111011, 0b10101101, 0b10100111,
  0b01100110, 0b00100011, 0b01010011, 0b01001101, 0b10011110, 0b01110100, 0b00010100, 0b11101110,
  0b11010101, 0b00111110, 0b10000111, 0b00111101, 0b11101010, 0b01111111, 0b11101001, 0b01100001,
  0b00010001, 0b01100111, 0b10000000, 0b11100011, 0b11101101, 0b00101110, 0b10010000, 0b11100001,
  0b11000101, 0b11111101, 0b10010010, 0b10000001, 0b00100000, 0b11010100, 0b01001000, 0b11000001,
  0b00000110, 0b00100100, 0b01010110, 0b00001001, 0b00000010, 0b10010011, 0b01111111, 0b01000111,
  0b00001110, 0b00010010, 0b11101001, 0b01101111, 0b10001110, 0b00000011, 0b11001101, 0b00010001,
  0b00001101, 0b00101111, 0b11111100, 0b10101111, 0b01111001, 0b11000010, 0b11111001, 0b01010110,
  0b11101110, 0b01010000, 0b01011100, 0b11110011, 0b01100101, 0b10010101, 0b10001000, 0b11001101,
  0b11001011, 0b01101011, 0b10111010, 0b00010100, 0b10110011, 0b01111100, 0b10010000, 0b10111001
};
unsigned char prn3[80] = {
  0b01100010, 0b00101010, 0b11010000, 0b01000010, 0b10010001, 0b00011110, 0b00111111, 0b11010011,
  0b11101110, 0b01011000, 0b01101000, 0b01011111, 0b10110110, 0b11000100, 0b00100101, 0b10000111,
  0b11100110, 0b10010111, 0b01110011, 0b01101111, 0b01110010, 0b11010101, 0b01110101, 0b11100010,
  0b11010010, 0b00010010, 0b01111110, 0b01100110, 0b10000001, 0b01000111, 0b01010001, 0b10011100,
  0b11001000, 0b10101111, 0b10101011, 0b01111101, 0b01011110, 0b00011011, 0b01010110, 0b00111101,
  0b00001110, 0b01010100, 0b10011110, 0b00010101, 0b00000100, 0b10101000, 0b00101011, 0b10110011,
  0b00011001, 0b11010100, 0b01110101, 0b11111010, 0b01100110, 0b00000110, 0b11011110, 0b11010010,
  0b11100001, 0b01000101, 0b01010010, 0b11000100, 0b00100100, 0b11000100, 0b01011010, 0b01100000,
  0b01111001, 0b01101111, 0b01110010, 0b01001000, 0b00010111, 0b10100111, 0b10010110, 0b00100000,
  0b11010000, 0b00001110, 0b00011101, 0b11011010, 0b11110111, 0b11010010, 0b10101110, 0b11100101
};
//Initialize the radio class, supplying the Gold Codes that correspond to 0 and 1
SpriteRadio m_radio = SpriteRadio(prn2, prn3);



void setup() 
{
  Serial.begin(9600); //Start serial connection
  
  m_radio.txInit();
  
  premult = 10; //Setup for temperature sensor
  mainTempCalibrate();
  mainTempRead(); // First reading is usually spurious
  
  Flash.read(flash_C, &p, 1); //Read flash memory segment C
  
  /*
  Startup_Initialise(); //As a first step if you have a problem with the code not working, uncomment these three lines
  Flash.erase(flash_D);
  Flash.write(flash_D, (unsigned char*) "A" ,1); 
  */
  
  if (p!='S') Startup_Initialise(); //If the first character is not 'S' (on first ever startup) do the initialisation code
  else Startup_Update();  //If it is not the first ever startup, update the data for the previous session
  
  Flash.read(flash_D, &q, 1);   //Read 'q' in the flash memory
  if ((q!='A') && (q!='B') && (q!='C')) //If 'q' is not A,B,C then: 
  {
      Flash.erase(flash_D);     //Clear the flash memory
      Flash.write(flash_D, (unsigned char*) "A" ,1);    //Write 'A' to the memory
  }
}

//----------------------------------------------------------------------------------------------------------------------------

void loop() 
{
  Flash.read(flash_D, &q, 1);    //Read 'q' in the memory
  switch(q)
  {
    case 'A':                 //If 'q' is 'A' then:
      Flash.erase(flash_D);     //Clear the memory...
      Flash.write(flash_D, (unsigned char*) "B" ,1);  //Write 'B' to it and...
      Temperature(); //Execute the tempterature sensor code
      break ;
    case 'B':                  //If 'q' is 'B' then:
      Flash.erase(flash_D);      //Clear the memory...
      Flash.write(flash_D, (unsigned char*) "C" ,1);  //Write 'C' to it and...
      Startup_Transmit(); //Execute the session info code
      break ;
    case 'C':                  //If 'q' is 'C' then:
      Flash.erase(flash_D);      //Clear the memory...
      Flash.write(flash_D, (unsigned char*) "A" ,1);  //Write 'A' to it and...
      MemoryTest(); //Execute the memory integrity test code
      break ;
    default:
      // If sprite powers down between flash.erase and flash.write we need to reinitialise memory
      Startup_Initialise(); //If the first character is not 'S' (on first ever startup) do the initialisation code
      Flash.erase(flash_D); //Clear the flash memory
      Flash.write(flash_D, (unsigned char*) "A" ,1); //Write 'A' to the memory
      break ;
  }
}


  //-------------------------------------------------------------------------------------------------------------------------------

  void Startup_Initialise()
  {
    erase_memory();
    unsigned char message[19]; //Make 'message' 19 bytes long
    message[0] = 'S'; //Set first character to 'S'
    union //Use union to translate floating points and integers to characters
    {
      float union_f;
      unsigned int union_i;
      char union_s[4];
    };
    union_i = 1; //Set character 2 and 3 as number of startups to 1
    message[1] = union_s[0];
    message[2] = union_s[1];

    union_f = millis(); //Set characters 4-7 to time since startup
    message[3] = union_s[0];
    message[4] = union_s[1];
    message[5] = union_s[2];
    message[6] = union_s[3];

    union_f = 0; //Set characters 8-11 to 0 (they will be used as minimum time awake)
    message[7] = union_s[0];
    message[8] = union_s[1];
    message[9] = union_s[2];
    message[10] = union_s[3];

    message[11] = union_s[0]; //Set characters 12-15 to 0 (they will be used as maximum time awake)
    message[12] = union_s[1];
    message[13] = union_s[2];
    message[14] = union_s[3];

    message[15] = union_s[0]; //Set characters 16-19 to 0 (they will be used as average time awake)
    message[16] = union_s[1];
    message[17] = union_s[2];
    message[18] = union_s[3];

    Flash.erase(flash_C);
    Flash.write(flash_C, (unsigned char*) message, 19); //Write this starting data to segment_C
  }

  //-------------------------------------------------------------------------------------------------------------------------------

  void Startup_Update()
  {
    unsigned char message[19]; //Make 'message' 19 bytes long
    for (int i = 0; i < 19; i ++) //Read message from memory
    {
      Flash.read(flash_C + i, &p, 1);
      message[i] = p;    
    }
    union //Use union to translate floating points and integers to characters
    {
      float union_f;
      unsigned int union_i;
      char union_s[4];
    };
    union_s[0] = message[1]; //Set number of startups to message
    union_s[1] = message[2];
    unsigned int NoStartups = union_i;
    union_s[0] = message[3]; //Set duration of last session to message
    union_s[1] = message[4];
    union_s[2] = message[5];
    union_s[3] = message[6];
    float Duration = union_f;
    union_s[0] = message[7]; //Set minimum duration to message
    union_s[1] = message[8];
    union_s[2] = message[9];
    union_s[3] = message[10];
    float MinDuration = union_f;
    union_s[0] = message[11]; //Set maximum duration to message
    union_s[1] = message[12];
    union_s[2] = message[13];
    union_s[3] = message[14];
    float MaxDuration = union_f;
    union_s[0] = message[15]; //Set average duration to message
    union_s[1] = message[16];
    union_s[2] = message[17];
    union_s[3] = message[18];
    float AveDuration = union_f;
    
    //Now update data for latest session
    if(MinDuration) MinDuration = min(Duration, MinDuration);
    else MinDuration = Duration;
    MaxDuration = max(Duration, MaxDuration);
    AveDuration = (Duration + ((NoStartups - 1) * AveDuration))/(NoStartups);
    Duration = millis();
    NoStartups ++;

    //Now write back to message
    union_i = NoStartups; //Set character 2 and 3 as number of startups to 1
    message[1] = union_s[0];
    message[2] = union_s[1];

    union_f = Duration; //Set characters 4-7 to time since startup
    message[3] = union_s[0];
    message[4] = union_s[1];
    message[5] = union_s[2];
    message[6] = union_s[3];

    union_f = MinDuration; //Set characters 8-11 to 0 (they will be used as minimum time awake)
    message[7] = union_s[0];
    message[8] = union_s[1];
    message[9] = union_s[2];
    message[10] = union_s[3];

    union_f = MaxDuration; //Set characters 12-15 to 0 (they will be used as maximum time awake)
    message[11] = union_s[0]; 
    message[12] = union_s[1];
    message[13] = union_s[2];
    message[14] = union_s[3];

    union_f = AveDuration;  //Set characters 16-19 to 0 (they will be used as average time awake)
    message[15] = union_s[0];
    message[16] = union_s[1];
    message[17] = union_s[2];
    message[18] = union_s[3];

    Flash.erase(flash_C);
    Flash.write(flash_C, (unsigned char*) message ,19); //Write this starting data to segment_C
  }

  //-------------------------------------------------------------------------------------------------------------------------------

  void Startup_Transmit()
  {
    unsigned char message[19]; //make 'message' 19 bytes long
    for (int i = 0; i < 19; i ++) //read message from memory
    {
      Flash.read(flash_C+i, &p, 1);
      message[i] = p;
    }
    union //Use union to translate floating points and integers to characters
    {
      float union_f;
      unsigned int union_i;
      char union_s[4];
    };
    union_s[0] = message[1]; //Set number of startups to message
    union_s[1] = message[2];
    unsigned int NoStartups = union_i;
    union_s[0] = message[3]; //Set duration of last session to message
    union_s[1] = message[4];
    union_s[2] = message[5];
    union_s[3] = message[6];
    float Duration = union_f;
    union_s[0] = message[7]; //Set minimum duration to message
    union_s[1] = message[8];
    union_s[2] = message[9];
    union_s[3] = message[10];
    float MinDuration = union_f;
    union_s[0] = message[11]; //Set maximum duration to message
    union_s[1] = message[12];
    union_s[2] = message[13];
    union_s[3] = message[14];
    float MaxDuration = union_f;
    union_s[0] = message[15]; //Set average duration to message
    union_s[1] = message[16];
    union_s[2] = message[17];
    union_s[3] = message[18];
    float AveDuration = union_f;

    //Now update data for latest session
    Duration = millis();
    //Now write back to message

    union_f = Duration; //Set characters 4-7 to time since startup
    message[3] = union_s[0];
    message[4] = union_s[1];
    message[5] = union_s[2];
    message[6] = union_s[3];


    Flash.erase(flash_C);
    Flash.write(flash_C, (unsigned char*) message ,19); //Write this starting data to segment_C

    unsigned int DurationSecs = Duration/1000;
    unsigned int MinDurationSecs = MinDuration/1000;
    unsigned int MaxDurationSecs = MaxDuration/1000;
    unsigned int AveDurationSecs = AveDuration/1000;
    char InfoTrans[18];
    unsigned int Info_len = 0;
   
    InfoTrans[0] = message[0];
    
    InfoTrans[1] = HighHex(highByte(MinDurationSecs)) ;
    InfoTrans[2] = LowHex(highByte(MinDurationSecs)) ;
    InfoTrans[3] = HighHex(lowByte(MinDurationSecs)) ;
    InfoTrans[4] = LowHex(lowByte(MinDurationSecs)) ;
    
    InfoTrans[5] = HighHex(highByte(MaxDurationSecs)) ;
    InfoTrans[6] = LowHex(highByte(MaxDurationSecs)) ;
    InfoTrans[7] = HighHex(lowByte(MaxDurationSecs)) ;
    InfoTrans[8] = LowHex(lowByte(MaxDurationSecs)) ;
    
    InfoTrans[9] = HighHex(highByte(AveDurationSecs)) ;
    InfoTrans[10] = LowHex(highByte(AveDurationSecs)) ;
    InfoTrans[11] = HighHex(lowByte(AveDurationSecs)) ;
    InfoTrans[12] = LowHex(lowByte(AveDurationSecs)) ;
    
    // Omit leading zeroes to save transmission time
    if (NoStartups<16)
    {
      InfoTrans[13] = LowHex(lowByte(NoStartups)) ;
      InfoTrans[14] = 0 ;
      Info_len = 14 ;    
    }
    else if (NoStartups<256)
    {
      InfoTrans[13] = HighHex(lowByte(NoStartups)) ;
      InfoTrans[14] = LowHex(lowByte(NoStartups)) ;
      InfoTrans[15] = 0 ;
      Info_len = 15 ; 
    }
    else if (NoStartups<4096)
    {
      InfoTrans[13] = LowHex(highByte(NoStartups)) ;
      InfoTrans[14] = HighHex(lowByte(NoStartups)) ;
      InfoTrans[15] = LowHex(lowByte(NoStartups)) ;
      InfoTrans[16] = 0 ;
      Info_len = 16 ; 
    }
    else
    {
      InfoTrans[13] = HighHex(highByte(NoStartups)) ;
      InfoTrans[14] = LowHex(highByte(NoStartups)) ;
      InfoTrans[15] = HighHex(lowByte(NoStartups)) ;
      InfoTrans[16] = LowHex(lowByte(NoStartups)) ;
      InfoTrans[17] = 0 ;
      Info_len = 17 ;
      }
    m_radio.transmit(InfoTrans, Info_len);
  }

//--------------------------------------------------------------------------------------------------------------------------------------

void Temperature() {  //Print the temperature sensor data from both sensors with headings to the serial monitor:

  unsigned int T_M ;
  unsigned int T_G ;
  
  TempM = mainTempRead();

  TempG = gyroTempRead();
  
  T_M = max(0 , 273 + (TempM / 10)) ;
  T_G = max(0 , 273 + (TempG / 10)) ;
    
  
  char TempTrans[10];
  unsigned int Temp_len=0;
  
    TempTrans[0] = 'T';
   // Omit leading zeroes to save transmission time
  if (max(T_M,T_G)<16)
  {
    TempTrans[1] = LowHex(lowByte(T_M)) ;
    TempTrans[2] = LowHex(lowByte(T_G)) ;
    TempTrans[3] = 0 ;
    Temp_len = 3 ;    
  }
  else if (max(T_M,T_G)<256)
  {
    TempTrans[1] = HighHex(lowByte(T_M)) ;
    TempTrans[2] = LowHex(lowByte(T_M)) ;
    TempTrans[3] = HighHex(lowByte(T_G)) ;
    TempTrans[4] = LowHex(lowByte(T_G)) ;
    TempTrans[5] = 0 ;
    Temp_len = 5 ;
  }
  else if (max(T_M,T_G)<4096)
  {
    TempTrans[1] = LowHex(highByte(T_M)) ;
    TempTrans[2] = HighHex(lowByte(T_M)) ;
    TempTrans[3] = LowHex(lowByte(T_M)) ;
    TempTrans[4] = LowHex(highByte(T_G)) ;
    TempTrans[5] = HighHex(lowByte(T_G)) ;
    TempTrans[6] = LowHex(lowByte(T_G)) ;
    TempTrans[7] = 0 ;
    Temp_len = 7 ;  
  }
  else
  {
    TempTrans[1] = HighHex(highByte(T_M)) ;
    TempTrans[2] = LowHex(highByte(T_M)) ;
    TempTrans[3] = HighHex(lowByte(T_M)) ;
    TempTrans[4] = LowHex(lowByte(T_M)) ;
    TempTrans[5] = HighHex(highByte(T_G)) ;
    TempTrans[6] = LowHex(highByte(T_G)) ;
    TempTrans[7] = HighHex(lowByte(T_G)) ;
    TempTrans[8] = LowHex(lowByte(T_G)) ;
    TempTrans[9] = 0 ;
    Temp_len = 9 ;
}
    m_radio.transmit(TempTrans, Temp_len);
}


void MemoryTest() {
      
  errors = check_memory();
      
  char MemTrans[6];
  unsigned int Mem_len=0;
  MemTrans[0] = 'R';
  // Omit leading zeroes to save transmission time
  if (errors<16)
  {
    MemTrans[1] = LowHex(lowByte(errors));
    MemTrans[2] = 0;
    Mem_len=2;     
  }
  else if (errors<256)
  {
    MemTrans[1] = HighHex(lowByte(errors));
    MemTrans[2] = LowHex(lowByte(errors));
    MemTrans[3] = 0;
    Mem_len=3; 
  }
  else if (errors<4096)
  {
    MemTrans[1] = LowHex(highByte(errors));
    MemTrans[2] = HighHex(lowByte(errors));
    MemTrans[3] = LowHex(lowByte(errors));
    MemTrans[4] = 0;
    Mem_len=4;  
  }
  else
  {
    MemTrans[1] = HighHex(highByte(errors));
    MemTrans[2] = LowHex(highByte(errors));
    MemTrans[3] = HighHex(lowByte(errors));
    MemTrans[4] = LowHex(lowByte(errors));
    MemTrans[5] = 0;
    Mem_len=5;
  }
  m_radio.transmit(MemTrans, Mem_len);
}
char HighHex(byte b)
{
  unsigned int i = b >> 4 ;
  return HexChar[i] ;
}
char LowHex(byte b)
{
  unsigned int i = b & 0x0F ;
  return HexChar[i] ;
}
