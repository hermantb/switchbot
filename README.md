# switchbot

Control switchbot curtains.

The tool is based on:
https://github.com/OpenWonderLabs/python-host.git

I wrote a c-version instead of a python version.

On the raspberry pi do:

make

You can find the mac address with ./switchbot scan<br>
These can also be found in the switchbot app.

To close the curtains do ./switchbot close mac<br>
To open the curtains do ./switchbot open mac<br>
To postion the curtains do ./switchbot <position in %> mac

The tool uses bluetoothctl and gatttool tools.<br>
These should be present on the raspberry pi.
