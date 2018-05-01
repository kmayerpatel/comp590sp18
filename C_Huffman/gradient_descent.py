#!/usr/bin/python3
import sys
from ctypes import c_ubyte
from numpy import linalg, transpose, dot

# Constants ***COMP 590-080 SPECIFIC***
HD_SIZE = 1280 * 720
SD_SIZE = 800 * 450
SUBDIVISIONS = 4
NUM_FRAMES = 150 * SUBDIVISIONS # TODO: Fix

# Parse command-line arguments
SIZE = HD_SIZE if "--hd" in sys.argv else SD_SIZE
SIZE //= SUBDIVISIONS
if len(sys.argv) < 2:
    print("Usage: " + sys.argv[0] + " <video_filename>")
    exit(1)

# Both arrays are FRAMES*SIZE
#frame_diffs = []
int_frame_diffs = []
#total_match_error_old = 0
#total_match_error_new = 0
#total_match_error_newf = 0

# Load frames, generate diffs, and compute coefficents
src_f = open(sys.argv[1], "rb")
out_f = open(sys.argv[1] + "_output", "wb")
last_f = bytearray()
for frame_num in range(0, NUM_FRAMES - 1):
    this_f = src_f.read(SIZE)
    print("Loaded frame " + str(frame_num))
    # Compute frame diffs for every frame after zero
    if frame_num > 0:
#        new_diff = bytearray()
        int_diff = []
        for byte_idx in range(0, SIZE):
#            diff_byte = c_ubyte(last_f[byte_idx] - this_f[byte_idx]).value
#            new_diff.append(diff_byte) # This weirdness keeps the diffs in range
            int_diff.append(last_f[byte_idx] - this_f[byte_idx])
        if frame_num > 2:
            diffs_transpose = transpose(int_frame_diffs)
#print("M_new: " + str(len(new_diff)) + " M_old: " + str(len(frame_diffs[0])) + " N_old: " + str(len(frame_diffs)))
            lsqres = linalg.lstsq(diffs_transpose, int_diff, rcond=None)
            coef = lsqres[0]
            err = lsqres[1].sum()
            projected_diff = dot(diffs_transpose, coef)
#            error_int = 0
#            error = 0
#            for byte_idx in range(0, SIZE):
#                error += abs(int_diff[byte_idx] - projected_diff[byte_idx])
#                error_int += abs(int_diff[byte_idx] - int(round(projected_diff[byte_idx])))
            if err is not 0:
                for byte_idx in range(0, SIZE):
                    out_f.write(c_ubyte(int_diff[byte_idx] - int(round(projected_diff[byte_idx]))))
            else:
                print("Full basis available. Not saving empty diff frame.")
#            print("Match error of least squares: {:>20d} {:>20f}".format(error_int, error))
#            error_old = 0
#            if frame_num > SUBDIVISIONS:
#                for byte_idx in range(0, SIZE):
#                    error_old += abs(new_diff[byte_idx] - frame_diffs[-SUBDIVISIONS][byte_idx])
#            print("Match error of old approach:  {:>20d}".format(error_old))
#            total_match_error_new += error_int
#            total_match_error_old += error_old
#            total_match_error_newf += error
#        frame_diffs.append(new_diff)
        int_frame_diffs.append(int_diff)
    last_f = this_f
# frames and frame_diffs arrays now populated to size NUM_FRAMES and NUM_FRAMES - 1 respectively
#print("Total match error of least squares: {:>20d} {:>20f}".format(total_match_error_new, total_match_error_newf))
#print("Total match error of old approach:  {:>20d}".format(total_match_error_old))
