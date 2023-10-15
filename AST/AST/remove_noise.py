from pydub import AudioSegment

# load the audio file
audio_file = AudioSegment.from_wav("./recorded/11.wav")

# apply a low-pass filter to remove high-frequency noise
filtered_audio = audio_file.low_pass_filter(1200)

# export the filtered audio to a new file
filtered_audio.export("./recorded/11_rem_noise.wav", format="wav")
