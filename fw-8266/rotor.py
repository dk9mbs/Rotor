import json
try:
    import usocket as socket
except:
    import socket

try:
    import network
except:
    print("network not found!")

f=open('config.json')
cfg=json.loads(f.read())

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


class WebServer:
    def __init__(self):
        pass

    def run(self):
        html='''<!DOCTYPE html>
        <html>
        <head><title>AG5ZL</title></head>
        <center><h2>WebServer for turning LED on </h2></center>
        <form>
        <button name="LED" value='ON' type='submit'> LED ON </button>
        <button name="LED" value='OFF' type='submit'> LED OFF </button>
        <br><br>
        '''
        s=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        s.setblocking(True)
        s.bind(('',80))
        s.listen(5)

        while True:
            conn, addr = s.accept()
            print('Got a connection from %s' % str(addr))
            #request = conn.recv(1024)

            f=conn.makefile('rwb', 0)
            request=WebRequest(f)

            print(request._request_url)
            #read the first line with method request url and http version
            #req = str(f.readline().decode()).split(" ")
            #request_url=req[1]
            #print("Request URL: %s" % request_url)
            #read the header until end of header
            #while True:
            #    line = f.readline()
            #    if not line or line == b'\r\n':
            #        break

            # send the response

            response = html
            conn.send('HTTP/1.1 200 OK\n')
            conn.send('Content-Type: text/html\n')
            conn.send('Connection: close\n\n')
            conn.sendall(response)
            conn.close()


do_connect(cfg['wlan']['essid'], cfg['wlan']['password'])
WebServer().run()
