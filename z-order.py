#!/usr/bin/python
# -*- encoding: utf-8 -*-
from __future__ import division

from PIL import Image
im = Image.new("RGB", (4096, 4096))
pix = im.load()

def splitnum(k, x):
    parts = [0] * k
    for n in range(24):
        bit = (x & (1 << n)) >> n
        q, r = divmod(n, k)
        parts[r] |= bit << q
    
    return parts

percent = -1
for n in range(2**24):
    x, y = splitnum(2, n)
    r, g, b = splitnum(3, n)
    pix[x, y] = (r, g, b)
    new_percent = n * 100 // 0x1000000
    if new_percent > percent:
        print "%d%% done" % new_percent
        percent = new_percent
        im.save("z-order.png")

im.save("z-order.png")
