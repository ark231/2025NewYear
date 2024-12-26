#!/usr/bin/env python3

with open("ttf.py", "r", encoding="utf8") as file:
    data = file.read()
data.replace("\r\n", "\n")
with open("ttf.py", "w", encoding="utf8") as file:
    file.write(data)
