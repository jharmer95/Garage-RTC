from flask import Flask, render_template
from flask_socketio import SocketIO
from datetime import datetime
import json
import socket
import sqlite3
import time

app = Flask(__name__)
app.config['SECRET_KEY'] = 'vnkdjnfjknfl1232#'
socketio = SocketIO(app)

STATUS_VALS = []
SETTING_VALS = []
BUF_STATUS_VALS = []
BUF_SETTING_VALS = []
UDP_IP = ''
UDP_PORT = 0
UDP_TIMEOUT = 0.0


@app.before_first_request
def init():
    loadLocalSettings()


@app.route('/')
def sessions():
    return render_template('index.html')


def messageReceived(methods=['GET', 'POST']):
    print('message was received!!!')


@app.route('/status')
def status():
    return render_template('status.html')


@app.route('/settings')
def settings():
    return render_template('settings.html')


@socketio.on('getStatus')
def handle_getStatus_event():
    global STATUS_VALS

    jStr = json.dumps(STATUS_VALS)
    socketio.emit('updateStatus', jStr, callback=messageReceived)


@socketio.on('refreshStatus')
def handle_refreshStatus_event():
    receiveStatus()


@socketio.on('setStatus')
def handle_setStatus_event(jStr):
    global STATUS_VALS

    print(jStr)

    for setting in jStr:
        name = setting['name']
        val = setting['value']
        setStatus(name, val)

    sendStatus()
    receiveStatus()


@socketio.on('getSettings')
def handle_getSettings_event():
    global SETTING_VALS

    jStr = json.dumps(SETTING_VALS)
    socketio.emit('updateSettings', jStr, callback=messageReceived)


@socketio.on('setSettings')
def handle_setSettings_event(jStr):
    global SETTING_VALS

    conn = sqlite3.connect('data/settings.db')

    print('Received settings request: ' + str(jStr))
    c = conn.cursor()

    for setting in jStr:
        name = setting['name']
        val = setting['value']
        print(val + ' ' + name)
        c.execute('UPDATE settings SET value=? WHERE name=?', [val, name])

    conn.commit()
    c.execute('SELECT * FROM settings')
    BUF_SETTING_VALS = sqlToDictList(c.fetchall())
    conn.close()
    sendSettings()


def sqlToDictList(results):
    outList = []

    for r in results:
        dictObj = {}
        dictObj['name'] = r[0]
        dictObj['value'] = r[1]
        outList.append(dictObj)

    return outList


def loadLocalSettings():
    global BUF_SETTING_VALS

    conn = sqlite3.connect('data/settings.db')
    c = conn.cursor()
    c.execute('SELECT * FROM settings')
    valStr = c.fetchall()
    BUF_SETTING_VALS = sqlToDictList(valStr)
    conn.close()
    updateLocalSettings()


def updateLocalSettings():
    global UDP_IP
    global UDP_PORT
    global UDP_TIMEOUT

    UDP_IP = str(getSetting('udpIP'))
    UDP_PORT = int(getSetting('udpPort'))
    UDP_TIMEOUT = float(getSetting('udpTimeout'))


def getSetting(name):
    global SETTING_VALS

    for val in SETTING_VALS:
        if val['name'] == name:
            return val['value']

    return None


def setStatus(name, value):
    global BUF_STATUS_VALS

    for val in BUF_STATUS_VALS:
        if val['name'] == name:
            val['value'] = value


def sendSettings():
    global BUF_SETTING_VALS
    global UDP_IP
    global UDP_PORT

    jStr = json.dumps(BUF_SETTING_VALS)
    print('Sending message: "' + jStr + '" to IP: ' +
        str(UDP_IP) + ':' + str(UDP_PORT))

    cmdStr = '{"cmd": "setStatus", "arg": "' + jStr + '"}'
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(cmdStr.encode('utf-8'), (UDP_IP, UDP_PORT))


def receiveSettings():
    global SETTING_VALS
    global UDP_IP
    global UDP_PORT
    global UDP_TIMEOUT

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(UDP_TIMEOUT)

    sock.sendto('{"cmd": "getSettings", "arg": ""}'.encode('utf-8'), (UDP_IP, UDP_PORT))

    try:
        data, server = sock.recvfrom(1024)
        print("data: " + str(data) + "\nserver: " + str(server))
        SETTING_VALS = data
        updateLocalSettings()
    except socket.timeout:
        print('REQUEST TIMED OUT!')


def receiveStatus():
    global STATUS_VALS
    global UDP_IP
    global UDP_PORT
    global UDP_TIMEOUT

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(UDP_TIMEOUT)

    mesg = '{"cmd": "getStatus", "arg": ""}'
    print('Sending message to ' + str(UDP_IP) + ':' + str(UDP_PORT))
    sock.sendto(mesg.encode('utf-8'), (UDP_IP, UDP_PORT))
    sock.close()
    try:
        sock2 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock2.settimeout(UDP_TIMEOUT)
        sock2.bind(('', UDP_PORT))
        data, server = sock2.recvfrom(1024)
        print("data: " + str(data) + "\nserver: " + str(server))
        STATUS_VALS = data
    except socket.timeout:
        print('REQUEST TIMED OUT!')


def sendStatus():
    global BUF_STATUS_VALS
    global UDP_IP
    global UDP_PORT

    jStr = json.dumps(BUF_STATUS_VALS)
    print('Sending message: "' + jStr + '" to IP: ' +
        str(UDP_IP) + ':' + str(UDP_PORT))

    cmdStr = '{"cmd": "setStatus", "arg": "' + jStr + '"}'
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(cmdStr.encode('utf-8'), (UDP_IP, UDP_PORT))


if __name__ == '__main__':
    socketio.run(app, debug=True, port=5555, host='0.0.0.0')
