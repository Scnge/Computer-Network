import os
from gtts import gTTS

# 禁用代理
os.environ["HTTP_PROXY"] = ""
os.environ["HTTPS_PROXY"] = ""

text = "你好，我叫才子桓，是一名22级网络空间安全学院的本科生。这是我的计算机网络的实验二的web界面。"
language = 'zh'
tts = gTTS(text=text, lang=language)
tts.save("output.mp3")
print("success")