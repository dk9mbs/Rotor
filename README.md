https://forum.pycom.io/topic/4625/simple-http-server

micropython	
========================

```
cd /tmp/venv
python3.7 -m venv micropython
. /tmp/venv/micropython/bin/activate

pip install esptool
pip install adafruit-ampy

esptool.py --port /dev/ttyUSB0 erase_flash
esptool.py --port /dev/ttyUSB0 --baud 115200 write_flash --flash_size=detect 0 /tmp/esp8266-20200911-v1.13.bin


pip freeze
```

## Access to the python console

### Start picocom

```

ampy -p /dev/ttyUSB0 run --no-output rotor.py 
picocom --baud 115200 /dev/ttyUSB0

wget -O - --header "Rotor-Deg: 145" http://192.168.2.113/api/v1.0/rotor/0

```

### Exit picocom

```
ctrl-a ctrl-x
```



## Quellen

[1. Getting started with MicroPython on the ESP8266 — MicroPython 1.13 documentation](https://docs.micropython.org/en/latest/esp8266/tutorial/intro.html)

[Unable to exit serial tool · Issue #67 · amperka/ino · GitHub](https://github.com/amperka/ino/issues/67)

<https://www.digikey.com/en/maker/projects/micropython-basics-load-files-run-code/fb1fcedaf11e4547943abfdd8ad825ce>





https://github.com/micropython/micropython-lib/blob/master/traceback/traceback.py
