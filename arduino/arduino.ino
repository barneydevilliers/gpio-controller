#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>



//RFID RELATED
//------------
#define SS_PIN 53
#define RST_PIN 5
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;
byte nuidPICC[3]; // Init array that will store new NUID


//SERVO RELATED
//-------------
Servo servo;


//COMMAND HANDLER RELATED
//-----------------------

#define COMMAND_RECEIVER_INTERCHAR_TIMEOUT 200

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
  COMMAND_NONE                   =  0,
  COMMAND_RESET                  =  1,
  COMMAND_RESET_SUCCESS          =  2,
  COMMAND_FIRMWARE_INFO          =  3,
  COMMAND_FIRMWARE_INFO_RESPONSE =  4,
  COMMAND_RESPONSE_SUCCESS       =  5,
  COMMAND_RESPONSE_FAILURE       =  6,
  COMMAND_GPIO_SET_MODE          =  7,
  COMMAND_GPIO_WRITE             =  8,
  COMMAND_GPIO_READ              =  9,
  COMMAND_GPIO_READ_RESPONSE     = 10,
  COMMAND_SERVO_MOVE             = 11,
  COMMAND_RFID_READ_EVENT        = 12,
  COMMAND_GPIO_INPUT_EVENT       = 13,
};




//---------
// UTILITY
//---------

unsigned long timeSince(unsigned long timestamp)
{
  unsigned long time_now = millis();

  if (timestamp <= time_now)
  {
    //Normal case use values as is.
    return (time_now - timestamp);
  }
  else
  {
    //Wraparound occurred.
    timestamp = 0xFFFFFFFFL - timestamp; //get the time before the wrap
    return (time_now + timestamp);       //add time now plus the time before the wrap.
  }
}

#define DEBOUNCE_TIME 50
enum PIN_STATE
{
  PIN_STATE_LOW,
  PIN_STATE_DEBOUNCING_TO_HIGH,
  PIN_STATE_HIGH,
  PIN_STATE_DEBOUNCING_TO_LOW
};

enum PIN_EVENT
{
  PIN_EVENT_NONE,
  PIN_EVENT_HIGH,
  PIN_EVENT_LOW
};


class InputPinMonitor
{
  public:

    byte Pin()
    {
      return m_pin;
    }

    bool Active()
    {
      return m_active;
    }

    InputPinMonitor()
    {
      Configure(0, false);
    }
    
    void Configure(byte pin, bool active)
    {
      m_active = active;
      if (m_active)
      {
        m_pin = pin;

        pinMode(m_pin, INPUT);
        if (HIGH == digitalRead(m_pin))
        {
          m_state = PIN_STATE_HIGH;
        }
        else
        {
          m_state = PIN_STATE_LOW;
        }
      }
    }

    PIN_EVENT Service()
    {
      if (m_active)
      {
        switch (m_state)
        {
          case PIN_STATE_LOW:
            if (HIGH == digitalRead(m_pin))
            {
              m_debounceStartTimeStamp = millis();
              m_state = PIN_STATE_DEBOUNCING_TO_HIGH;
            }
            break;

          case PIN_STATE_DEBOUNCING_TO_HIGH:
            if (DEBOUNCE_TIME < timeSince(m_debounceStartTimeStamp))
            {
              if (HIGH == digitalRead(m_pin))
              {
                m_state = PIN_STATE_HIGH;
                return PIN_EVENT_HIGH;
              }
              else
              {
                //Debounced failed.  Seemed like spurious signal.  Return to previous state.
                m_state = PIN_STATE_LOW;
              }
            }
            break;

          case PIN_STATE_HIGH:
            if (HIGH == digitalRead(m_pin))
            {
              m_debounceStartTimeStamp = millis();
              m_state = PIN_STATE_DEBOUNCING_TO_LOW;
            }
            break;

          case PIN_STATE_DEBOUNCING_TO_LOW:
            if (DEBOUNCE_TIME < timeSince(m_debounceStartTimeStamp))
            {
              if (LOW == digitalRead(m_pin))
              {
                m_state = PIN_STATE_LOW;
                return PIN_EVENT_LOW;
              }
              else
              {
                //Debounced failed.  Seemed like spurious signal.  Return to previous state.
                m_state = PIN_STATE_HIGH;
              }
            }
            break;

          default:
            m_state = PIN_STATE_LOW;
            break;
        }
      }
      return PIN_EVENT_NONE;
    }

