#!/usr/bin/python
# -*- encoding: utf-8 -*-
from __future__ import division

from PIL import Image

def splitnum(k, x):
    parts = [0] * k
    for n in range(24):
        parts[n % k] |= ((x >> n) & 1) << (n // k)
    return tuple(parts)

im = Image.new("RGB", (4096, 4096))
pix = im.load()
for n in range(2**24):
    pix[splitnum(2, n)] = splitnum(3, n)
im.save("the-universal-tartan.png")
