from flask import Flask,request , jsonify ,json
import wave
app = Flask(__name__)

wf = wave.open('audio.wav', 'wb') # Open a WAV file for writing
wf.setnchannels(1) # Set the number of channels to 1 (mono)
wf.setsampwidth(2) # Set the sample width to 2 bytes (16 bits)
wf.setframerate(44100) # Set the sample rate to 44100 Hz

@app.route('/i2s_samples', methods = ['POST'])
def api_message():

    if request.headers['Content-Type'] == 'text/plain':
        print("Text Message: " + request.data)

    elif request.headers['Content-Type'] == 'application/json':
        return "JSON Message: " + json.dumps(request.json)

    elif request.headers['Content-Type'] == 'application/octet-stream':
        # print((request.data))
        wf.writeframes(request.data)
        print("Binary message written!")
        return "ok"

    else:
        return "415 Unsupported Media Type ;)"


if __name__ == '__main__':
    app.run(host="0.0.0.0",port=5003)
