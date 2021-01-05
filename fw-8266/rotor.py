import json
import sys
import network

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
import machine
import utime

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

    def init(self):
        self._pin_enabled.value(0)

        self.init_dir(-1)
        while not self.is_limit_switch_pressed():
            self.do_step(1)

        self.init_dir(1)
        while self.is_limit_switch_pressed():
            self.do_step(1)

        print("Limit switch: %s" % self._pin_limit_switch.value())
        self._pin_enabled.value(1)
        self._current_pos_deg=0

    def do_step(self, wait_ms):
        self._pin_step.value(1)
        utime.sleep_ms(wait_ms)
        self._pin_step.value(0)
        utime.sleep_ms(wait_ms)

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
    def move(self, pos_deg):
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

            self.do_step(2)
            steps_moved+=1

        self._pin_enabled.value(1)

        print("Steps moved %s:" % steps_moved)
        self._current_pos_deg=self._current_pos_deg+(steps_moved*self._step_size_deg*factor)

#
# socket server classes
#
class WebRequest:
    def __init__(self, f):
        self._header=[]
        self._method=""
        self._request_url=""
        self._f=f
        #self._request_url=str(f.readline().decode()).replace('\n','').replace('\r','')

    def get_request_url(self):
        return self._request_url

    def read_line(self):
        try:
            line=str(self._f.readline().decode()).replace('\n','').replace('\r','')
            self._request_url=line
            if line=='73' or line=='quit' or line=='bye' or line =='':
                return None

            return line
        except Exception as e:
            print("73 de DK9MBS")
            return None

class WebResponse:
    def __init__(self, conn):
        self._conn=conn

    def write_line(self, value):
        self._conn.send(value+"\n")

class WebServer:
    def __init__(self):
        pass

    def run(self, callback):
        s=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        s.setblocking(True)
        s.bind(('',4533))
        s.listen(5)

        while True:
            try:
                conn, addr = s.accept()
                response=WebResponse(conn)
                print('Got a connection from %s' % str(addr))
                #request = conn.recv(1024)

                f=conn.makefile('rwb', 0)
                request=WebRequest(f)

                while request.read_line()!=None:
                    print("Before callback")
                    callback(request, response)
                    print("After callback")


            except Exception as e:
                sys.print_exception(e)


            try:
                #response=("RPRT %s\n" % status_code)
                #conn.sendall(response)
                conn.close()
            except Exception as e:
                sys.print_exception(e)

def move_stepper(request, response):
    print("*** begin of move_stepper ***")

    command=str(request.get_request_url().split(" ")[0])
    print("request_url: %s detected command: %s" % (request.get_request_url(), command))

    if command=='P':
        print("start moving azi")
        stepper=SharedMemory.create_azi_stepper()
        print("Current position in degrees (before move) %s:" % stepper.get_current_pos_deg())
        #target_pos_deg=int(request.get_request_url().split("/")[4])
        tmp=(request.get_request_url().split(" ")[1]).replace(',','.')
        target_pos_deg=float(tmp)
        print("target position in degrees: %s" % target_pos_deg)
        stepper.move(target_pos_deg)
        print("Current position in degrees (after move) %s:" % stepper.get_current_pos_deg())
        response.write_line("RPRT 0")
    elif command=='p':
        #get the position
        print("sending rotor  current position")
        #response.write_line("get_pos:")
        #response.write_line("Azimuth: 0.000000")
        #response.write_line("Elevation: 0.000000")
        response.write_line("0.000000")
        response.write_line("0.000000")
    elif command=='S':
        # Stop the rotor
        response.write_line("RPRT 0")
    elif command.upper()=='INIT':
        print("start init")
        stepper=SharedMemory.create_azi_stepper()
        print("Current position in degrees (before move) %s:" % stepper.get_current_pos_deg())
        stepper.init()
        print("Current position in degrees (after move) %s:" % stepper.get_current_pos_deg())


    print("*** end move_stepper ***")

do_connect(cfg['wlan']['essid'], cfg['wlan']['password'])
WebServer().run(move_stepper)


