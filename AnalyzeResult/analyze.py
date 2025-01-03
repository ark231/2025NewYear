#!/usr/bin/env python3

# https://stackoverflow.com/questions/42163058/how-to-turn-a-video-into-numpy-array

import cv2
import numpy as np
import numpy.linalg as linalg

import sys
from rich.progress import Progress


# def im_brightness(im):
#     return np.array([np.array([linalg.norm(px) for px in row]) for row in im])
#


def im_redness(im):
    return np.array([np.array([px[0] for px in row]) for row in im])


cap = cv2.VideoCapture(sys.argv[1])
frameCount = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
frameWidth = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
frameHeight = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))

meanbuf = np.empty((frameHeight, frameWidth, 3), float)
maxbuf = np.empty((frameHeight, frameWidth, 3), float)
redbuf = np.empty((frameHeight, frameWidth, 3), float)
fc = 0
ret = True
with Progress() as progress:
    task = progress.add_task("load image", total=frameCount)

    while fc < frameCount and ret:
        ret, newbuf = cap.read()
        meanbuf += newbuf
        # maxpos = np.where(im_brightness(maxbuf) < im_brightness(newbuf))
        # maxbuf[maxpos] = newbuf[maxpos]
        redpos = np.where(im_redness(redbuf) < im_redness(newbuf))
        redbuf[redpos] = newbuf[redpos]
        progress.update(task, advance=1)
        fc += 1

cap.release()

meanbuf = (meanbuf / frameCount).astype(np.uint8)
maxbuf = maxbuf.astype(np.uint8)
redbuf = redbuf.astype(np.uint8)

cv2.imwrite(f"{sys.argv[1]}_mean.png", meanbuf)
cv2.imwrite(f"{sys.argv[1]}_max.png", maxbuf)
cv2.imwrite(f"{sys.argv[1]}_red.png", redbuf)

# cv2.namedWindow("averaged")
# cv2.imshow("averaged", meanbuf)
# cv2.waitKey(0)
#
# cv2.namedWindow("max")
# cv2.imshow("max", maxbuf)
#
# cv2.waitKey(0)
