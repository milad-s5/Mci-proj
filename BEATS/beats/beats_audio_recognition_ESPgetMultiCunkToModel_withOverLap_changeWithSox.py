import numpy as np
import pandas as pd
import torch
import torchaudio
from BEATs import BEATs, BEATsConfig
import sox 

# Define the original sample rate of the input audio
orig_sample_rate = 44100

# Mapping mid to display_name
df = pd.read_csv('class_labels_indices.csv')
mid_to_display = {mid: display for mid, display in zip(df['mid'], df['display_name'])}

# load the fine-tuned checkpoints
checkpoint = torch.load('./BEATs_iter3_plus_AS2M_finetuned_on_AS2M_cpt1.pt')

cfg = BEATsConfig(checkpoint['cfg'])
BEATs_model = BEATs(cfg)
BEATs_model.load_state_dict(checkpoint['model'])
BEATs_model.eval()

print("model loaded!")

def resample_audio(audio_input, orig_sample_rate, target_sample_rate):
    torchaudio.save('audio1.wav', audio_input, orig_sample_rate)
    audio_file = 'audio1.wav'

    # set input audio sampling rate must to 16kHz
    tfm = sox.Transformer()
    tfm.set_input_format(file_type='wav')
    tfm.set_output_format(rate=target_sample_rate)
    out_name = 'sound-16k.wav'
    tfm.build(audio_file, out_name)

    resampled_audio, _ = torchaudio.load(out_name)    
    return resampled_audio

def SED(audio_input, target_sample_rate=16000):
    # Resample the audio to the target sample rate
    audio_input = audio_input * 10
    audio_input_resampled = resample_audio(audio_input, orig_sample_rate, target_sample_rate)

    # Save the resampled audio to a WAV file
    padding_mask = torch.zeros(audio_input_resampled.shape[0], audio_input_resampled.shape[1]).bool()
    probs = BEATs_model.extract_features(audio_input_resampled, padding_mask=padding_mask)[0]

    top5_label_prob_list = []
    display_names_list = []

    for i, (top5_label_prob, top5_label_idx) in enumerate(zip(*probs.topk(k=5))):
        top5_label = [checkpoint['label_dict'][label_idx.item()] for label_idx in top5_label_idx]
        display_names = [mid_to_display[mid] for mid in top5_label]

        top5_label_prob_list.append(top5_label_prob)
        display_names_list.append(display_names)

        
    return display_names_list, top5_label_prob_list

# Global variables to keep track of received audio chunks
received_chunks = []
chunk_counter = 0

# Function to process and reset received audio chunks
def process_received_chunks():
    global received_chunks, chunk_counter

    # if len(received_chunks) == 3:
    concatenated_audio = torch.cat(received_chunks, dim=1)  # Concatenate two received chunks
    display_names, top5_label_prob = SED(concatenated_audio)

    for i, (display_names_i, top5_label_prob_i) in enumerate(zip(display_names, top5_label_prob)):
        if display_names_i[0] in ["Gunshot, gunfire", "Machine gun", "Cap gun"]:
            print(f"\033[91mTop 5 predicted labels of the {i}th audio are {display_names_i[0]} with probabilities {top5_label_prob_i[0]}\033[0m")
        else:
            print(f"Top 5 predicted labels of the {i}th audio are {display_names_i[0]} with probabilities {top5_label_prob_i[0]}")
    # Reset the received chunks and counter
    # received_chunks = []
    received_chunks = received_chunks[-1:]
    chunk_counter = 0


from flask import Flask,request , jsonify ,json
import wave
app = Flask(__name__)

wf = wave.open('audio.wav', 'wb') # Open a WAV file for writing
wf.setnchannels(1) # Set the number of channels to 1 (mono)
wf.setsampwidth(2) # Set the sample width to 2 bytes (16 bits)
wf.setframerate(orig_sample_rate) # Set the sample rate to 44100 Hz

@app.route('/i2s_samples', methods = ['POST'])
def api_message():
    global received_chunks, chunk_counter
    
    if request.headers['Content-Type'] == 'text/plain':
        print("Text Message: " + request.data)

    elif request.headers['Content-Type'] == 'application/json':
        return "JSON Message: " + json.dumps(request.json)

    elif request.headers['Content-Type'] == 'application/octet-stream':
        # print((request.data))
        audio_input = request.data
        audio_input = np.frombuffer(audio_input, dtype=np.int16)  
        audio_input = torch.tensor(audio_input, dtype=torch.float32) / 32767.0 
        audio_input = audio_input.view(1, -1) 

        # Add the received audio chunk to the list
        received_chunks.append(audio_input)
        chunk_counter += 1

        if chunk_counter == 4:  # Concatenate and process after two received chunks
            process_received_chunks()



        wf.writeframes(request.data)
        print("Binary message written!")
        return "ok"

    else:
        return "415 Unsupported Media Type ;)"


if __name__ == '__main__':
    app.run(host="0.0.0.0",port=5003)
