from pydub import AudioSegment

# load the audio file
audio_file = AudioSegment.from_wav("./recorded/l.wav")

# amplify the audio by 6 decibels
amplified_audio = audio_file + 16

# export the amplified audio to a new file
amplified_audio.export("./recorded/l22_amp_rem_noise.wav", format="wav")
