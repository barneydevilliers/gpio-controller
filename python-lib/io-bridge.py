#!/usr/bin/python

import serial
import time
from time import time
from time import sleep

CommandIds = {
  "COMMAND_NONE"                   :0,
  "COMMAND_RESET"                  :1,
  "COMMAND_RESET_SUCCESS"          :2,
  "COMMAND_FIRMWARE_INFO"          :3,
  "COMMAND_FIRMWARE_INFO_RESPONSE" :4,
  "COMMAND_RESPONSE_SUCCESS"       :5,
  "COMMAND_RESPONSE_FAILURE"       :6,
  "COMMAND_GPIO_SET_MODE"          :7,
  "COMMAND_GPIO_WRITE"             :8,
  "COMMAND_GPIO_READ"              :9,
  "COMMAND_GPIO_READ_RESPONSE"     :10,
  "COMMAND_SERVO_MOVE"             :11,
  "COMMAND_RFID_READ_EVENT"        :12,
  "COMMAND_GPIO_INPUT_EVENT"       :13
}


def TimeSince(timestamp):
	return time() - timestamp

class ReceivedPacket:
	MAX_COMMAND_DATA  = 64
        START_OF_PACKET_BYTE = 0xA5
	
	STATE_START_OF_PACKET  = 0,
	STATE_COMMAND          = 1,
	STATE_DATA_LENGTH      = 2,
	STATE_DATA_PAYLOAD     = 3,
	STATE_BCC              = 4
	  
	state = STATE_START_OF_PACKET
	received_command = CommandIds["COMMAND_NONE"]
	received_data    = []
	bcc = 0
	indicated_data_length = 0
	processed_data_length = 0
	packetStartTime = time()
	
	complete = False

	def processReceivedBytes(self,port):
		#Loop until all bytes in the serial receive buffer has been consumed.
		while port.inWaiting() > 0:
			incomingByte = ord(port.read(1))
			#print incomingByte

			#Packet Timeout calculations
			if 0.2 < TimeSince(self.packetStartTime):
				state = ReceivedPacket.STATE_START_OF_PACKET
			self.packetStartTime = time() #Update time to now since we got a new byte

			#Update the packet bcc calculation
			self.bcc ^= incomingByte;

			if self.state == ReceivedPacket.STATE_START_OF_PACKET:
				if (ReceivedPacket.START_OF_PACKET_BYTE == incomingByte):
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
				elif (ReceivedPacket.MAX_COMMAND_DATA >= self.indicated_data_length):
					#Check that we can hold that much data
				  	#All good to get the payload
					self.received_data = []
					self.processed_data_length = 0
				  	self.state = ReceivedPacket.STATE_DATA_PAYLOAD
				else:
				  	#Reject this packet immediately
				  	self.state = ReceivedPacket.STATE_START_OF_PACKET

			elif self.state == ReceivedPacket.STATE_DATA_PAYLOAD:
				self.received_data.append(incomingByte)
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
				  	self.complete = True
				  	self.state = ReceivedPacket.STATE_START_OF_PACKET
					return (True, self.received_command, self.received_data)
		return (False, None, None)




class PortProtocol:

	port = None
        rxPacket = ReceivedPacket()

	def createAndOpenPort(self):
		self.port = serial.Serial(
			port='/dev/ttyACM0',
			baudrate=57600,
			parity=serial.PARITY_NONE,
			stopbits=serial.STOPBITS_ONE,
			bytesize=serial.EIGHTBITS
			)
		self.port.close()
		self.port.open()

		self.port.flushInput()
		self.port.flushOutput()

	def displayReceivedCommand(self, Command, Data):
		for commandName, commandId in CommandIds.iteritems():
		    	if commandId == Command:
				print commandName + " : " +str(Data)
		

	def readResponse(self):
		startTime = time()
		Complete = False
		Command  = None
		Data     = None
        	while ((Complete == False) & (TimeSince(startTime) < 0.2)):
                	Complete, Command, Data = self.rxPacket.processReceivedBytes(self.port)
                	if Complete == False:
				sleep(0.05)
		if Complete == True:
			self.displayReceivedCommand(Command,Data)
		return (Complete, Command, Data)


	def sendAndConfirmCommand(self, command, data = []):
		Complete = False
		Command  = None
		Data     = None
		Attempts = 0
		while (Command != CommandIds["COMMAND_RESPONSE_SUCCESS"]) & (Attempts < 3):
			self.sendCommand(command, data)
			Complete, Command, Data = self.readResponse()
			Attempts += 1
		if Command == CommandIds["COMMAND_RESPONSE_SUCCESS"]:
			return True
		else:
			return False

	def sendCommand(self, command, data = []):
		#Build packet
		packet = []
		packet.append(0xA5)       #start of packet byte
		packet.append(command) #command byte
		packet.append(len(data))  #data length
		for byte in data:         #data contents if any
			packet.append(byte)
		
		#Calculate and Append BCC value
		bcc = 0;
		for byte in packet:
			bcc ^= byte
		packet.append(bcc)
	
		#print "sending " + str(packet)
	
		#Send to port
		packet_raw = bytearray(packet)
		self.port.write(packet_raw)




class dispenserManager():
	STATE_STARTUP            = 1
	STATE_WAITING_FOR_TAG    = 2
	STATE_WAITING_FOR_BUTTON = 3
	STATE_WAITING_FOR_SERVO  = 4



	def serviceStateMachine():
		if self.state == STATE_STARTUP:
			print "a"
		elif self.state == STATE_WAITING_FOR_TAG:
			print "a"
                elif self.state == STATE_WAITING_FOR_BUTTON:
			print "a"
                elif self.state == STATE_WAITING_FOR_SERVO:
			print "a"


port = PortProtocol();
port.createAndOpenPort()


#1 is OUTPUT
#0 is INPUT
port.sendAndConfirmCommand(CommandIds["COMMAND_GPIO_SET_MODE"], [ 30, 0])
port.sendAndConfirmCommand(CommandIds["COMMAND_GPIO_SET_MODE"], [ 31, 0])
port.sendAndConfirmCommand(CommandIds["COMMAND_GPIO_SET_MODE"], [ 32, 0])
port.sendAndConfirmCommand(CommandIds["COMMAND_GPIO_SET_MODE"], [ 33, 0])
port.sendAndConfirmCommand(CommandIds["COMMAND_GPIO_SET_MODE"], [ 34, 0])
port.sendAndConfirmCommand(CommandIds["COMMAND_GPIO_SET_MODE"], [ 35, 0])
port.sendAndConfirmCommand(CommandIds["COMMAND_GPIO_SET_MODE"], [ 36, 0])
port.sendAndConfirmCommand(CommandIds["COMMAND_GPIO_SET_MODE"], [ 37, 0])
port.sendAndConfirmCommand(CommandIds["COMMAND_GPIO_SET_MODE"], [ 38, 0])
port.sendAndConfirmCommand(CommandIds["COMMAND_GPIO_SET_MODE"], [ 39, 0])

while (True):
	port.readResponse()

#print port.sendAndConfirmCommand(CommandIds["COMMAND_SERVO_MOVE"], [ 10, 90])
#print port.sendAndConfirmCommand(CommandIds["COMMAND_SERVO_MOVE"], [ 10, 30])
#print port.sendAndConfirmCommand(CommandIds["COMMAND_SERVO_MOVE"], [ 10, 150])


#gpio read event (after it was debounced)


