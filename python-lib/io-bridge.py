#!/usr/bin/python

import serial

	


port = serial.Serial(
	port='/dev/ttyACM0',
	baudrate=57600,
	stopbits=serial.STOPBITS_TWO,
	bytesize=serial.SEVENBITS
)

packet = [0xA5, 11, 2, 13, 90]

bcc = 0;
for byte in packet:
	bcc ^= byte
packet.append(bcc)
	

packet_raw = bytearray(packet)



print str(packet)

print port.isOpen()

#port.read
#while port.inWaiting() > 0:
	

#port.write(
