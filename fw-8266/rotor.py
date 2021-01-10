import json
import sys
import network
import machine
import utime

try:
    import usocket as socket
except:
    import socket


f=open('config.json')
cfg=json.loads(f.read())

#
# WLAN functions
#
def do_connect(essid, password):
    sta_if = network.WLAN(network.STA_IF)
    ap_if = network.WLAN(network.AP_IF)
    ap_if.active(False)
    print("starting network ...")
    if not sta_if.isconnected():
        print('connecting to network...')
        sta_if.active(True)
        sta_if.connect(essid, password)
        while not sta_if.isconnected():
            print(".")
            pass
    print('network config:', sta_if.ifconfig())


#
# Shared Memory class for exchange objects and values between instances
#
class SharedMemory:
    _azi_stepper=None

    @classmethod
    def create_azi_stepper(cls):
        if cls._azi_stepper==None:
            print("Create new azi stepper instance")
            cls._azi_stepper=Stepper(step_size_deg=0.06, step_period_ms=4)
            cls._azi_stepper.init()

        return cls._azi_stepper

#
# Stepper 8255 class
#


class Stepper:
    def __init__(self,**kwargs):
        self._step_size_deg=float(kwargs['step_size_deg'])
        self._step_period_ms=int(kwargs['step_period_ms'])
        self._pin_step = machine.Pin(12, machine.Pin.OUT)
        self._pin_dir = machine.Pin(13, machine.Pin.OUT)
        self._pin_enabled = machine.Pin(14, machine.Pin.OUT)
        self._pin_limit_switch = machine.Pin(5, machine.Pin.IN, machine.Pin.PULL_UP)
        self._current_pos_deg=0

    def get_pin_enabled(self):
        return self._pin_enabled

    def get_pin_step(self):
        return self._pin_step

    def get_pin_dir(self):
        return self._pin_dir

    def get_pin_limit_switch(self):
        return self._pin_limit_switch

    def get_current_pos_deg(self):
        return self._current_pos_deg

    async def init(self):
        #self._pin_enabled.value(0)
        await self.activate()

        # move the axis backwards
        self.init_dir(-1)
        moved_deg=0
        while not self.is_limit_switch_pressed():
            await self.do_step(1)
            moved_deg+=self._step_size_deg
            if moved_deg >=360:
                await self.deactivate()
                return False

        # move the axis forewards
        self.init_dir(1)
        while self.is_limit_switch_pressed():
            await self.do_step(1)

        for _ in range(0,100):
            await self.do_step(1)

        print("Limit switch: %s" % self._pin_limit_switch.value())
        #self._pin_enabled.value(1)
        await self.deactivate()
        self._current_pos_deg=0

        return True

    async def deactivate(self):
        self._pin_enabled.value(1)

    async def activate(self):
        self._pin_enabled.value(0)

    async def do_step(self, wait_ms):
        self._pin_step.value(1)
        await uasyncio.sleep_ms(wait_ms)
        #utime.sleep_ms(wait_ms)
        self._pin_step.value(0)
        await uasyncio.sleep_ms(wait_ms)
        #utime.sleep_ms(wait_ms)
    #
    # set the direction by the factor:
    # 1=forward (for example: 270 -> 360)
    # -1=back (for example: 360 -> 270)
    #
    def init_dir(self, factor):
        if factor==1:
            self._pin_dir.value(0)
        elif factor==-1:
            self._pin_dir.value(1)

    #
    # return True or False
    #
    def is_limit_switch_pressed(self):
        if self.get_pin_limit_switch().value()==1:
            return True
        else:
            return False

    #
    # Move the stepper to the target possition (deg)
    #
    async def move(self, pos_deg):
        if self._current_pos_deg<pos_deg:
            factor=1
            self.init_dir(factor)
            delta_deg=pos_deg-self._current_pos_deg
        else:
            factor=-1
            self.init_dir(factor)
            delta_deg=self._current_pos_deg-pos_deg

        # calculate the steps from degrees to steps
        delta_steps=int(delta_deg/self._step_size_deg)
        self._pin_enabled.value(0)

        print("Moving stepper: %s" % delta_steps)
        steps_moved=0
        for x in range(0, delta_steps):
            if self.is_limit_switch_pressed():
                print("limit switch detected")
                break

            await self.do_step(0)
            steps_moved+=1

        self._pin_enabled.value(1)

        print("Steps moved %s:" % steps_moved)
        self._current_pos_deg=self._current_pos_deg+(steps_moved*self._step_size_deg*factor)

