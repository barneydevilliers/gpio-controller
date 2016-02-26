
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>


//RFID RELATED
//------------
#define SS_PIN 10
#define RST_PIN 9
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;
byte nuidPICC[3]; // Init array that will store new NUID


//SERVO RELATED
//-------------
#define SERVO_PIN 4
Servo servo;


//COMMAND HANDLER RELATED
//-----------------------

enum COMMAND_HANDLER_STATE
{
  STATE_START_OF_PACKET  = 0,
  STATE_COMMAND          = 1,
  STATE_DATA_LENGTH      = 2,
  STATE_DATA_PAYLOAD     = 3,
  STATE_BCC              = 4
};

enum COMMAND_INSTRUCTION
{
  COMMAND_NONE                =  0,
  COMMAND_RESET               =  1,
  COMMAND_GET_FIRMWARE_INFO   =  2,
  COMMAND_RESPONSE_SUCCESS    =  3,
  COMMAND_RESPONSE_FAILURE    =  4,

  COMMAND_GPIO_SET_DIRECTION  =  5,
  COMMAND_GPIO_WRITE          =  6,
  COMMAND_GPIO_READ           =  7,

  COMMAND_SERVO_MOVE          =  8,

  COMMAND_RFID_READ_EVENT     =  9,
};

//-------------
//MAIN ELEMENTS
//-------------

void setup()
{


  //SERVO RELATED
  //-------------
  servo.attach(SERVO_PIN);

}

void loop()
{
  service_command_receiver();

  service_rfid_reader();
}

//-----------------------
//COMMAND HANDLER RELATED
//-----------------------

#define MAX_COMMAND_DATA 64

static COMMAND_INSTRUCTION received_command = COMMAND_NONE;
static byte                received_data[MAX_COMMAND_DATA];
static byte                received_data_length;

static COMMAND_INSTRUCTION transmit_command = COMMAND_NONE;
static byte                transmit_data[MAX_COMMAND_DATA];
static byte                transmit_data_length;

void service_command_processor()
{
  switch (received_command)
  {
    case COMMAND_NONE:
      //Nothing to be done.  Simply acknowledge it.
      service_command_respond_simple(COMMAND_RESPONSE_SUCCESS);
      break;
      
    case COMMAND_RESET:
      break;
      
    case COMMAND_GET_FIRMWARE_INFO:
      break;
      
    case COMMAND_GPIO_SET_DIRECTION:
      break;
      
    case COMMAND_GPIO_WRITE:
      break;
      
    case COMMAND_GPIO_READ:
      break;
      
    case COMMAND_SERVO_MOVE:
      break;

    default:
      //Unknown command.
      service_command_respond_simple(COMMAND_RESPONSE_FAILURE);
      break;
  }
}

void service_command_respond_simple(COMMAND_INSTRUCTION command)
{
  //Define the simple packet
  transmit_command = command;
  memset(transmit_data, 0, MAX_COMMAND_DATA);
  transmit_data_length = 0;

  service_command_send_packet();
}

void service_command_send_packet()
{
  //First we need to create the bcc value
  byte bcc = 0xA5;
  bcc ^= transmit_command;
  bcc ^= transmit_data_length;
  byte index;
  for (index = 0; index < transmit_data_length; index++)
  {
    bcc ^= transmit_data[index];
  }

  //Build and send out the packet
  Serial.write(0xA5);
  Serial.write(transmit_command);
  Serial.write(transmit_data_length);
  Serial.write(transmit_data,transmit_data_length);
  Serial.write(bcc);
}

void service_command_receiver()
{

  static COMMAND_HANDLER_STATE state = STATE_START_OF_PACKET;
  static byte indicated_data_length  = 0;
  static byte processed_data_length  = 0;
  static byte bcc                    = 0;

  while (Serial.available() > 0)
  {
    byte incomingByte = Serial.read();

    //Always update the packet bcc calculation
    bcc ^= incomingByte;


#warning do interpacket timeout check.

    switch (state)
    {
      case STATE_START_OF_PACKET:
        bcc = 0; //restart the bcc calculation here.
        if (0xA5 == incomingByte) //check if we got a good start of packet byte
        {
          state = STATE_COMMAND;
        }
        break;

      case STATE_COMMAND:
        received_command = (COMMAND_INSTRUCTION)incomingByte;
        state = STATE_DATA_LENGTH;
        break;

      case STATE_DATA_LENGTH:
        indicated_data_length = incomingByte;
        processed_data_length = 0; //reset the processed data length before we get the data
        memset(received_data, 0, MAX_COMMAND_DATA);

        if (0 == indicated_data_length)
        {
          //No data, simply go to BCC field.
          state = STATE_BCC;
        }
        else if (MAX_COMMAND_DATA >= indicated_data_length) //Check that we can hold that much data
        {
          //All good to get the payload
          state = STATE_DATA_PAYLOAD;
        }
        else
        {
          //Reject this packet immediately
          state = STATE_START_OF_PACKET;
        }
        break;

      case STATE_DATA_PAYLOAD:
        received_data[processed_data_length] = incomingByte;
        processed_data_length++;
        if (processed_data_length < indicated_data_length)
        {
          //remain here getting payload data
          state = STATE_DATA_PAYLOAD;
        }
        else
        {
          //done with the payload. Move on.
          received_data_length = processed_data_length;
          state = STATE_BCC;
        }
        break;

      case STATE_BCC:
        //Perform BCC Check
        if (0 == bcc)
        {
          //all is well, the the packet for futher processing and start state machine again.
          service_command_processor();
          state = STATE_START_OF_PACKET;
        }
        break;
    }
  }

}





//SERVO RELATED
//-------------

void servoManage(int newPosition)
{
  servo.write(newPosition);
  delay(15);
}


//RFID RELATED
//------------

void service_rfid_reader()
{
  // Look for new cards
  if ( ! rfid.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been readed
  if ( ! rfid.PICC_ReadCardSerial())
    return;

  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.println(rfid.PICC_GetTypeName(piccType));

  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
      piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
      piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    return;
  }

  if (rfid.uid.uidByte[0] != nuidPICC[0] ||
      rfid.uid.uidByte[1] != nuidPICC[1] ||
      rfid.uid.uidByte[2] != nuidPICC[2] ||
      rfid.uid.uidByte[3] != nuidPICC[3] ) {
    Serial.println(F("A new card has been detected."));

    // Store NUID into nuidPICC array
    for (byte i = 0; i < 4; i++) {
      nuidPICC[i] = rfid.uid.uidByte[i];
    }

    Serial.println(F("The NUID tag is:"));
    Serial.print(F("In hex: "));
    printHex(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
    Serial.print(F("In dec: "));
    printDec(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
  }
  else Serial.println(F("Card read previously."));

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}

/**
   Helper routine to dump a byte array as hex values to Serial.
*/
void printHex(byte *buffer, byte bufferSize)
{
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

/**
   Helper routine to dump a byte array as dec values to Serial.
*/
void printDec(byte *buffer, byte bufferSize)
{
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}
