#!/usr/bin/python

import serial
import time

class Commands:
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
  COMMAND_GPIO_INPUT_EVENT       = 13


def createAndOpenPort():
	port = serial.Serial(
		port='/dev/ttyACM0',
		baudrate=57600,
		parity=serial.PARITY_NONE,
		stopbits=serial.STOPBITS_ONE,
		bytesize=serial.EIGHTBITS
		)
	port.flushInput()
	port.flushOutput()
	#time.sleep(1) #might become required to wait for the board to power up.
	#perhaps just send a reset response packet to ensure all is well before continuing.
	return port

def sendCommand(port, command, data = []):
	#Build packet
	packet = []
	packet.append(0xA5)       #start of packet byte
	packet.append(command[0]) #command byte
	packet.append(len(data))  #data length
	for byte in data:         #data contents if any
		packet.append(byte)
	
	#Calculate and Append BCC value
	bcc = 0;
	for byte in packet:
		bcc ^= byte
		print str(byte) + " : " + str(bcc)
	packet.append(bcc)
	
	print str(packet)
	
	#Send to port
	packet_raw = bytearray(packet)
	port.write(packet_raw)


class ReceivedPacket:
	MAX_COMMAND_DATA  = 64
        START_OF_PACKET_BYTE = 0xA5
	
	STATE_START_OF_PACKET  = 0,
	STATE_COMMAND          = 1,
	STATE_DATA_LENGTH      = 2,
	STATE_DATA_PAYLOAD     = 3,
	STATE_BCC              = 4
	  
	state = STATE_START_OF_PACKET
	received_command = Commands.COMMAND_NONE
	received_data    = []
	bcc = 0
	indicated_data_length = 0;
	processed_data_length = 0;
	
	complete = False

	def processReceivedBytes(self,port):
		
		
		
		#Loop until all bytes in the serial receive buffer has been consumed.
		while port.inWaiting() > 0:
			incomingByte = ord(port.read(1))
			
			print "got byte " + hex(incomingByte)

	    #//Timeout calculations
	    #if (COMMAND_RECEIVER_INTERCHAR_TIMEOUT < timeSince(timeSinceLastRx))
	    #{
	    #  //Timeout all packets if there was a break in comms for more than the interchar timeout
	    #  state = STATE_START_OF_PACKET;
	    #}
	    #timeSinceLastRx = millis(); //Update time to now since we got a new byte

			#Update the packet bcc calculation
			self.bcc ^= incomingByte;

			if self.state == ReceivedPacket.STATE_START_OF_PACKET:
				if (ReceivedPacket.START_OF_PACKET_BYTE == incomingByte): #check if we got a good start of packet byte
					self.bcc = 0
					self.bcc ^= incomingByte #restart the bcc calculation here.
					self.state = ReceivedPacket.STATE_COMMAND
					
			elif self.state == ReceivedPacket.STATE_COMMAND:
				self.received_command = incomingByte
				self.state = ReceivedPacket.STATE_DATA_LENGTH


			elif self.state == ReceivedPacket.STATE_DATA_LENGTH:
				self.indicated_data_length = incomingByte
				self.processed_data_length = 0 #reset the processed data length before we get the data

				if (0 == self.indicated_data_length):
				  #No data, simply go to BCC field.
				  self.state = ReceivedPacket.STATE_BCC
				elif (ReceivedPacket.MAX_COMMAND_DATA >= self.indicated_data_length): #Check that we can hold that much data
				  #All good to get the payload
				  self.state = ReceivedPacket.STATE_DATA_PAYLOAD
				else:
				  #Reject this packet immediately
				  self.state = ReceivedPacket.STATE_START_OF_PACKET

			elif self.state == ReceivedPacket.STATE_DATA_PAYLOAD:
				self.received_data[processed_data_length] = incomingByte
				self.processed_data_length += 1
				if (self.processed_data_length < self.indicated_data_length):
				  #remain here getting payload data
				  self.state = ReceivedPacket.STATE_DATA_PAYLOAD
				else:
				  #done with the payload. Move on.
				  self.received_data_length = self.processed_data_length
				  self.state = ReceivedPacket.STATE_BCC

			elif self.state == ReceivedPacket.STATE_BCC:
				#Perform BCC Check
				if (0 == self.bcc):
				  #all is well, the the packet for futher processing and start state machine again.
				  print("Got a good response packet")
				  self.complete = True
				  self.state = ReceivedPacket.STATE_START_OF_PACKET
		
	
	
def readResponse():
	rxPacket = ReceivedPacket();
	while (rxPacket.complete == False):
		rxPacket.processReceivedBytes(port)
		if rxPacket.complete == False:
			time.sleep(0.05)
	



class dispenserManager():
	STATE_STARTUP            = 1
	STATE_WAITING_FOR_TAG    = 2
	STATE_WAITING_FOR_BUTTON = 3
	STATE_WAITING_FOR_SERVO  = 4



	def serviceStateMachine():
		if self.state == STATE_STARTUP:
			
		elif self.state == STATE_WAITING_FOR_TAG:
                elif self.state == STATE_WAITING_FOR_BUTTON:
                elif self.state == STATE_WAITING_FOR_SERVO:


port = createAndOpenPort();

sendCommand(port,Commands.COMMAND_SERVO_MOVE, [ 10, 90])
readResponse()
time.sleep(0.5)
sendCommand(port,Commands.COMMAND_SERVO_MOVE, [ 10, 30])
readResponse()
time.sleep(0.5)
sendCommand(port,Commands.COMMAND_SERVO_MOVE, [ 10, 150])
readResponse()


#What about retries?

#What about timeout on rx of packets?

#gpio read event (after it was debounced)


