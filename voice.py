import queue, json, subprocess
import sounddevice as sd
from vosk import Model, KaldiRecognizer
MODEL = "/home/firstteam/sanad/models/vosk-model-small-en-us-0.15"
q = queue.Queue()
def cb(indata, frames, t, status): q.put(bytes(indata))
model = Model(MODEL)
rec = KaldiRecognizer(model, 48000)
print("Listening...", flush=True)
with sd.RawInputStream(samplerate=48000, blocksize=24000, device=2, dtype="int16", channels=1, callback=cb):
    while True:
        data = q.get()
        if rec.AcceptWaveform(data):
            txt = json.loads(rec.Result()).get("text", "").strip()
            if txt and txt not in ["huh","hm","hmm","uh","ah","oh","the","a"]:
                print("HEARD:", txt, flush=True)
                if "brow" in txt or "chrome" in txt or "internet" in txt:
                    subprocess.Popen(["bash","-c","pgrep -x chromium >/dev/null || chromium &"])
                elif "close" in txt:
                    subprocess.Popen(["wtype", "-M", "alt", "-k", "F4", "-m", "alt"])
                else:
                    subprocess.Popen(["wtype", txt + " "])
