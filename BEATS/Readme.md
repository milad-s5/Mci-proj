# Run in terminal
```
python3 -m venv venv
source venv/bin/activate
pip install --upgrade pip

git clone https://github.com/microsoft/unilm
cp -r ./unilm/beats .
cd beats
curl -C - -o BEATs_iter3_plus_AS2M_finetuned_on_AS2M_cpt1.pt "https://valle.blob.core.windows.net/share/BEATs/BEATs_iter3_plus_AS2M_finetuned_on_AS2M_cpt1.pt?sv=2020-08-04&st=2023-03-01T07%3A51%3A05Z&se=2033-03-02T07%3A51%3A00Z&sr=c&sp=rl&sig=QJXmSJG9DbMKf48UDIU1MfzIro8HQOf3sqlNXiflY1I%3D"
curl -C - -o BEATs_iter3_plus_AS2M_finetuned_on_AS2M_cpt2.pt "https://valle.blob.core.windows.net/share/BEATs/BEATs_iter3_plus_AS2M_finetuned_on_AS2M_cpt2.pt?sv=2020-08-04&st=2023-03-01T07%3A51%3A05Z&se=2033-03-02T07%3A51%3A00Z&sr=c&sp=rl&sig=QJXmSJG9DbMKf48UDIU1MfzIro8HQOf3sqlNXiflY1I%3D"
curl -C - -o class_labels_indices.csv 'http://storage.googleapis.com/us_audioset/youtube_corpus/v1/csv/class_labels_indices.csv'
```