  private:
    bool      m_active;
    PIN_STATE m_state;
    byte      m_pin;
    long      m_debounceStartTimeStamp;
};



//-------------
//MAIN ELEMENTS
//-------------

void setup()
{
  //-----------------------
  //COMMAND HANDLER RELATED
  //-----------------------
  Serial.begin(57600);

  //------------
  //RFID RELATED
  //------------
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
  for (byte i = 0; i < 6; i++)
  {
    key.keyByte[i] = 0xFF;
  }
  

  //Send reset command response to indicate we have successfully started up.
  service_command_respond_simple(COMMAND_RESET_SUCCESS);
}

void loop()
{
  service_command_receiver();

  service_pin_monitor();

  service_rfid_reader();
}

#define INPUT_MONITORS 10
static InputPinMonitor monitors[INPUT_MONITORS];
  
bool configure_pin_monitor(byte pin)
{
  //Try to find a monitor that already monitors this pin.
  for (int i = 0; i < INPUT_MONITORS; i++)
  {
    if (monitors[i].Pin() == pin)
    {
      //Already monitoring this pin.  Return success and exit.
      return true;
    }
  }
  //Try to find a monitor that already monitors this pin.
  for (int i = 0; i < INPUT_MONITORS; i++)
  {
    if (!monitors[i].Active())
    {
      //Found an empty monitor, use it.
      monitors[i].Configure(pin,true);
      return true;
    }
  }
  //Did not find one monitoring this pin or could find an empty one to start. Failure.
  return false;
}

void service_pin_monitor()
{


  for (int i = 0; i < INPUT_MONITORS; i++)
  {
    PIN_EVENT event = monitors[i].Service();
    switch (event)
    {
      case PIN_EVENT_HIGH:
      {
        long value = (monitors[i].Pin() << 8) + 1;
        service_command_respond_with_value(COMMAND_GPIO_INPUT_EVENT, value);
        break;
      }
      case PIN_EVENT_LOW:
      {
         long value = (monitors[i].Pin() << 8) + 0;
        service_command_respond_with_value(COMMAND_GPIO_INPUT_EVENT, value);
        break;       
      }
    }
  }
}


//-----------------------
//COMMAND HANDLER RELATED
//-----------------------

#define MAX_COMMAND_DATA 64
#define START_OF_PACKET_BYTE 0xA5

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
#warning todo COMMAND_RESET
      break;

    case COMMAND_FIRMWARE_INFO:
#warning todo COMMAND_GET_FIRMWARE_INFO
      break;

    case COMMAND_GPIO_SET_MODE:
      if (2 == received_data_length)
      {
        if (OUTPUT == received_data[1])
        {
          pinMode(received_data[0], received_data[1]);
          service_command_respond_simple(COMMAND_RESPONSE_SUCCESS);
        }
        else if (INPUT == received_data[1])
        {
          //Configure input pin event monitor
          if (true == configure_pin_monitor(received_data[0]))
          {
            service_command_respond_simple(COMMAND_RESPONSE_SUCCESS);
          }
          else
          {
            service_command_respond_simple(COMMAND_RESPONSE_FAILURE);
          }
        }
        else
        {
          service_command_respond_simple(COMMAND_RESPONSE_FAILURE);
        }
      }
      else
      {
        //Not the correct number of data bytes in payload.
        service_command_respond_simple(COMMAND_RESPONSE_FAILURE);
      }
      break;

    case COMMAND_GPIO_WRITE:
      if (2 == received_data_length)
      {
        digitalWrite(received_data[0], received_data[1]);
        service_command_respond_simple(COMMAND_RESPONSE_SUCCESS);
      }
      else
      {
        //Not the correct number of data bytes in payload.
        service_command_respond_simple(COMMAND_RESPONSE_FAILURE);
      }
      break;

    case COMMAND_GPIO_READ:
      if (1 == received_data_length)
      {
        byte pinValue = digitalRead(received_data[0]);
        service_command_respond_with_value(COMMAND_GPIO_READ_RESPONSE, pinValue);
      }
      else
      {
        //Not the correct number of data bytes in payload.
        service_command_respond_simple(COMMAND_RESPONSE_FAILURE);
      }

    case COMMAND_SERVO_MOVE:
      if (2 == received_data_length)
      {
        servoManage(received_data[0], received_data[1]);
        service_command_respond_simple(COMMAND_RESPONSE_SUCCESS);
      }
      else
      {
        //Not the correct number of data bytes in payload.
        service_command_respond_simple(COMMAND_RESPONSE_FAILURE);
      }
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

