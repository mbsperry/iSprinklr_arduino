# For testing connection with the arduino. Essentially identical to spinkler_serial.py in the isprinklr_api project

import serial
import time
import random
import secrets

# Serial communication protocol:
# Incoming packets are 8 bytes, response is 7 bytes

# Handshake:
# [BEGIN][Conn ID][SYN][EMPTY][EMPTY][Checksum 2 byte][END]
# Response - 7 bytes
# [BEGIN][Conn ID][SYN][ACK][Checksum 2 bytes][END]
# [BEGIN][Conn ID]][ACK][EMPTY][EMPTY][Checksum 2 bytes][END]

# Command sequence
# [BEGIN][Conn ID][CMD][DATA 2 bytes][Checksum 2 bytes][END]
# [BEGIN][Conn ID][ACK][Empty byte][Checksum 2 bytes][END] or [BEGIN][Conn ID][ERR][ERR type][Checksum 2 bytes][END]

# Bad packets, improperly formated packet, data underun, etc, drop with timeout after 1500ms
# Server can resend packet if did not receive ACK response


BEGIN = b'\xff'
END = b'\xaf'
START_SPRINKLER = b'\x65'
STOP_SPRINKLER = b'\x72'
SYN = b'\xee'
ACK = b'\xae'
EMPTY = b'\x00'
ERR = b'\xdd'
BAD_CMD = b'\x69'
BAD_SPRINKLER = b'\x6f'
BAD_DURATION = b'\x70'

# Fletcher16 checksum
# Returns the checksum of the data at a 16 bit integer
def fletcher16(data):
  sum1 = 0
  sum2 = 0

  for byte in data:
      sum1 = (sum1 + byte) % 255
      sum2 = (sum2 + sum1) % 255

  checksum = (sum2 << 8) | sum1
  return checksum

# Wrapper function to the start the sprinkler
# Returns true if the sprinkler was started successfully
def startSprinkler(sprinkler, duration):
    cmd = START_SPRINKLER + sprinkler.to_bytes(1, byteorder='big') + duration.to_bytes(1, byteorder='big')
    return writeCmd(cmd)

# Wrapper function to stop the sprinkler
# Returns true if the sprinkler was stopped successfully
def stopSprinkler(sprinkler):
    cmd = STOP_SPRINKLER + sprinkler.to_bytes(1, byteorder='big') + EMPTY
    return writeCmd(cmd)

# Handshake with Arduino
# Send begin, conn_id, SYN, and 2 empty bytes, checksum, and END
# Receive begin, conn_id, SYN, ACK, checksum, and END
# Check that the received checksum = expectedChk
# Send ACK, 2 empty bytes, checksum, and END to complete handshake and return true
def handshake(arduino, conn_id):
    attempt = 0
    cmd = conn_id + SYN + EMPTY + EMPTY
    chk = fletcher16(cmd)
    byteStr = BEGIN + cmd + chk.to_bytes(2, byteorder='big') + END
    expectedChk = fletcher16(byteStr)
    while (attempt < 3):
        data = False
        attempt += 1
        print(f'Writing: {byteStr}')
        arduino.write(byteStr)
        arduino.flush()
        time.sleep(0.1)
        while (arduino.inWaiting() > 0):   
            data = arduino.read(7)
            print(f'Received {data}')
        if not data:
            time.sleep(0.1)
            continue
        # Check that the received checksum = expectedChk
        # The arduino returns the checksum in reverse order, hence using little endian
        if (data[1:6] == conn_id + SYN + ACK + expectedChk.to_bytes(2, byteorder='little')):
            cmd = conn_id + ACK + EMPTY + EMPTY
            chk = fletcher16(cmd)
            byteStr = BEGIN + cmd + chk.to_bytes(2, byteorder='big') + END
            arduino.write(byteStr)
            arduino.flush()
            return True
        else:
            time.sleep(0.1)
    print('Handshake failed')
    return False


# Write a command to the arduino
# Returns true if the command was written successfully
# Starts with handshake, if successful, send command
def writeCmd(cmd):
    # arduino = serial.serial_for_url('rfc2217://localhost:4000', baudrate=9600, timeout=1)
    arduino = serial.Serial('/dev/ttyACM0', 9600, timeout=1)
    conn_id = (int(time.time()) % 255).to_bytes(1, byteorder='big')
    cmd = conn_id + cmd
    if (handshake(arduino, conn_id)):
        attempt = 0
        chk = fletcher16(cmd)
        byteStr = BEGIN + cmd + chk.to_bytes(2, byteorder='big') + END
        expectedChk = fletcher16(byteStr)
        while  (attempt < 3):
            data = False
            attempt += 1
            arduino.write(byteStr)
            arduino.flush()
            if (arduino.inWaiting() > 0):   
                data = arduino.read(7)
            if (data):
                # Check that the received checksum = expectedChk
                # The arduino returns the checksum in reverse order, hendce using little endian
                if (data[1:6] == conn_id + ACK + b'\x00' + expectedChk.to_bytes(2, byteorder='little')):
                    arduino.close()
                    return True
            else:
                time.sleep(0.5)
    arduino.close()
    print('Command failed')
    return False

# Testing function
# write a string of random bytes to the arduino to make sure it doesn't get out of sync
def garbage():
    # First generate a random length string of random bytes
    length = random.randint(1, 30)
    noise = secrets.token_bytes(length)
    # Send garbage to the arduino to simulate noise
    # arduino = serial.serial_for_url('rfc2217://localhost:4000', baudrate=9600, timeout=1)
    arduino = serial.Serial('/dev/ttyACM0', 9600, timeout=1)
    arduino.write(noise)
    arduino.flush()
    arduino.close()

def test(withGarbage):
    i = 1
    passed = True
    while i < 9:
        print(f'Starting test {i}')
        if (withGarbage == True):
            garbage()

        sprinkler = random.randint(1, 8)
        duration = random.randint(1, 30)
        if (startSprinkler(sprinkler, duration) == False):
            print('Failed to start sprinkler')
            passed = False
            break
        time.sleep(0.2)
        if (stopSprinkler(sprinkler) == False):
            print('Failed to stop sprinkler')
            passed = False
            break
        print(f'Test {i} passed')
        i += 1
        time.sleep(0.2)
    print('Testing bad commands')
    if (startSprinkler(9, 5) == True):
        print('Bad sprinkler test failed')
        passed = False
    else:
        print('Bad sprinkler test passed')
    print 
    if (startSprinkler(5, 61) == True):
        print('Bad duration test failed')
        passed = False
    else:
        print('Bad duration test passed')
    if (passed == True):
        print('Testing complete')
        print('*** All tests passed ***')
    else:  
        print('Testing complete')
        print('!!! One or more tests failed !!!')


if __name__ == '__main__':
    print('Starting tests')
    print('Testing without garbage')
    test(False)
    print('Testing with garbage')
    test(True)
