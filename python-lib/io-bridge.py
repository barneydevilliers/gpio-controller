#!/usr/bin/python

import serial
import time

	


port = serial.Serial(
	port='/dev/ttyACM0',
	baudrate=57600,
	parity=serial.PARITY_NONE,
	stopbits=serial.STOPBITS_ONE,
	bytesize=serial.EIGHTBITS
)
port.flushInput()
port.flushOutput()
time.sleep(2)

packet = [0xA5, 11, 2, 13, 90]

#Append BCC value
bcc = 0;
for byte in packet:
	bcc ^= byte
	print str(byte) + " : " + str(bcc)
packet.append(bcc)
	

packet_raw = bytearray(packet)

port.write(packet_raw)


print str(packet)


time.sleep(1)

while port.inWaiting() > 0:
	received_byte = port.read(1)
	print "got byte " + hex(ord(received_byte))

time.sleep(1)

while port.inWaiting() > 0:
	received_byte = port.read(1)
	print "got byte " + hex(ord(received_byte))


#port.write(
