#!/usr/bin/env python3
import os
for d in os.listdir("."):
    if not os.path.isdir(d):
        continue
    for a in os.listdir(d):
        for i in os.listdir(f"{d}/{a}"):
            os.rename(f"{d}/{a}/{i}", f'{d}/{a}/{i.replace("deepin", "gxde")}')
