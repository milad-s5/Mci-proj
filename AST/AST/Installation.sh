# install python 3.8
python3.8 -m venv venv
source venv/bin/activate
which python3.8
python3.8 -V
pip install --upgrade pip
# pip freeze > requirements.txt
pip --default-timeout=100 install -r ./requirements.txt 
curl -C - -L -o './pretrained_models/audio_mdl.pth' 'https://www.dropbox.com/s/cv4knew8mvbrnvq/audioset_0.4593.pth?dl=1'
python ./AST_Inference_Demo_CPU.py
deactivate
