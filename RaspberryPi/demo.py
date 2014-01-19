#
# Fairywren Project Demo: 
#   -- how to let your Raspberry Pi and on-board Arduino talk to each other 
#
# Geekroo Technologies - http://geekroo.com
# 



from smbus import SMBus
from time import *
from Fairywren import Fairywren

import time

##############################################
# All above is the Fairywren Class code.
# Leave that untouched (unless you know what you are doing),
# and write your Fairywren-controlling rPi routines in place of what is below.

##############################################

bus = SMBus(1);     ###  Only works for the Raspberry Pi Revision 2
   
# The second parameter "5" below is I2C address 
# which should match the definition of I2C_SLAVE_ADDR in 
# Arduino sketch Fairywren.ino
fw1  = Fairywren(bus, 5)
 
fw1.setPinAsOutput(9)   

# The following code gives you a very simple example 
# how you can Read/Write Arduino pins

for x in range (0, 8):
    fw1.digitalWrite(9,1)          # LED on
    print fw1.digitalRead(9)       # Get LED status
    sleep(0.2)
    fw1.digitalWrite(9,0)          # LED off
    print fw1.digitalRead(9)       # Get LED status
    print fw1.analogRead(1)        # You can also read from A1, like this.
    sleep(1.5)
