'''***************************************************************************
          File: main.py
   Description: Implements a web page for the IoT Garage Door controller.  See
                https://github.com/jharmer95/Garage-RTC/ for details on the
                Open GarageRTC project.
       Authors: Daniel Zajac,  danzajac@umich.edu
                Jackson Harmer, jharmer@umich.edu

***************************************************************************'''

'''***************************************************************************
*     Include libraries and references
*
***************************************************************************'''

'''***************************************************************************
*     Library: flask
*      Author: Armin Ronacher
*      Source: http://flask.pocoo.org/
*     Version: 1.0.2
* Description: Flask is a microframework for Python based on Werkzeug, Jinja 2
*              and good intentions. And before you ask: It's BSD licensed!
***************************************************************************'''

'''***************************************************************************
*     Library: Flask Socket IO extentions
*      Author: Miguel Grinberg
*      Source: https://flask-socketio.readthedocs.io/en/latest/
*     Version: n/a
* Description: Flask-SocketIO gives Flask applications access to low latency
*              bi-directional communications between the clients and the
*              server. The client-side application can use any of the
*              SocketIO official clients libraries in Javascript, C++, Java
*              and Swift, or any compatible client to establish a permanent
*              connection to the server.
***************************************************************************'''

'''***************************************************************************
*     Library: Python JSON library
*      Author: Python Software Foundation
*      Source: https://docs.python.org/2/library/json.html
*     Version: 2.7
* Description: Provides JSON encoding/decoding capabilities in Python
***************************************************************************'''

'''***************************************************************************
*     Library: Python Socket Library
*      Author: Python Software Foundation
*      Source: https://docs.python.org/2/library/socket.html
*     Version: 2.7
* Description: Provides networking sockets in Python.
***************************************************************************'''

'''***************************************************************************
*     Library: sqlite3 Database Library
*      Author: Python Software Foundation
*      Source: https://docs.python.org/2/library/sqlite3.html
*     Version: 2.7
* Description: Provides SQL Lite database connectivity in python.
***************************************************************************'''

from flask import Flask, render_template
from flask_socketio import SocketIO
import json
import socket
import sqlite3
app = Flask(__name__)
app.config['SECRET_KEY'] = 'vnkdjnfjknfl1232#'
socketio = SocketIO(app)

STATUS_VALS = []
SETTING_VALS = []
BUF_STATUS_VALS = [{'name': 'lightStatus', 'value': False},
                {'name': 'doorStatus', 'value': False},
                {'name': 'alarmStatus', 'value': False}]
UDP_IP = '192.168.1.155'
UDP_PORT = 1234
UDP_TIMEOUT = 1


@app.before_first_request
def init():
    """Initialization function to perform certain actions before a client can connect"""
    loadLocalSettings()


@app.route('/')
def sessions():
    """Renders the root document (index.html)"""
    return render_template('index.html')


def messageReceived(methods=['GET', 'POST']):
    """Callback method to indicate a successful message was received"""
    print('message was received!!!')


@app.route('/status')
def status():
    """Redirects the /status URL to render status.html"""
    return render_template('status.html')


@app.route('/settings')
def settings():
    """Redirects the /settings URL to render settings.html"""
    return render_template('settings.html')


@socketio.on('getStatus')
def handle_getStatus_event():
    """Sends the current status values to the client"""
    global STATUS_VALS
    socketio.emit('updateStatus', str(STATUS_VALS), callback=messageReceived)


@socketio.on('refreshStatus')
def handle_refreshStatus_event():
    """Handles the refreshStatus even to fetch the status from the controller"""
    receiveStatus()


def convert(input):
    """
    from https://stackoverflow.com/questions/13101653/python-convert-complex-dictionary-of-strings-from-unicode-to-ascii
    Converts unicode characters to ASCII to be compatible with the ESP32

    :param input: object (dict, list) or unicode string to convert
    :returns: converted ASCII string
    """
    if isinstance(input, dict):
        return dict((convert(key), convert(value)) for key, value in input.iteritems())
    elif isinstance(input, list):
        return [convert(element) for element in input]
    elif isinstance(input, unicode):
        return input.encode('utf-8')
    else:
        return input


