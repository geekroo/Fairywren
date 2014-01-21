#
# Fairywren Project: 
#   -- Safely Turn Off the System
#
# Geekroo Technologies - http://geekroo.com
# 

from smbus import SMBus
from time import *
from Fairywren import Fairywren

import time
import os

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

# Send "Ready" status
fw1.ackRpiReady()
print "Rpi to Fairywren Ready" 
time.sleep(1)
while True:
    # feed WTD
    fw1.ackRpiWTD() 
    #print "ackRpiWTD"   
    time.sleep(1)
    poweroff = fw1.ReadRpiPOFF()
    #print poweroff
    if(poweroff==0x55):
        print "shutdown now" 
        os.system('sudo shutdown -h now')
    time.sleep(3)
