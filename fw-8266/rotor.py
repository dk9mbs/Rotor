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

def run_webserver():
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
        request = conn.recv(1024)
        request = str(request)
        print('Content = %s' % request)
        led_on = request.find('/?led=on')
        led_off = request.find('/?led=off')
        if led_on == 6:
            print('LED ON')
            #led.value(1)
        if led_off == 6:
            print('LED OFF')
            #led.value(0)
        response = html
        conn.send('HTTP/1.1 200 OK\n')
        conn.send('Content-Type: text/html\n')
        conn.send('Connection: close\n\n')
        conn.sendall(response)
        conn.close()


do_connect(cfg['wlan']['essid'], cfg['wlan']['password'])
run_webserver()