@socketio.on('setStatus')
def handle_setStatus_event(jStr):
    """
    Handles the setStatus event to parse a JSON string and send it to the controller

    :param jStr: JSON object to be parsed (format: list of dictionaries: {'name': <statusName>, 'value': <statusValue>})
    """
    out = convert(jStr)

    for setting in out:
        name = setting['name']
        val = setting['value']
        setStatus(name, val)

    sendStatus()
    receiveStatus()


@socketio.on('getSettings')
def handle_getSettings_event():
    """Handles the getSettings event to send the current configured setting values to the client"""
    global SETTING_VALS

    jStr = json.dumps(SETTING_VALS)
    socketio.emit('updateSettings', jStr, callback=messageReceived)


@socketio.on('setSettings')
def handle_setSettings_event(jStr):
    """
    Handles the setSettings even to update the current configured settings and store them in the database

    :param jStr: a JSON object (format: list of dictionaries: {'name': <statusName>, 'value': <statusValue>})
    """
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
    SETTING_VALS = sqlToDictList(c.fetchall())
    conn.close()


def sqlToDictList(results):
    """
    Converts the output of an SQL query to a list of dictionaries

    :param results: Output of a SQL query (i.e. from a 'fetchall' command)
    :returns: A list of dictionaries containing the data from the query
    """
    outList = []

    for r in results:
        dictObj = {}
        dictObj['name'] = r[0]
        dictObj['value'] = r[1]
        outList.append(dictObj)

    return outList


def loadLocalSettings():
    """Loads local settings from the sqlite database"""
    global SETTING_VALS

    conn = sqlite3.connect('data/settings.db')
    c = conn.cursor()
    c.execute('SELECT * FROM settings')
    valStr = c.fetchall()
    SETTING_VALS = sqlToDictList(valStr)
    conn.close()


def updateLocalSettings():
    """Updates web server settings like the UDP IP address, port, and the timeout"""
    global UDP_IP
    global UDP_PORT
    global UDP_TIMEOUT

    UDP_IP = str(getSetting('udpIP'))
    UDP_PORT = int(getSetting('udpPort'))
    UDP_TIMEOUT = float(getSetting('udpTimeout'))


def getSetting(name):
    """
    Returns the value of a specified setting

    :param name: string containing the name of the desired setting
    :returns: value of the desired setting
    """
    global SETTING_VALS

    for val in SETTING_VALS:
        if val['name'] == name:
            return val['value']

    return None


def setStatus(name, value):
    """
    Adds status tuple to local buffer status values

    :param name: Name of status to add
    :param value: Value of named status
    """
    global BUF_STATUS_VALS

    for val in BUF_STATUS_VALS:
        if val['name'] == name:
            val['value'] = value
        else:
            val['value'] = False


def receiveStatus():
    """Receives the current status from the controller and stores it in memory"""
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
    """Sends current stored status to the controller"""
    global BUF_STATUS_VALS
    global UDP_IP
    global UDP_PORT

    jStr = json.dumps(BUF_STATUS_VALS)
    print("sendStatus: " + str(BUF_STATUS_VALS))
    print('Sending message: "' + jStr + '" to IP: ' +
        str(UDP_IP) + ':' + str(UDP_PORT))

    bitWiseValue = 0
    # convert to a bitwise operator:
    for var in BUF_STATUS_VALS:
        if var['name'] == "alarmStatus":
            bitWiseValue = bitWiseValue | (0x1 & var['value'])
        if var['name'] == "doorStatus":
            bitWiseValue = bitWiseValue | (var['value'] << 1)
        if var['name'] == "lightStatus":
            bitWiseValue = bitWiseValue | (var['value'] << 2)

    cmdStr = '{CMD:' + chr(bitWiseValue) + '}'
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(cmdStr.encode('utf-8'), (UDP_IP, UDP_PORT))


if __name__ == '__main__':
    socketio.run(app, debug=True, port=5555, host='0.0.0.0')