#
# http server classes
#
class WebRequest:
    def __init__(self, f):
        self._header=[]
        self._method=""
        self._request_url=""
        #read the first line with method request url and http version
        req = str(f.readline().decode()).split(" ")
        self._request_url=req[1]
        #read the header until end of header
        while True:
            line = f.readline()
            self._header.append(line)
            if not line or line == b'\r\n':
                break

    def get_request_url(self):
        return self._request_url

class WebServer:
    def __init__(self, host, port):
        self._host=host
        self._port=int(port)

    async def run(self, callback):
        html='''{"status": "ok"}\n'''
        s=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        s.setblocking(True)
        s.bind(('',self._port))
        s.listen(1)

        while True:
            status_code="200 OK"

            try:
                conn, addr = s.accept()
                print('Got a connection from %s' % str(addr))
                #request = conn.recv(1024)

                f=conn.makefile('rwb', 0)
                request=WebRequest(f)

                print("Before callback")
                #callback(request)
                task=uasyncio.create_task(callback(request))
                await task
                print("After callback")

            except Exception as e:
                status_code="500 Error in callback"
                sys.print_exception(e)


            try:
                response = html
                conn.send("HTTP/1.1 %s\n" % status_code)
                conn.send('Content-Type: text/json\n')
                conn.send('Connection: close\n\n')
                conn.sendall(response)
                conn.close()
            except Exception as e:
                sys.print_exception(e)

class BaseCommand:
    def __init__(self, request_url):
        self._request_url=request_url

    def get_command(self):
        return "DUMMY"

    def get_arg_by_id(self):
        return "DUMMY"

class HTTPCommand(BaseCommand):
    def __init__(self, request_url):
        super().__init__(request_url)

    def get_command(self):
        return str(self._request_url.split("/")[3]).upper()

    def get_arg_by_id(self, id):
        return int(self._request_url.split("/")[id])

async def move_stepper(request):
    print("*** begin of move_stepper ***")
    cmd=HTTPCommand(request.get_request_url())
    print("request_url %s" % request.get_request_url())

    if cmd.get_command()=='AZI':
        print("start moving azi")
        stepper=SharedMemory.create_azi_stepper()
        print("Current position in degrees (before move) %s:" % stepper.get_current_pos_deg())
        #target_pos_deg=int(request.get_request_url().split("/")[4])
        target_pos_deg=cmd.get_arg_by_id(4)
        print("target position in degrees: %s" % target_pos_deg)
        await stepper.move(target_pos_deg)
        print("Current position in degrees (after move) %s:" % stepper.get_current_pos_deg())
    elif cmd.get_command()=='INIT':
        print("start init")
        stepper=SharedMemory.create_azi_stepper()
        print("Current position in degrees (before move) %s:" % stepper.get_current_pos_deg())
        result = await stepper.init()
        if not result:
            print("Error init stepper")
            raise NameError('Error in init: no limit switch detected!')
        
        print("Current position in degrees (after move) %s:" % stepper.get_current_pos_deg())
    elif cmd.get_command()=='AZI-POSITION':
        pass

    print("*** end move_stepper ***")

do_connect(cfg['wlan']['essid'], cfg['wlan']['password'])

import uasyncio
async def main(host, port):
    print("starting server ...")
    server=WebServer(host, port)
    uasyncio.run(server.run(move_stepper))

uasyncio.run(main('', 80))