void service_command_respond_with_value(COMMAND_INSTRUCTION command, unsigned long value)
{
  //Define the simple packet
  transmit_command = command;
  memset(transmit_data, 0, MAX_COMMAND_DATA);
  transmit_data[0] = 0xFF & (value >> 24);
  transmit_data[1] = 0xFF & (value >> 16);
  transmit_data[2] = 0xFF & (value >>  8);
  transmit_data[3] = 0xFF & (value);
  transmit_data_length = 4;

  service_command_send_packet();
}

void service_command_respond_with_byte_buffer(COMMAND_INSTRUCTION command, byte* data, byte dataLength)
{
  //Define the simple packet
  transmit_command = command;
  memset(transmit_data, 0, MAX_COMMAND_DATA);

  if (dataLength > MAX_COMMAND_DATA)
  {
    dataLength = MAX_COMMAND_DATA;
  }
  
  for (int i = 0; i < dataLength; i++)
  {
    transmit_data[i] = data[i];
  }
  transmit_data_length = dataLength;

  service_command_send_packet();
}

void service_command_send_packet()
{
  //First we need to create the bcc value
  byte bcc = START_OF_PACKET_BYTE;
  bcc ^= transmit_command;
  bcc ^= transmit_data_length;
  byte index;
  for (index = 0; index < transmit_data_length; index++)
  {
    bcc ^= transmit_data[index];
  }

  //Build and send out the packet
  Serial.write(START_OF_PACKET_BYTE);
  Serial.write(transmit_command);
  Serial.write(transmit_data_length);
  Serial.write(transmit_data, transmit_data_length);
  Serial.write(bcc);
}

void service_command_receiver()
{
  static COMMAND_HANDLER_STATE state   = STATE_START_OF_PACKET;
  static byte indicated_data_length    = 0;
  static byte processed_data_length    = 0;
  static byte bcc                      = 0;
  static unsigned long timeSinceLastRx = 0;

  //Loop until all bytes in the serial receive buffer has been consumed.
  while (Serial.available() > 0)
  {
    byte incomingByte = Serial.read();

    //Timeout calculations
    if (COMMAND_RECEIVER_INTERCHAR_TIMEOUT < timeSince(timeSinceLastRx))
    {
      //Timeout all packets if there was a break in comms for more than the interchar timeout
      state = STATE_START_OF_PACKET;
    }
    timeSinceLastRx = millis(); //Update time to now since we got a new byte

    //Update the packet bcc calculation
    bcc ^= incomingByte;

    switch (state)
    {
      case STATE_START_OF_PACKET:
        if (START_OF_PACKET_BYTE == incomingByte) //check if we got a good start of packet byte
        {
          bcc = 0;
          bcc ^= incomingByte; //restart the bcc calculation here.
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


// \param servo_pin Pin to be used for servo
// \param angle     Angle to send servo to
void servoManage(byte servo_pin, byte angle)
{
  servo.attach(servo_pin);
  servo.write(angle);
}


//RFID RELATED
//------------

void service_rfid_reader()
{
  // Look for new cards
  if ( ! rfid.PICC_IsNewCardPresent())
  {
    //Serial.println(F("No A "));
    return;
  }

  // Verify if the NUID has been readed
  if ( ! rfid.PICC_ReadCardSerial())
  {
    //Serial.println(F("No B "));
    return;
  }

  //Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  //Serial.println(rfid.PICC_GetTypeName(piccType));

  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
      piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
      piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    //Serial.println(F("Your tag is not of type MIFARE Classic."));
    return;
  }

                                              
  service_command_respond_with_byte_buffer(COMMAND_RFID_READ_EVENT, rfid.uid.uidByte, 4);

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}

