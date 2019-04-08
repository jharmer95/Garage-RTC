from flask import Flask, render_template
from flask_socketio import SocketIO
from datetime import datetime
import json
import sqlite3

app = Flask(__name__)
app.config['SECRET_KEY'] = 'vnkdjnfjknfl1232#'
socketio = SocketIO(app)
conn = sqlite3.connect('data/settings.db')

TEST_VALS = [True, False, True]

@app.route('/')
def sessions():
    return render_template('index.html')


def messageReceived(methods=['GET', 'POST']):
    print('message was received!!!')


@app.route('/status')
def status():
    return render_template('status.html')


@app.route('/chat')
def chat():
    return render_template('chat_example.html')


@socketio.on('my event')
def handle_my_custom_event(jStr, methods=['GET', 'POST']):
    print('received my event: ' + str(jStr))
    socketio.emit('my response', jStr, callback=messageReceived)


@socketio.on('toggle')
def handle_toggle_event(jStr):
    print(jStr)
    setPins(jStr)
    socketio.emit('update pins', jStr, callback=messageReceived)


@socketio.on('pull pins')
def handle_pull_pins_event():
    global TEST_VALS

    updatePins()

    pinDict = [{'id': 'btn1', 'val': TEST_VALS[0]}, {'id': 'btn2', 'val': TEST_VALS[1]}, {'id': 'btn3', 'val': TEST_VALS[2]}]

    jStr = json.dumps(pinDict)
    print(jStr)
    socketio.emit('update pins', jStr, callback=messageReceived)


@socketio.on('fetch settings')
def handle_fetch_settings_event():
    global conn

    c = conn.cursor()
    c.execute('SELECT * FROM settings')
    jStr = json.dumps(c.fetchall())
    socketio.emit('send settings', jStr, callback=messageReceived)


@socketio.on('change settings')
def handle_change_settings_event(jStr):
    global conn

    c = conn.cursor()
    
    for setting in jStr:
        name = setting['name']
        val = setting['value']
        c.execute('UPDATE settings SET value=? WHERE name=?', val, name)
    
    conn.commit()


def updatePins():
    global TEST_VALS

    with open('data/pins.txt') as fp:
        lineCount = 0
        for line in fp:
            if lineCount > len(TEST_VALS):
                break
            TEST_VALS[lineCount] = bool(int(line))
            lineCount += 1


def setPins(jStr):
    global TEST_VALS

    lineNum = None

    if jStr['id'] == 'btn1':
        lineNum = 0
    elif jStr['id'] == 'btn2':
        lineNum = 1
    elif jStr['id'] == 'btn3':
        lineNum = 2

    if lineNum == None:
        return

    TEST_VALS[lineNum] = jStr['val']
    writeVals()


def writeVals():
    global TEST_VALS

    wrtStr = ''

    for val in TEST_VALS:
        wrtStr += str(int(val)) + '\n'

    with open('data/pins.txt', 'w') as fp:
        fp.write(wrtStr)


if __name__ == '__main__':
    socketio.run(app, debug=True, port=5555, host='0.0.0.0')
