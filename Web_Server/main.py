from flask import Flask, render_template
from flask_socketio import SocketIO
from datetime import datetime
import json

app = Flask(__name__)
app.config['SECRET_KEY'] = 'vnkdjnfjknfl1232#'
socketio = SocketIO(app)

TEST_VALS = [True, False, True]
MUTEX = False

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
def handle_my_custom_event(json, methods=['GET', 'POST']):
    #print('received my event: ' + str(json))
    socketio.emit('my response', json, callback=messageReceived)


@socketio.on('toggle')
def handle_toggle_event(json):
    print(json)

    setPins(json)

    socketio.emit('update pins', json, callback=messageReceived)


@socketio.on('pull pins')
def handle_pull_pins_event():
    global TEST_VALS

    updatePins()

    pinDict = [{'id': 'btn1', 'val': TEST_VALS[0]}, {'id': 'btn2', 'val': TEST_VALS[1]}, {'id': 'btn3', 'val': TEST_VALS[2]}]

    jStr = json.dumps(pinDict)
    print(jStr)
    socketio.emit('update pins', jStr, callback=messageReceived)


def updatePins():
    global TEST_VALS
    global MUTEX

    if MUTEX == True:
        return
    else:
        MUTEX = True

    with open('data/pins.txt') as fp:
        lineCount = 0
        for line in fp:
            if lineCount > len(TEST_VALS):
                break
            TEST_VALS[lineCount] = bool(int(line))
            lineCount += 1

    MUTEX = False


def setPins(json):
    global TEST_VALS

    lineNum = None

    #print('ID: ' + json['id'] + ' VAL: ' + str(json['val']))

    if json['id'] == 'btn1':
        lineNum = 0
    elif json['id'] == 'btn2':
        lineNum = 1
    elif json['id'] == 'btn3':
        lineNum = 2

    if lineNum == None:
        return

    TEST_VALS[lineNum] = json['val']
    writeVals()


def writeVals():
    global TEST_VALS
    global MUTEX

    if MUTEX == True:
        return
    else:
        MUTEX = True

    wrtStr = ''

    for val in TEST_VALS:
        wrtStr += str(int(val)) + '\n'

    #print(wrtStr)

    with open('data/pins.txt', 'w') as fp:
        fp.write(wrtStr)

    MUTEX = False


if __name__ == '__main__':
    socketio.run(app, debug=True, port=5555, host='0.0.0.0')
