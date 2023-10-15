import librosa
import librosa.display
import matplotlib.pyplot as plt
import numpy as np

# Load the audio file
audio_file = './sample_audios/gun_recorded.wav' 
y, sr = librosa.load(audio_file)

# Calculate the spectrogram
D = librosa.amplitude_to_db(np.abs(librosa.stft(y)), ref=np.max)

# Set the backend to 'Agg' for non-GUI rendering
plt.switch_backend('agg')

# Display the spectrogram
plt.figure(figsize=(10, 6))
librosa.display.specshow(D, sr=sr, x_axis='time', y_axis='log')
plt.colorbar(format='%+2.0f dB')
plt.title('Spectrogram of Voice')
plt.savefig('./spec/spectrogram.png')  # Save the figure as an image file
